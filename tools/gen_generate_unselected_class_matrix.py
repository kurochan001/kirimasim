#!/usr/bin/env python3
# Copyright (c) 2026 The kirimasim Authors
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

"""Generates the matrix tests of a class declared in a generate block that
is not instantiated (TEST_SUITE.md, "Mechanism matrix") in
tests/suite/hier/.

  tools/gen_generate_unselected_class_matrix.py           # writes the files
  tools/gen_generate_unselected_class_matrix.py --check   # exit 1 if a file differs from what it would write

The rule: 27.5 and 27.4 of the standard. Only the generate block that a
condition selects, and only the iterations a loop makes, are
instantiated. A class and a variable of it in a block that is not
instantiated are not part of the design: nothing is made for them, and
the block that is selected is not disturbed, also when it declares a
class of the same name.

The axes: the generate construct (if with no else, if with else, case, a
loop of no iterations), what the class has (a static property, a static
queue, a static method with a static property, a property that is not
static), and the place of the construct (the top module, a module
instantiated twice with the two values of the condition, an interface, a
generate block that is instantiated).

In each test the block that is not selected declares a class of the same
name as the selected one, with other values, and a variable of it. The
expected output is computed here from the values, without reading
kirimasim's code. A person checks it once for all the tests made from it
(TEST_SUITE.md, "Sampling by people").
"""

import argparse
import sys
from pathlib import Path

OUT = Path(__file__).resolve().parent.parent / "tests" / "suite" / "hier"
# The version of the rules below (TEST_SUITE.md, :generator:). Raise it
# whenever they change.
VERSION = 1

CONSTRUCTS = ["if", "if_else", "case", "loop"]
PLACES = ["top", "child", "interface", "nested"]

# (key, the members of the class as a function of a value v, the expression
# that reads the value back through the class C or its handle h).
MEMBERS = [
    ("static_property", lambda v: f"static int s = {v};", "C::s"),
    ("static_queue", lambda v: f"static int q [$] = '{{{v}, {v + 1}}};", "C::q[0]"),
    ("static_method", lambda v: f"static int s = {v}; static function int get(); return s; "
                                "endfunction", "C::get()"),
    ("property", lambda v: f"int f = {v};", "h.f"),
]


def block(label, members, read, v, n):
    """The items of one generate block: a class, a variable of it, and a
    line that reads the value back, at a time of its own (the value of P
    and the number of the member), so the order of the lines does not
    depend on the order of the processes (4.7)."""
    return [f"class C; {members(v)} endclass", "C h = new;",
            f'initial #(P * 10 + {n}) $display("%m {label}: %0d", {read});']


def construct(kind, key, members, read, v, sel, n):
    """The generate construct for the parameter value sel (1 or 2). Returns
    the lines and the name of the block that is instantiated, or None."""
    label = key
    if kind == "if":
        # No else: with sel == 2 nothing is instantiated.
        lines = [f"if (P == 1) begin : {key}_a"] + \
                ["  " + x for x in block(label, members, read, v, n)] + ["end"]
        return lines, (f"{key}_a" if sel == 1 else None), v
    if kind == "if_else":
        lines = [f"if (P == 1) begin : {key}_a"] + \
                ["  " + x for x in block(label, members, read, v, n)] + \
                [f"end else begin : {key}_b"] + \
                ["  " + x for x in block(label, members, read, v + 100, n)] + ["end"]
        return lines, (f"{key}_a" if sel == 1 else f"{key}_b"), (v if sel == 1 else v + 100)
    if kind == "case":
        lines = ["case (P)", f"  1: begin : {key}_a"] + \
                ["    " + x for x in block(label, members, read, v, n)] + \
                ["  end", f"  default: begin : {key}_b"] + \
                ["    " + x for x in block(label, members, read, v + 100, n)] + \
                ["  end", "endcase"]
        return lines, (f"{key}_a" if sel == 1 else f"{key}_b"), (v if sel == 1 else v + 100)
    # A loop of P - 1 iterations: none with sel == 1, one with sel == 2.
    lines = [f"for (genvar i = 0; i < P - 1; i++) begin : {key}_l"] + \
            ["  " + x for x in block(label, members, read, v, n)] + ["end"]
    return lines, (f"{key}_l[0]" if sel == 2 else None), v


