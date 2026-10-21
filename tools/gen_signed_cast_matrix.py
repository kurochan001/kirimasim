#!/usr/bin/env python3
# Copyright (c) 2026 The kirimasim Authors
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

"""Generates the matrix test of $signed, $unsigned and the casts
signed'() and unsigned'(), of TEST_SUITE.md ("Mechanism matrix"), in
tests/suite/values/.

  tools/gen_signed_cast_matrix.py           # writes the files
  tools/gen_signed_cast_matrix.py --check   # exit 1 if a file differs from what it would write

$signed and signed'() give the same bits, signed; $unsigned and
unsigned'() the same bits, unsigned (11.7, 6.24.1). Where the value is
used then follows its signedness (VALUE_SEMANTICS.md 2, 3): it is
extended by its sign bit, x and z included, or by 0; it compares as a
signed or an unsigned number, and any x or z makes the comparison x;
>>> fills with the sign bit or with 0; %0d prints it as a signed or an
unsigned number, or x, X, z, Z (SYSTEM_TASKS.md 3.4).

The axes: the width (8, 64, 65, 100), two- and four-state, declared
signed and unsigned; the top bit (1, 0, and x and z in a four-state
value); the spelling ($signed, $unsigned, signed'(), unsigned'()); the
use (assigned to a wider unsigned variable, printed with %0d, compared
with 0 by <, shifted by >>> 3 into the wider variable); the place (a
module variable and an automatic variable of a task). The wider variable
is a multiple of 4 bits wide, so that every hexadecimal digit has four
bits.

The expected outputs follow from the rules above, computed below. For
reference (checked 2026-10-18): Icarus Verilog 13.0 prints every line as
expected, and Verilator 5.052 every line of the two-state variables (it
takes a four-state variable that is assigned z as a three-state one).
"""

from __future__ import annotations

import argparse
import sys
from pathlib import Path

OUT = Path(__file__).resolve().parent.parent / "tests" / "suite" / "values"
NAME = "signed_cast_matrix"
# The version of the rules below (TEST_SUITE.md, :generator:). Raise it
# whenever they change.
VERSION = 1
WIDTHS = [8, 64, 65, 100]
TYPES = [(four, signed) for four in (True, False) for signed in (False, True)]
# (name, the text with E for the operand, whether the result is signed)
SPELLINGS = [("fn_signed", "$signed(E)", True), ("fn_unsigned", "$unsigned(E)", False),
             ("cast_signed", "signed'(E)", True), ("cast_unsigned", "unsigned'(E)", False)]
USES = ["assign", "decimal", "less", "ashr"]
PLACES = ["module", "task"]
PATTERN = int("5a3c1e0f" "f0e1d2c3" "b4a59687" "78695a4b" "3c2d1e0f", 16)


class V:
    """A four-state value: a and b planes (b=1: x when a=1, z when a=0)."""

    def __init__(self, w, a, b=0, signed=False):
        m = (1 << w) - 1
        self.w, self.a, self.b, self.signed = w, a & m, b & m, signed


def operand(w, top):
    """The variable's value: a pattern below the top bit, which is `top`."""
    a = PATTERN & ((1 << (w - 1)) - 1)
    t = 1 << (w - 1)
    if top == "1":
        return V(w, a | t)
    if top == "x":
        return V(w, a | t, t)
    if top == "z":
        return V(w, a, t)
    return V(w, a)


def extend(v, w):
    """v at width w, by its own signedness (its sign bit, x or z included)."""
    a, b = v.a, v.b
    if w > v.w and v.signed:
        fill = ((1 << w) - 1) ^ ((1 << v.w) - 1)
        if (a >> (v.w - 1)) & 1:
            a |= fill
        if (b >> (v.w - 1)) & 1:
            b |= fill
    return V(w, a, b, v.signed)


