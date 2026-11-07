#!/usr/bin/env python3
# Copyright (c) 2026 The kirimasim Authors
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
"""run_suite_test.py judges the exit code only when the test gives :expected_exit:
(TEST_SUITE.md); without it, an internal error or a crash still fails the test."""
import subprocess
import sys
import tempfile
from pathlib import Path

TOOLS = Path(__file__).resolve().parent.parent.parent / "tools"
failures = []


def judged(tmp: Path, name: str, expected_exit: str | None, ending: str) -> int:
    """ending: an exit code for the run, or "signal" for a run that a signal stops."""
    case = tmp / name
    case.mkdir()
    test = case / "t.sv"
    item = f":expected_exit: {expected_exit}\n" if expected_exit is not None else ""
    test.write_text("/*\n:name: t\n:description: d\n:tags: t\n:type: simulation\n"
                    ":lrm: 1800-2023 4.7\n:oracle: spec\n:spec_check: ai\n:fourstate: no\n"
                    f":nondeterministic: no\n:expected_stdout: a\n{item}*/\n"
                    "module top;\nendmodule\n", encoding="utf-8")
    end = ("import os, signal\nos.kill(os.getpid(), signal.SIGSEGV)\n" if ending == "signal"
           else f"sys.exit({ending})\n")
    fake = case / "fake"
    fake.write_text(f"#!{sys.executable}\nimport sys\nif sys.argv[1] == 'parse':\n    sys.exit(0)\n"
                    f"print('a')\nsys.stdout.flush()\n{end}", encoding="utf-8")
    fake.chmod(0o755)
    run = subprocess.run([sys.executable, str(TOOLS / "run_suite_test.py"), "--kirimasim", str(fake), str(test)],
                         capture_output=True, text=True, stdin=subprocess.DEVNULL)
    return run.returncode


def main() -> int:
    with tempfile.TemporaryDirectory() as directory:
        tmp = Path(directory)
        for name, expected_exit, ending, passes in (
                # Not given: the code is not compared.
                ("absent_finish", None, "0", True),
                ("absent_fatal", None, "1", True),
                ("absent_error", None, "2", True),
                ("absent_wall_time", None, "4", True),
                # ... but an internal error or a crash fails.
                ("absent_internal_error", None, "3", False),
                ("absent_shell_crash", None, "139", False),
                ("absent_126", None, "126", False),
                ("absent_signal", None, "signal", False),
                # Given: compared as before.
                ("zero_given_zero", "0", "0", True),
                ("zero_given_error", "0", "2", False),
                ("nonzero_given_zero", "nonzero", "0", False),
                ("nonzero_given_fatal", "nonzero", "1", True),
                ("two_given_two", "2", "2", True),
                ("two_given_one", "2", "1", False),
                ("three_given_three", "3", "3", True)):
            code = judged(tmp, name, expected_exit, ending)
            if (code == 0) != passes:
                failures.append(f"{name}: exit code {code}")
    for failure in failures:
        print("FAIL: " + failure, file=sys.stderr)
    if failures:
        return 1
    print("suite exit: the exit code is judged only when the test gives it")
    return 0


if __name__ == "__main__":
    sys.exit(main())
