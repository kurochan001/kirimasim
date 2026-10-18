#!/usr/bin/env python3
"""Generates the matrix test of integral values wider than 64 bits of
TEST_SUITE.md ("Mechanism matrix") in tests/suite/values/.

  tools/gen_wide_matrix.py           # writes the files
  tools/gen_wide_matrix.py --check   # exit 1 if a file differs from what it would write

A value of any width follows the same rules as one of 64 bits or fewer
(SIR_SPEC.md 3.2, VALUE_SEMANTICS.md 1.3); the VM keeps it in 64-bit
chunks. The axes: the width (65, 100, 128, 200: one bit past a word, a
partial second word, two full words, four words), two- and four-state,
signed and unsigned; the operators (arithmetic, with an unknown operand
and a zero divisor; bitwise with x and z; shifts by constants and by a
variable, past 64; comparisons, equality, case equality and wildcard
equality; reductions and their negations; constant and variable part- and
bit-selects; concatenation, replication;
widening, narrowing and two-state conversion; the logical operators and
the truth of an if condition; conversion to a real); the container (module
variables, kept in slots, and automatic variables of a task, kept in its
frame). A second module writes and reads a value through every place it
can live: an unpacked array element, a packed structure member, a
function's argument and result, a task's output, a ref to a whole
array (its elements and a part at a run-time position), a nonblocking
assignment to the whole and to a part, a continuous assignment, force and
release, an input and an output port, and a part and a bit at a run-time
position.

The expected outputs follow from VALUE_SEMANTICS.md (2, 3, 7.4) and the %h and
%d rules of SYSTEM_TASKS.md 3.2, computed below by a four-state model. A
person checks them once for all the tests made from them (TEST_SUITE.md,
"Sampling by people").
"""

from __future__ import annotations

import argparse
import random
import struct
import sys
from pathlib import Path

OUT = Path(__file__).resolve().parent.parent / "tests" / "suite" / "values"
NAME = "wide_matrix"
# The version of the rules below (TEST_SUITE.md, :generator:). Raise it
# whenever they change.
VERSION = 4
WIDTHS = [65, 100, 128, 200]


# --- A four-state value: a and b planes, like kernel W4 (b=1: x when a=1,
# z when a=0) -------------------------------------------------------------

class V:
    def __init__(self, w, a, b=0, signed=False):
        m = (1 << w) - 1
        self.w, self.a, self.b, self.signed = w, a & m, b & m, signed

    @property
    def mask(self):
        return (1 << self.w) - 1

    def known(self):
        return self.b == 0

    def to_int(self):
        if self.signed and (self.a >> (self.w - 1)) & 1:
            return self.a - (1 << self.w)
        return self.a


def allx(w, signed=False):
    return V(w, (1 << w) - 1, (1 << w) - 1, signed)


def resize(v, w, signed=None):
    """Extends by v's own signedness (its sign bit, x or z included) or truncates."""
    signed = v.signed if signed is None else signed
    a, b = v.a, v.b
    if w > v.w and v.signed:
        fill = ((1 << w) - 1) ^ v.mask
        if (a >> (v.w - 1)) & 1:
            a |= fill
        if (b >> (v.w - 1)) & 1:
            b |= fill
    return V(w, a, b, signed)


def to2(v):
    return V(v.w, v.a & ~v.b, 0, v.signed)


def arith(op, x, y, w, signed):
    x, y = resize(x, w), resize(y, w)
    if not (x.known() and y.known()):
        return allx(w, signed)
    p, q = (x.to_int(), y.to_int()) if signed else (x.a, y.a)
    if op in "/%" and q == 0:
        return allx(w, signed)
    if op == "+":
        r = p + q
    elif op == "-":
        r = p - q
    elif op == "*":
        r = p * q
    elif op == "/":
        r = abs(p) // abs(q) * (1 if (p < 0) == (q < 0) else -1)
    else:
        r = abs(p) % abs(q) * (1 if p >= 0 else -1)
    return V(w, r, 0, signed)


def bitwise(op, x, y, w, signed):
    x, y = resize(x, w), resize(y, w)
    m = (1 << w) - 1
    k0x, k1x = ~x.a & ~x.b & m, x.a & ~x.b & m
    k0y, k1y = ~y.a & ~y.b & m, y.a & ~y.b & m
    if op == "&":
        k0, k1 = k0x | k0y, k1x & k1y
    elif op == "|":
        k0, k1 = k0x & k0y, k1x | k1y
    else:
        both = (k0x | k1x) & (k0y | k1y)
        val = (x.a ^ y.a) if op == "^" else ~(x.a ^ y.a)
        k1 = val & both & m
        k0 = ~val & both & m
    unk = m & ~(k0 | k1)
    return V(w, k1 | unk, unk, signed)


