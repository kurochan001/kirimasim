#!/usr/bin/env python3
"""Generates the four-state matrix test of TEST_SUITE.md ("4 値の行列") for
values of 64 bits or fewer in tests/suite/values/.

  tools/gen_logic_matrix.py           # writes the files
  tools/gen_logic_matrix.py --check   # exit 1 if a file differs from what it would write

The rules are VALUE_SEMANTICS.md 3 and 4: the operators with x and z
(3.3 to 3.9), the truth of an if condition (4.1), the match of case, casez
and casex with x, z and ? on either side (4.2) and the edges of 4.3. The
axes: every pair of 0, 1, x and z for the one-bit operators; for vectors,
the width (4, 7, 33, 64: one word, partly and fully used), two- and
four-state and the signedness, with values that hold x and z, under the
bitwise, reduction, equality, wildcard equality, relational, arithmetic
and logical operators, shifts by a known and an unknown amount, the
conditional operator with an unknown condition, and a bit-select at an
unknown position (held in a variable: a constant one is an
error); the condition of if; the three kinds of case against
items with x, z and ?; and every change of a one-bit signal, and changes
of the upper bit of a vector, against posedge, negedge, edge and a plain
event control.

The expected outputs come from the four-state model below (the one of
gen_wide_matrix.py, with the rules this test adds). A person checks them
once for all the tests made from them (TEST_SUITE.md, "人の抜き取り").
"""

from __future__ import annotations

import argparse
import itertools
import random
import sys
from pathlib import Path

from gen_wide_matrix import (V, allx, arith, bitnot, bitwise, equality, land, lnot, lor,
                             reduce, relational, resize, shift, truth, wildcard)

OUT = Path(__file__).resolve().parent.parent / "tests" / "suite" / "values"
NAME = "logic_matrix"
# The version of the rules below (TEST_SUITE.md, :generator:). Raise it
# whenever they change.
VERSION = 1
BITS = "01xz"


def v(s, signed=False):
    """A value from its bits, most significant first."""
    a = b = 0
    for c in s:
        a, b = (a << 1) | (c in "1x"), (b << 1) | (c in "xz")
    return V(len(s), a, b, signed)


def bits(x):
    return "".join("x" if (x.b >> i) & 1 and (x.a >> i) & 1 else "z" if (x.b >> i) & 1
                   else str((x.a >> i) & 1) for i in range(x.w - 1, -1, -1))


def lit(x):
    return f"{x.w}'{'s' if x.signed else ''}b{bits(x)}"


ONE, ZERO, X1 = V(1, 1), V(1, 0), V(1, 1, 1)


def select(cond, x, y):
    """?: (3.9): an unknown condition merges the two sides bit by bit."""
    t = truth(cond)
    if not t.b:
        return x if t.a else y
    if equality("==", x, y).a == 1 and not equality("==", x, y).b:
        return x
    m = x.mask
    same = ~(x.a ^ y.a) & ~x.b & ~y.b & m
    return V(x.w, (x.a & same) | (~same & m), ~same & m, x.signed)


def case_match(kind, e, item):
    """case, casez, casex (4.2): which bits are left out, then ===."""
    if kind == "case":
        dont = 0
    elif kind == "casez":
        dont = (e.b & ~e.a) | (item.b & ~item.a)
    else:
        dont = e.b | item.b
    keep = ~dont & e.mask
    return (e.a & keep) == (item.a & keep) and (e.b & keep) == (item.b & keep)


def posedge(a, b):
    return (a == "0" and b in "1xz") or (a in "xz" and b == "1")


def negedge(a, b):
    return (a == "1" and b in "0xz") or (a in "xz" and b == "0")


ONE_BIT = [  # (tag, operator text, function of two one-bit values)
    ("and", "&", lambda a, b: bitwise("&", a, b, 1, False)),
    ("or", "|", lambda a, b: bitwise("|", a, b, 1, False)),
    ("xor", "^", lambda a, b: bitwise("^", a, b, 1, False)),
    ("xnor", "~^", lambda a, b: bitwise("~^", a, b, 1, False)),
    ("eq", "==", lambda a, b: equality("==", a, b)),
    ("ne", "!=", lambda a, b: equality("!=", a, b)),
    ("ceq", "===", lambda a, b: equality("===", a, b)),
    ("cne", "!==", lambda a, b: equality("!==", a, b)),
    ("weq", "==?", lambda a, b: wildcard("==?", a, b)),
    ("wne", "!=?", lambda a, b: wildcard("!=?", a, b)),
    ("land", "&&", land),
    ("lor", "||", lor),
    ("limp", "->", lambda a, b: lor(lnot(a), b)),
    ("leqv", "<->", lambda a, b: land(lor(lnot(a), b), lor(lnot(b), a))),
    ("lt", "<", lambda a, b: relational("<", a, b)),
    ("ge", ">=", lambda a, b: relational(">=", a, b)),
    ("add", "+", lambda a, b: arith("+", a, b, 1, False)),
]
ONE_BIT_UNARY = [
    ("not", "~", bitnot),
    ("lnot", "!", lnot),
    ("neg", "-", lambda a: arith("-", V(1, 0), a, 1, False)),
]


