#!/usr/bin/env python3
# Copyright (c) 2026 The kirimasim Authors
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

"""Generates the matrix tests of the longest static prefix of selects from
unpacked arrays and structures in the implicit sensitivity of always_comb
and always_latch, of TEST_SUITE.md ("Mechanism matrix"), in
tests/suite/proc/.

  tools/gen_static_prefix_sensitivity_matrix.py           # writes the files
  tools/gen_static_prefix_sensitivity_matrix.py --check   # exit 1 if a file differs from what it would write

The implicit sensitivity takes each select the block reads by the
expansion of its longest static prefix (9.2.2.2.1; SCHEDULER_SPEC.md
6.3). An indexing select is a static prefix when what it selects from is
one and its index is a constant; a member select is one when what it
selects from is one (11.5.3). So an index that is not a constant ends
the prefix at the array it selects from, whatever members and constant
indices follow it: the block reads all of that array, the other members
of its elements too. A select with constant indices only is its own
prefix. always_latch has the same sensitivity (9.2.2.3).

The axes: the block (always_comb, always_latch); where the select is
read (in the block, in a function it calls); and the shape of the select
(an array of structures, of nested structures, with two dimensions, with
an array member, inside a structure, of packed structures, with members
of other types, with descending ranges that do not start at 0, and a
plain array of two dimensions; the first index that is not a constant in
each position, or none); and where the array is and how it is named (a
variable of the module, of a package, of an interface, of a generate
block, a hierarchical name). Each test changes one element or member at
a time, after the block ran at time 0, and prints how many times the
block has run: one more for a change inside the expansion of the prefix,
the same for one outside.

No test has a member that is a queue, a dynamic array or a class handle:
kirimasim does not watch these in an array for an implicit sensitivity
yet, and reports a block whose prefix holds one as unsupported.

The expected outputs follow from the rules above, computed below from
the list of changes of each shape.
"""

from __future__ import annotations

import argparse
import sys
from pathlib import Path

OUT = Path(__file__).resolve().parent.parent / "tests" / "suite" / "proc"
PREFIX = "static_prefix_sensitivity_"
# The version of the rules below (TEST_SUITE.md, :generator:). Raise it
# whenever they change.
VERSION = 1

BLOCKS = ["always_comb", "always_latch"]
PAIR = "typedef struct {logic [7:0] f; logic [7:0] g;} pair_t;"
OUTER = "typedef struct {logic [7:0] h; pair_t in;} outer_t;"
# (name, the declarations, the select with {i} for the index that is not a
# constant, the value i has, the longest static prefix, the changes in
# order: (what is assigned, whether it is inside the expansion of the
# prefix, the value when it is not a byte), what stands before the module)
SHAPES = [
    ("member", [PAIR, "pair_t arr [0:1];"], "arr[{i}].f", 0, "arr",
     [("arr[0].g", True), ("arr[1].g", True), ("arr[1].f", True)]),
    ("member_static", [PAIR, "pair_t arr [0:1];"], "arr[0].f", 0, "arr[0].f",
     [("arr[0].g", False), ("arr[1].f", False), ("arr[0].f", True)]),
    ("nested_member", [PAIR, OUTER, "outer_t o [0:1];"], "o[{i}].in.f", 0, "o",
     [("o[0].h", True), ("o[1].in.g", True), ("o[1].in.f", True)]),
    ("nested_member_static", [PAIR, OUTER, "outer_t o [0:1];"], "o[1].in.f", 0, "o[1].in.f",
     [("o[1].h", False), ("o[1].in.g", False), ("o[0].in.f", False), ("o[1].in.f", True)]),
    ("second_index", [PAIR, "pair_t a2 [0:1][0:1];"], "a2[0][{i}].f", 0, "a2[0]",
     [("a2[0][1].g", True), ("a2[1][0].f", False), ("a2[1][1].g", False), ("a2[0][1].f", True)]),
    ("first_index", [PAIR, "pair_t a2 [0:1][0:1];"], "a2[{i}][0].f", 0, "a2",
     [("a2[0][1].g", True), ("a2[1][1].f", True), ("a2[1][0].g", True)]),
    ("array_member_outer", ["typedef struct {logic [7:0] k; logic [7:0] m [0:1];} arrm_t;",
                            "arrm_t am [0:1];"], "am[{i}].m[1]", 0, "am",
     [("am[0].k", True), ("am[1].m[0]", True), ("am[1].k", True)]),
    ("array_member_inner", ["typedef struct {logic [7:0] k; logic [7:0] m [0:1];} arrm_t;",
                            "arrm_t am [0:1];"], "am[1].m[{i}]", 0, "am[1].m",
     [("am[1].k", False), ("am[0].m[0]", False), ("am[1].m[1]", True)]),
    ("inside_structure", [PAIR, "struct {logic [7:0] z; pair_t ps [0:1];} s;"], "s.ps[{i}].f", 0,
     "s.ps", [("s.z", False), ("s.ps[1].g", True), ("s.ps[0].g", True)]),
    ("packed_member", ["typedef struct packed {logic [7:0] f; logic [7:0] g;} ppair_t;",
                       "ppair_t pa [0:1];"], "pa[{i}].f", 0, "pa",
     [("pa[0].g", True), ("pa[1].g", True), ("pa[1].f", True)]),
    ("descending_ranges", [PAIR, "pair_t dn [3:2][5:4];"], "dn[3][{i}].f", 5, "dn[3]",
     [("dn[3][4].g", True), ("dn[2][5].f", False), ("dn[2][4].g", False), ("dn[3][4].f", True)]),
    ("plain_array", ["logic [7:0] mem [0:1][0:1];"], "mem[0][{i}]", 0, "mem[0]",
     [("mem[0][1]", True), ("mem[1][0]", False), ("mem[1][1]", False)]),
    ("mixed_members", ["typedef struct {logic [7:0] f; string s; real r; int n;} mixed_t;",
                       "mixed_t mx [0:1];"], "mx[{i}].f", 0, "mx",
     [("mx[1].s", True, '"a"'), ("mx[1].r", True, "1.5"), ("mx[0].n", True, "3")]),
    ("package_variable", [], "pk::arr[{i}].f", 0, "pk::arr",
     [("pk::arr[0].g", True), ("pk::arr[1].g", True)],
     ["package pk;", f"  {PAIR}", "  pair_t arr [0:1];", "endpackage"]),
    ("interface_member", ["ifc i0();"], "i0.arr[{i}].f", 0, "i0.arr",
     [("i0.arr[0].g", True), ("i0.arr[1].g", True)],
     ["interface ifc;", f"  {PAIR}", "  pair_t arr [0:1];", "endinterface"]),
    ("generate_block", [PAIR, "if (1) begin : gen", "  pair_t arr [0:1];", "end"],
     "gen.arr[{i}].f", 0, "gen.arr", [("gen.arr[0].g", True), ("gen.arr[1].g", True)]),
    ("hierarchical_name", [PAIR, "pair_t arr [0:1];"], "top.arr[{i}].f", 0, "top.arr",
     [("arr[0].g", True), ("top.arr[1].g", True)]),
]
# (name, what the description calls it)
READS = [("block", "in the block"), ("function", "in a function it calls")]


