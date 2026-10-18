#!/usr/bin/env python3
"""Generates the matrix test of selects outside the range of
TEST_SUITE.md ("Mechanism matrix") in tests/suite/values/.

  tools/gen_select_matrix.py           # writes the files
  tools/gen_select_matrix.py --check   # exit 1 if a file differs from what it would write

VALUE_SEMANTICS.md 3.11 (11.5.1, 7.4.5): a bit-select whose index is out
of range, x or z reads x (0 in a two-state value) and writes nothing; a
part-select partly out of range reads x for the bits outside and writes
only the bits inside; one wholly outside, or at an unknown position, reads
x and writes nothing. An unpacked array read at an invalid index gives the
element type's default and a write there does nothing.

The axes of the packed part: the place of the value (a module variable,
an element of a module array, an automatic variable and an element of an
automatic array of a task, and an element through a ref to a module array
and to an automatic array); the width (8, and 100, held in two words) and
two- and four-state; the declared range ([W-1:0], and for module
variables also [W+3:4] and [0:W-1]); the position, always known only at
run time (inside, crossing a word for the wide value; partly below; partly
above; wholly above; wholly below at a negative index; x; a 70-bit value
past 2**64; a longint 2**63 - 2, where position plus width would
overflow 64 bits; a 70-bit -2**64 + 2, whose low word alone reads 2); and
the access (a bit, an indexed part +: and -:, a
replication of a part, and writes of a bit and of the parts, blocking and,
in a module variable or an element of a module array, nonblocking).

The unpacked part: arrays of 8- and 100-bit elements, two- and
four-state, in a module, in a task and through refs to both, read and
written (blocking, and nonblocking for the module array; slang rejects a
nonblocking write through a ref, 10.4.2) at a valid
index, past the end, at -1, at x and at the 70-bit value.

A third part reads and writes a string's characters (VALUE_SEMANTICS.md
7.6, 6.16): s[i] and getc give 0 outside the string, s[i] = c and putc
change nothing there; in a module and in a task, at the same positions.
getc and putc take an int, so their position is converted first: x is
0, and a wider value keeps its low 32 bits (6.16.2, 6.16.3).

The expected outputs follow from the rules above, computed below. A
person checks them once for all the tests made from them (TEST_SUITE.md,
"Sampling by people").
"""

from __future__ import annotations

import argparse
import random
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from gen_wide_matrix import V, hexdigits, literal  # noqa: E402

OUT = Path(__file__).resolve().parent.parent / "tests" / "suite" / "values"
NAME = "select_matrix"
# The version of the rules below (TEST_SUITE.md, :generator:). Raise it
# whenever they change.
VERSION = 1

WIDTHS = [8, 100]
PART = {8: 4, 100: 12}  # the width of a part
HUGE = 1 << 68  # the 70-bit position
MAX = (1 << 63) - 2  # a longint position, where position + width would overflow 64 bits
WRAP = -(1 << 64) + 2  # a 70-bit signed position whose low word alone reads 2
PLACES = ["mod", "elem", "auto", "auto_elem", "ref_mod", "ref_auto"]
SLOT_PLACES = {"mod", "elem"}  # where a nonblocking write may go (not through a ref)


def ranges(w, place):
    """(label, left, right) of the declared ranges for a place."""
    out = [("d", w - 1, 0)]
    if place == "mod":
        out += [("o", w + 3, 4), ("a", 0, w - 1)]
    return out


def offset(left, right, i):
    """The bit offset (0 = least significant) of index i, or None outside."""
    if left >= right:
        return i - right if right <= i <= left else None
    return right - i if left <= i <= right else None


def index_name(i):
    """The variable that holds a position: k (int), kx (x), kh and kw (70
    bits), kl (longint)."""
    return {None: "kx", HUGE: "kh", MAX: "kl", WRAP: "kw"}.get(i, "k")


def part_indices(left, right, base, width):
    """The indices of [base +: width], most significant first (11.5.1)."""
    if left >= right:
        return list(range(base + width - 1, base - 1, -1))
    return list(range(base, base + width))


def positions(w, left, right):
    """(case, index value or None for x) of the run-time positions."""
    lo, hi = min(left, right), max(left, right)
    return [("in", lo + (2 if w == 8 else 58)), ("low", lo - 2), ("high", hi - 1),
            ("out", hi + 5), ("neg", -20), ("x", None), ("huge", HUGE), ("max", MAX),
            ("wrap", WRAP)]