def one_bit():
    sv, exp = [], []
    for tag, op, fn in ONE_BIT:
        args, out = [], ""
        for a, b in itertools.product(BITS, BITS):
            args.append(f"1'b{a} {op} 1'b{b}")
            out += bits(fn(v(a), v(b)))
        sv.append(f"    $display(\"bit {tag} {'%b' * 16}\", {', '.join(args)});")
        exp.append(f"bit {tag} {out}")
    for tag, op, fn in ONE_BIT_UNARY:
        args = [f"{op}1'b{a}" for a in BITS]
        sv.append(f"    $display(\"bit {tag} {'%b' * 4}\", {', '.join(args)});")
        exp.append(f"bit {tag} {''.join(bits(fn(v(a))) for a in BITS)}")
    return sv, exp


def random_bits(rng, w, four):
    if not four:
        return "".join(rng.choice("01") for _ in range(w))
    # Mostly known, with some x and z, and a run of each.
    s = [rng.choice("0101x z"[:7].replace(" ", "")) for _ in range(w)]
    return "".join(s)


def vectors(rng):
    sv, exp, decl = [], [], []
    k = 0
    for w in (4, 7, 33, 64):
        for four, signed in ((True, False), (True, True), (False, False)):
            k += 1
            t = f"{'logic' if four else 'bit'}{' signed' if signed else ''} [{w - 1}:0]"
            a = v(random_bits(rng, w, four), signed)
            b = v(random_bits(rng, w, four), signed)
            if four:
                a = v("x" + bits(a)[1:-2] + "z1", signed) if w > 3 else a
            c = v("1" + "0" * (w - 1), signed)  # a known value
            n = f"v{k}_"
            decl.append(f"  {t} {n}a, {n}b, {n}c, {n}r;")
            tag = f"{'4' if four else '2'}{'s' if signed else 'u'}{w}"
            sv += [f"    {n}a = {lit(a)};", f"    {n}b = {lit(b)};", f"    {n}c = {lit(c)};"]
            amt = min(5, w - 1)
            cases = [
                ("and", f"{n}a & {n}b", bitwise("&", a, b, w, signed), w),
                ("or", f"{n}a | {n}b", bitwise("|", a, b, w, signed), w),
                ("xor", f"{n}a ^ {n}b", bitwise("^", a, b, w, signed), w),
                ("xnor", f"{n}a ~^ {n}c", bitwise("~^", a, c, w, signed), w),
                ("not", f"~{n}a", bitnot(a), w),
                ("add", f"{n}a + {n}c", arith("+", a, c, w, signed), w),
                ("shl", f"{n}a << {amt}", shift("<<", a, amt), w),
                ("shr", f"{n}a >> {amt}", shift(">>", a, amt), w),
                ("ashr", f"{n}a >>> {amt}", shift(">>>", a, amt), w),
                ("shlx", f"{n}c << 2'bx1", allx(w, signed), w),
                ("selx", f"1'bx ? {n}a : {n}b", select(X1, a, b), w),
                ("selsame", f"1'bz ? {n}a : {n}a", select(X1, a, a), w),
            ]
            for tg, e, val, ww in cases:
                sv.append(f"    {n}r = {e}; $display(\"{tag} {tg} %b\", {n}r);")
                val = val if four else V(val.w, val.a & ~val.b, 0)
                exp.append(f"{tag} {tg} {bits(V(ww, val.a, val.b))}")
            one = [
                ("rand", f"&{n}a", reduce("&", a)), ("rnand", f"~&{n}a", lnot_bit(reduce("&", a))),
                ("ror", f"|{n}a", reduce("|", a)), ("rnor", f"~|{n}a", lnot_bit(reduce("|", a))),
                ("rxor", f"^{n}a", reduce("^", a)), ("rxnor", f"~^{n}a", lnot_bit(reduce("^", a))),
                ("eq", f"{n}a == {n}b", equality("==", a, b)),
                ("eqself", f"{n}a == {n}a", equality("==", a, a)),
                ("ceq", f"{n}a === {n}a", equality("===", a, a)),
                ("weq", f"{n}c ==? {n}a", wildcard("==?", c, a)),
                ("lt", f"{n}c < {n}a", relational("<", c, a)),
                ("ltknown", f"{n}c < {n}c", relational("<", c, c)),
                ("land", f"{n}a && {n}c", land(a, c)),
                ("lor", f"{n}a || 1'b0", lor(a, ZERO)),
                ("lnot", f"!{n}a", lnot(a)),
                ("bitx", f"{n}a[ix]", X1 if four else ZERO),
            ]
            for tg, e, val in one:
                sv.append(f"    $display(\"{tag} {tg} %b\", {e});")
                val = val if four else V(1, val.a & ~val.b, 0)
                exp.append(f"{tag} {tg} {bits(val)}")
    return decl, sv, exp


