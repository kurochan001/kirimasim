#!/usr/bin/env python3
# Copyright (c) 2026 The kirimasim Authors
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
"""run_suite_test.py gives --single-unit for :compilation_unit: single (TEST_SUITE.md, "Header items passed"):
to the parse and the run, before the test's file; nothing without the item; and any other value is a header
problem, with kirimasim not run."""
import subprocess
import sys
import tempfile
from pathlib import Path

TOOLS = Path(__file__).resolve().parent.parent.parent / "tools"
HEADER = (":name: t\n:description: d\n:tags: t\n:type: simulation\n:lrm: 1800-2023 3.12.1\n"
          ":oracle: spec\n:spec_check: ai\n:fourstate: no\n:nondeterministic: no\n:expected_stdout: ok\n")


def judge(case: Path, item: str):
    """Runs the judge on a test with the extra header line item, against a
    kirimasim that records its arguments; returns the result and the calls."""
    test = case / "t.sv"
    test.write_text(f"/*\n{HEADER}{item}*/\nmodule top;\nendmodule\n", encoding="utf-8")
    log = case / "calls.txt"
    log.unlink(missing_ok=True)
    fake = case / "fake"
    fake.write_text(f"#!{sys.executable}\nimport sys\nwith open({str(log)!r}, 'a') as f:\n"
                    "    f.write(repr(sys.argv[1:]) + '\\n')\nif sys.argv[1] != 'parse':\n    print('ok')\n",
                    encoding="utf-8")
    fake.chmod(0o755)
    run = subprocess.run([sys.executable, str(TOOLS / "run_suite_test.py"), "--kirimasim", str(fake), str(test)],
                         capture_output=True, text=True, stdin=subprocess.DEVNULL)
    calls = [eval(line) for line in log.read_text().splitlines()] if log.exists() else []
    return run, calls, str(test.resolve())


def main() -> int:
    failures = []
    with tempfile.TemporaryDirectory() as directory:
        case = Path(directory)
        run, calls, test = judge(case, ":compilation_unit: single\n")
        if run.returncode != 0:
            failures.append(f"single: the test does not pass: {run.stdout}{run.stderr}")
        if len(calls) != 2 or calls[0][0] != "parse":
            failures.append(f"single: not one parse and one run: {calls}")
        for call in calls:
            if call.count("--single-unit") != 1:
                failures.append(f"single: --single-unit is not given once: {call}")
            elif call.index("--single-unit") > call.index(test):
                failures.append(f"single: --single-unit comes after the test's file: {call}")
        run, calls, _ = judge(case, "")
        if run.returncode != 0 or len(calls) != 2:
            failures.append(f"none: the test does not pass: {run.stdout}{run.stderr}")
        if any("--single-unit" in call for call in calls):
            failures.append(f"none: --single-unit given without the item: {calls}")
        run, calls, _ = judge(case, ":compilation_unit: separate\n")
        if run.returncode == 0 or ":compilation_unit:" not in run.stdout + run.stderr:
            failures.append(f"separate: not reported as a header problem: {run.stdout}{run.stderr}")
        if calls:
            failures.append(f"separate: kirimasim was run: {calls}")
    for failure in failures:
        print("FAIL: " + failure, file=sys.stderr)
    if failures:
        return 1
    print("suite compilation unit: --single-unit for single, nothing without it, other values rejected")
    return 0


if __name__ == "__main__":
    sys.exit(main())
