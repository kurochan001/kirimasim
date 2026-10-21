#!/usr/bin/env python3
# Copyright (c) 2026 The kirimasim Authors
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

"""Generates the matrix test of the conditional operator whose operands
have side effects, of TEST_SUITE.md ("Mechanism matrix"), in
tests/suite/values/.

  tools/gen_cond_op_matrix.py           # writes the files
  tools/gen_cond_op_matrix.py --check   # exit 1 if a file differs from what it would write

When either operand of ?: can change something, only the side the
condition picks is evaluated, and both when it is x or z (11.4.11,
VALUE_SEMANTICS.md 3.9); front branches to each side, and the sides pass
their value to the block after them as a block argument (SIR_SPEC.md
5.1). The value passes whole whatever its width.

The axes: the width (64, 65, 100, 128, 200: one word, one bit past it, a
partial second word, two full words, four words), two- and four-state,
signed and unsigned; the upper bits of the first operand (known, x or z;
four-state only); the operand with the side effect (the first, the
second, both); the side effect (a function call, an assignment
expression); the condition (0, 1, x, z); the container (an initial
procedure, an automatic task, a function that returns the result, a
branch of a fork, a continuous assignment). The initial procedure takes
every combination; the other containers take the function call and
known upper bits. A two-state operand pair under an x or z condition is
one value twice, so that its result is that value (3.9: equal by ==).

The expected outputs follow from VALUE_SEMANTICS.md 3.9 (Table 11-20 of
11.4.11) and the %h rules of SYSTEM_TASKS.md 3.2 and 3.4, computed below.
For reference (checked 2026-10-18): Verilator 5.052 prints every line
whose value is known under conditions 0 and 1 as expected, once the
procedures assign no z to the condition (with a z assigned, it takes the
condition as 0 throughout). Icarus Verilog 14.0 (devel) prints every line
as expected, the assignment expressions turned into calls, except the
one-bit top digit of a 65-bit value with x or z there, which it prints X
or Z from a function's result although x or z from the same value in a
variable; the comment in the test says so.
"""

from __future__ import annotations

import argparse
import sys
from pathlib import Path

OUT = Path(__file__).resolve().parent.parent / "tests" / "suite" / "values"
NAME = "cond_op_matrix"
# The version of the rules below (TEST_SUITE.md, :generator:). Raise it
# whenever they change.
VERSION = 1
WIDTHS = [64, 65, 100, 128, 200]
TYPES = [(four, signed) for four in (True, False) for signed in (False, True)]
UPPERS = ["known", "x", "z"]
SIDES = ["first", "second", "both"]
EFFECTS = ["call", "assign"]
CONDS = ["0", "1", "x", "z"]
CONTAINERS = ["task", "function", "fork", "assign"]

PATTERN_L = int("a5c3e1f00f1e3c5a" "0123456789abcdef" "1122334455667788" "99aabbccddeeff00", 16)
PATTERN_R = int("5a3c1e0ff0e1c3a5" "fedcba9876543210" "8877665544332211" "00ffeeddccbbaa99", 16)


class V:
    """A four-state value: a and b planes (b=1: x when a=1, z when a=0)."""

    def __init__(self, w, a, b=0):
        m = (1 << w) - 1
        self.w, self.a, self.b = w, a & m, b & m


def first_value(w, upper):
    """The first operand: every word differs, the top bit is 1; for x and
    z, the eight top bits and bits 67:64 of a value wider than 72 bits."""
    a = (PATTERN_L & ((1 << w) - 1)) | (1 << (w - 1))
    unknown = ((1 << 8) - 1) << max(w - 8, 0)
    if w > 72:
        unknown |= 0xF << 64
    if upper == "x":
        return V(w, a | unknown, unknown)
    if upper == "z":
        return V(w, a & ~unknown, unknown)
    return V(w, a)


def second_value(w):
    return V(w, PATTERN_R)


def merge(l, r):
    """?: with an x or z condition (VALUE_SEMANTICS.md 3.9)."""
    if l.b == 0 and r.b == 0 and l.a == r.a:
        return l
    same = ~(l.a ^ r.a) & ~l.b & ~r.b & ((1 << l.w) - 1)
    unknown = ~same & ((1 << l.w) - 1)
    return V(l.w, (l.a & same) | unknown, unknown)


def result(cond, l, r):
    if cond == "1":
        return l
    if cond == "0":
        return r
    return merge(l, r)


