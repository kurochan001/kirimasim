#!/usr/bin/env python3
# Copyright (c) 2026 The kirimasim Authors
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

"""Generates the matrix test of range selects of packed arrays, of
TEST_SUITE.md ("Mechanism matrix"), in tests/suite/values/.

  tools/gen_packed_range_matrix.py           # writes the files
  tools/gen_packed_range_matrix.py --check   # exit 1 if a file differs from what it would write

A range select of a packed array selects elements of the dimension it
indexes, each as wide as one element of that dimension (7.4.1, 11.5.1):
on logic [3:0][7:0] a, a[2:1] is the bits 23:8. The bounds are declared
indices of that dimension. A select partly outside the range reads x for
the elements outside and writes only those inside; one at an x position
reads x and writes nothing (VALUE_SEMANTICS.md 3.11).

The axes: the declared range of the dimension (descending and ascending,
from 0, from 2 and through negative indices); the shape (a vector, so an
element of one bit; two dimensions with elements of 1, 8 and 65 bits;
three dimensions, elements of 2 x 4 bits); the select (a simple range;
+: and -: with a constant base and with a variable one; +: and -: partly
outside; +: at an x base); the place (a module variable, an element of a
module's unpacked array, a member of a packed structure, which takes
constant selects only, and an automatic variable of a task); the access
(a read, a blocking write and a nonblocking write, which an automatic
variable does not take, 10.4.2). Two more parts take the constant
selects: a continuous assignment to the selected elements of a net (the
others have no driver and are z, 10.3), and an event control on the
selected elements (9.4.2), which a change of another element does not
wake. Reads print in binary, so that no hexadecimal digit mixes bits
inside and outside the range.

The expected outputs follow from the rules above, computed below.
"""

from __future__ import annotations

import argparse
import sys
from pathlib import Path

OUT = Path(__file__).resolve().parent.parent / "tests" / "suite" / "values"
NAME = "packed_range_matrix"
# The version of the rules below (TEST_SUITE.md, :generator:). Raise it
# whenever they change.
VERSION = 1

# (left, right) of the dimension the select indexes; four elements each.
RANGES = [(3, 0), (0, 3), (5, 2), (2, 5), (1, -2), (-2, 1)]
# (name, the packed dimensions after the first, the element width)
SHAPES = [
    ("vector", "", 1),
    ("e1", "[0:0]", 1),
    ("e8", "[7:0]", 8),
    ("e65", "[64:0]", 65),
    ("e2x4", "[1:0][3:0]", 8),
]
SELECTS = ["simple", "up", "down", "up_var", "down_var", "up_part", "down_part", "up_x"]
CONST_SELECTS = ["simple", "up", "down"]
ACCESSES = ["read", "write", "nba"]
PLACES = ["module", "element", "member", "task"]
INIT = int("0123456789abcdef" "f0e1d2c3b4a59687" "1122334455667788" "99aabbccddeeff00"
           "a5c3e1f00f1e3c5a", 16)
PAT = int("deadbeefcafef00d" "5a5a5a5a3c3c3c3c" "0f0f0f0fc0c0c0c0", 16)


def rep(pattern, bits):
    """`bits` bits of the pattern, repeated as needed."""
    v, have = 0, 0
    while have < bits:
        v |= pattern << have
        have += pattern.bit_length()
    return v & ((1 << bits) - 1)


def offset(rng, d):
    """The element number of declared index d, from the right bound."""
    left, right = rng
    return d - right if left >= right else right - d


def index(rng, o):
    """The declared index of element number o."""
    left, right = rng
    return right + o if left >= right else right - o


def select(rng, kind):
    """The select's text with B for a variable base, the base's value
    (None for x) and the declared indices it covers."""
    d1, d2 = index(rng, 1), index(rng, 2)
    lo, hi = min(d1, d2), max(d1, d2)
    top, bottom = max(rng), min(rng)
    if kind == "simple":
        return f"[{d2}:{d1}]", None, [d1, d2]
    if kind == "up":
        return f"[{lo} +: 2]", None, [lo, lo + 1]
    if kind == "down":
        return f"[{hi} -: 2]", None, [hi - 1, hi]
    if kind == "up_var":
        return "[B +: 2]", lo, [lo, lo + 1]
    if kind == "down_var":
        return "[B -: 2]", hi, [hi - 1, hi]
    if kind == "up_part":
        return "[B +: 2]", top, [top, top + 1]
    if kind == "down_part":
        return "[B -: 2]", bottom, [bottom - 1, bottom]
    return "[B +: 2]", None, None


