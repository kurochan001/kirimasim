#!/usr/bin/env python3
# Copyright (c) 2026 The kirimasim Authors
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
"""Which lines of src/ tests/suite/ reaches and kirimasim-tests does not, and which
tests that a port could take reach them.

Build kirimasim with clang source-based coverage on src/ alone, then run this:

  printf '[clang]\\nsource:<checkout>/src/*=allow\\ndefault:skip\\n' > <list>
  CC=clang CXX=clang++ cmake --preset dev -B <build> \\
      -DCMAKE_CXX_FLAGS_DEBUG="-O1 -g -fprofile-instr-generate -fcoverage-mapping -fprofile-list=<list>" \\
      -DCMAKE_EXE_LINKER_FLAGS=-fprofile-instr-generate
  cmake --build <build> --target kirimasim
  python3 coverage.py --kirimasim <checkout> --tests <checkout> --binary <kirimasim> \\
      --llvm-bin <folder> --outcomes <outcomes.tsv of measure.py> --work <folder> --out <folder>

A line reached is not a behaviour checked: a test can reach a line and judge nothing
about it, and a line can hold several behaviours. A test that fails still counts, and
kirimasim-tests' tests with :dpi_c: or :vpi_c: are not run by its kirimasim runner.
The measure is of one implementation, and it changes as kirimasim changes.

Two readings of "a control simulator confirms the expected value" are reported.
The strict one is that of kirimasim-tests: Icarus Verilog gives the expected output,
or Verilator does for a :fourstate: no test, as Verilator has two states. The broad
one also takes Verilator's output for a :fourstate: yes test. The first candidates
are chosen with the strict reading.

The output holds only paths relative to the checkout.
"""

import argparse
import concurrent.futures
import json
import os
import shutil
import subprocess
from collections import Counter
from pathlib import Path

from measure import split_test

PORTABLE = ("stdout_only", "several_allowed_outputs")
Line = tuple[str, int]


def parse_lcov(text: str, root: str) -> dict[Line, bool]:
    """Each line of src/, whether it ran."""
    found: dict[Line, bool] = {}
    source = None
    for line in text.splitlines():
        if line.startswith("SF:"):
            name = line[3:]
            source = name[len(root) + 1 :] if name.startswith(root + "/src/") else None
        elif line.startswith("DA:") and source:
            number, count = line[3:].split(",")[:2]
            key = (source, int(number))
            found[key] = found.get(key, False) or int(count) > 0
    return found


class Profiles:
    def __init__(self, binary: str, llvm_bin: str, root: str):
        self.binary, self.llvm_bin, self.root = binary, llvm_bin, root

    def lines(self, raw: list[Path], merged: Path) -> dict[Line, bool]:
        subprocess.run(
            [f"{self.llvm_bin}/llvm-profdata", "merge", "-sparse", *map(str, raw), "-o", str(merged)],
            check=True, capture_output=True,
        )
        text = subprocess.run(
            [
                f"{self.llvm_bin}/llvm-cov", "export", "-format=lcov", "-skip-branches",
                "-skip-expansions", f"-instr-profile={merged}", self.binary,
            ],
            check=True, capture_output=True, text=True, errors="replace",
        ).stdout
        return parse_lcov(text, self.root)


