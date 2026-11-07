#!/usr/bin/env python3
# Copyright (c) 2026 The kirimasim Authors
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
"""tools/suite_table.py counts what a suite's tests say and use, not their comments."""
import subprocess
import sys
import tempfile
from pathlib import Path

TOOL = Path(__file__).resolve().parent.parent.parent / "tools" / "suite_table.py"
HEAD = "/*\n:name: t\n:description: d\n:tags: t\n:type: {kind}\n:lrm: 1800-2023 {clause}\n:oracle: {oracle}\n" \
       ":spec_check: {check}\n:fourstate: {four}\n:nondeterministic: {nondet}\n{more}*/\n"


def test(kind="simulation", clause="6.8", oracle="spec iverilog@13.0", check="ai", four="no", nondet="no", more="", body=""):
    return HEAD.format(kind=kind, clause=clause, oracle=oracle, check=check, four=four, nondet=nondet, more=more) + body


WANT = {
    "Tests": "5", "Simulation tests": "4", "`:spec_check: human`": "1", "`:oracle:` `spec` alone": "1",
    "`:fourstate: yes`": "1", "`:nondeterministic: yes`": "1", "First clause in chapter 4": "1",
    "`:should_fail_because:`": "1", "Made by a generator": "1", "`$strobe`": "1", "`$monitor`": "0",
    "`$fopen`": "0", "`$readmem`": "1", "`$writemem`": "0", "`$dumpvars`": "0", "DPI-C import": "1",
    "`$test$plusargs`, `$value$plusargs`": "1",
}


def main() -> int:
    failures = []
    with tempfile.TemporaryDirectory() as directory:
        folder = Path(directory)
        files = {
            "a": test(clause="6.8 4.4.2", check="human", oracle="spec", body='module top; initial $strobe("x"); endmodule\n'),
            "b": test(clause="4.4.2", four="yes", more=":generator: g.py@1\n",
                      body='module top; initial $readmemh("f",m); endmodule\n'),
            "c": test(nondet="yes", body='import "DPI-C" function int f();\nmodule top; endmodule\n'),
            "d": test(kind="parsing", more=":should_fail_because: illegal\n",
                      body='module top; initial if ($test$plusargs("p")) ; endmodule\n'),
            # Only in a comment or a string: not a use.
            "e": test(body='// $monitor and $fopen\n/* $dumpvars */\nmodule top; initial $display("$writemem"); endmodule\n'),
        }
        for name, text in files.items():
            (folder / (name + ".sv")).write_text(text, encoding="utf-8")
        # A source that tests name in :files: is not a test.
        (folder / "source.sv").write_text("module helper; initial $monitor; endmodule\n", encoding="utf-8")
        run = subprocess.run([sys.executable, str(TOOL), f"s={folder}"], capture_output=True, text=True,
                             stdin=subprocess.DEVNULL)
        if run.returncode:
            failures.append(f"exit code {run.returncode}: {run.stderr}")
        got = dict(tuple(cell.strip() for cell in line.strip("|").split("|")) for line in run.stdout.splitlines()[2:])
        for row, value in WANT.items():
            if got.get(row) != value:
                failures.append(f"{row}: {got.get(row)}, expected {value}")
        bad = subprocess.run([sys.executable, str(TOOL), "s=" + str(folder / "missing")], capture_output=True,
                             text=True, stdin=subprocess.DEVNULL)
        if bad.returncode != 2:
            failures.append(f"a missing folder: exit code {bad.returncode}")
    for failure in failures:
        print("FAIL: " + failure, file=sys.stderr)
    if failures:
        return 1
    print("suite table: the counts follow the headers and the source, not comments or strings")
    return 0


if __name__ == "__main__":
    sys.exit(main())
