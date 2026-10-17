#!/usr/bin/env python3
"""Generates the matrix test of static subroutines called from several
processes of TEST_SUITE.md ("機構の行列") in tests/suite/proc/.

  tools/gen_static_call_matrix.py           # writes the files
  tools/gen_static_call_matrix.py --check   # exit 1 if a file differs from what it would write

A static subroutine keeps its variables and arguments in one place for
every call (13.3.1, SIR_SPEC.md 3.3). Copying a call's values into its
arguments is not an assignment in the calling procedure, so several
always_comb, always_latch and always_ff procedures may call the same
static subroutine (9.2.2.2, 9.2.2.4, SIR_SPEC.md 10 rule 6); a call runs
to its end before another process runs, so each caller gets its own
result.

The axes: the subroutine (a function, a task with an output argument,
a function that calls another static function); the caller (two always_comb, two always_latch, two
always_ff, an always @*, a continuous assignment, and the procedure that
drives the test); and the iff condition of always_ff, a guard that calls
a static function also called by an always_comb. Each caller reads its
own input and writes its own output; for each step the driver sets every
input, makes a clock edge, and prints every output.

The expected outputs follow from the rules above, computed below. A
person checks them once for all the tests made from them (TEST_SUITE.md,
"人の抜き取り").
"""

from __future__ import annotations

import argparse
import random
import sys
from pathlib import Path

OUT = Path(__file__).resolve().parent.parent / "tests" / "suite" / "proc"
NAME = "static_call_matrix"
# The version of the rules below (TEST_SUITE.md, :generator:). Raise it
# whenever they change.
VERSION = 1

# (name, the value of a call with {x}, or of a task call with {x} and {y};
# the result for x)
SUBS = [
    ("func", "f({x})", lambda x: (x + 1) & 0xFF),
    ("task", "tk({x}, {y});", lambda x: (x + 2) & 0xFF),
    ("nested", "h({x})", lambda x: ((x + 1) & 0xFF) ^ 0x0F),
]
CALLERS = ["comb0", "comb1", "latch0", "latch1", "ff0", "ff1", "star", "cont", "direct"]
STEPS = 3
# The iff part: the enable's values, one step each.
ENABLES = "10x1"


def caller(c, sub, form, x, y):
    """The module item of caller c of the subroutine, or None."""
    is_task = sub == "task"
    assign = form.format(x=x, y=y) if is_task else f"{y} = {form.format(x=x)};"
    nba = form.format(x=x, y=y) if is_task else f"{y} <= {form.format(x=x)};"
    if c.startswith("comb"):
        return f"  always_comb {assign}"
    if c.startswith("latch"):
        return f"  always_latch if (le) {assign}"
    if c.startswith("ff"):
        if is_task:
            # the task's output goes to a variable of the procedure
            return f"  always_ff @(posedge clk) begin {form.format(x=x, y=y + '_t')} {y} <= {y}_t; end"
        return f"  always_ff @(posedge clk) {nba}"
    if c == "star":
        return f"  always @* {assign}"
    if c == "cont":
        return None if is_task else f"  assign {y} = {form.format(x=x)};"
    return None  # direct: the driver calls it