def read_bits(v, four, left, right, indices):
    """The bits read at indices (None for an unknown position), as %b text."""
    out = []
    for i in indices:
        off = None if i is None else offset(left, right, i)
        if off is None:
            out.append("x" if four else "0")
        else:
            a, b = (v.a >> off) & 1, (v.b >> off) & 1
            out.append(("x" if a else "z") if b else str(a))
    return "".join(out)


def write_bits(v, left, right, indices, value, vw):
    """v with the bits of value (vw wide, msb first) written at indices."""
    a, b = v.a, v.b
    if any(i is None for i in indices):
        return V(v.w, a, b)
    for k, i in enumerate(indices):
        off = offset(left, right, i)
        if off is None:
            continue
        bit = vw - 1 - k
        ba, bb = (value.a >> bit) & 1, (value.b >> bit) & 1
        a = (a & ~(1 << off)) | (ba << off)
        b = (b & ~(1 << off)) | (bb << off)
    return V(v.w, a, b)


def initial(rng, w, four, left, right):
    """A start value: random, 1 at the bits the bit writes aim at, and in
    four-state an x and a z near the top."""
    v = V(w, rng.getrandbits(w), 0)
    for _, i in positions(w, left, right):
        off = None if i is None else offset(left, right, i)
        if off is not None:
            v = V(w, v.a | (1 << off), v.b)
    if four:
        v = V(w, v.a | (1 << (w - 1)) | (1 << (w - 3)), v.b | (1 << (w - 1)) | (1 << (w - 3)))
        v = V(w, v.a & ~(1 << (w - 1)), v.b)  # the top bit z, the third x
    return v


def packed_case(n, place, w, four, rlabel, left, right, rng):
    """(module declarations, tasks, initial statements, expected lines)."""
    kind = "logic" if four else "bit"
    typ = f"t{n}_t"
    decl = [f"  typedef {kind} [{left}:{right}] {typ};"]
    tasks, body, exp = [], [], []
    p = PART[w]
    tag = f"{place}.{'4' if four else '2'}.{w}.{rlabel}"
    init = initial(rng, w, four, left, right)
    rhs = V(p, 0b0110 if p == 4 else 0x6A5)
    if place == "mod":
        decl.append(f"  {typ} s{n};")
        target = f"s{n}"
    elif place == "elem":
        decl.append(f"  {typ} e{n}[0:1];")
        target = f"e{n}[1]"
    elif place == "ref_mod":
        decl.append(f"  {typ} m{n}[0:1];")
        target = "r[1]"
    elif place == "auto":
        target = "v"
    elif place == "auto_elem":
        target = "va[1]"
    else:
        target = "r[1]"
    nba = place in SLOT_PLACES
    st = []
    for case, i in positions(w, left, right):
        idx = index_name(i)
        if idx == "k":
            st.append(f"k = {i};")
        bits = part_indices(left, right, i, p) if i is not None else [None] * p
        bit = [i]
        st.append(f"{target} = {literal(init)};")
        st.append(f'$display("{tag} {case} bit %b", {target}[{idx}]);')
        exp.append(f"{tag} {case} bit {read_bits(init, four, left, right, bit)}")
        st.append(f'$display("{tag} {case} up %b", {target}[{idx} +: {p}]);')
        exp.append(f"{tag} {case} up {read_bits(init, four, left, right, bits)}")
        st.append(f'$display("{tag} {case} down %b", {target}[{idx} + {p - 1} -: {p}]);')
        exp.append(f"{tag} {case} down {read_bits(init, four, left, right, bits)}")
        st.append(f'$display("{tag} {case} rep %b", {{2{{{target}[{idx} +: {p}]}}}});')
        exp.append(f"{tag} {case} rep {read_bits(init, four, left, right, bits) * 2}")
        writes = [("wbit", f"{target}[{idx}] = 1'b0;", bit, V(1, 0), 1),
                  ("wup", f"{target}[{idx} +: {p}] = {literal(rhs)};", bits, rhs, p),
                  ("wdown", f"{target}[{idx} + {p - 1} -: {p}] = {literal(rhs)};", bits, rhs, p)]
        if nba:
            writes += [("nbit", f"{target}[{idx}] <= 1'b0;", bit, V(1, 0), 1),
                       ("nup", f"{target}[{idx} +: {p}] <= {literal(rhs)};", bits, rhs, p)]
        for op, stmt, where, value, vw in writes:
            wait = " #1;" if op.startswith("n") else ""
            st.append(f"{target} = {literal(init)}; {stmt}{wait} "
                      f'$display("{tag} {case} {op} %h", {target});')
            after = write_bits(init, left, right, where, value, vw)
            exp.append(f"{tag} {case} {op} {hexdigits(after)}")
    pad = "    "
    if place in ("mod", "elem"):
        body += [pad + s for s in st]
    elif place in ("auto", "auto_elem"):
        local = f"{typ} v;" if place == "auto" else f"{typ} va[0:1];"
        tasks.append(f"  task automatic p{n};")
        tasks.append(f"    {local}")
        tasks += ["    " + s for s in st]
        tasks.append("  endtask")
        body.append(f"{pad}p{n}();")
    else:
        tasks.append(f"  task automatic p{n}(ref {typ} r[0:1]);")
        tasks += ["    " + s for s in st]
        tasks.append("  endtask")
        if place == "ref_mod":
            body.append(f"{pad}p{n}(m{n});")
        else:
            tasks.append(f"  task automatic q{n};")
            tasks.append(f"    {typ} la[0:1];")
            tasks.append(f"    p{n}(la);")
            tasks.append("  endtask")
            body.append(f"{pad}q{n}();")
    return decl, tasks, body, exp


