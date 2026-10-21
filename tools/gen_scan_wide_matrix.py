#!/usr/bin/env python3
# Copyright (c) 2026 The kirimasim Authors
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

"""Generates the matrix test of $sscanf into integral variables wider
than 64 bits, of TEST_SUITE.md ("Mechanism matrix"), in
tests/suite/values/.

  tools/gen_scan_wide_matrix.py           # writes the files
  tools/gen_scan_wide_matrix.py --check   # exit 1 if a file differs from what it would write

$sscanf reads a value of any width the same way (21.3.4.3, SYSTEM_TASKS.md
6.3): %b, %o and %h read the digits of their radix and x, z and ? as
digits, ? being z, and the value read is extended with 0 on the left; %d
reads a sign and decimal digits, or one x, z or ?, which is every bit; %s
reads
characters, the last one lowest. A value that does not fit puts its low
bits in, and a two-state variable takes x and z as 0 (VALUE_SEMANTICS.md
1.2).

The axes: the width (65, 100, 128, 200), two- and four-state, signed and
unsigned; the input (hexadecimal, octal and binary digits with x and z
around bit 64 and longer than the variable, and with ?; a decimal number
that fits, one that does not, a negative one, x, z and ?; a short and a
long string); the place (a module variable, an element of a module's unpacked
array, an automatic variable of a task). Each line prints the count
$sscanf returns and the variable.

The expected outputs follow from the rules above and the %h rules of
SYSTEM_TASKS.md 3.2 and 3.4, computed below.
"""

from __future__ import annotations

import argparse
import sys
from pathlib import Path

OUT = Path(__file__).resolve().parent.parent / "tests" / "suite" / "values"
NAME = "scan_wide_matrix"
# The version of the rules below (TEST_SUITE.md, :generator:). Raise it
# whenever they change.
VERSION = 2
WIDTHS = [65, 100, 128, 200]
TYPES = [(four, signed) for four in (True, False) for signed in (False, True)]
PLACES = ["module", "element", "task"]
RADIX = {"b": 1, "o": 3, "h": 4}


class V:
    """A four-state value: a and b planes (b=1: x when a=1, z when a=0)."""

    def __init__(self, w, a, b=0):
        m = (1 << w) - 1
        self.w, self.a, self.b = w, a & m, b & m


def digits_value(text, conv, w):
    bits = RADIX[conv]
    a = b = 0
    for ch in text:
        if ch == "_":
            continue
        a <<= bits
        b <<= bits
        m = (1 << bits) - 1
        if ch in "xX":
            a |= m
            b |= m
        elif ch in "zZ?":
            b |= m
        else:
            a |= int(ch, 16)
    return V(w, a, b)


def decimal_value(text, w):
    if text in ("x", "X"):
        return V(w, -1, -1)
    if text in ("z", "Z", "?"):
        return V(w, 0, -1)
    return V(w, int(text.replace("_", "")) % (1 << w))


def string_value(text, w):
    a = 0
    for ch in text:
        a = (a << 8) | ord(ch)
    return V(w, a)


def to2(v):
    return V(v.w, v.a & ~v.b)


def hexdigits(v):
    """%h with the automatic width (SYSTEM_TASKS.md 3.2, 3.4)."""
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


def put(digits, pos, ch):
    """digits with the digit `pos` places from the right set to ch."""
    if pos >= len(digits):
        return digits
    k = len(digits) - 1 - pos
    return digits[:k] + ch + digits[k + 1:]


