#!/usr/bin/env python3
# Copyright (c) 2026 The kirimasim Authors
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

"""Compare simulation observables from two kirimasim builds."""

import argparse
from functools import partial
from concurrent.futures import ThreadPoolExecutor
import hashlib
import json
from pathlib import Path
import re
import shutil
import subprocess
import tempfile

from run_suite_test import other_files, read_header

ROOT = Path(__file__).resolve().parent.parent
# The revision and version identify the executable independently of simulation.
BANNER = re.compile(rb"(?m)^(kirimasim: info: )kirimasim [^\n]+?( seed=\d+ args:)")
# Only normalize the writer's date and build version at the start of a VCD.
VCD_HEADER = re.compile(rb"\A\$date\n\t[^\n]*\n\$end\n\$version\n\tkirimasim [^\n]+\n")
OPTIONS = {
    "default": (),
    "steps": ("--max-steps=1000",),
    "events": ("--max-slot-events=1000",),
    "zero-delay": ("--max-zero-delay=1",),
    "bounds": ("--bounds-check=warn",),
    "wave": ("--wave=compare.vcd",),
    "random": ("--init-values=random", "--seed=271828"),
}


def suite_cases():
    suite = ROOT / "tests/suite"
    for path in sorted(suite.rglob("*.sv")):
        meta, problems = read_header(path)
        if problems:
            raise ValueError(f"{path.relative_to(suite)}: {problems}")
        if "simulation" not in meta.get("type", "").split():
            continue
        tops = [f"--top={name}" for name in meta.get("top_module", "").split()]
        args = [*tops, *other_files(path, meta), str(path), *meta.get("args", "").split()]
        yield str(path.relative_to(suite)), "default", tuple(args), False


def benchmark_cases(roots):
    for root in roots:
        for path in sorted(root.glob("*.sv")):
            for count in (1, 300):
                for name, flags in OPTIONS.items():
                    yield path.name, f"{name}/n={count}", (*flags, str(path), f"+n={count}"), name == "wave"


def output_files(work):
    outputs = {}
    for path in work.rglob("*"):
        if path.is_file():
            with path.open("rb") as stream:
                head = stream.read(4096)
                digest = hashlib.sha256(VCD_HEADER.sub(b"<VCD build header>\n", head, count=1))
                for block in iter(lambda: stream.read(65536), b""):
                    digest.update(block)
            outputs[str(path.relative_to(work))] = digest.digest()
    return outputs


PORTABLE_COUNTS = ("sched.", "proc.", "slot.", "nba.", "wait.", "sim.")


def comparable_stdout(stdout, stats):
    if stats not in ("portable", "observables"):
        return stdout
    body, _, last = stdout.rstrip(b"\n").rpartition(b"\n")
    counts = json.loads(last)
    if stats == "observables":
        end = ("sim.end_time", "sim.end_reason")
        if not all(key in counts for key in end):
            raise ValueError("simulation end counters missing")
        return body, {key: counts[key] for key in end}
    return body, {k: v for k, v in counts.items() if k.startswith(PORTABLE_COUNTS)}


def run(binary, extra, args, work, stats, timeout):
    flags = [*extra] if stats == "off" else ["--stats=json", *extra]
    result = subprocess.run(
        [binary, *flags, *args], cwd=work, stdin=subprocess.DEVNULL,
        capture_output=True, timeout=timeout, check=False,
    )
    stderr = BANNER.sub(rb"\1<build>\2", result.stderr, count=1)
    if extra:
        # The driver's banner includes its invocation. Remove exactly the
        # leading mode flags supplied here, retaining every fixture argument.
        banner, sep, rest = stderr.partition(b"\n")
        actual = b" args: " + b" ".join(flag.encode() for flag in flags) + b" "
        kept = b" args: " if stats == "off" else b" args: --stats=json "
        if banner.startswith(b"kirimasim: info: <build> seed="):
            banner = banner.replace(actual, kept, 1)
        stderr = banner + sep + rest
    stdout = result.stdout
    if result.returncode != 3 and result.returncode >= 0:
        stdout = comparable_stdout(stdout, stats)
    return result.returncode, stdout, stderr, output_files(work)