def test(kind, place):
    name = f"generate_unselected_class_{kind}_{place}"
    what = {"if": "an if generate with no else", "if_else": "an if-else generate",
            "case": "a case generate", "loop": "a loop generate of no iterations"}[kind]
    where = {"top": "the top module", "child": "a module instantiated with both values",
             "interface": "an interface", "nested": "a generate block that is instantiated"}[place]
    sv = ["/*", f":name: {name}",
          f":description: a class and a variable of it in the block of {what} that is not "
          f"instantiated are not part of the design, in {where}; the block that is selected "
          "keeps its own class of the same name",
          ":tags: hier", ":type: simulation", ":lrm: 1800-2023 27.5 27.4 8.9", ":oracle: spec",
          ":spec_check: ai", ":fourstate: no", ":nondeterministic: no",
          f":generator: gen_generate_unselected_class_matrix.py@{VERSION}", ":top_module: top",
          f":expected_stdout: @{name}.expected", "*/",
          "// Generated by tools/gen_generate_unselected_class_matrix.py; change the generator, "
          "not this file.", "`timescale 1ns/1ns"]
    # The parameter values of the element that holds the constructs, and the
    # path of each.
    if place == "top":
        holders = [("top", 2)]
    elif place == "child":
        holders = [("top.u1", 1), ("top.u2", 2)]
    elif place == "interface":
        holders = [("top.i1", 1), ("top.i2", 2)]
    else:
        holders = [("top.outer", 2)]
    items, expected = [], []
    for n, (key, members, read) in enumerate(MEMBERS):
        v = 10 * (n + 1)
        lines, _, _ = construct(kind, key, members, read, v, 1, n)
        items += lines
        for path, sel in holders:
            _, chosen, value = construct(kind, key, members, read, v, sel, n)
            if chosen is not None:
                expected.append((sel * 10 + n, f"{path}.{chosen} {key}: {value}"))
    indent = ["  " + x for x in items]
    if place == "top":
        sv += ["module top;", "  localparam P = 2;"] + indent
        sv += ['  initial #100 $display("done");', "endmodule"]
    elif place == "child":
        sv += ["module child #(parameter P = 1) ();"] + indent + ["endmodule"]
        sv += ["module top;", "  child #(1) u1 ();", "  child #(2) u2 ();",
               '  initial #100 $display("done");', "endmodule"]
    elif place == "interface":
        sv += ["interface holder #(parameter P = 1) ();"] + indent + ["endinterface"]
        sv += ["module top;", "  holder #(1) i1 ();", "  holder #(2) i2 ();",
               '  initial #100 $display("done");', "endmodule"]
    else:
        sv += ["module top;", "  localparam P = 2;", "  if (P == 2) begin : outer"]
        sv += ["  " + x for x in indent] + ["  end"]
        sv += ['  initial #100 $display("done");', "endmodule"]
    return name, sv, [text for _, text in sorted(expected)] + ["done"]


def files():
    out = {}
    for kind in CONSTRUCTS:
        for place in PLACES:
            name, sv, expected = test(kind, place)
            out[f"{name}.sv"] = "\n".join(sv) + "\n"
            out[f"{name}.expected"] = "\n".join(expected) + "\n"
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
                print("gen_generate_unselected_class_matrix: "
                      f"{path.relative_to(OUT.parent.parent.parent)} is out of date; run "
                      "tools/gen_generate_unselected_class_matrix.py", file=sys.stderr)
                bad += 1
        else:
            path.write_text(text, encoding="utf-8")
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main())