def bitnot(x):
    m = x.mask
    unk = x.b
    return V(x.w, (~x.a & ~x.b & m) | unk, unk, x.signed)


def shift(op, x, n):
    """x shifted by the known amount n, in x's own width."""
    w, m = x.w, x.mask
    if op == "<<":
        return V(w, (x.a << n) & m, (x.b << n) & m, x.signed)
    if op == ">>>" and x.signed:
        r = resize(x, w + n)
        return V(w, r.a >> n, r.b >> n, True)
    return V(w, x.a >> n, x.b >> n, x.signed)


ONE, ZERO, X1 = V(1, 1), V(1, 0), V(1, 1, 1)


def relational(op, x, y):
    w = max(x.w, y.w)
    signed = x.signed and y.signed
    x, y = resize(x, w, signed), resize(y, w, signed)
    if not (x.known() and y.known()):
        return X1
    p, q = x.to_int(), y.to_int()
    r = {"<": p < q, "<=": p <= q, ">": p > q, ">=": p >= q}[op]
    return ONE if r else ZERO


def equality(op, x, y):
    w = max(x.w, y.w)
    signed = x.signed and y.signed
    x, y = resize(x, w, signed), resize(y, w, signed)
    if op in ("===", "!=="):
        same = x.a == y.a and x.b == y.b
        return ONE if same == (op == "===") else ZERO
    both = ~x.b & ~y.b & ((1 << w) - 1)
    if (x.a ^ y.a) & both:
        r = False
    elif x.b or y.b:
        return X1
    else:
        r = True
    return ONE if r == (op == "==") else ZERO


def reduce(op, x):
    m = x.mask
    k0, k1 = ~x.a & ~x.b & m, x.a & ~x.b & m
    if op == "&":
        return ZERO if k0 else (ONE if k1 == m else X1)
    if op == "|":
        return ONE if k1 else (ZERO if k0 == m else X1)
    if x.b:
        return X1
    return ONE if bin(x.a).count("1") % 2 else ZERO


def wildcard(op, x, y):
    """==? and !=?: the x and z bits of the right side match anything
    (VALUE_SEMANTICS.md 3.4)."""
    w = max(x.w, y.w)
    signed = x.signed and y.signed
    x, y = resize(x, w, signed), resize(y, w, signed)
    care = ~y.b & ((1 << w) - 1)
    r = equality("==", V(w, x.a & care, x.b & care), V(w, y.a & care, y.b & care))
    return r if op == "==?" or r.b else V(1, 1 - r.a)


def truth(x):
    """True with a 1 bit, false with all 0, else unknown (VALUE_SEMANTICS.md 3.5)."""
    m = x.mask
    if x.a & ~x.b & m:
        return ONE
    return ZERO if (x.a | x.b) & m == 0 else X1


def lnot(x):
    t = truth(x)
    return X1 if t.b else V(1, 1 - t.a)


def land(x, y):
    p, q = truth(x), truth(y)
    if (not p.b and p.a == 0) or (not q.b and q.a == 0):
        return ZERO
    return X1 if p.b or q.b else ONE


def lor(x, y):
    p, q = truth(x), truth(y)
    if (not p.b and p.a) or (not q.b and q.a):
        return ONE
    return X1 if p.b or q.b else ZERO


def leqv_imp(x, y):
    """x -> y, which is !x || y (11.4.7)."""
    return lor(lnot(x), y)


def leqv(x, y):
    """x <-> y, which is (x -> y) && (y -> x) (11.4.7)."""
    return land(leqv_imp(x, y), leqv_imp(y, x))


def taken(x):
    """Which branch an if takes: the else branch unless x is true (12.4)."""
    t = truth(x)
    return "t" if t.a and not t.b else "f"


def realbits(x):
    """The bits of real'(x): x and z bits count as 0 (VALUE_SEMANTICS.md 7.4)."""
    n = x.a & ~x.b & x.mask
    if x.signed and n >> (x.w - 1):
        n -= 1 << x.w
    return struct.pack(">d", float(n)).hex()