def run_suite_test(root: Path, binary: str, test: str, profile: str) -> None:
    subprocess.run(
        [
            "python3", "tools/run_suite_test.py", "--kirimasim", binary,
            "--diagnostic-profile=slang-main", test,
        ],
        cwd=root, env={**os.environ, "LLVM_PROFILE_FILE": profile}, capture_output=True,
    )


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--kirimasim", type=Path, required=True)
    ap.add_argument("--tests", type=Path, required=True)
    ap.add_argument("--binary", required=True)
    ap.add_argument("--llvm-bin", required=True)
    ap.add_argument("--outcomes", type=Path, required=True)
    ap.add_argument("--work", type=Path, required=True)
    ap.add_argument("--out", type=Path, required=True)
    ap.add_argument("-j", "--jobs", type=int, default=16)
    args = ap.parse_args()
    root = args.kirimasim.resolve()
    profiles = Profiles(args.binary, args.llvm_bin, str(root))
    shutil.rmtree(args.work, ignore_errors=True)
    for name in ("suite", "tests", "each"):
        (args.work / name).mkdir(parents=True)

    rows = [line.rstrip("\n").split("\t") for line in args.outcomes.read_text().splitlines()]
    with concurrent.futures.ThreadPoolExecutor(args.jobs) as pool:
        pool_profile = str(args.work / "suite/%8m.profraw")
        list(pool.map(lambda r: run_suite_test(root, args.binary, r[0], pool_profile), rows))
    runner = subprocess.run(
        ["python3", "tools/run_kirimasim.py", "--kirimasim", args.binary, "-j", str(args.jobs)],
        cwd=args.tests.resolve(), capture_output=True, text=True, errors="replace",
        env={**os.environ, "LLVM_PROFILE_FILE": str(args.work / "tests/%8m.profraw")},
    )
    runner_summary = [line for line in runner.stdout.splitlines() if " tests match" in line][-1:]
    suite = profiles.lines(sorted((args.work / "suite").iterdir()), args.work / "suite.profdata")
    other = profiles.lines(sorted((args.work / "tests").iterdir()), args.work / "tests.profdata")
    in_suite = {k for k, ran in suite.items() if ran}
    in_other = {k for k, ran in other.items() if ran}
    only = in_suite - in_other

    def per_test(row: list[str]) -> tuple[str, set[Line]]:
        name = row[0].replace("/", "__")
        raw = args.work / "each" / f"{name}.profraw"
        merged = args.work / "each" / f"{name}.profdata"
        run_suite_test(root, args.binary, row[0], str(raw))
        reached = {k for k, ran in profiles.lines([raw], merged).items() if ran}
        raw.unlink()
        merged.unlink()
        return row[0], reached & only

    portable = [r for r in rows if r[1] in PORTABLE]
    with concurrent.futures.ThreadPoolExecutor(args.jobs) as pool:
        reach = dict(pool.map(per_test, portable))

    two_state = {
        r[0]: split_test((root / r[0]).read_text(encoding="utf-8", errors="ignore"))[0].get("fourstate") == "no"
        for r in portable
    }

    def strict(row: list[str]) -> bool:
        return row[3] == "matches" or (row[4] == "matches" and two_state[row[0]])

    def broad(row: list[str]) -> bool:
        return "matches" in (row[3], row[4])

    def union(selected: list[list[str]]) -> set[Line]:
        return set().union(*[reach[r[0]] for r in selected]) if selected else set()

    def greedy(candidates: list[list[str]]) -> list[tuple[str, int]]:
        left = union(candidates)
        pool_left = {r[0]: reach[r[0]] for r in candidates if reach[r[0]]}
        chosen = []
        while left:
            best = min(pool_left, key=lambda p: (-len(pool_left[p] & left), p))
            gain = pool_left[best] & left
            left -= gain
            chosen.append((best, len(gain)))
            del pool_left[best]
        return chosen

    generator_of = {r[0]: r[2] for r in rows}
    strict_tests = [r for r in portable if strict(r)]
    broad_tests = [r for r in portable if broad(r)]
    chosen = greedy(strict_tests)
    chosen_broad = greedy(broad_tests)

    def share(picked: list[tuple[str, int]]) -> list[dict[str, int]]:
        return [
            {"tests": n, "lines": sum(g for _, g in picked[:n])}
            for n in (5, 10, 25, 50, 100, 200, 500)
            if n < len(picked)
        ] + [{"tests": len(picked), "lines": sum(g for _, g in picked)}]

    libraries: dict[str, Counter] = {}
    for key in set(suite) | set(other):
        row = libraries.setdefault(key[0].split("/")[1], Counter())
        a, b = key in in_suite, key in in_other
        row["lines"] += 1
        row["both"] += a and b
        row["only_tests_suite"] += a and not b
        row["only_kirimasim_tests"] += b and not a
        row["neither"] += not a and not b

    def family_gain(picked: list[tuple[str, int]]) -> list[list]:
        tests: Counter = Counter()
        lines: Counter = Counter()
        for path, gain in picked:
            tests[generator_of[path]] += 1
            lines[generator_of[path]] += gain
        return [[g, tests[g], n] for g, n in lines.most_common()]

    files = Counter(k[0] for k in only)
    missing = only - union(portable)
    summary = {
        "kirimasim_tests_runner": runner_summary,
        "lines": len(set(suite) | set(other)),
        "reached_by_tests_suite": len(in_suite),
        "reached_by_kirimasim_tests": len(in_other),
        "only_tests_suite": len(only),
        "only_kirimasim_tests": len(in_other - in_suite),
        "by_library": {k: dict(v) for k, v in sorted(libraries.items())},
        "only_tests_suite_by_file": files.most_common(25),
        "portable_tests": len(portable),
        "only_lines_reached_by_portable_tests": len(union(portable)),
        "only_lines_no_portable_test_reaches_by_file": Counter(k[0] for k in missing).most_common(15),
        "strict": {
            "confirmed_tests": len(strict_tests),
            "only_lines_reached": len(union(strict_tests)),
            "first_candidates_curve": share(chosen),
            "first_candidates_by_generator": family_gain(chosen),
        },
        "broad": {
            "confirmed_tests": len(broad_tests),
            "only_lines_reached": len(union(broad_tests)),
            "first_candidates_curve": share(chosen_broad),
        },
    }
    args.out.mkdir(parents=True, exist_ok=True)
    (args.out / "coverage.json").write_text(json.dumps(summary, indent=1) + "\n")
    (args.out / "first_candidates.txt").write_text(
        "".join(f"{gain}\t{path}\n" for path, gain in chosen)
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
