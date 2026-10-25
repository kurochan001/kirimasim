#!/usr/bin/env python3
# Copyright (c) 2026 The kirimasim Authors
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

"""Generates the matrix tests of what the action of a deferred assertion
writes and the restriction of always_comb, always_latch and always_ff, of
TEST_SUITE.md ("Mechanism matrix"): a design in tests/cli/ whose run
stops at a run-time error (run_cli_tests.py compares its error lines),
and tests in tests/suite/proc/.

  tools/gen_deferred_write_matrix.py           # writes the files
  tools/gen_deferred_write_matrix.py --check   # exit 1 if a file differs from what it would write

The action of a deferred assertion runs after the procedure that ran the
assertion: in the Reactive region for #0, in the Postponed region for
final (16.4.1). What it writes, with what it calls, is no part of what
always_comb, always_latch and always_ff write (9.2.2.2, 9.2.2.3, 9.2.2.4),
as what their tasks write is not: other blocks and processes may write it
too, and when the block reads it, it stays in the sensitivity of
always_comb and always_latch (9.2.2.2.1). A final action that writes a
variable of the module is a run-time error of the Postponed region
(4.4.2.9). The action of a deferred assertion that another process
runs may write what such a block writes too, as the block's own action
may: slang's analysis leaves the actions of deferred assertions out on
purpose, and kirimasim follows it (slang issue #2040; SCHEDULER_SPEC.md
2.2, 6.3, 6.4; SIR_SPEC.md 3.4). In the design of tests/cli/, the #0
actions run, and the first final action, the always_comb's, the first in
the file, stops the run with the run-time error of the Postponed region
at its write, exit code 2 (the order in which kirimasim settles final
actions).

The axes: the block (always_comb, always_latch, always_ff); where the
assertion is (in the block, in a function it calls, in a task it calls);
how the action writes (in a function, in a function that function calls,
in a task, through a ref argument; slang does not let the action of an
assertion in a function call a task); #0 or final; who runs the action
(two blocks, with an initial procedure writing directly; another process
writing what a block writes: legal; the block itself).

The expected outputs follow from the rules above, computed below.
"""

from __future__ import annotations

import argparse
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
CLI = ROOT / "tests" / "cli"
SUITE = ROOT / "tests" / "suite" / "proc"
NAME = "deferred_write_matrix"
PREFIX = "deferred_write_"
# The version of the rules below (TEST_SUITE.md, :generator:). Raise it
# whenever they change.
VERSION = 1
POSTPONED = "a write or an event in the Postponed region [top @ 1ns]"
# Where the design of tests/cli/ stops: its first final action, at time 0.
STOP = "a write or an event in the Postponed region [top @ 0ns]"

# (name, the keyword, the head of the block, the clause)
BLOCKS = [
    ("comb", "always_comb", "always_comb begin", "9.2.2.2"),
    ("latch", "always_latch", "always_latch if (en) begin", "9.2.2.3"),
    ("ff", "always_ff", "always_ff @(posedge clk) begin", "9.2.2.4"),
]
# Where the assertion is: in the block, or in a function or a task it calls.
PLACES = ["block", "function", "task"]
# How the action writes: the subroutines, with {w} for the write of {x},
# {t} for the type and {s} for a suffix; the action, with {v} for the
# variable. The action through a ref argument writes at the action.
WAYS = [
    ("function", ["function automatic void act{s}();", "  {w}", "endfunction"], "act{s}()"),
    ("function_function", ["function automatic void in{s}();", "  {w}", "endfunction",
                           "function automatic void act{s}();", "  in{s}();", "endfunction"],
     "act{s}()"),
    ("task", ["task automatic act{s}();", "  {w}", "endtask"], "act{s}()"),
    ("ref", ["function automatic void act{s}(ref {t} r);", "  {w}", "endfunction"], "act{s}({v})"),
]
WAY_WORDS = {"function": "a function", "function_function": "a function that a function calls",
             "task": "a task", "ref": "a ref argument of a function"}


def legal(place, way):
    """slang does not let an action in a function call a task."""
    return not (place == "function" and way[0] == "task")


def way_lines(way, v, s, write, typ):
    """The subroutines of a way, and the line of the write in them."""
    wname, subs, _ = way
    x = "r" if wname == "ref" else v
    lines = [t.format(s=s, t=typ, w=write.format(x=x)) for t in subs]
    return lines, lines.index("  " + write.format(x=x))


def action(way, v, s):
    return way[2].format(s=s, v=v)


