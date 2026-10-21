#!/usr/bin/env python3
# Copyright (c) 2026 The kirimasim Authors
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

"""Generates the matrix tests of delays that do not fit in the time, of
TEST_SUITE.md ("Mechanism matrix"), in tests/suite/sched/.

  tools/gen_delay_overflow_matrix.py           # writes the files
  tools/gen_delay_overflow_matrix.py --check   # exit 1 if a file differs from what it would write

The time is an unsigned 64-bit count of steps of the design's precision
(SCHEDULER_SPEC.md 1). A delay is converted to steps by its scope's time
unit; a delay that takes the time past the largest time, 2^64 - 1 steps,
is a run-time error of the design: one error line at the statement,
exit code 2, and final runs (SCHEDULER_SPEC.md 8, 16.2). That holds
whether the delay is known at run time or is a constant, and whether
the steps already do not fit in 64 bits after the conversion (SIR_SPEC.md
1: a delay of 2^64 steps or more). A delay that ends exactly at the
largest time, or before it, runs.

The axes: the statement (a wait, a nonblocking assignment and a ->> with
a delay held in an int variable, a 100-bit variable or a real one, and
written as an integer or real constant; a continuous assignment and a
gate with a constant delay); the time scale (1s/1fs, where a delay is
multiplied by 10^15, and 1ns/1ns, where it is not); the delay (the
largest that fits from time 0, and one more: 18446 and 18447 seconds at
1s/1fs; 2^64 - 1 and 2^64 steps at 1ns/1ns). A wait on -1, in an int
variable and as a constant, is read as 2^64 - 1 time units (9.4.1): past
the largest time at 1s/1fs, and exactly the largest time at 1ns/1ns. One
file each, since an error ends the run.

The container, for the wait on an int variable and on a constant at
1s/1fs: a static and an automatic task of the module, a fork branch, a
generate block, an instantiated module and a task of a package. The same
statement means the same in each (CLAUDE.md), so each gives the same line
as the initial procedure; the hierarchical name in it is the instance's
(SCHEDULER_SPEC.md 16.2), top for a generate block and top.u for the
instance. The line of the package's task is not compared, as SYSTEM_TASKS.md
3.8 does not say yet how a package's name is written.

The expected outputs follow from the rules above, computed below.
"""

from __future__ import annotations

import argparse
import sys
from pathlib import Path

OUT = Path(__file__).resolve().parent.parent / "tests" / "suite" / "sched"
PREFIX = "delay_overflow_"
# The version of the rules below (TEST_SUITE.md, :generator:). Raise it
# whenever they change.
VERSION = 1
OVERFLOW = "the delay takes the time past the largest time"

# (name, time scale, precision exponent, the delay that fits and the one
# that does not, as an int, a 100-bit literal, a real, an integer
# constant, a real constant, and a negative int and integer constant)
SCALES = [
    ("s", "1s/1fs", "0fs", ("18446", "18446", "18446.0", "18446", "18446.0", None),
     ("18447", "18447", "18447.0", "18447", "18447.0", "-1")),
    ("ns", "1ns/1ns", "0ns",
     (None, "100'hffff_ffff_ffff_ffff", None, "64'hffff_ffff_ffff_ffff", None, "-1"),
     (None, "100'h1_0000_0000_0000_0000", None, "65'h1_0000_0000_0000_0000", None, None)),
]

# (name, the declarations with {int} {wide} {real} for the variables'
# initial values, the statements of an initial procedure with {const} and
# {rconst}, ERR marking the error, the lines it prints when it fits, which
# delay forms it needs)
KINDS = [
    ("wait_int", "  int d = {int};\n", ["#d $display(\"woke\"); // ERR"], ["woke"], "int"),
    ("wait_wide", "  logic [99:0] d = {wide};\n", ["#d $display(\"woke\"); // ERR"], ["woke"],
     "wide"),
    ("wait_real", "  real d = {real};\n", ["#d $display(\"woke\"); // ERR"], ["woke"], "real"),
    ("wait_const", "", ["#({const}) $display(\"woke\"); // ERR"], ["woke"], "const"),
    ("wait_real_const", "", ["#({rconst}) $display(\"woke\"); // ERR"], ["woke"], "rconst"),
    ("wait_neg_int", "  int d = {neg};\n", ["#d $display(\"woke\"); // ERR"], ["woke"], "neg"),
    ("wait_neg_const", "", ["#({neg}) $display(\"woke\"); // ERR"], ["woke"], "neg"),
    ("nba_int", "  int d = {int};\n", ["q <= #d 1'b1; // ERR"], [], "int"),
    ("nba_wide", "  logic [99:0] d = {wide};\n", ["q <= #d 1'b1; // ERR"], [], "wide"),
    ("nba_const", "", ["q <= #({const}) 1'b1; // ERR"], [], "const"),
    ("trigger_nb_int", "  int d = {int};\n", ["->> #d ev; // ERR"], ["ev"], "int"),
    ("trigger_nb_const", "", ["->> #({const}) ev; // ERR"], ["ev"], "const"),
]
# The containers: (name, the lines before module top, the items of top,
# the statements of its initial procedure, the name of the delay variable
# in the statement, the hierarchical name of the error line or None when
# it is not compared). {V} is the delay, ERR marks the error.
CONTAINERS = [
    ("task", [], ["task t(input int v);", "  #{V} $display(\"woke\"); // ERR", "endtask"],
     ["t(d);"], "v", "top"),
    ("auto_task", [],
     ["task automatic t(input int v);", "  #{V} $display(\"woke\"); // ERR", "endtask"],
     ["t(d);"], "v", "top"),
    ("fork", [], [], ["fork", "  #{V} $display(\"woke\"); // ERR", "join"], "d", "top"),
    ("generate", [], ["if (1) begin : g", "  initial #{V} $display(\"woke\"); // ERR", "end"], [],
     "d", "top"),
    ("instance", ["module child;", "  int d = {int};", "  initial #{V} $display(\"woke\"); // ERR",
                  "endmodule"], ["child u();"], [], "d", "top.u"),
    ("package", ["package p;", "  task automatic t(input int v);",
                 "    #{V} $display(\"woke\"); // ERR", "  endtask", "endpackage"], [],
     ["p::t(d);"], "v", None),
]
# Continuous assignments and gates, with a constant delay: (name, the item
# with ERR, the lines it prints when it fits)
ITEMS = [
    ("assign_const", "assign #({const}) w = a; // ERR", []),
    ("gate_const", "buf #({const}) g(w, a); // ERR", []),
]


