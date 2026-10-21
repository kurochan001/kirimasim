#!/usr/bin/env python3
# Copyright (c) 2026 The kirimasim Authors
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

"""Generates the matrix test of streaming unpacked aggregates of
TEST_SUITE.md ("Mechanism matrix") in tests/suite/values/.

  tools/gen_stream_matrix.py           # writes the files
  tools/gen_stream_matrix.py --check   # exit 1 if a file differs from what it would write

A streaming concatenation of a fixed-size unpacked array or an unpacked
structure lays out the elements in foreach order (from the left bound,
the rightmost dimension fastest) and the members in declaration order,
and takes them back in the same order as a target (11.4.14.1,
VALUE_SEMANTICS.md 8). `>>` keeps the stream; `<< k` reverses the order
of its k-bit blocks; a target of `<< k` takes the inverse. Every shape
here is 16 bits wide and every k divides 16, so no block is short. Bits
are kept as they are, x and z included.

The axes: the shape (a structure, one with a nested structure, one with
an unpacked array member, an array of structures, a structure holding an
array of structures, a two-dimensional array, an array with a descending
range); the operation (packing with >>, << , << 4 and << 8, a cast of a
stream used as an operand, and unpacking with >>, << 4 and << 8); and
where the aggregate lives (a module variable, and an automatic variable
of a task for the shapes an automatic variable may have).

The expected outputs follow from the rules above, computed below. A
person checks them once for all the tests made from them (TEST_SUITE.md,
"Sampling by people").
"""

from __future__ import annotations

import argparse
import sys
from pathlib import Path

OUT = Path(__file__).resolve().parent.parent / "tests" / "suite" / "values"
NAME = "stream_matrix"
# The version of the rules below (TEST_SUITE.md, :generator:). Raise it
# whenever they change.
VERSION = 1

# (name, the declaration with {n} for the variable, its leaves in stream
# order: (path with {n}, width), whether an automatic variable may have
# it)
SHAPES = [
    ("struct", "struct { logic [3:0] a; logic [7:0] b; logic [3:0] c; } {n};",
     [("{n}.a", 4), ("{n}.b", 8), ("{n}.c", 4)], True),
    ("nested", "struct { logic [3:0] a; struct { logic [1:0] x; logic [5:0] y; } s; "
     "logic [3:0] c; } {n};",
     [("{n}.a", 4), ("{n}.s.x", 2), ("{n}.s.y", 6), ("{n}.c", 4)], True),
    ("array_member", "struct { logic [3:0] a; logic [1:0] m [0:1]; logic [7:0] b; } {n};",
     [("{n}.a", 4), ("{n}.m[0]", 2), ("{n}.m[1]", 2), ("{n}.b", 8)], False),
    ("struct_array", "struct { logic [3:0] a; logic [3:0] b; } {n} [0:1];",
     [("{n}[0].a", 4), ("{n}[0].b", 4), ("{n}[1].a", 4), ("{n}[1].b", 4)], False),
    ("struct_array_member",
     "struct { logic [3:0] a; struct { logic [1:0] p; logic [1:0] q; } s [0:1]; "
     "logic [3:0] b; } {n};",
     [("{n}.a", 4), ("{n}.s[0].p", 2), ("{n}.s[0].q", 2), ("{n}.s[1].p", 2), ("{n}.s[1].q", 2),
      ("{n}.b", 4)], False),
    ("array_2d", "logic [3:0] {n} [0:1][0:1];",
     [("{n}[0][0]", 4), ("{n}[0][1]", 4), ("{n}[1][0]", 4), ("{n}[1][1]", 4)], True),
    ("array_down", "logic [3:0] {n} [3:0];",
     [("{n}[3]", 4), ("{n}[2]", 4), ("{n}[1]", 4), ("{n}[0]", 4)], True),
]
WIDTH = 16
SOURCE = "10x10110z01x1100"  # what the leaves hold before packing
VALUE = "0z1101x010011101"   # what is unpacked
# (name, statement with {x} for the aggregate and {w} for a 16-bit
# variable, packs)
OPS = [
    ("pack", "{w} = {{>>{{{x}}}}};", True),
    ("pack_l1", "{w} = {{<<{{{x}}}}};", True),
    ("pack_l4", "{w} = {{<<4{{{x}}}}};", True),
    ("pack_l8", "{w} = {{<<8{{{x}}}}};", True),
    ("cast", "{w} = ~w_t'({{>>{{{x}}}}});", True),
    ("unpack", "{{>>{{{x}}}}} = 16'b{v};", False),
    ("unpack_l4", "{{<<4{{{x}}}}} = 16'b{v};", False),
    ("unpack_l8", "{{<<8{{{x}}}}} = 16'b{v};", False),
]