def compare(case, baseline, candidate, baseline_args=(), candidate_args=(), stats="all", timeout=120):
    name, mode, args, wave = case
    try:
        with tempfile.TemporaryDirectory(prefix="kirimasim-compare-") as directory:
            work = Path(directory) / "run"
            work.mkdir()
            before = run(baseline, baseline_args, args, work, stats, timeout)
            shutil.rmtree(work)
            work.mkdir()
            after = run(candidate, candidate_args, args, work, stats, timeout)
    except (OSError, ValueError, subprocess.TimeoutExpired) as error:
        return {"test": name, "mode": mode, "differences": [type(error).__name__]}
    differences = []
    if before[0] != after[0]:
        differences.append("exit code")
    a, b = before[1], after[1]
    if isinstance(a, tuple) and isinstance(b, tuple):
        old_output, old_counts = a
        new_output, new_counts = b
        if old_output != new_output:
            differences.append("stdout")
        differences.extend(key for key in sorted(old_counts.keys() | new_counts.keys())
                           if key not in old_counts or key not in new_counts or
                           old_counts[key] != new_counts[key])
    elif a != b:
        differences.append("stdout (including stats JSON)" if stats == "all" else "stdout")
    if before[2] != after[2]:
        differences.append("stderr")
    codes = (before[0], after[0])
    if 3 in codes:
        differences.append("internal simulator error")
    if any(code < 0 for code in codes):
        differences.append("terminated by signal")
    old_files, new_files = before[3], after[3]
    differences.extend(name for name in sorted(old_files.keys() | new_files.keys())
                       if old_files.get(name) != new_files.get(name))
    if wave and "compare.vcd" not in old_files and "compare.vcd" not in new_files:
        differences.append("missing compare.vcd")
    if differences:
        return {"test": name, "mode": mode, "differences": differences,
                "exit_codes": [before[0], after[0]]}
    return None


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--baseline", type=Path, required=True)
    parser.add_argument("--candidate", type=Path, required=True)
    parser.add_argument("--baseline-arg", action="append", default=[], metavar="FLAG",
                        help="prepend a mode flag to the baseline invocation (repeatable)")
    parser.add_argument("--candidate-arg", action="append", default=[], metavar="FLAG",
                        help="prepend a mode flag to the candidate invocation (repeatable)")
    parser.add_argument("--stats", choices=("all", "off", "portable", "observables"), default="all",
                        help="observables checks HDL output and the final time and reason across schedulers; "
                             "portable also checks scheduling counters for VM/generated execution")
    parser.add_argument("--timeout", type=int, default=120, help="seconds per engine and input")
    parser.add_argument("--suite", action="store_true", help="include all simulation fixtures")
    parser.add_argument("--bench-dir", type=Path, action="append", default=[])
    parser.add_argument("--jobs", type=int, default=8)
    parser.add_argument("--output", type=Path, help="save the comparison results as JSON")
    args = parser.parse_args()
    if args.jobs < 1 or args.timeout < 1 or not (args.suite or args.bench_dir):
        parser.error("choose inputs and a positive job count")
    roots = [path.resolve() for path in args.bench_dir]
    if any(not path.is_dir() or not any(path.glob("*.sv")) for path in roots):
        parser.error("each benchmark directory must have at least one .sv file")
    baseline, candidate = args.baseline.resolve(), args.candidate.resolve()
    cases = list(suite_cases()) if args.suite else []
    cases.extend(benchmark_cases(roots))
    with ThreadPoolExecutor(max_workers=args.jobs) as pool:
        check = partial(compare, baseline=baseline, candidate=candidate, baseline_args=args.baseline_arg,
                        candidate_args=args.candidate_arg, stats=args.stats, timeout=args.timeout)
        failures = [failure for failure in pool.map(check, cases) if failure is not None]
    result = {"comparisons": len(cases), "matched": len(cases) - len(failures),
              "mismatches": failures}
    if args.output:
        args.output.write_text(json.dumps(result, indent=2) + "\n", encoding="utf-8")
    for failure in failures[:20]:
        print(f"{failure['test']} ({failure['mode']}): {', '.join(failure['differences'])}")
    print(f"{result['matched']}/{result['comparisons']} simulation comparisons match")
    return int(bool(failures))


if __name__ == "__main__":
    raise SystemExit(main())