ARRAY_PLACES = ["mod", "auto", "ref_mod", "ref_auto"]
STRING = "abc"


def int_arg(i):
    """A position passed to an int argument: x is 0, then the low 32 bits."""
    v = 0 if i is None else i & 0xFFFFFFFF
    return v - (1 << 32) if v >> 31 else v


def string_case(n, place):
    """Strings (VALUE_SEMANTICS.md 7.6, 6.16): s[i] and getc read 0 outside;
    s[i] = c and putc change nothing there."""
    tag = f"string.{place}"
    s = "s" if place == "auto" else f"str{n}"
    decl = [] if place == "auto" else [f"  string str{n};"]
    st, exp = [], []
    for case, i in [("valid", 1), ("past", 3), ("minus", -1), ("x", None), ("huge", HUGE),
                    ("max", MAX), ("wrap", WRAP)]:
        idx = index_name(i)
        if idx == "k":
            st.append(f"k = {i};")
        def at(j):
            ok = j is not None and 0 <= j < len(STRING)
            return (f"{ord(STRING[j]):02x}" if ok else "00",
                    STRING[:j] + "z" + STRING[j + 1:] if ok else STRING)
        byte, after = at(i)
        # getc and putc take an int (6.16.2, 6.16.3): x becomes 0 and a wider
        # value keeps its low 32 bits.
        mbyte, mafter = at(int_arg(i))
        st.append(f'{s} = "{STRING}"; $display("{tag} {case} read %h", {s}[{idx}]);')
        exp.append(f"{tag} {case} read {byte}")
        st.append(f'$display("{tag} {case} getc %h", {s}.getc({idx}));')
        exp.append(f"{tag} {case} getc {mbyte}")
        st.append(f'{s}[{idx}] = "z"; $display("{tag} {case} write %s", {s});')
        exp.append(f"{tag} {case} write {after}")
        st.append(f'{s} = "{STRING}"; {s}.putc({idx}, "z"); $display("{tag} {case} putc %s", {s});')
        exp.append(f"{tag} {case} putc {mafter}")
    tasks, body = [], []
    if place == "auto":
        tasks += [f"  task automatic p{n};", "    string s;"] + ["    " + x for x in st]
        tasks.append("  endtask")
        body.append(f"    p{n}();")
    else:
        body += ["    " + x for x in st]
    return decl, tasks, body, exp