def files():
    rng = random.Random(2 * 4099)
    decl, items, exp = [], [], []
    inputs = []  # (variable, sub, caller)
    for sub, form, _ in SUBS:
        for c in CALLERS:
            x, y = f"a_{c}_{sub}", f"y_{c}_{sub}"
            item = caller(c, sub, form, x, y)
            if c == "cont" and item is None:
                continue
            extra = f", {y}_t" if c.startswith("ff") and sub == "task" else ""
            decl.append(f"  logic [7:0] {x}, {y}{extra};")
            if item is not None:
                items.append(item)
            inputs.append((x, y, sub, c))
    lines = []
    funcs = {name: fn for name, _, fn in SUBS}
    forms = {name: form for name, form, _ in SUBS}
    for step in range(STEPS):
        vals = {x: rng.getrandbits(8) for x, _, _, _ in inputs}
        lines.append("    #10 " + " ".join(f"{x} = 8'h{v:02x};" for x, v in vals.items()))
        lines.append("    #2 clk = 1'b1;")
        lines.append("    #2 clk = 1'b0;")
        for x, y, sub, c in inputs:
            if c == "direct":
                # the driver's own call, between the others' calls
                if sub == "task":
                    lines.append(f"    {forms[sub].format(x=x, y=y)}")
                else:
                    lines.append(f"    {y} = {forms[sub].format(x=x)};")
            lines.append(f'    $display("{step} {sub} {c} %h", {y});')
            exp.append(f"{step} {sub} {c} {funcs[sub](vals[x]):02x}")
    # iff: always_ff with a guard that calls a static function, which an
    # always_comb calls too.
    items += [
        "  always_ff @(posedge clk iff fe(en)) q0 <= q0 + 1;",
        "  always_ff @(posedge clk iff fe(en)) q1 <= q1 + 2;",
        "  always_comb ze = fe(en);",
    ]
    q0 = q1 = 0
    for e in ENABLES:
        lines.append(f"    #10 en = 1'b{e};")
        lines.append("    #2 clk = 1'b1;")
        lines.append("    #2 clk = 1'b0;")
        lines.append(f'    $display("iff {e} %0d %0d %b", q0, q1, ze);')
        if e == "1":
            q0, q1 = q0 + 1, q1 + 2
        exp.append(f"iff {e} {q0} {q1} {e}")
    head = [
        "/*",
        f":name: {NAME}",
        ":description: static functions and tasks called from several "
        "always_comb, always_latch and always_ff procedures, always @*, a continuous assignment "
        "and the driver, and from the iff condition of always_ff, each caller with its own "
        "result",
        ":tags: proc subroutines",
        ":type: simulation",
        ":lrm: 1800-2023 9.2.2.2 9.2.2.3 9.2.2.4 9.4.2.3 13.3.1 13.4",
        ":oracle: spec",
        ":spec_check: ai",
        ":fourstate: yes",
        ":nondeterministic: no",
        f":generator: gen_static_call_matrix.py@{VERSION}",
        ":top_module: top",
        f":expected_stdout: @{NAME}.expected",
        "*/",
        "// Generated by tools/gen_static_call_matrix.py; change the generator, not this file.",
        "`timescale 1ns/1ns",
        "module top;",
        "  logic clk = 1'b0, le = 1'b1, en = 1'b0, ze;",
        "  int q0 = 0, q1 = 0;",
        "  function logic [7:0] f(logic [7:0] x);",
        "    return x + 8'h01;",
        "  endfunction",
        "  function logic [7:0] h(logic [7:0] x);",
        "    return f(x) ^ 8'h0f;",
        "  endfunction",
        "  function logic fe(logic e);",
        "    return e;",
        "  endfunction",
        "  task tk(input logic [7:0] x, output logic [7:0] y);",
        "    y = x + 8'h02;",
        "  endtask",
    ]
    sv = head + decl + items + ["  initial begin"] + lines + ["    $finish;", "  end",
                                                            "endmodule", ""]
    return {f"{NAME}.sv": "\n".join(sv), f"{NAME}.expected": "\n".join(exp) + "\n"}


def main():
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("--check", action="store_true")
    args = ap.parse_args()
    bad = 0
    for name, text in files().items():
        path = OUT / name
        if args.check:
            if not path.exists() or path.read_text(encoding="utf-8") != text:
                print(f"gen_static_call_matrix: {path.relative_to(OUT.parent.parent.parent)} "
                      "is out of date; run tools/gen_static_call_matrix.py", file=sys.stderr)
                bad += 1
        else:
            path.write_text(text, encoding="utf-8")
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main())