def assertion(kind, cond, act):
    return f"assert {'#0' if kind == 'observed' else 'final'} ({cond}) else {act};"


def checker(place, s, stmt):
    """The function or task that holds the assertion."""
    word = "function automatic void" if place == "function" else "task automatic"
    end = "endfunction" if place == "function" else "endtask"
    return [f"{word} chk{s}(logic [7:0] x);", f"  {stmt}", end]


def block(b, stmts):
    return [b[2]] + ["  " + x for x in stmts] + ["end"]


def cli_files():
    """Another process's action writes what each block writes: legal, but the
    first final action's write stops the run (4.4.2.9)."""
    file = f"{NAME}.sv"
    lines = [f"// Generated by tools/gen_deferred_write_matrix.py (version {VERSION}); do not edit.",
             "module top;", "  logic clk = 1'b0, en = 1'b1;", "  logic [7:0] a = 8'd1;"]
    stderr = []

    def add(b, runner, kind, place, way):
        bname, _, head, _ = b
        s = f"_{bname}_{runner}_{kind}_{place}_{way[0]}"
        v = f"t{s}"
        lines.append(f"  logic [7:0] {v};")
        subs, w = way_lines(way, v, s, "{x} = 8'd5;", "logic [7:0]")
        write_row = len(lines) + 1 + w
        lines.extend("  " + x for x in subs)
        act = action(way, v, s)
        if place != "block":
            lines.extend("  " + x for x in checker(place, s, assertion(kind, "x == 8'd0", act)))
        own = f"{v} <= a;" if bname == "ff" else f"{v} = a;"
        lines.append(f"  {head.removesuffix(' begin')} {own}")
        if place == "block":
            stmt = assertion(kind, "a == 8'd0", act)
            if runner == "initial":
                lines.append(f"  initial {stmt}")
            else:
                lines.extend(["  always_comb begin", f"    {stmt}", "  end"])
        else:
            lines.append(f"  initial chk{s}(a);")
        # The final actions write in a function (WAYS[0]); the first stops the run.
        if kind == "final" and not stderr:
            stderr.append(f"{file}:{write_row}: error: {STOP}")

    for b in BLOCKS:
        for place in PLACES:
            for way in WAYS:
                if legal(place, way):
                    add(b, "initial", "observed", place, way)
        add(b, "initial", "final", "block", WAYS[0])
        add(b, "comb", "observed", "block", WAYS[0])
    lines.append("endmodule")
    return {CLI / file: "\n".join(lines) + "\n", CLI / f"{NAME}.stderr": "\n".join(stderr) + "\n"}


def header(name, description, lrm, stderr=False):
    lines = [
        "/*",
        f":name: {name}",
        f":description: {description}",
        ":tags: proc drivers",
        ":type: simulation",
        ":lrm: 1800-2023 " + " ".join(lrm),
        ":oracle: spec",
        ":spec_check: ai",
        ":fourstate: no",
        ":nondeterministic: no",
        f":generator: gen_deferred_write_matrix.py@{VERSION}",
        ":issue: 22",
        ":top_module: top",
        f":expected_stdout: @{name}.expected",
    ]
    if stderr:
        lines += [":expected_exit: 2", f":expected_stderr: @{name}.stderr"]
    return lines + ["*/",
                    "// Generated by tools/gen_deferred_write_matrix.py; change the generator, not "
                    "this file.",
                    "`timescale 1ns/1ns"]


def shared_case(b, kind, place, way):
    """Two blocks' actions and an initial procedure write one variable."""
    bname, keyword, _, clause = b
    name = f"{PREFIX}{kind}_{place}_{way[0]}_{bname}"
    lines = ["module top;", "  logic clk = 1'b0, en = 1'b1;", "  logic [7:0] a = 8'd0;",
             "  int fails = 0;"]
    subs, w = way_lines(way, "fails", "", "{x}++;", "int")
    first = len(lines) + 1
    lines += ["  " + x for x in subs]
    write = first + w
    if place == "block":
        stmt = assertion(kind, "a == 8'd0", action(way, "fails", ""))
    else:
        lines += ["  " + x for x in checker(place, "", assertion(kind, "x == 8'd0",
                                                                 action(way, "fails", "")))]
        stmt = "chk(a);"
    for _ in range(2):
        lines += ["  " + x for x in block(b, [stmt])]
    # Each block fails once, at 1: on a for always_comb and always_latch,
    # on the clock for always_ff.
    lines += ["  initial begin", "    fails = 10;", "    #1 a = 8'd1;", "    clk = 1'b1;",
              "    #1 $display(\"fails=%0d\", fails);", "  end"]
    if kind == "final":
        lines.append("  final $display(\"final ran\");")
    lines.append("endmodule")
    where = {"block": "", "function": " in a function it calls",
             "task": " in a task it calls"}[place]
    if kind == "observed":
        description = (f"the #0 deferred assertions of two {keyword} blocks{where} write a "
                       f"variable through {WAY_WORDS[way[0]]}, as an initial procedure does: the "
                       "action is no part of the block")
        return name, header(name, description, ["16.4", "16.4.1", clause]) + lines, \
            ["fails=12"], None
    description = (f"the final deferred assertion of a {keyword} block writes a variable of the "
                   f"module through {WAY_WORDS[way[0]]}: a run-time error of the Postponed region, "
                   "not of the block")
    head = header(name, description, ["16.4", "16.4.1", "4.4.2.9", clause], True)
    return name, head + lines, ["final ran"], [f"{name}.sv:{len(head) + write}: error: {POSTPONED}"]


