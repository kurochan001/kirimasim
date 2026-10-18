#!/usr/bin/env python3
"""Generates the matrix test of integral values converted to strings of
TEST_SUITE.md ("Mechanism matrix") in tests/suite/values/.

  tools/gen_string_conversion_matrix.py           # writes the files
  tools/gen_string_conversion_matrix.py --check   # exit 1 if a file differs from what it would write

An integral value cast to a string becomes its bytes, the most
significant first, after zero-extension to a whole number of bytes, and
its zero bytes are dropped: a string holds no "\\0" (6.16,
VALUE_SEMANTICS.md 7.6). x and z bits read as 0 before the zero bytes
are dropped (a rule of VALUE_SEMANTICS.md 7.6; the standard does not say
how 4-state values convert), so a byte of only x and z bits is dropped
too.

The axes: the value (16, 24, 32 and 12 bits, all zero, wider than 64
bits, signed, and with x or z in a bit, a nibble, a whole byte, all
bits, the top bits of a part byte and a signed value); where it is converted (a constant assigned to a string,
an integral variable assigned to one, an argument of a string formal, a
function's return value, a member of an unpacked structure, an element
of an array of strings, and an automatic string of a task). Each line
shows the length and the text.

The expected outputs follow from the rules above, computed below. A
person checks them once for all the tests made from them (TEST_SUITE.md,
"Sampling by people").
"""

from __future__ import annotations

import argparse
import sys
from pathlib import Path

OUT = Path(__file__).resolve().parent.parent / "tests" / "suite" / "values"
NAME = "string_conversion_matrix"
# The version of the rules below (TEST_SUITE.md, :generator:). Raise it
# whenever they change.
VERSION = 2

# (name, the declaration of the variable with {n}, the literal, its width
# and value, with x and z bits read as 0)
VALUES = [
    ("v16", "logic [15:0] {n};", "16'h4142", 16, 0x4142),
    ("v24_zero_inside", "logic [23:0] {n};", "24'h410042", 24, 0x410042),
    ("v32_leading_zeros", "logic [31:0] {n};", "32'h00000041", 32, 0x41),
    ("v12", "logic [11:0] {n};", "12'h044", 12, 0x044),
    ("v8_zero", "logic [7:0] {n};", "8'h00", 8, 0),
    ("v80", "logic [79:0] {n};", "80'h4100_4200_4300_0044_0045", 80,
     0x4100_4200_4300_0044_0045),
    ("signed16", "logic signed [15:0] {n};", "16'sh5a59", 16, 0x5A59),
    ("x_byte", "logic [15:0] {n};", "16'h41xx", 16, 0x4100),
    ("x_bit", "logic [15:0] {n};", "16'b0100_0001_0100_001x", 16, 0x4142),
    ("z_nibble", "logic [23:0] {n};", "24'h4z4243", 24, 0x404243),
    ("all_x", "logic [7:0] {n};", "8'hxx", 8, 0),
    ("z_top_of_12", "logic [11:0] {n};", "12'hz41", 12, 0x041),
    ("signed_x", "logic signed [15:0] {n};", "16'sh4x41", 16, 0x4041),
]
PLACES = ["constant", "variable", "argument", "return", "member", "element", "automatic"]


def known(literal):
    """The value of a sized literal with its x and z bits read as 0."""
    width, rest = literal.split("'")
    base = {"h": 16, "b": 2}[rest.lstrip("s")[0]]
    digits = rest.lstrip("s")[1:].replace("_", "")
    bits = 4 if base == 16 else 1
    value = 0
    for c in digits:
        value = (value << bits) | (0 if c in "xz" else int(c, base))
    return value & ((1 << int(width)) - 1)


def text(width, value):
    """The string a value of the width becomes."""
    nbytes = (width + 7) // 8
    out = []
    for k in reversed(range(nbytes)):
        b = (value >> (8 * k)) & 0xFF
        if b:
            out.append(chr(b))
    return "".join(out)


def files():
    decl, funcs, body, auto, exp = [], [], [], [], []
    decl += [
        "  typedef struct { int k; string m; } rec_t;",
        "  string s;",
        "  rec_t r;",
        "  string sa [0:2];",
        "  function automatic string pass(string a);",
        "    return a;",
        "  endfunction",
    ]
    for name, d, literal, width, value in VALUES:
        assert known(literal) == value, name
        decl.append("  " + d.replace("{n}", name))
        funcs += [f"  function automatic string ret_{name}();",
                  f"    return string'({literal});", "  endfunction"]
        expect = text(width, value)
        for place in PLACES:
            tag = f"{place} {name}"
            show = {"member": "r.m", "element": "sa[1]", "automatic": "as"}.get(place, "s")
            stmt = {
                "constant": f"s = string'({literal});",
                "variable": f"{name} = {literal}; s = string'({name});",
                "argument": f"{name} = {literal}; s = pass(string'({name}));",
                "return": f"s = ret_{name}();",
                "member": f"{name} = {literal}; r.m = string'({name});",
                "element": f"{name} = {literal}; sa[1] = string'({name});",
                "automatic": f"{name} = {literal}; as = string'({name});",
            }[place]
            line = f'{stmt} $display("{tag} %0d [%s]", {show}.len(), {show});'
            (auto if place == "automatic" else body).append(line)
            exp.append(f"{tag} {len(expect)} [{expect}]")
    # The automatic lines come last, in the task.
    exp = [e for e in exp if not e.startswith("automatic")] + \
          [e for e in exp if e.startswith("automatic")]
    head = [
        "/*",
        f":name: {NAME}",
        ":description: integral values converted to strings: bytes from the most significant, "
        "zero-extended to whole bytes, x and z bits read as 0, zero bytes dropped, for "
        "constants, variables, arguments, return values, structure members, array elements and "
        "automatic strings, values of 8 to 80 bits, signed and unsigned, with and without x "
        "and z",
        ":tags: values strings",
        ":type: simulation",
        ":lrm: 1800-2023 6.16 13.5",
        ":oracle: spec",
        ":spec_check: ai",
        ":fourstate: yes",
        ":nondeterministic: no",
        f":generator: gen_string_conversion_matrix.py@{VERSION}",
        ":top_module: top",
        f":expected_stdout: @{NAME}.expected",
        "*/",
        "// Generated by tools/gen_string_conversion_matrix.py; change the generator, not this "
        "file.",
        "`timescale 1ns/1ns",
        "module top;",
    ]
    task = ["  task automatic t;", "    string as;"] + ["    " + s for s in auto] + ["  endtask"]
    sv = (head + decl + funcs + task + ["  initial begin"] + ["    " + s for s in body]
          + ["    t();", "  end", "endmodule", ""])
    return {f"{NAME}.sv": "\n".join(sv), f"{NAME}.expected": "\n".join(exp) + "\n"}


def main():
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("--check", action="store_true")
    args = ap.parse_args()
    bad = 0
    for name, text_ in files().items():
        path = OUT / name
        if args.check:
            if not path.exists() or path.read_text(encoding="utf-8") != text_:
                print(f"gen_string_conversion_matrix: "
                      f"{path.relative_to(OUT.parent.parent.parent)} is out of date; run "
                      "tools/gen_string_conversion_matrix.py", file=sys.stderr)
                bad += 1
        else:
            path.write_text(text_, encoding="utf-8")
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main())