def header(name, fits, timescale, compared=True):
    lines = [
        "/*",
        f":name: {name}",
        ":description: a delay that takes the time past the largest time is a run-time error, "
        "whether it is known at run time or a constant and whether its steps fit in 64 bits; "
        "one that ends at the largest time or before runs",
        ":tags: sched delay",
        ":type: simulation",
        ":lrm: 1800-2023 3.14.2 9.4.1 10.3.3 28.16",
        ":oracle: spec",
        ":spec_check: ai",
        ":fourstate: yes",
        ":nondeterministic: no",
        f":generator: gen_delay_overflow_matrix.py@{VERSION}",
        ":issue: 7",
        ":top_module: top",
        f":expected_stdout: @{name}.expected",
    ]
    if not fits:
        lines += [":expected_exit: 2"]
        if compared:
            lines += [f":expected_stderr: @{name}.stderr"]
    return lines + ["*/",
                    "// Generated by tools/gen_delay_overflow_matrix.py; change the generator, not "
                    "this file.",
                    f"`timescale {timescale}"]


def one(name, timescale, zero, fits, decls, stmts, items, prints, pre=(), scope="top"):
    lines = header(name, fits, timescale, scope is not None)
    lines += list(pre)
    lines += ["module top;", "  logic q = 1'b0;", "  logic a = 1'b0;", "  wire w;", "  event ev;"]
    lines += [x for x in decls.splitlines() if x]
    lines += [f"  {x}" for x in items]
    lines += ["  initial @ev $display(\"ev\");", "  initial begin", "    #0 a = 1'b1;"]
    lines += [f"    {x}" for x in stmts]
    lines += ["  end", "  final $display(\"final q=%b w=%b\", q, w);", "endmodule", ""]
    err = None
    for number, line in enumerate(lines, 1):
        if "// ERR" in line:
            err = number
    lines = [line.replace(" // ERR", "") for line in lines]
    out = list(prints) if fits else []
    # q is set when the nonblocking assignment runs; w follows a after the
    # delay of the continuous assignment or the gate.
    q = "1" if fits and name.startswith(PREFIX + "nba") else "0"
    driven = name.startswith((PREFIX + "assign", PREFIX + "gate"))
    w = "1" if fits and driven else ("x" if driven else "z")
    out.append(f"final q={q} w={w}")
    files = {f"{name}.sv": "\n".join(lines), f"{name}.expected": "\n".join(out) + "\n"}
    if not fits and scope is not None:
        files[f"{name}.stderr"] = f"{name}.sv:{err}: error: {OVERFLOW} [{scope} @ {zero}]\n"
    return files


def files():
    out = {}
    for sname, timescale, zero, fit, over in SCALES:
        for fits, values in ((True, fit), (False, over)):
            tag = "fits" if fits else "over"
            v = {"int": values[0], "wide": values[1], "real": values[2], "const": values[3],
                 "rconst": values[4], "neg": values[5]}
            for kname, decls, stmts, prints, needs in KINDS:
                if v[needs] is None:
                    continue
                name = f"{PREFIX}{kname}_{sname}_{tag}"
                d = decls.format(**v)
                s = [x.replace("{const}", v["const"] or "").replace("{rconst}", v["rconst"] or "")
                     .replace("{neg}", v["neg"] or "") for x in stmts]
                out.update(one(name, timescale, zero, fits, d, s, [], prints))
            for kname, item, prints in ITEMS:
                if v["const"] is None:
                    continue
                name = f"{PREFIX}{kname}_{sname}_{tag}"
                out.update(one(name, timescale, zero, fits, "", [],
                               [item.replace("{const}", v["const"])], prints))
            if sname != "s":
                continue
            for cname, pre, items, stmts, var, scope in CONTAINERS:
                for form, delay in (("int", var), ("const", f"({v['const']})")):
                    name = f"{PREFIX}{cname}_{form}_{sname}_{tag}"
                    fill = [x.replace("{V}", delay).replace("{int}", v["int"]) for x in pre]
                    out.update(one(name, timescale, zero, fits, f"  int d = {v['int']};\n",
                                   [x.replace("{V}", delay) for x in stmts],
                                   [x.replace("{V}", delay) for x in items], ["woke"],
                                   fill, scope))
    return out


def main():
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("--check", action="store_true")
    args = ap.parse_args()
    bad = 0
    for name, text in files().items():
        path = OUT / name
        if args.check:
            if not path.exists() or path.read_text(encoding="utf-8") != text:
                print(f"gen_delay_overflow_matrix: {path.relative_to(OUT.parent.parent.parent)} is "
                      "out of date; run tools/gen_delay_overflow_matrix.py", file=sys.stderr)
                bad += 1
        else:
            path.write_text(text, encoding="utf-8")
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main())
