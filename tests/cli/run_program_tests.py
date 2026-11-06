#!/usr/bin/env python3
# Copyright (c) 2026 The kirimasim Authors
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

"""Check program metadata and strict illegal-input diagnostics (24.3, 24.6)."""

import argparse
import re
import sys
import tempfile
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / "tools"))
import bounded_run  # noqa: E402


INVALID = [
    "program worker; always #1; endprogram module top; worker p(); endmodule",
    "program worker; int value; endprogram module top; worker p(); initial $display(\"%0d\",p.value); endmodule",
    "package pkg; program; function int value(); return 7; endfunction endprogram endpackage module top; initial $display(\"%0d\",pkg::value()); endmodule",
    "module leaf; endmodule program worker; leaf u(); endprogram module top; worker p(); endmodule",
    "interface leaf; endinterface program worker; leaf u(); endprogram module top; worker p(); endmodule",
]

# How the end of a run reads (20.2, 24.3, 24.7; SCHEDULER_SPEC.md 16.2): the
# implicit $finish takes level 1 whichever way the last program ends, an
# explicit level stays as written, and a run with no program initial is not
# ended by this rule. Each row: name, design, stdout, reason, messages.
FINISH = "kirimasim: info: finish at {}"
RUN = "module top; {} endmodule"
ONE = "module top; worker u(); endmodule"
ENDINGS = [
    ("the last initial ends",
     'program worker; initial #2 $display("a"); endprogram ' + ONE, "a\n", "finish",
     [FINISH.format("2 ns")]),
    ("the last of several initials ends",
     'program worker; initial #2 $display("a"); initial #4 $display("b"); endprogram ' + ONE,
     "a\nb\n", "finish", [FINISH.format("4 ns")]),
    ("waiting on a module's clock",
     "program worker(input logic clk); initial repeat (3) @(posedge clk); endprogram "
     "module top; logic clk = 0; always #5 clk = ~clk; worker u(clk); endmodule",
     "", "finish", [FINISH.format("25 ns")]),
    ("$exit", "program worker; initial begin #7; $exit(); #1 $display(\"late\"); end endprogram " + ONE,
     "", "finish", [FINISH.format("7 ns")]),
    ("$exit from a task of the program",
     "program worker; task stop(); #3; $exit(); endtask initial begin stop(); "
     '$display("late"); end endprogram ' + ONE,
     "", "finish", [FINISH.format("3 ns")]),
    ("$exit from a task of a package",
     'package pkg; task automatic stop(); #3; $exit(); endtask endpackage '
     'program worker; initial begin pkg::stop(); $display("late"); end endprogram ' + ONE,
     "", "finish", [FINISH.format("3 ns")]),
    ("$exit from a fork child",
     "program worker; initial begin fork begin #3; $exit(); end join_none #9 "
     '$display("late"); end endprogram ' + ONE,
     "", "finish", [FINISH.format("3 ns")]),
    ("descendants are still waiting",
     'program worker; initial begin fork #9 $display("late"); join_none #3; end endprogram ' + ONE,
     "", "finish", [FINISH.format("3 ns")]),
    ("two programs end at different times",
     'program a; initial #2 $display("a"); endprogram program b; initial #5 $display("b"); '
     "endprogram module top; a x(); b y(); endmodule",
     "a\nb\n", "finish", [FINISH.format("5 ns")]),
    ("an array of program instances",
     "program worker; initial #2; endprogram module top; worker p[2](); endmodule",
     "", "finish", [FINISH.format("2 ns")]),
    ("a final procedure runs after",
     'program worker; initial #2; endprogram module top; worker u(); final $display("f"); endmodule',
     "f\n", "finish", [FINISH.format("2 ns")]),
    ("explicit $finish(0) stays silent", "program worker; initial #2 $finish(0); endprogram " + ONE,
     "", "finish", []),
    ("explicit $stop(0) stays silent", "program worker; initial #2 $stop(0); endprogram " + ONE,
     "", "stop", []),
    ("explicit $finish keeps its position line", "program worker; initial #2 $finish; endprogram " + ONE,
     "", "finish", ["end.sv:2: $finish at 2 ns", FINISH.format("2 ns")]),
    ("a program with no initial does not end the run",
     'program worker; endprogram module top; worker u(); initial #2 $display("m"); endmodule',
     "m\n", "quiescent", ["kirimasim: info: quiescent at 2 ns"]),
]


def check_endings(simulator, tmp):
    path = Path(tmp) / "end.sv"
    for name, design, stdout, reason, messages in ENDINGS:
        path.write_text("`timescale 1ns/1ps\n" + design + "\n")
        ran = bounded_run.run([simulator, path.name], cwd=tmp, timeout=10)
        got = [line for line in ran.stderr.splitlines() if not line.startswith("kirimasim: info: kirimasim ")]
        if ran.returncode or ran.stdout != stdout or got != messages:
            raise AssertionError(f"program ending, {name}: exit {ran.returncode}\n"
                                 f"stdout {ran.stdout!r}, want {stdout!r}\nmessages {got!r}, want {messages!r}")
        stats = bounded_run.run([simulator, "--stats=json", path.name], cwd=tmp, timeout=10)
        if f'"sim.end_reason": "{reason}"' not in stats.stdout:
            raise AssertionError(f"program ending, {name}: end reason is not {reason}\n{stats.stdout}")


def main():
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--kirimasim", required=True, type=Path)
    args = parser.parse_args()
    simulator = str(args.kirimasim.resolve())
    with tempfile.TemporaryDirectory(prefix="kirimasim-program-cli-") as tmp:
        path = Path(tmp) / "program.sv"
        for source in INVALID:
            path.write_text(source)
            for options in (("parse",), ("--dump-sir",), ()):
                ran = bounded_run.run([simulator, *options, str(path)], cwd=tmp, timeout=10)
                if ran.returncode != 2 or ran.stdout or "error:" not in ran.stderr or "internal error" in ran.stderr:
                    raise AssertionError(f"program illegal input {options}: {ran.returncode}\n{ran.stdout}\n{ran.stderr}")
        path.write_text("program worker; initial #1; endprogram module top; worker p[2](); endmodule\n")
        ran = bounded_run.run([simulator, "--dump-sir", str(path)], cwd=tmp, timeout=10)
        rows = re.findall(r"^instance .* program$", ran.stdout, re.M)
        if ran.returncode or len(rows) != 2:
            raise AssertionError(f"program instance metadata: {ran.returncode}\n{ran.stdout}\n{ran.stderr}")
        check_endings(simulator, tmp)
    print(f"program CLI: 15 illegal-input checks, 2 instance identities and {len(ENDINGS)} endings checked")
    return 0


if __name__ == "__main__":
    sys.exit(main())