def header(name, description, lrm):
    return [
        "/*",
        f":name: {name}",
        f":description: {description}",
        ":tags: proc",
        ":type: simulation",
        ":lrm: 1800-2023 " + " ".join(lrm),
        ":oracle: spec",
        ":spec_check: ai",
        ":fourstate: no",
        ":nondeterministic: no",
        f":generator: gen_static_prefix_sensitivity_matrix.py@{VERSION}",
        ":issue: 127",
        ":top_module: top",
        f":expected_stdout: @{name}.expected",
        "*/",
        "// Generated by tools/gen_static_prefix_sensitivity_matrix.py; change the generator, "
        "not this file.",
        "`timescale 1ns/1ns",
    ]


def case(block, read, shape):
    sname, decls, select, start, prefix, changes = shape[:6]
    before = shape[6] if len(shape) > 6 else []
    rname, where = read
    name = f"{PREFIX}{block}_{rname}_{sname}"
    lines = before + ["module top;"] + [f"  {d}" for d in decls]
    lines += [f"  int i = {start};", "  logic [7:0] y;", "  int runs;"]
    if rname == "function":
        lines += ["  function automatic logic [7:0] rd(int k);",
                  f"    return {select.format(i='k')};", "  endfunction"]
        value = "rd(i)"
    else:
        value = select.format(i="i")
    lines += [f"  {block} begin", "    runs++;", f"    y = {value};", "  end", "  initial begin"]
    expected, runs = [], 1
    for n, change in enumerate(changes):
        target, inside = change[:2]
        assigned = change[2] if len(change) > 2 else f"8'd{n + 1}"
        runs += int(inside)
        lines += [f"    #1 {target} = {assigned};",
                  f"    #1 $display(\"{target}: runs=%0d\", runs);"]
        expected.append(f"{target}: runs={runs}")
    lines += ["    $finish;", "  end", "endmodule"]
    lrm = ["9.2.2.2.1", "11.5.3"] + (["9.2.2.3"] if block == "always_latch" else [])
    description = (f"{block} reads {select.format(i='i')} {where}, whose longest static prefix "
                   f"is {prefix}: it runs again for a change inside {prefix} only")
    return name, header(name, description, lrm) + lines, expected


def files():
    out = {}
    for block in BLOCKS:
        for read in READS:
            for shape in SHAPES:
                name, text, expected = case(block, read, shape)
                out[f"{name}.sv"] = "\n".join(text) + "\n"
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
                print("gen_static_prefix_sensitivity_matrix: "
                      f"{path.relative_to(OUT.parent.parent.parent)} is out of date; run "
                      "tools/gen_static_prefix_sensitivity_matrix.py", file=sys.stderr)
                bad += 1
        else:
            path.write_text(text, encoding="utf-8")
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main())
