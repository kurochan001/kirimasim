#!/usr/bin/env python3
# Copyright (c) 2026 The kirimasim Authors
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
"""run_suite_test.py gives :incdirs: and :defines: to kirimasim (TEST_SUITE.md, "Header items passed"):
include directories after those of :files:, macros as -D with their values whole, to the parse and the run."""
import subprocess
import sys
import tempfile
from pathlib import Path

TOOLS = Path(__file__).resolve().parent.parent.parent / "tools"


def main() -> int:
    failures = []
    with tempfile.TemporaryDirectory() as directory:
        case = Path(directory)
        (case / "inc").mkdir()
        (case / "more").mkdir()
        (case / "inc" / "a.svh").write_text("", encoding="utf-8")
        test = case / "t.sv"
        test.write_text("/*\n:name: t\n:description: d\n:tags: t\n:type: simulation\n:lrm: 1800-2023 22.4\n"
                        ":oracle: spec\n:spec_check: ai\n:fourstate: no\n:nondeterministic: no\n"
                        ":files: inc/a.svh\n:incdirs: more inc\n:defines: FAST LEVEL=2 SUM=1+1\n"
                        ":expected_stdout: ok\n*/\nmodule top;\nendmodule\n", encoding="utf-8")
        log = case / "calls.txt"
        fake = case / "fake"
        fake.write_text(f"#!{sys.executable}\nimport sys\nwith open({str(log)!r}, 'a') as f:\n"
                        "    f.write(repr(sys.argv[1:]) + '\\n')\nif sys.argv[1] != 'parse':\n    print('ok')\n",
                        encoding="utf-8")
        fake.chmod(0o755)
        run = subprocess.run([sys.executable, str(TOOLS / "run_suite_test.py"), "--kirimasim", str(fake), str(test)],
                             capture_output=True, text=True, stdin=subprocess.DEVNULL)
        if run.returncode != 0:
            failures.append(f"the test does not pass: {run.stdout}{run.stderr}")
        calls = [eval(line) for line in log.read_text().splitlines()] if log.exists() else []
        want = [f"+incdir+{(case / 'inc').resolve()}", f"+incdir+{(case / 'more').resolve()}",
                "-DFAST", "-DLEVEL=2", "-DSUM=1+1"]
        if len(calls) != 2 or calls[0][0] != "parse":
            failures.append(f"not one parse and one run: {calls}")
        for call in calls:
            got = [a for a in call if a.startswith(("+incdir+", "-D"))]
            if got != want:
                failures.append(f"options {got}, expected {want}")
            if call.index(want[-1]) > call.index(str(test.resolve())):
                failures.append("the options come after the test's file")
    for failure in failures:
        print("FAIL: " + failure, file=sys.stderr)
    if failures:
        return 1
    print("suite inputs: :incdirs: and :defines: reach both runs in order")
    return 0


if __name__ == "__main__":
    sys.exit(main())
