#!/usr/bin/env python3
# Copyright (c) 2026 The kirimasim Authors
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

"""Compare simulation observables from two kirimasim builds."""

import argparse
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


def run(binary, args, work):
    result = subprocess.run(
        [binary, "--stats=json", *args], cwd=work, stdin=subprocess.DEVNULL,
        capture_output=True, timeout=120, check=False,
    )
    stderr = BANNER.sub(rb"\1<build>\2", result.stderr)
    return result.returncode, result.stdout, stderr, output_files(work)


def compare(case, baseline, candidate):
    name, mode, args, wave = case
    try:
        with tempfile.TemporaryDirectory(prefix="kirimasim-compare-") as directory:
            work = Path(directory) / "run"
            work.mkdir()
            before = run(baseline, args, work)
            shutil.rmtree(work)
            work.mkdir()
            after = run(candidate, args, work)
    except (OSError, subprocess.TimeoutExpired) as error:
        return {"test": name, "mode": mode, "differences": [type(error).__name__]}
    fields = ("exit code", "stdout (including stats JSON)", "stderr")
    differences = [field for field, a, b in zip(fields, before, after) if a != b]
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
    parser.add_argument("--suite", action="store_true", help="include all simulation fixtures")
    parser.add_argument("--bench-dir", type=Path, action="append", default=[])
    parser.add_argument("--jobs", type=int, default=8)
    parser.add_argument("--output", type=Path, help="save the comparison results as JSON")
    args = parser.parse_args()
    if args.jobs < 1 or not (args.suite or args.bench_dir):
        parser.error("choose inputs and a positive job count")
    roots = [path.resolve() for path in args.bench_dir]
    if any(not path.is_dir() or not any(path.glob("*.sv")) for path in roots):
        parser.error("each benchmark directory must have at least one .sv file")
    baseline, candidate = args.baseline.resolve(), args.candidate.resolve()
    cases = list(suite_cases()) if args.suite else []
    cases.extend(benchmark_cases(roots))
    with ThreadPoolExecutor(max_workers=args.jobs) as pool:
        failures = [failure for failure in pool.map(
            lambda case: compare(case, baseline, candidate), cases) if failure is not None]
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
