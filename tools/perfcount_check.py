#!/usr/bin/env python3
"""Checks event counts against tests/perf_counts/expected.json.

For every test file listed in expected.json, runs haremasim with
--stats=json and compares each counter with its expected value
(PERFORMANCE.md 4). A test whose :type: includes "simulation" is run to its
end (`haremasim --stats=json <file>`), any other only parsed
(`haremasim --stats=json parse <file>`); the JSON is the last line of
standard output. The check fails when a counter is 10% or more above its
expected value (any increase from an expected 0 counts), when a text
counter such as sim.end_reason differs, when an expected counter is
missing, or when the run reports a counter that expected.json does not
list yet.

Counts going down never fail the check; lower the expected value in a
separate change, with the reason in the commit message.
"""

import argparse
import json
import re
import subprocess
import sys
from pathlib import Path

THRESHOLD_NUM, THRESHOLD_DEN = 11, 10  # fail at >= 110% of the expected count


def too_high(actual: int, expected: int) -> bool:
    return actual > expected and actual * THRESHOLD_DEN >= expected * THRESHOLD_NUM


def is_simulation(path: Path) -> bool:
    m = re.search(r"/\*(.*?)\*/", path.read_text(encoding="utf-8"), re.DOTALL)
    for line in (m.group(1) if m else "").splitlines():
        fm = re.match(r"^:type:\s*(.*)$", line.strip())
        if fm:
            return "simulation" in fm.group(1).split()
    return False


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("--haremasim", required=True, help="path to the haremasim executable")
    ap.add_argument("--expected", required=True, type=Path, help="expected.json")
    ap.add_argument("--root", required=True, type=Path,
                    help="repository root; paths in expected.json are relative to it")
    args = ap.parse_args()

    expected_all = json.loads(args.expected.read_text(encoding="utf-8"))
    failures: list[str] = []
    for test, expected in sorted(expected_all.items()):
        path = args.root / test
        cmd = [args.haremasim, "--stats=json"]
        if not is_simulation(path):
            cmd.append("parse")
        proc = subprocess.run(cmd + [str(path)], capture_output=True, text=True, check=False)
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