def hexdigits(v):
    out = []
    for d in range(v.w // 4 - 1, -1, -1):
        a, b = (v.a >> (4 * d)) & 15, (v.b >> (4 * d)) & 15
        if b == 0:
            out.append("%x" % a)
        elif b == 15 and a == 15:
            out.append("x")
        elif b == 15 and a == 0:
            out.append("z")
        elif a & b:
            out.append("X")
        else:
            out.append("Z")
    return "".join(out)


def decimal(v):
    """%0d (SYSTEM_TASKS.md 3.4)."""
    m = (1 << v.w) - 1
    if v.b:
        x = v.a & v.b
        if x == m:
            return "x"
        if v.b == m and x == 0:
            return "z"
        return "X" if x else "Z"
    if v.signed and (v.a >> (v.w - 1)) & 1:
        return str(v.a - (1 << v.w))
    return str(v.a)


def less_than_zero(v):
    """v < 0, with 0 an int: signed only if v is signed (11.8.1)."""
    if v.b:
        return "x"
    if not v.signed:
        return "0"
    w = max(v.w, 32)
    e = extend(v, w)
    return "1" if (e.a >> (w - 1)) & 1 else "0"


def ashr(v, w, n):
    """v extended to w, then >>> n: the sign bit fills when signed."""
    e = extend(v, w)
    a, b = e.a >> n, e.b >> n
    if e.signed:
        fill = ((1 << w) - 1) ^ ((1 << (w - n)) - 1)
        if (e.a >> (w - 1)) & 1:
            a |= fill
        if (e.b >> (w - 1)) & 1:
            b |= fill
    return V(w, a, b)


def wider(w):
    return (w + 8 + 3) // 4 * 4


def tname(w, four, signed):
    return f"{'logic' if four else 'bit'}{' signed' if signed else ''} [{w - 1}:0]"


def files():
    decls, module, task, exp, task_exp = [], [], [], [], []
    for w in WIDTHS:
        rw = wider(w)
        for four, signed in TYPES:
            key = f"w{w}_{'4' if four else '2'}{'s' if signed else 'u'}"
            decls.append(f"  {tname(w, four, signed)} v_{key};")
            decls.append(f"  logic [{rw - 1}:0] r_{key};")
            task.append(f"    begin\n      {tname(w, four, signed)} v;\n"
                        f"      logic [{rw - 1}:0] r;")
            for top in ["1", "0", "x", "z"] if four else ["1", "0"]:
                v = operand(w, top)
                lit = (f"{w}'b" + "".join(
                    "x" if (v.b >> i) & 1 and (v.a >> i) & 1 else
                    "z" if (v.b >> i) & 1 else str((v.a >> i) & 1)
                    for i in range(w - 1, -1, -1)))
                for place in PLACES:
                    var, res = (f"v_{key}", f"r_{key}") if place == "module" else ("v", "r")
                    ind = "    " if place == "module" else "      "
                    lines = module if place == "module" else task
                    lines.append(f"{ind}{var} = {lit};")
                    for sname, text, rsigned in SPELLINGS:
                        e = text.replace("E", var)
                        r = V(w, v.a, v.b, rsigned)
                        for use in USES:
                            tag = f"{place} {key} top{top} {sname} {use}"
                            if use == "assign":
                                lines.append(f"{ind}{res} = {e};")
                                lines.append(f'{ind}$display("{tag} %h", {res});')
                                want = hexdigits(extend(r, rw))
                            elif use == "decimal":
                                lines.append(f'{ind}$display("{tag} %0d", {e});')
                                want = decimal(r)
                            elif use == "less":
                                lines.append(f'{ind}$display("{tag} %b", {e} < 0);')
                                want = less_than_zero(r)
                            else:
                                lines.append(f"{ind}{res} = {e} >>> 3;")
                                lines.append(f'{ind}$display("{tag} %h", {res});')
                                want = hexdigits(ashr(r, rw, 3))
                            (exp if place == "module" else task_exp).append(f"{tag} {want}")
            task.append("    end")
    head = [
        "/*",
        f":name: {NAME}",
        ":description: $signed, $unsigned, signed'() and unsigned'() on values of 8, 64, 65 "
        "and 100 bits, two- and four-state, declared signed and unsigned, with the top bit 1, "
        "0, x and z: assigned to a wider variable, printed with %0d, compared with 0 and "
        "shifted by >>>, in a module variable and an automatic variable",
        ":tags: values sign",
        ":type: simulation",
        ":lrm: 1800-2023 6.24.1 11.7 11.8.1 11.4.10",
        ":oracle: spec",
        ":spec_check: ai",
        ":fourstate: yes",
        ":nondeterministic: no",
        f":generator: gen_signed_cast_matrix.py@{VERSION}",
        ":top_module: top",
        f":expected_stdout: @{NAME}.expected",
        "*/",
        "// Generated by tools/gen_signed_cast_matrix.py; change the generator, not this file.",
        "module top;",
    ]
    sv = head + decls + ["  task automatic run();"] + task + ["  endtask"]
    sv += ["  initial begin"] + module + ["    run();", "  end", "endmodule", ""]
    return {f"{NAME}.sv": "\n".join(sv), f"{NAME}.expected": "\n".join(exp + task_exp) + "\n"}


def main():
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("--check", action="store_true")
    args = ap.parse_args()
    bad = 0
    for name, text in files().items():
        path = OUT / name
        if args.check:
            if not path.exists() or path.read_text(encoding="utf-8") != text:
                print(f"gen_signed_cast_matrix: {path.relative_to(OUT.parent.parent.parent)} is "
                      "out of date; run tools/gen_signed_cast_matrix.py", file=sys.stderr)
                bad += 1
        else:
            path.write_text(text, encoding="utf-8")
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main())