def select(x, lsb, w):
    return V(w, x.a >> lsb, x.b >> lsb)


def concat(*parts):
    a = b = w = 0
    for p in parts:
        a, b, w = (a << p.w) | p.a, (b << p.w) | p.b, w + p.w
    return V(w, a, b)


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


# --- The design ------------------------------------------------------------

def tname(w, four, signed):
    return f"{'logic' if four else 'bit'}{' signed' if signed else ''} [{w - 1}:0]"


def literal(v):
    """A sized literal of v's bits."""
    digits = hexdigits(v).replace("X", "x").replace("Z", "z")
    if "X" in hexdigits(v) or "Z" in hexdigits(v):
        bits = "".join("x" if (v.b >> i) & 1 and (v.a >> i) & 1 else
                       "z" if (v.b >> i) & 1 else str((v.a >> i) & 1)
                       for i in range(v.w - 1, -1, -1))
        return f"{v.w}'{'s' if v.signed else ''}b{bits}"
    return f"{v.w}'{'s' if v.signed else ''}h{digits}"


def operands(rng, w, four, signed):
    a = V(w, rng.getrandbits(w), 0, signed)
    b = V(w, rng.getrandbits(w), 0, signed)
    d = V(w, rng.getrandbits(w // 2) | 1, 0, signed)
    if four:
        unk = rng.getrandbits(w) & rng.getrandbits(w) & rng.getrandbits(w)
        c = V(w, rng.getrandbits(w) | 0, unk, signed)
        # every hex digit kind: one digit all x, one all z
        c = V(w, c.a | 0xF0, c.b | 0xFF0, signed)
    else:
        c = V(w, rng.getrandbits(w), 0, signed)
    return a, b, c, d


def kval(w):
    """The run-time position and shift: past 64 where the width allows."""
    return min(67, w - 31)


def op_cases(w, four, signed, a, b, c, d):
    """(tag, expression over a b c d k, expected value, show width)."""
    zero = V(w, 0, 0, signed)
    cx = bitwise("&", c, bitnot(c), w, signed)  # 0 where c is known, x elsewhere
    kv = kval(w)
    cases = [
        ("add", "a + b", arith("+", a, b, w, signed)),
        ("sub", "a - b", arith("-", a, b, w, signed)),
        ("mul", "a * b", arith("*", a, b, w, signed)),
        ("div", "a / d", arith("/", a, d, w, signed)),
        ("mod", "a % d", arith("%", a, d, w, signed)),
        ("neg", "-a", arith("-", zero, a, w, signed)),
        ("addx", "a + c", arith("+", a, c, w, signed)),
        ("div0", "a / zero", arith("/", a, zero, w, signed)),
        ("and", "a & c", bitwise("&", a, c, w, signed)),
        ("or", "a | c", bitwise("|", a, c, w, signed)),
        ("xor", "a ^ c", bitwise("^", a, c, w, signed)),
        ("xnor", "a ~^ b", bitwise("~^", a, b, w, signed)),
        ("not", "~c", bitnot(c)),
        ("shl5", "a << 5", shift("<<", a, 5)),
        ("shl70", "c << 70", shift("<<", c, 70)),
        ("shr70", "c >> 70", shift(">>", c, 70)),
        ("ashr70", "a >>> 70", shift(">>>", a, 70)),
        ("shlk", "a << k", shift("<<", a, kv)),
        ("shrk", "c >> k", shift(">>", c, kv)),
    ]
    out = [(t, e, v, None) for t, e, v in cases]
    one_bit = [
        ("lt", "a < b", relational("<", a, b)),
        ("le", "a <= a", relational("<=", a, a)),
        ("gt", "a > b", relational(">", a, b)),
        ("ge", "b >= a", relational(">=", b, a)),
        ("ltx", "c < a", relational("<", c, a)),
        ("eq", "a == b", equality("==", a, b)),
        ("ne", "a != b", equality("!=", a, b)),
        ("eqself", "a == a", equality("==", a, a)),
        ("eqx", "c == c", equality("==", c, c)),
        ("ceq", "c === c", equality("===", c, c)),
        ("cne", "c !== a", equality("!==", c, a)),
        ("rand", "&a", reduce("&", a)),
        ("ror", "|c", reduce("|", c)),
        ("rxor", "^a", reduce("^", a)),
        ("randx", "&c", reduce("&", c)),
        ("rxorx", "^c", reduce("^", c)),
        ("bit", "c[k]", select(c, kv, 1)),
        ("lnot", "! c", lnot(c)),
        ("lnotx", "! ( c & ~c )", lnot(cx)),
        ("land", "a && c", land(a, c)),
        ("landx", "a && ( c & ~c )", land(a, cx)),
        ("land0", "( c & ~c ) && zero", land(cx, zero)),
        ("lor", "c || zero", lor(c, zero)),
        ("lorx", "zero || ( c & ~c )", lor(zero, cx)),
        ("limp", "a -> c", leqv_imp(a, c)),
        ("limpx", "( c & ~c ) -> zero", leqv_imp(cx, zero)),
        ("leqv", "a <-> zero", leqv(a, zero)),
        ("leqvx", "( c & ~c ) <-> a", leqv(cx, a)),
        ("rnand", "~&a", bitnot(reduce("&", a))),
        ("rnor", "~|c", bitnot(reduce("|", c))),
        ("rxnor", "~^c", bitnot(reduce("^", c))),
        ("wne", "a !=? c", wildcard("!=?", a, c)),
        ("weqx", "c ==? a", wildcard("==?", c, a)),
    ]
    out += [(t, e, v, 1) for t, e, v in one_bit]
    parts = [
        ("top40", f"c[{w - 1} -: 40]", select(c, w - 40, 40)),
        ("mid20", f"a[{min(70, w - 20)} +: 20]", select(a, min(70, w - 20), 20)),
        ("var30", "c[k +: 30]", select(c, kv, 30)),
        ("cat", f"{{a[39:0], b[{w - 1} -: 30]}}", concat(select(a, 0, 40), select(b, w - 30, 30))),
        ("rep", "{3{c[29:0]}}", concat(*[select(c, 0, 30)] * 3)),
        ("low40", "a[39:0]", select(a, 0, 40)),
    ]
    out += [(t, e, v, v.w) for t, e, v in parts]
    return out


def ops_module(w, four, signed, idx, rng):
    """One case: the operand declarations, the statements and expected lines."""
    a, b, c, d = operands(rng, w, four, signed)
    ty = tname(w, four, signed)
    p = f"w{idx}_"
    decl = [f"  {ty} {p}a, {p}b, {p}c, {p}d, {p}zero, {p}r;",
            f"  {tname(w, False, signed)} {p}r2;",
            f"  logic signed [39:0] {p}s40;"]
    lines = []
    exp = []

    def body(prefix, names, pad):
        st = [f"{pad}{names['a']} = {literal(a)};", f"{pad}{names['b']} = {literal(b)};",
              f"{pad}{names['c']} = {literal(c)};", f"{pad}{names['d']} = {literal(d)};",
              f"{pad}{names['zero']} = '0;", f"{pad}k = 8'd{kval(w)};"]
        ex = []
        for tag, expr, val, show in op_cases(w, four, signed, a, b, c, d):
            e = expr
            for n in ("zero", "a", "b", "c", "d"):
                e = e.replace(n, names[n]) if n == "zero" else \
                    " ".join(names[n] if tok == n else tok for tok in e.split(" "))
            e = (e.replace("&a", "&" + names["a"]).replace("|c", "|" + names["c"])
                  .replace("^a", "^" + names["a"]).replace("&c", "&" + names["c"])
                  .replace("^c", "^" + names["c"]).replace("~c", "~" + names["c"])
                  .replace("-a", "-" + names["a"]))
            for n in ("a", "b", "c"):
                e = e.replace(f"{n}[", f"{names[n]}[").replace(f"{{{n}[", f"{{{names[n]}[")
            if show is None:
                st.append(f"{pad}{names['r']} = {e};")
                st.append(f"{pad}$display(\"{prefix} {tag} %h\", {names['r']});")
                v = val if four else to2(val)
                ex.append(f"{prefix} {tag} {hexdigits(V(w, v.a, v.b))}")
            else:
                st.append(f"{pad}$display(\"{prefix} {tag} %h\", {e});")
                v = val if four else to2(val)
                ex.append(f"{prefix} {tag} {hexdigits(V(show, v.a, v.b))}")
        # conversions: to two-state, widening a signed 40-bit value, narrowing
        st += [f"{pad}{names['r2']} = {names['c']};",
               f"{pad}$display(\"{prefix} to2 %h\", {names['r2']});",
               f"{pad}{names['s40']} = {names['c']}[{w - 1} -: 40];",
               f"{pad}{names['r']} = {names['s40']};",
               f"{pad}$display(\"{prefix} sext %h\", {names['r']});",
               f"{pad}$display(\"{prefix} dec %0d\", {names['a']});",
               f"{pad}$display(\"{prefix} itor %h\", $realtobits(real'({names['a']})));",
               f"{pad}$display(\"{prefix} itorc %h\", $realtobits($itor({names['c']})));",
               f"{pad}if ({names['c']}) $display(\"{prefix} ifc t\"); "
               f"else $display(\"{prefix} ifc f\");",
               f"{pad}if ({names['c']} & ~{names['c']}) $display(\"{prefix} ifx t\"); "
               f"else $display(\"{prefix} ifx f\");",
               f"{pad}if (!{names['zero']}) $display(\"{prefix} if0 t\"); "
               f"else $display(\"{prefix} if0 f\");"]
        r2 = to2(c)
        s40 = select(c, w - 40, 40)
        s40.signed = True
        sext = resize(s40, w)
        sext = sext if four else to2(sext)
        ex += [f"{prefix} to2 {hexdigits(V(w, r2.a, 0))}",
               f"{prefix} sext {hexdigits(V(w, sext.a, sext.b))}",
               f"{prefix} dec {a.to_int()}",
               f"{prefix} itor {realbits(a)}",
               f"{prefix} itorc {realbits(c)}",
               f"{prefix} ifc {taken(c)}",
               f"{prefix} ifx {taken(bitwise('&', c, bitnot(c), w, signed))}",
               f"{prefix} if0 t"]
        return st, ex

    names = {n: p + n for n in ("a", "b", "c", "d", "zero", "r", "r2", "s40")}
    st, ex = body(f"{'4' if four else '2'}{'s' if signed else 'u'}{w} module", names, "    ")
    lines += st
    exp += ex
    # The same in an automatic task, on its frame.
    task = [f"  task automatic t{idx}();",
            f"    {ty} a, b, c, d, zero, r;",
            f"    {tname(w, False, signed)} r2;",
            "    logic signed [39:0] s40;"]
    tnames = {n: n for n in names}
    tst, tex = body(f"{'4' if four else '2'}{'s' if signed else 'u'}{w} task", tnames, "    ")
    task += tst + ["  endtask"]
    lines.append(f"    t{idx}();")
    exp += tex
    return decl, task, lines, exp


def places():
    """The place matrix on logic [99:0] (module wide_places)."""
    w = 100
    rng = random.Random(1001)
    a = V(w, rng.getrandbits(w))
    b = V(w, rng.getrandbits(w))
    m = (1 << w) - 1
    sv = """module wide_places;
  typedef struct packed { logic [99:0] hi; logic [27:0] lo; } pair_t;
  logic [99:0] a, b, v, n, f, arr[4];
  pair_t s;
  logic [7:0] k;
  wire [99:0] cw = a ^ b;
  wire [99:0] po;
  wide_child child(.i(a), .o(po));
  function automatic logic [99:0] mix(logic [99:0] x, logic [99:0] y);
    return x ^ (y << 3);
  endfunction
  task automatic flip(input logic [99:0] x, output logic [99:0] y);
    y = ~x;
  endtask
  task automatic bump(ref logic [99:0] x);
    x = x + 1;
  endtask
  task automatic bumpauto(input logic [99:0] x, output logic [99:0] y);
    logic [99:0] t[3];
    t[0] = '0;
    t[2] = x;
    bump(t[2]);
    poke(t[2]);
    y = t[2] ^ t[0] ^ t[1];
  endtask
  task automatic bumpauto2(input logic [99:0] x, output logic [99:0] y);
    logic [99:0] t[3];
    t[0] = '0;
    t[1] = '1;
    t[2] = x;
    bump(t[2]);
    poke(t[2]);
    y = t[2] ^ t[0] ^ ~t[1];
  endtask
  task automatic arrref(ref logic [99:0] x[3], output logic [99:0] y);
    logic [7:0] at;
    at = 70;
    x[1] = x[0] + 1;
    x[2][at +: 8] = 8'h5a;
    y = x[0] ^ x[1] ^ x[2];
  endtask
  task automatic arrauto(input logic [99:0] a0, output logic [99:0] y);
    logic [99:0] t[3];
    t[0] = a0;
    t[2] = '0;
    arrref(t, y);
  endtask
  task automatic poke(ref logic [99:0] x);
    logic [7:0] at;
    at = 80;
    x[at +: 1] = ~x[at +: 1];
  endtask
  logic [127:0] w128;
  logic [99:0] arr3[3];
  wire [99:0] rn;
  assign rn = a;
  assign rn = b;
  initial begin
"""
    # After every line of top, which all come at time 0: the order of the
    # processes at time 0 is not fixed by the standard (4.2).
    sv += f"    #2;\n    a = {literal(a)};\n    b = {literal(b)};\n    #1;\n"
    exp = []
    st = []
    st.append("arr[2] = a; arr[3] = arr[2] ^ b; $display(\"places array %h\", arr[3]);")
    exp.append(f"places array {hexdigits(V(w, a.a ^ b.a))}")
    st.append("s.hi = a; s.lo = 28'habcdef1; $display(\"places struct %h %h\", s, s.hi[99:72]);")
    exp.append(f"places struct {hexdigits(V(128, (a.a << 28) | 0xabcdef1))} "
               f"{hexdigits(V(28, a.a >> 72))}")
    st.append("$display(\"places function %h\", mix(a, b));")
    exp.append(f"places function {hexdigits(V(w, a.a ^ ((b.a << 3) & m)))}")
    st.append("flip(a, v); $display(\"places task %h\", v);")
    exp.append(f"places task {hexdigits(V(w, ~a.a & m))}")
    st.append("n <= b; n[99 -: 36] <= a[35:0]; #1 $display(\"places nba %h\", n);")
    exp.append(f"places nba {hexdigits(V(w, ((a.a & ((1 << 36) - 1)) << 64) | (b.a & ((1 << 64) - 1))))}")
    st.append("$display(\"places assign %h\", cw);")
    exp.append(f"places assign {hexdigits(V(w, a.a ^ b.a))}")
    st.append("$display(\"places port %h\", po);")
    exp.append(f"places port {hexdigits(V(w, ~a.a & m))}")
    st.append("f = b; force f = a; $display(\"places force %h\", f); release f; f = b;"
              " $display(\"places release %h\", f);")
    exp.append(f"places force {hexdigits(a)}")
    exp.append(f"places release {hexdigits(b)}")
    st.append("v = a; k = 67; v[k +: 30] = 30'h3fff_0000; v[k - 1] = 1'bz;"
              " $display(\"places varpart %h\", v);")
    vv = V(w, (a.a & ~(((1 << 30) - 1) << 67)) | (0x3fff0000 << 67))
    vv = V(w, vv.a & ~(1 << 66), 1 << 66)
    exp.append(f"places varpart {hexdigits(vv)}")
    # Built-in functions, nets, $sscanf, refs and the remaining operators.
    st.append("$display(\"places countones %0d onehot %b isunknown %b clog2 %0d\","
              " $countones(a), $onehot(a), $isunknown(a), $clog2(a));")
    exp.append(f"places countones {bin(a.a).count('1')} onehot {int(bin(a.a).count('1') == 1)} "
               f"isunknown 0 clog2 {(a.a - 1).bit_length()}")
    st.append("$display(\"places net %h\", rn);")
    exp.append(f"places net {hexdigits(bitwise('|', V(w, a.a & b.a), V(w, (a.a ^ b.a), (a.a ^ b.a)), w, False))}")
    big = (1 << 90) + 12345
    st.append(f"void'($sscanf(\"{hexdigits(a)} {big} -{big}\", \"%h %d %d\", v, n, f));"
              " $display(\"places sscanf %h %h %h\", v, n, f);")
    exp.append(f"places sscanf {hexdigits(a)} {hexdigits(V(w, big))} {hexdigits(V(w, -big))}")
    st.append("v = a; bump(v); arr[1] = b; bump(arr[1]); $display(\"places ref %h %h\", v, arr[1]);")
    exp.append(f"places ref {hexdigits(V(w, a.a + 1))} {hexdigits(V(w, b.a + 1))}")
    st.append("bumpauto(a, v); $display(\"places refauto %h\", v);")
    # t[1] keeps its default, x in every bit.
    exp.append(f"places refauto {hexdigits(allx(w))}")
    st.append("bumpauto2(a, v); $display(\"places refauto2 %h\", v);")
    exp.append(f"places refauto2 {hexdigits(V(w, (a.a + 1) ^ (1 << 80)))}")
    st.append("arr3[0] = a; arr3[2] = b; arrref(arr3, v);"
              " $display(\"places arrref %h %h %h\", v, arr3[1], arr3[2]);")
    a1 = (a.a + 1) & m
    a2 = (b.a & ~(0xff << 70)) | (0x5a << 70)
    exp.append(f"places arrref {hexdigits(V(w, a.a ^ a1 ^ a2))} {hexdigits(V(w, a1))} "
               f"{hexdigits(V(w, a2))}")
    st.append("arrauto(a, v); $display(\"places arrauto %h\", v);")
    exp.append(f"places arrauto {hexdigits(V(w, a.a ^ a1 ^ (0x5a << 70)))}")
    st.append("v = a ** 3; $display(\"places pow %h\", v);")
    exp.append(f"places pow {hexdigits(V(w, a.a ** 3))}")
    st.append("k = 8'bx; v = k[0] ? a : b; $display(\"places selx %h\", v);")
    agree = ~(a.a ^ b.a) & m
    exp.append(f"places selx {hexdigits(V(w, (a.a & agree) | (~agree & m), ~agree & m))}")
    st.append(f"$display(\"places wildeq %b %b\", a ==? {w}'h{hexdigits(a)[:-1]}?, a ==? b);")
    exp.append("places wildeq 1 0")
    st.append(f"casez (a) {w}'h{hexdigits(a)[:5]}{'?' * (len(hexdigits(a)) - 5)}:"
              " $display(\"places casez hit\"); default: $display(\"places casez miss\"); endcase")
    exp.append("places casez hit")
    st.append("v = a; v++; v += b; $display(\"places incr %h\", v);")
    exp.append(f"places incr {hexdigits(V(w, a.a + 1 + b.a))}")
    st.append("w128 = {<<8{a[95:0], 32'h11223344}}; $display(\"places stream %h\", w128);")
    sv_bytes = (((a.a & ((1 << 96) - 1)) << 32) | 0x11223344).to_bytes(16, "big")[::-1]
    exp.append(f"places stream {sv_bytes.hex()}")
    sv += "".join(f"    {x}\n" for x in st)
    sv += "  end\nendmodule\n\n"
    sv += ("module wide_child(input logic [99:0] i, output logic [99:0] o);\n"
           "  assign o = ~i;\nendmodule\n")
    return sv, exp


def files():
    rng = random.Random(2 * 7919)  # version 2's seed: new cases keep the earlier lines
    decls, tasks, lines, exp = [], [], [], []
    idx = 0
    for w in WIDTHS:
        for four in (True, False):
            for signed in (False, True):
                d, t, l, e = ops_module(w, four, signed, idx, rng)
                decls += d
                tasks += t
                lines += l
                exp += e
                idx += 1
    head = [
        "/*",
        f":name: {NAME}",
        ":description: integral values wider than 64 bits: operators, selects, concatenation "
        "and conversions at 65, 100, 128 and 200 bits, two- and four-state, signed and "
        "unsigned, on module and automatic variables, and a value through every place it can "
        "live",
        ":tags: values wide",
        ":type: simulation",
        ":lrm: 1800-2023 6.11 6.12 11.4 11.4.7 11.5 11.8 12.4 21.2.1.2",
        ":oracle: spec",
        ":spec_check: ai",
        ":fourstate: yes",
        ":nondeterministic: no",
        f":generator: gen_wide_matrix.py@{VERSION}",
        f":top_module: top",
        f":expected_stdout: @{NAME}.expected",
        "*/",
        "// Generated by tools/gen_wide_matrix.py; change the generator, not this file.",
        "`timescale 1ns/1ns",
        "module top;",
        "  logic [7:0] k;",
    ]
    sv = head + decls + tasks + ["  wide_places places();", "  initial begin"] + lines
    sv += ["  end", "endmodule", ""]
    psv, pexp = places()
    text = "\n".join(sv) + "\n" + psv
    return {f"{NAME}.sv": text, f"{NAME}.expected": "\n".join(exp + pexp) + "\n"}


def main():
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("--check", action="store_true")
    args = ap.parse_args()
    bad = 0
    for name, text in files().items():
        path = OUT / name
        if args.check:
            if not path.exists() or path.read_text(encoding="utf-8") != text:
                print(f"gen_wide_matrix: {path.relative_to(OUT.parent.parent.parent)} is out of "
                      "date; run tools/gen_wide_matrix.py", file=sys.stderr)
                bad += 1
        else:
            path.write_text(text, encoding="utf-8")
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main())