def lnot_bit(x):
    """~ of a one-bit result."""
    return bitnot(x)


def conditions():
    sv, exp = [], []
    values = ["1'b0", "1'b1", "1'bx", "1'bz", "2'b1x", "2'b0x", "4'bzz10", "4'bzz00"]
    args = []
    out = ""
    for s in values:
        sv.append(f"    if ({s}) $write(\"t\"); else $write(\"e\");")
        out += "t" if truth(v(s.split("'b")[1])).a == 1 and not truth(v(s.split("'b")[1])).b else "e"
    sv.append("    $display(\"\");")
    exp.append(out)
    return sv, exp


def cases():
    sv, exp = [], []
    exprs = ["10x0", "10z0", "1010", "1x1z"]
    items = ["10x0", "10z0", "1010", "10?0", "1xx0", "zzzz", "1x1z"]
    for kind in ("case", "casez", "casex"):
        out = ""
        for e in exprs:
            for it in items:
                sv.append(f"    {kind} (4'b{e}) 4'b{it}: $write(\"1\"); default: $write(\"0\"); endcase")
                out += "1" if case_match(kind, v(e), v(it.replace("?", "z"))) else "0"
            sv.append("    $write(\" \");")
            out += " "
        sv.append(f"    $display(\"{kind}\");")
        exp.append(out + kind)
    return sv, exp


def edges():
    """Every change of a one-bit signal, then the upper bit of a vector."""
    sv = ["  logic s;", "  logic [1:0] u;", "  int pe, ne, ee, ch, upe;",
          "  always @(posedge s) pe = pe + 1;", "  always @(negedge s) ne = ne + 1;",
          "  always @(edge s) ee = ee + 1;", "  always @(s) ch = ch + 1;",
          "  always @(posedge u) upe = upe + 1;", "  initial begin", "    #100;"]
    exp_line = ""
    for a, b in itertools.product(BITS, BITS):
        sv += [f"    s = 1'b{a};", "    #1 pe = 0; ne = 0; ee = 0; ch = 0;", f"    s = 1'b{b};",
               "    #1 $write(\"%0d%0d%0d%0d \", pe, ne, ee, ch);"]
        p, n = posedge(a, b), negedge(a, b)
        exp_line += f"{int(p)}{int(n)}{int(p or n)}{int(a != b)} "
    sv.append("    $display(\"edges\");")
    up = ""
    for a, b in (("00", "10"), ("10", "11"), ("01", "x1"), ("x0", "01")):
        sv += [f"    u = 2'b{a};", "    #1 upe = 0;", f"    u = 2'b{b};",
               "    #1 $write(\"%0d\", upe);"]
        up += str(int(posedge(a[-1], b[-1])))
    sv += ["    $display(\" lsb\");", "  end"]
    return sv, [exp_line + "edges", up + " lsb"]


def files():
    rng = random.Random(VERSION * 104729)
    s1, e1 = one_bit()
    decl, s2, e2 = vectors(rng)
    s3, e3 = conditions()
    s4, e4 = cases()
    s5, e5 = edges()
    head = [
        "/*",
        f":name: {NAME}",
        ":description: four-state values of 64 bits or fewer: the one-bit operators on every "
        "pair of 0, 1, x and z; vector operators with x and z at 4, 7, 33 and 64 bits; if "
        "conditions; case, casez and casex with x, z and ?; and the edges of every change",
        ":tags: values fourstate",
        ":type: simulation",
        ":lrm: 1800-2023 9.4.2 11.4 12.4 12.5 12.5.1",
        ":oracle: spec",
        ":spec_check: ai",
        ":fourstate: yes",
        ":nondeterministic: no",
        f":generator: gen_logic_matrix.py@{VERSION}",
        f":expected_stdout: @{NAME}.expected",
        "*/",
        "// Generated by tools/gen_logic_matrix.py; change the generator, not this file.",
        "`timescale 1ns/1ns",
        "module top;",
    ]
    sv = (head + ["  logic [1:0] ix;"] + decl + s5 + ["  initial begin", "    ix = 2'bx1;"] + s1 + s2
          + s3 + s4 + ["  end", "endmodule", ""])
    # The operators all print at time 0, the edges from time 100 on.
    return {f"{NAME}.sv": "\n".join(sv), f"{NAME}.expected": "\n".join(e1 + e2 + e3 + e4 + e5) + "\n"}


def main():
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("--check", action="store_true")
    args = ap.parse_args()
    bad = 0
    for name, text in files().items():
        path = OUT / name
        if args.check:
            if not path.exists() or path.read_text(encoding="utf-8") != text:
                print(f"gen_logic_matrix: {path.relative_to(OUT.parent.parent.parent)} is out of "
                      "date; run tools/gen_logic_matrix.py", file=sys.stderr)
                bad += 1
        else:
            path.write_text(text, encoding="utf-8")
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main())