def inputs(w):
    """(name, format, input text, value read)."""
    out = []
    # Hexadecimal: two digits more than the variable holds, x at bits
    # 67:64, z at 71:68 and lowest.
    h = ("123456789abcdef0fedcba9876543210" * 2)[: (w + 3) // 4 + 2]
    h = put(put(put(h, 16, "x"), 17, "z"), 0, "Z")
    out.append(("hex", "%h", h, digits_value(h, "h", w)))
    # ? is z (SYSTEM_TASKS.md 6.3), lowest and at bits 71:68.
    hq = put(put(h, 0, "?"), 17, "?")
    out.append(("hex_q", "%h", hq, digits_value(hq, "h", w)))
    # Octal: digit 21 covers bits 65:63, digit 22 bits 68:66.
    o = ("7654321" * 12)[: (w + 2) // 3 + 1]
    o = put(put(o, 21, "x"), 22, "z")
    out.append(("oct", "%o", o, digits_value(o, "o", w)))
    # Binary: the top bit 1, bit 64 x (the top bit at 65), bit 63 z, bit 0
    # 1, the rest 0, and an _.
    bits = ["0"] * w
    bits[0], bits[w - 1 - 64], bits[w - 1 - 63], bits[w - 1] = "1", "x", "z", "1"
    b = "".join(bits)
    b = b[:8] + "_" + b[8:]
    out.append(("bin", "%b", b, digits_value(b, "b", w)))
    fits = (1 << (w - 1)) + 1234567890123456789
    out.append(("dec", "%d", str(fits), decimal_value(str(fits), w)))
    over = (1 << (w + 3)) + 987654321987654321
    out.append(("dec_over", "%d", str(over), decimal_value(str(over), w)))
    neg = "-" + str(fits)
    out.append(("dec_neg", "%d", neg, decimal_value(neg, w)))
    for name, text in [("dec_x", "x"), ("dec_z", "Z"), ("dec_q", "?")]:
        out.append((name, "%d", text, decimal_value(text, w)))
    out.append(("str_short", "%s", "Hi!", string_value("Hi!", w)))
    long = "The quick brown fox jumps over"
    out.append(("str_long", "%s", long.replace(" ", "_"),
                string_value(long.replace(" ", "_"), w)))
    return out


def tname(w, four, signed):
    return f"{'logic' if four else 'bit'}{' signed' if signed else ''} [{w - 1}:0]"


def files():
    decls, module, tasks, calls, exp = [], [], [], [], []
    task_exp = []
    for w in WIDTHS:
        for four, signed in TYPES:
            t = tname(w, four, signed)
            key = f"w{w}_{'4' if four else '2'}{'s' if signed else 'u'}"
            decls += [f"  {t} v_{key};", f"  {t} m_{key} [0:2];"]
            body = [f"  task automatic t_{key}();", f"    {t} v;", "    int n;"]
            for place in PLACES:
                target = {"module": f"v_{key}", "element": f"m_{key}[1]", "task": "v"}[place]
                lines = body if place == "task" else module
                for name, fmt, text, value in inputs(w):
                    tag = f"{place} {key} {name}"
                    lines.append(f'    n = $sscanf("{text}", "{fmt}", {target});')
                    lines.append(f'    $display("{tag} %0d %h", n, {target});')
                    want = f"{tag} 1 {hexdigits(value if four else to2(value))}"
                    (task_exp if place == "task" else exp).append(want)
            tasks += body + ["  endtask"]
            calls.append(f"    t_{key}();")
    head = [
        "/*",
        f":name: {NAME}",
        ":description: $sscanf into integral variables of 65, 100, 128 and 200 bits, two- "
        "and four-state, signed and unsigned: hexadecimal, octal and binary digits with x, z "
        "and ?, decimal numbers that fit and do not, negative ones, x, z and ?, and short and "
        "long strings, into a module variable, an element of an unpacked array and an "
        "automatic variable",
        ":tags: values wide",
        ":type: simulation",
        ":lrm: 1800-2023 21.3.4.3",
        ":oracle: spec",
        ":spec_check: ai",
        ":fourstate: yes",
        ":nondeterministic: no",
        f":generator: gen_scan_wide_matrix.py@{VERSION}",
        ":top_module: top",
        f":expected_stdout: @{NAME}.expected",
        "*/",
        "// Generated by tools/gen_scan_wide_matrix.py; change the generator, not this file.",
        "// Icarus Verilog 13.0 prints every four-state line as expected but those",
        "// with ?, which it reads as x; it keeps x and z in the two-state variables,",
        "// which hold 0 there (VALUE_SEMANTICS.md 1.2). Verilator 5.052 reads at most",
        "// 64 bits with %d.",
        "module top;",
        "  int n;",
    ]
    sv = head + decls + tasks + ["  initial begin"] + module + calls + ["  end", "endmodule", ""]
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
                print(f"gen_scan_wide_matrix: {path.relative_to(OUT.parent.parent.parent)} is "
                      "out of date; run tools/gen_scan_wide_matrix.py", file=sys.stderr)
                bad += 1
        else:
            path.write_text(text, encoding="utf-8")
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main())