def array_case(n, place, w, four, rng):
    """The unpacked part: (declarations, tasks, statements, expected lines)."""
    kind = "logic" if four else "bit"
    typ = f"a{n}_t"
    decl = [f"  typedef {kind} [{w - 1}:0] {typ};"]
    tasks, body, exp = [], [], []
    tag = f"array.{place}.{'4' if four else '2'}.{w}"
    vals = [V(w, rng.getrandbits(w)) for _ in range(4)]
    if four:
        vals[2] = V(w, vals[2].a | 0xF0, vals[2].b | 0xFF0)
    new = V(w, rng.getrandbits(w))
    default = V(w, (1 << w) - 1, (1 << w) - 1) if four else V(w, 0)
    arr = {"mod": f"g{n}", "auto": "la", "ref_mod": "r", "ref_auto": "r"}[place]
    if place in ("mod", "ref_mod"):
        decl.append(f"  {typ} g{n}[0:3];")
    st = []
    show = ", ".join(f"{arr}[{j}]" for j in range(4))
    for case, i in [("valid", 1), ("past", 4), ("minus", -1), ("x", None), ("huge", HUGE),
                    ("max", MAX), ("wrap", WRAP)]:
        idx = index_name(i)
        if idx == "k":
            st.append(f"k = {i};")
        st.append(" ".join(f"{arr}[{j}] = {literal(vals[j])};" for j in range(4)))
        ok = i is not None and 0 <= i <= 3
        st.append(f'$display("{tag} {case} read %h", {arr}[{idx}]);')
        exp.append(f"{tag} {case} read {hexdigits(vals[i] if ok else default)}")
        after = [new if ok and j == i else vals[j] for j in range(4)]
        ops = [("write", f"{arr}[{idx}] = {literal(new)};")]
        if place == "mod":
            ops.append(("nba", f"{arr}[{idx}] <= {literal(new)};"))
        for op, stmt in ops:
            st.append(" ".join(f"{arr}[{j}] = {literal(vals[j])};" for j in range(4)))
            wait = " #1;" if op == "nba" else ""
            st.append(f'{stmt}{wait} $display("{tag} {case} {op} %h %h %h %h", {show});')
            exp.append(f"{tag} {case} {op} " + " ".join(hexdigits(x) for x in after))
    pad = "    "
    if place == "mod":
        body += [pad + s for s in st]
    elif place == "auto":
        tasks += [f"  task automatic p{n};", f"    {typ} la[0:3];"]
        tasks += ["    " + s for s in st]
        tasks.append("  endtask")
        body.append(f"{pad}p{n}();")
    else:
        tasks.append(f"  task automatic p{n}(ref {typ} r[0:3]);")
        tasks += ["    " + s for s in st]
        tasks.append("  endtask")
        if place == "ref_mod":
            body.append(f"{pad}p{n}(g{n});")
        else:
            tasks += [f"  task automatic q{n};", f"    {typ} la[0:3];", f"    p{n}(la);",
                      "  endtask"]
            body.append(f"{pad}q{n}();")
    return decl, tasks, body, exp


def files():
    rng = random.Random(3_11)
    decls, tasks, body, exp = [], [], [], []
    n = 0
    for place in PLACES:
        for w in WIDTHS:
            for four in (True, False):
                for rlabel, left, right in ranges(w, place):
                    d, t, b, e = packed_case(n, place, w, four, rlabel, left, right, rng)
                    decls += d
                    tasks += t
                    body += b
                    exp += e
                    n += 1
    for place in ARRAY_PLACES:
        for w in WIDTHS:
            for four in (True, False):
                d, t, b, e = array_case(n, place, w, four, rng)
                decls += d
                tasks += t
                body += b
                exp += e
                n += 1
    for place in ("mod", "auto"):
        d, t, b, e = string_case(n, place)
        decls += d
        tasks += t
        body += b
        exp += e
        n += 1
    head = [
        "/*",
        f":name: {NAME}",
        ":description: bit-selects, indexed part-selects and unpacked array elements at "
        "positions inside, partly outside, wholly outside, x and past 2**64, read and written, "
        "on 8- and 100-bit values, two- and four-state, in module and automatic variables and "
        "array elements and through refs",
        ":tags: values select",
        ":type: simulation",
        ":lrm: 1800-2023 6.16 7.4.5 11.4.12.1 11.5.1 13.5.2",
        ":oracle: spec",
        ":spec_check: ai",
        ":fourstate: yes",
        ":nondeterministic: no",
        f":generator: gen_select_matrix.py@{VERSION}",
        ":top_module: top",
        f":expected_stdout: @{NAME}.expected",
        "*/",
        "// Generated by tools/gen_select_matrix.py; change the generator, not this file.",
        "`timescale 1ns/1ns",
        "module top;",
        "  int k;",
        "  integer kx;",
        f"  logic [69:0] kh = 70'h{HUGE:x};",
        f"  longint kl = 64'sh{MAX:x};",
        f"  logic signed [69:0] kw = 70'sh{WRAP & ((1 << 70) - 1):x};",
    ]
    sv = head + decls + tasks + ["  initial begin"] + body + ["  end", "endmodule", ""]
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
                print(f"gen_select_matrix: {path.relative_to(OUT.parent.parent.parent)} is out of "
                      "date; run tools/gen_select_matrix.py", file=sys.stderr)
                bad += 1
        else:
            path.write_text(text, encoding="utf-8")
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main())