def own_case(b, way):
    """The block's own action writes what the block writes."""
    bname, keyword, _, clause = b
    name = f"{PREFIX}own_{way[0]}_{bname}"
    lines = ["module top;", "  logic clk = 1'b0, en = 1'b1;", "  logic [7:0] a = 8'd0, t;"]
    subs, _ = way_lines(way, "t", "", "{x} = 8'd9;", "logic [7:0]")
    lines += ["  " + x for x in subs]
    own = "t <= a;" if bname == "ff" else "t = a;"
    lines += ["  " + x for x in block(b, [own, assertion("observed", "a == 8'd0",
                                                         action(way, "t", ""))])]
    # At 1 the block writes 1, and its action 9 after it.
    lines += ["  initial begin", "    #1 a = 8'd1;", "    clk = 1'b1;",
              "    #1 $display(\"t=%0d\", t);", "  end", "endmodule"]
    description = (f"the action of a #0 deferred assertion in {keyword} writes through "
                   f"{WAY_WORDS[way[0]]} what the block writes: the block's own action may")
    return name, header(name, description, ["16.4", "16.4.1", clause]) + lines, ["t=9"], None


def read_case(b):
    """The block reads what its own action writes: that is in its sensitivity."""
    bname, keyword, _, _ = b
    name = f"{PREFIX}read_{bname}"
    lines = ["module top;", "  logic en = 1'b1;", "  logic [7:0] x = 8'd0, y;", "  int runs;",
             "  function automatic void fix();", "    x = 8'd2;", "  endfunction"]
    lines += ["  " + t for t in block(b, ["runs++;", "y = x;",
                                          "assert #0 (x != 8'd1) else fix();"])]
    # It runs at 0, at 1 when x becomes 1, and again when its action makes
    # x 2.
    lines += ["  initial begin", "    #1 x = 8'd1;",
              "    #1 $display(\"runs=%0d y=%0d\", runs, y);", "  end", "endmodule"]
    description = (f"{keyword} reads a variable that only the action of its #0 deferred assertion "
                   "writes: a change of it runs the block again")
    return name, header(name, description, ["9.2.2.2.1", "16.4", "16.4.1"]) + lines, \
        ["runs=3 y=2"], None


def files():
    out = cli_files()
    cases = [shared_case(b, "observed", p, w) for b in BLOCKS for p in PLACES for w in WAYS
             if legal(p, w)]
    cases += [shared_case(b, "final", "block", w) for b in BLOCKS for w in WAYS]
    cases += [own_case(b, w) for b in BLOCKS for w in (WAYS[0], WAYS[3])]
    cases += [read_case(b) for b in BLOCKS if b[0] != "ff"]
    for name, text, expected, stderr in cases:
        out[SUITE / f"{name}.sv"] = "\n".join(text) + "\n"
        out[SUITE / f"{name}.expected"] = "\n".join(expected) + "\n"
        if stderr is not None:
            out[SUITE / f"{name}.stderr"] = "\n".join(stderr) + "\n"
    return out


def main():
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("--check", action="store_true")
    args = ap.parse_args()
    bad = 0
    for path, text in files().items():
        if args.check:
            if not path.exists() or path.read_text(encoding="utf-8") != text:
                print(f"gen_deferred_write_matrix: {path.relative_to(ROOT)} is out of date; run "
                      "tools/gen_deferred_write_matrix.py", file=sys.stderr)
                bad += 1
        else:
            path.write_text(text, encoding="utf-8")
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main())
