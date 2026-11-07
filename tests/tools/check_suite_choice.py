#!/usr/bin/env python3
# Copyright (c) 2026 The kirimasim Authors
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
"""run_suite_test.py compares kirimasim with <name>.choice (TEST_SUITE.md, "choice")."""
import subprocess
import sys
import tempfile
from pathlib import Path

TOOLS = Path(__file__).resolve().parent.parent.parent / "tools"
failures = []


def judged(tmp: Path, name: str, nondeterministic: str, expected: str, choice: str | None) -> int:
    case = tmp / name
    case.mkdir()
    test = case / "t.sv"
    test.write_text("/*\n:name: t\n:description: d\n:tags: t\n:type: simulation\n"
                    ":lrm: 1800-2023 4.7\n:oracle: spec choice\n:spec_check: ai\n:fourstate: no\n"
                    f":nondeterministic: {nondeterministic}\n:expected_stdout: @t.expected\n*/\n"
                    "module top;\nendmodule\n", encoding="utf-8")
    (case / "t.expected").write_text(expected, encoding="utf-8")
    if choice is not None:
        (case / "t.choice").write_text(choice, encoding="utf-8")
    fake = tmp / "fake"
    fake.write_text(f"#!{sys.executable}\nimport sys\nif sys.argv[1] != 'parse':\n    print('a')\n",
                    encoding="utf-8")
    fake.chmod(0o755)
    run = subprocess.run([sys.executable, str(TOOLS / "run_suite_test.py"), "--kirimasim", str(fake), str(test)],
                         capture_output=True, text=True, stdin=subprocess.DEVNULL)
    return run.returncode


def main() -> int:
    both = "a\n---\nb\n"
    with tempfile.TemporaryDirectory() as directory:
        tmp = Path(directory)
        for name, nondeterministic, expected, choice, passes in (
                ("any_candidate_without_a_choice", "yes", "b\n---\na\n", None, True),
                ("the_chosen_result", "yes", both, "a\n", True),
                ("another_allowed_result", "yes", both, "b\n", False),
                ("a_choice_outside_the_candidates", "yes", both, "c\n", False),
                ("a_choice_without_nondeterministic", "no", "a\n", "a\n", False)):
            code = judged(tmp, name, nondeterministic, expected, choice)
            if (code == 0) != passes:
                failures.append(f"{name}: exit code {code}")
    for failure in failures:
        print("FAIL: " + failure, file=sys.stderr)
    if failures:
        return 1
    print("suite choice: the judge compares kirimasim with its chosen result")
    return 0


if __name__ == "__main__":
    sys.exit(main())
