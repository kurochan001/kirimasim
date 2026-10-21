#!/usr/bin/env python3
# Copyright (c) 2026 The kirimasim Authors
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

"""Checks event counts against tests/perf_counts/expected.json.

For every test file listed in expected.json, runs kirimasim with
--stats=json and compares each counter with its expected value
(PERFORMANCE.md 4). A test whose :type: includes "simulation" is run to its
end (`kirimasim --stats=json <file>`) and must end with the exit code of
its :expected_exit: (0 when absent); any other is only parsed
(`kirimasim --stats=json parse <file>`) and must end with 0, or with 2
for a test with :should_fail_because:. The JSON is
the last line of standard output. A header with problems
(tools/suite_header.py) fails the check. The check fails when a counter is 10% or more above its
expected value (any increase from an expected 0 counts), when a text
counter such as sim.end_reason differs, when an expected counter is
missing, or when the run reports a counter that expected.json does not
list yet. A run that writes more than 16 MiB to stdout or stderr is
stopped and fails the check (tools/bounded_run.py).

Counts going down never fail the check; lower the expected value in a
separate change, with the reason in the commit message.
"""

import argparse
import json
import sys
from pathlib import Path

import bounded_run
from suite_header import read_header

THRESHOLD_NUM, THRESHOLD_DEN = 11, 10  # fail at >= 110% of the expected count


def too_high(actual: int, expected: int) -> bool:
    return actual > expected and actual * THRESHOLD_DEN >= expected * THRESHOLD_NUM


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("--kirimasim", required=True, help="path to the kirimasim executable")
    ap.add_argument("--expected", required=True, type=Path, help="expected.json")
    ap.add_argument("--root", required=True, type=Path,
                    help="repository root; paths in expected.json are relative to it")
    args = ap.parse_args()

    expected_all = json.loads(args.expected.read_text(encoding="utf-8"))
    failures: list[str] = []
    for test, expected in sorted(expected_all.items()):
        path = args.root / test
        meta, problems = read_header(path)
        if problems:
            failures += [f"{test}: header: {p}" for p in problems]
            continue
        cmd = [args.kirimasim, "--stats=json"]
        simulation = "simulation" in meta["type"].split()
        if not simulation:
            cmd.append("parse")
        try:
            proc = bounded_run.run(cmd + [str(path)])
        except bounded_run.OutputLimit as e:
            failures.append(f"{test}: {e}")
            continue
        if simulation:
            want_exit = int(meta.get("expected_exit", "0"))
        else:
            want_exit = 2 if "should_fail_because" in meta else 0
        if proc.returncode != want_exit:
            failures.append(f"{test}: exit code {proc.returncode}, expected {want_exit}")
        lines = proc.stdout.splitlines()
        try:
            actual = json.loads(lines[-1] if lines else "")
        except json.JSONDecodeError:
            failures.append(f"{test}: the last line of stdout is not JSON: {proc.stdout!r}")
            continue
        for name, exp in sorted(expected.items()):
            if name not in actual:
                failures.append(f"{test}: counter {name} is missing")
            elif isinstance(exp, str) or isinstance(actual[name], str):
                if actual[name] != exp:
                    failures.append(f"{test}: {name} = {actual[name]!r}, expected {exp!r}")
            elif too_high(actual[name], exp):
                failures.append(f"{test}: {name} = {actual[name]}, expected {exp} (+10% or more)")
        for name in sorted(set(actual) - set(expected)):
            failures.append(f"{test}: counter {name} is not in expected.json")

    for f in failures:
        print(f"FAIL: {f}", file=sys.stderr)
    if failures:
        return 1
    print(f"perfcount: {len(expected_all)} test(s) within budget")
    return 0


if __name__ == "__main__":
    sys.exit(main())