def hexdigits(v):
    """%h with the automatic width (SYSTEM_TASKS.md 3.2)."""
    out = []
    for d in range((v.w + 3) // 4 - 1, -1, -1):
        n = min(4, v.w - 4 * d)
        m = (1 << n) - 1
        a, b = (v.a >> (4 * d)) & m, (v.b >> (4 * d)) & m
        if b == 0:
            out.append("%x" % a)
        elif b == m and a == m:
            out.append("x")
        elif b == m and a == 0:
            out.append("z")
        elif a & b:
            out.append("X")
        else:
            out.append("Z")
    return "".join(out)


def literal(v):
    """A sized literal of v's bits."""
    if v.b == 0:
        return f"{v.w}'h{v.a:x}"
    bits = "".join("x" if (v.b >> i) & 1 and (v.a >> i) & 1 else
                   "z" if (v.b >> i) & 1 else str((v.a >> i) & 1)
                   for i in range(v.w - 1, -1, -1))
    return f"{v.w}'b{bits}"


def tname(w, four, signed):
    return f"t{w}_{'4' if four else '2'}{'s' if signed else 'u'}"


def operands(w, four, upper, cond):
    l = first_value(w, upper)
    r = second_value(w)
    if not four and cond in ("x", "z"):
        r = l
    return l, r


def arm(t, side, which, effect, v):
    """One operand of ?: as source text: with the side effect when `side`
    names it."""
    if side == which or side == "both":
        if effect == "call":
            return f"f_{t}({t}'({literal(v)}))"
        return f"(v{1 if which == 'first' else 2}_{t} = {t}'({literal(v)}))"
    return f"{t}'({literal(v)})"


def cond_literal(c):
    return f"1'b{c}"


def fcall(t, side, which, name):
    """An operand inside g_<t>_<side>: the argument `name`, through a call
    when `side` names it."""
    if side == which or side == "both":
        return f"f_{t}({name})"
    return name


def files():
    decls = []
    for w in WIDTHS:
        for four, signed in TYPES:
            t = tname(w, four, signed)
            decls += [f"  typedef {'logic' if four else 'bit'}{' signed' if signed else ''} "
                      f"[{w - 1}:0] {t};",
                      f"  {t} v1_{t}, v2_{t};",
                      f"  function automatic {t} f_{t}({t} x);",
                      "    return x;",
                      "  endfunction"]
            for side in SIDES:
                decls += [f"  function automatic {t} g_{t}_{side}(logic c, {t} l, {t} r);",
                          f"    return c ? {fcall(t, side, 'first', 'l')} : "
                          f"{fcall(t, side, 'second', 'r')};",
                          "  endfunction"]

    # The initial procedure: every combination.
    initial, exp = [], []
    for w in WIDTHS:
        for four, signed in TYPES:
            t = tname(w, four, signed)
            for upper in UPPERS if four else ["known"]:
                for effect in EFFECTS:
                    for side in SIDES:
                        for c in CONDS:
                            l, r = operands(w, four, upper, c)
                            e = (f"c ? {arm(t, side, 'first', effect, l)} : "
                                 f"{arm(t, side, 'second', effect, r)}")
                            what = f"initial {t} {upper} {effect} {side} {c}"
                            initial += [f"    c = {cond_literal(c)};",
                                        f'    $display("{what} %h", {e});']
                            exp.append(f"{what} {hexdigits(result(c, l, r))}")

    # The other containers: the function call, known upper bits.
    cases = []
    for w in WIDTHS:
        for four, signed in TYPES:
            t = tname(w, four, signed)
            for side in SIDES:
                for c in CONDS:
                    l, r = operands(w, four, "known", c)
                    cases.append((t, side, c, l, r))
    body = {container: [] for container in CONTAINERS}
    assigns = []
    for container in CONTAINERS:
        for k, (t, side, c, l, r) in enumerate(cases):
            e = (f"{arm(t, side, 'first', 'call', l)} : "
                 f"{arm(t, side, 'second', 'call', r)}")
            what = f"{container} {t} {side} {c}"
            if container == "task":
                body[container] += [f"    cc = {cond_literal(c)};",
                                    f'    $display("{what} %h", cc ? {e});']
            elif container == "function":
                body[container].append(
                    f'    $display("{what} %h", g_{t}_{side}({cond_literal(c)}, '
                    f"{t}'({literal(l)}), {t}'({literal(r)})));")
            elif container == "fork":
                body[container] += [f"        cc = {cond_literal(c)};",
                                    f'        $display("{what} %h", cc ? {e});']
            else:
                assigns += [f"  {t} a{k};", f"  assign a{k} = c{c} ? {e};"]
                body[container].append(f'    $display("{what} %h", a{k});')
            exp.append(f"{what} {hexdigits(result(c, l, r))}")

    head = [
        "/*",
        f":name: {NAME}",
        ":description: the conditional operator with side effects in its operands passes "
        "its whole value at 64, 65, 100, 128 and 200 bits, two- and four-state, signed and "
        "unsigned, with x and z in the upper bits, under conditions 0, 1, x and z, in an "
        "initial procedure, an automatic task, a function, a fork branch and a continuous "
        "assignment",
        ":tags: values conditional wide",
        ":type: simulation",
        ":lrm: 1800-2023 11.4.11",
        ":oracle: spec",
        ":spec_check: ai",
        ":fourstate: yes",
        ":nondeterministic: no",
        f":generator: gen_cond_op_matrix.py@{VERSION}",
        ":issue: 2",
        ":top_module: top",
        f":expected_stdout: @{NAME}.expected",
        "*/",
        "// Generated by tools/gen_cond_op_matrix.py; change the generator, not this file.",
        "// Icarus Verilog 14.0 (devel) prints X or Z for the one-bit top digit of",
        "// a 65-bit function result that is x or z, but x or z for the same value",
        "// in a variable; the expected values follow SYSTEM_TASKS.md 3.4.",
        "`timescale 1ns/1ns",
        "module top;",
        "  logic c;",
        "  logic c0 = 1'b0, c1 = 1'b1, cx = 1'bx, cz = 1'bz;",
    ]
    sv = head + decls + assigns
    sv += ["  task automatic run_task();", "    logic cc;"] + body["task"] + ["  endtask"]
    sv += ["  initial begin"] + initial + ["    run_task();"] + body["function"]
    sv += ["    fork", "      begin", "        logic cc;"] + body["fork"] + ["      end", "    join"]
    sv += ["    #1;"] + body["assign"] + ["  end", "endmodule", ""]
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
                print(f"gen_cond_op_matrix: {path.relative_to(OUT.parent.parent.parent)} is out of "
                      "date; run tools/gen_cond_op_matrix.py", file=sys.stderr)
                bad += 1
        else:
            path.write_text(text, encoding="utf-8")
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main())