def bits_of(a, b, w):
    """%b of a w-bit value with x where b is 1."""
    return "".join("x" if (b >> i) & 1 else str((a >> i) & 1) for i in range(w - 1, -1, -1))


def hexdigits(a, z, w):
    """%h (SYSTEM_TASKS.md 3.2, 3.4) of a value whose bits in z are z."""
    out = []
    for d in range((w + 3) // 4 - 1, -1, -1):
        n = min(4, w - 4 * d)
        m = (1 << n) - 1
        da, dz = (a >> (4 * d)) & m, (z >> (4 * d)) & m
        out.append("%x" % da if dz == 0 else "z" if dz == m else "Z")
    return "".join(out)


def low_bit(rng, ew, covered):
    return min(offset(rng, d) for d in covered) * ew


def expected(rng, ew, covered, access):
    """The read's bits, or the whole value after the write."""
    total = 4 * ew
    init = rep(INIT, total)
    pat = rep(PAT, 2 * ew)
    if covered is None:
        if access == "read":
            return bits_of(0, (1 << (2 * ew)) - 1, 2 * ew)
        return hexdigits(init, 0, total)
    low = low_bit(rng, ew, covered)
    if access == "read":
        a = b = 0
        for i in range(2 * ew):
            p = low + i
            if 0 <= p < total:
                a |= ((init >> p) & 1) << i
            else:
                b |= 1 << i
        return bits_of(a, b, 2 * ew)
    v = init
    for i in range(2 * ew):
        p = low + i
        if 0 <= p < total:
            v = (v & ~(1 << p)) | (((pat >> i) & 1) << p)
    return hexdigits(v, 0, total)


def driven(rng, ew, covered):
    """A net whose selected elements a continuous assignment drives."""
    total = 4 * ew
    low = low_bit(rng, ew, covered)
    pat = rep(PAT, 2 * ew)
    sel = ((1 << (2 * ew)) - 1) << low
    return hexdigits(pat << low, ((1 << total) - 1) & ~sel, total)


def place_name(place, r, sname):
    if place == "module":
        return f"v{r}_{sname}"
    if place == "element":
        return f"m{r}_{sname}[1]"
    if place == "member":
        return f"s{r}_{sname}.f"
    return "v"


def files():
    decls, module, tasks, calls, exp = [], [], [], [], []
    nets, net_lines, waits, events = [], [], [], []
    for r, rng in enumerate(RANGES):
        for sname, inner, ew in SHAPES:
            total = 4 * ew
            dims = f"[{rng[0]}:{rng[1]}]{inner}"
            init = f"{total}'h{rep(INIT, total):x}"
            pat = f"{2 * ew}'h{rep(PAT, 2 * ew):x}"
            decls += [f"  logic {dims} v{r}_{sname};",
                      f"  logic {dims} m{r}_{sname} [0:1];",
                      f"  struct packed {{ logic [3:0] g; logic {dims} f; logic [3:0] h; }} "
                      f"s{r}_{sname};"]
            body = []
            for place in PLACES:
                name = place_name(place, r, sname)
                lines = body if place == "task" else module
                for kind in SELECTS:
                    text, base, covered = select(rng, kind)
                    if place == "member" and kind not in CONST_SELECTS:
                        continue
                    for access in ACCESSES:
                        if place == "task" and access == "nba":
                            continue
                        tag = f"{place} [{rng[0]}:{rng[1]}] {sname} {kind} {access}"
                        sel = text.replace("B", "b")
                        lines.append(f"    {name} = {init};")
                        if "B" in text:
                            lines.append(f"    b = {base if base is not None else chr(39) + 'x'};")
                        if access == "read":
                            lines.append(f'    $display("{tag} %b", {name}{sel});')
                        elif access == "write":
                            lines.append(f"    {name}{sel} = {pat};")
                            lines.append(f'    $display("{tag} %h", {name});')
                        else:
                            lines.append(f"    {name}{sel} <= {pat};")
                            lines.append(f'    #1 $display("{tag} %h", {name});')
                        exp.append(("task" if place == "task" else "main", tag,
                                    expected(rng, ew, covered, access)))
            tname = f"t{r}_{sname}"
            tasks += [f"  task automatic {tname}();", f"    logic {dims} v;", "    integer b;"]
            tasks += body + ["  endtask"]
            calls.append(f"    {tname}();")
            for kind in CONST_SELECTS:
                text, _, covered = select(rng, kind)
                k = len(events)
                tag = f"assign [{rng[0]}:{rng[1]}] {sname} {kind}"
                nets += [f"  wire {dims} w{k};", f"  assign w{k}{text} = {pat};"]
                net_lines.append(f'    $display("{tag} %h", w{k});')
                exp.append(("assign", tag, driven(rng, ew, covered)))
                decls.append(f"  logic {dims} e{k} = {init};")
                events.append((f"event [{rng[0]}:{rng[1]}] {sname} {kind}", f"e{k}", text, rng))
    # The waits wake after the procedure above has printed everything,
    # each at a time of its own: another element changes, then a selected
    # one, which wakes it.
    start = sum(1 for line in module if line.startswith("    #1 "))
    drives = [f"    #{start};"]
    for j, (tag, e, text, rng) in enumerate(events):
        waits.append(f"  initial begin\n    @({e}{text});\n"
                     f'    $display("{tag} %0t", $time);\n  end')
        drives += [f"    #1 {e}[{index(rng, 0)}] = ~{e}[{index(rng, 0)}];",
                   f"    #1 {e}[{index(rng, 1)}] = ~{e}[{index(rng, 1)}];"]
        exp.append(("event", tag, str(start + 2 * j + 2)))
    head = [
        "/*",
        f":name: {NAME}",
        ":description: range selects of packed arrays select whole elements of the dimension "
        "they index: simple, +: and -: with constant and variable bases, partly outside and "
        "at x; read, written and written nonblocking in a module variable, an element of an "
        "unpacked array, a member of a packed structure and an automatic variable, driven by "
        "a continuous assignment and waited on; descending, ascending and negative ranges, "
        "elements of 1, 8 and 65 bits and of two dimensions",
        ":tags: values select",
        ":type: simulation",
        ":lrm: 1800-2023 7.4.1 7.4.5 9.4.2 10.3 11.5.1",
        ":oracle: spec",
        ":spec_check: ai",
        ":fourstate: yes",
        ":nondeterministic: no",
        f":generator: gen_packed_range_matrix.py@{VERSION}",
        ":issue: 3",
        ":top_module: top",
        f":expected_stdout: @{NAME}.expected",
        "*/",
        "// Generated by tools/gen_packed_range_matrix.py; change the generator, not this file.",
        "`timescale 1ns/1ns",
        "module top;",
        "  integer b;",
    ]
    sv = head + decls + nets + tasks + waits
    sv += ["  initial begin"] + module + calls + net_lines + ["  end"]
    sv += ["  initial begin"] + drives + ["  end", "endmodule", ""]
    order = [t for c in ["main", "task", "assign", "event"] for t in exp if t[0] == c]
    lines = [f"{tag} {value}" for _, tag, value in order]
    return {f"{NAME}.sv": "\n".join(sv), f"{NAME}.expected": "\n".join(lines) + "\n"}


def main():
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("--check", action="store_true")
    args = ap.parse_args()
    bad = 0
    for name, text in files().items():
        path = OUT / name
        if args.check:
            if not path.exists() or path.read_text(encoding="utf-8") != text:
                print(f"gen_packed_range_matrix: {path.relative_to(OUT.parent.parent.parent)} is "
                      "out of date; run tools/gen_packed_range_matrix.py", file=sys.stderr)
                bad += 1
        else:
            path.write_text(text, encoding="utf-8")
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main())