def blocks_reversed(bits, k):
    return "".join(bits[i:i + k] for i in reversed(range(0, len(bits), k)))


def invert(bits):
    return "".join({"0": "1", "1": "0"}.get(c, "x") for c in bits)


def result(op):
    """The 16-bit variable after a packing, or the stream the leaves hold
    after an unpacking."""
    return {
        "pack": SOURCE,
        "pack_l1": blocks_reversed(SOURCE, 1),
        "pack_l4": blocks_reversed(SOURCE, 4),
        "pack_l8": blocks_reversed(SOURCE, 8),
        "cast": invert(SOURCE),
        "unpack": VALUE,
        "unpack_l4": blocks_reversed(VALUE, 4),
        "unpack_l8": blocks_reversed(VALUE, 8),
    }[op]


def split(bits, leaves):
    out, at = [], 0
    for _, w in leaves:
        out.append(bits[at:at + w])
        at += w
    return out


def statements(where, shape, leaves, var, w):
    """The statements of every operation on one aggregate, and the expected
    lines."""
    lines, exp = [], []
    paths = [p.replace("{n}", var) for p, _ in leaves]
    for op, form, packs in OPS:
        tag = f"{where} {shape} {op}"
        if packs:
            for (path, _), bits in zip(leaves, split(SOURCE, leaves)):
                lines.append(f"{path.replace('{n}', var)} = {len(bits)}'b{bits};")
            lines.append(form.format(x=var, w=w))
            lines.append(f'$display("{tag} %b", {w});')
            exp.append(f"{tag} {result(op)}")
        else:
            lines.append(form.format(x=var, v=VALUE))
            fmt = " ".join("%b" for _ in leaves)
            lines.append(f'$display("{tag} {fmt}", {", ".join(paths)});')
            exp.append(f"{tag} " + " ".join(split(result(op), leaves)))
    return lines, exp


def files():
    decl, body, auto_decl, auto_body, exp, auto_exp = [], [], [], [], [], []
    for shape, d, leaves, automatic in SHAPES:
        assert sum(w for _, w in leaves) == WIDTH
        decl.append("  " + d.replace("{n}", f"x_{shape}"))
        lines, e = statements("module", shape, leaves, f"x_{shape}", "w")
        body += ["    " + s for s in lines]
        exp += e
        if automatic:
            auto_decl.append("    " + d.replace("{n}", f"y_{shape}"))
            lines, e = statements("automatic", shape, leaves, f"y_{shape}", "aw")
            auto_body += ["    " + s for s in lines]
            auto_exp += e
    head = [
        "/*",
        f":name: {NAME}",
        ":description: streaming of unpacked structures and arrays: a structure, a nested one, "
        "one with an unpacked array member, an array of structures, a structure holding one, a "
        "two-dimensional array and a descending array, packed with >>, <<, << 4 and << 8, cast "
        "as an operand, and unpacked with >>, << 4 and << 8, four-state, in module and "
        "automatic variables",
        ":tags: values streaming",
        ":type: simulation",
        ":lrm: 1800-2023 11.4.14 11.4.14.1 11.4.14.2 11.4.14.3",
        ":oracle: spec",
        ":spec_check: ai",
        ":fourstate: yes",
        ":nondeterministic: no",
        f":generator: gen_stream_matrix.py@{VERSION}",
        ":top_module: top",
        f":expected_stdout: @{NAME}.expected",
        "*/",
        "// Generated by tools/gen_stream_matrix.py; change the generator, not this file.",
        "`timescale 1ns/1ns",
        "module top;",
        "  typedef logic [15:0] w_t;",
        "  logic [15:0] w;",
    ]
    task = (["  task automatic t;", "    logic [15:0] aw;"] + auto_decl + auto_body
            + ["  endtask"])
    sv = (head + decl + task + ["  initial begin"] + body + ["    t();", "  end", "endmodule",
                                                              ""])
    return {f"{NAME}.sv": "\n".join(sv), f"{NAME}.expected": "\n".join(exp + auto_exp) + "\n"}


def main():
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("--check", action="store_true")
    args = ap.parse_args()
    bad = 0
    for name, text in files().items():
        path = OUT / name
        if args.check:
            if not path.exists() or path.read_text(encoding="utf-8") != text:
                print(f"gen_stream_matrix: {path.relative_to(OUT.parent.parent.parent)} is out "
                      "of date; run tools/gen_stream_matrix.py", file=sys.stderr)
                bad += 1
        else:
            path.write_text(text, encoding="utf-8")
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main())
