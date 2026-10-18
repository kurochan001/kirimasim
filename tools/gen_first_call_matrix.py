#!/usr/bin/env python3
"""Generates the matrix tests of the first call of a static function of
TEST_SUITE.md ("Mechanism matrix") in tests/suite/proc/.

  tools/gen_first_call_matrix.py           # writes the files
  tools/gen_first_call_matrix.py --check   # exit 1 if a file differs from what it would write

A static function's variables and arguments are one per instance
(13.3.1, SIR_SPEC.md 3.3), made the first time the function is called.
That call may sit inside an expression that is being lowered, such as an
index of the element being written, and must leave that expression's
meaning alone.

One small design per place of the first call: an index of an element
written, read, written by a nonblocking, compound or increment
assignment, an index of a two-dimensional array and of an array of
structures, a bit-select and an indexed part-select written and read, a
string index, an argument of another call, an index of an output
argument, a delay, an intra-assignment delay, a $display argument, a
condition, a case item, a loop bound, a wait condition, a net declaration assignment,
a continuous assignment, and an input port's connection. The function
has many variables, so that its first call adds more to the design than
the design held before.

The expected outputs follow from the rules above, computed below. A
person checks them once for all the tests made from them (TEST_SUITE.md,
"Sampling by people").
"""

from __future__ import annotations

import argparse
import sys
from pathlib import Path

OUT = Path(__file__).resolve().parent.parent / "tests" / "suite" / "proc"
PREFIX = "first_call_"
# The version of the rules below (TEST_SUITE.md, :generator:). Raise it
# whenever they change.
VERSION = 2

LOCALS = 64
INIT = "'{8'h01, 8'h02, 8'h03, 8'h04}"
# (place, declarations, statements with f(...) the first call, what is
# printed, the expected line). f returns its argument; g adds 1; t writes
# 8'h44 to its output. A place in a module item has no statements but a
# wait for the continuous assignments.
PLACES = [
    ("store_index", ["logic [7:0] m [0:3];"], ["m[f(1)] = 8'h55;"], "m[1]", "55"),
    ("load_index", [f"logic [7:0] m [0:3] = {INIT};", "logic [7:0] y;"], ["y = m[f(2)];"], "y",
     "03"),
    ("nba_index", ["logic [7:0] m [0:3];"], ["m[f(3)] <= 8'h77;", "#1;"], "m[3]", "77"),
    ("compound_index", [f"logic [7:0] m [0:3] = {INIT};"], ["m[f(0)] += 8'h10;"], "m[0]", "11"),
    ("increment_index", [f"logic [7:0] m [0:3] = {INIT};"], ["m[f(1)]++;"], "m[1]", "03"),
    ("index_2d", ["logic [7:0] m [0:3][0:1];"], ["m[f(2)][1] = 8'h66;"], "m[2][1]", "66"),
    ("struct_index", ["typedef struct packed { logic [3:0] a; logic [3:0] b; } s_t;",
                      "s_t m [0:3];"], ["m[f(2)].b = 4'h9;", "m[2].a = 4'h1;"], "m[2]", "19"),
    ("bit_store", ["logic [7:0] v = 8'h00;"], ["v[f(3)] = 1'b1;"], "v", "08"),
    ("bit_load", ["logic [7:0] v = 8'h08;", "logic y;"], ["y = v[f(3)];"], "y", "1"),
    ("part_store", ["logic [7:0] v = 8'h00;"], ["v[f(5) -: 2] = 2'b11;"], "v", "30"),
    ("part_load", ["logic [7:0] v = 8'h30;", "logic [1:0] y;"], ["y = v[f(5) -: 2];"], "y",
     "3"),
    ("string_index", ["string s = \"abc\";", "byte y;"], ["y = s[f(1)];"], "y", "62"),
    ("call_argument", ["logic [7:0] y;"], ["y = g(f(4));"], "y", "05"),
    ("output_index", ["logic [7:0] m [0:3];"], ["t(m[f(1)]);"], "m[1]", "44"),
    ("delay", ["logic [7:0] y;"], ["#(f(1)) y = 8'h11;"], "y", "11"),
    ("intra_delay", ["logic [7:0] y;"], ["y = #(f(1)) 8'h22;"], "y", "22"),
    ("display", ["logic [7:0] y = 8'h00;"], ['$display("%0d", f(7));'], "y", "00"),
    ("condition", ["logic [7:0] y;"], ["y = f(1) ? 8'haa : 8'hbb;"], "y", "aa"),
    ("case_item", ["logic [7:0] y;"], ["case (2) f(2): y = 8'hc1; default: y = 8'hc2; endcase"],
     "y", "c1"),
    ("loop_bound", ["logic [7:0] y = 8'h00;"], ["for (int i = 0; i < f(3); i++) y++;"], "y",
     "03"),
    ("wait", [f"logic [7:0] m [0:3] = {INIT};", "logic [7:0] y;"],
     ["wait (m[f(1)] == 8'h02) y = 8'h5e;"], "y", "5e"),
    ("net_declaration", ["logic [7:0] a = 8'h03;", "wire [7:0] w = f(a);"], ["#1;"], "w", "03"),
    ("continuous_assignment", ["logic [7:0] a = 8'h04;", "wire [7:0] w;", "assign w = f(a);"],
     ["#1;"], "w", "04"),
    ("port", ["logic [7:0] a = 8'h05;", "child c(f(a));"], ["#1;"], "c.i", "05"),
]
CHILD = ["module child(input logic [7:0] i);", "endmodule", ""]


def design(place, decls, stmts, shown, value):
    name = PREFIX + place
    names = ", ".join(f"s{k}" for k in range(LOCALS))
    head = [
        "/*",
        f":name: {name}",
        ":description: the first call of a static function with many variables inside "
        f"{place.replace('_', ' ')}",
        ":tags: proc subroutines",
        ":type: simulation",
        ":lrm: 1800-2023 13.3.1 13.4",
        ":oracle: spec",
        ":spec_check: ai",
        ":fourstate: yes",
        ":nondeterministic: no",
        f":generator: gen_first_call_matrix.py@{VERSION}",
        ":top_module: top",
        f":expected_stdout: @{name}.expected",
        "*/",
        "// Generated by tools/gen_first_call_matrix.py; change the generator, not this file.",
        "`timescale 1ns/1ns",
    ]
    sv = head + (CHILD if place == "port" else []) + [
        "module top;",
        "  function int f(int x);",
        f"    int {names};",
        f"    {{{names}}} = '0;",
        f"    s{LOCALS - 1} = x;",
        f"    return s{LOCALS - 1};",
        "  endfunction",
        "  function logic [7:0] g(logic [7:0] x);",
        "    return x + 8'h01;",
        "  endfunction",
        "  task t(output logic [7:0] y);",
        "    y = 8'h44;",
        "  endtask",
        *(f"  {d}" for d in decls),
        "  initial begin",
        *(f"    {s}" for s in stmts),
        f'    $display("%h", {shown});',
        "  end",
        "endmodule",
        "",
    ]
    exp = (["7"] if place == "display" else []) + [value]
    return {f"{name}.sv": "\n".join(sv), f"{name}.expected": "\n".join(exp) + "\n"}


def files():
    out = {}
    for place, decls, stmts, shown, value in PLACES:
        out.update(design(place, decls, stmts, shown, value))
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
                print(f"gen_first_call_matrix: {path.relative_to(OUT.parent.parent.parent)} is "
                      "out of date; run tools/gen_first_call_matrix.py", file=sys.stderr)
                bad += 1
        else:
            path.write_text(text, encoding="utf-8")
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main())
