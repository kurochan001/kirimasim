#!/usr/bin/env python3
# Copyright (c) 2026 The kirimasim Authors
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

"""Generates the string matrix test of TEST_SUITE.md ("Mechanism matrix") in
tests/suite/values/.

  tools/gen_string_matrix.py           # writes the files
  tools/gen_string_matrix.py --check   # exit 1 if a file differs from what it would write

The rules: VALUE_SEMANTICS.md 7.3 and 7.6 and 6.16 of the standard. An
integral value becomes the string of its bytes, most significant first,
with zero bytes dropped (x and z bits read as 0, 1.2); a string becomes
an integral value with its last character in the low byte, cut or
extended with 0 to the width. Concatenation and replication take a count
known only at run time (0 gives ""). s[i] out of range reads 0, and a
write of 0 or out of range changes nothing. The methods: len, putc and
getc (out of range: no change, 0), toupper, tolower, compare and
icompare (-1, 0, 1), substr (out of range: ""), atoi, atohex, atooct and
atobin (digits and _ only, no sign), atoreal, itoa, hextoa (lower case),
octtoa, bintoa and realtoa (%g). The axes: the operation, values at and
past the ends, widths of one and of two words, and the container (module
variables and automatic variables of a function).

A person checks the expected output once for all the tests made from it
(TEST_SUITE.md, "Sampling by people").
"""

import argparse
import sys
from pathlib import Path

OUT = Path(__file__).resolve().parent.parent / "tests" / "suite" / "values"
NAME = "string_matrix"
# The version of the rules below (TEST_SUITE.md, :generator:). Raise it
# whenever they change.
VERSION = 1


def int_to_string(value, width):
    data = value.to_bytes((width + 7) // 8, "big")
    return "".join(chr(b) for b in data if b != 0)


def string_to_int(s, width):
    n = 0
    for c in s.encode():
        n = (n << 8) | c
    return n & ((1 << width) - 1)


def digits(s, base, chars):
    n, seen = 0, False
    for c in s:
        if c == "_":
            continue
        if c.lower() not in chars:
            break
        n = n * base + int(c, base)
        seen = True
    return n if seen else 0


def compare(a, b):
    return (a > b) - (a < b)


def body(prefix, pad):
    st, ex = [], []

    def line(tag, fmt, args, text):
        st.append(f"{pad}$display(\"{prefix} {tag} {fmt}\", {args});")
        ex.append(f"{prefix} {tag} {text}")

    # Integer to string at run time: zero bytes dropped, x read as 0.
    for k, (lit, value, width) in enumerate([
            ("32'h48_00_69_21", 0x48006921, 32),
            ("32'h0000_4142", 0x4142, 32),
            (f"100'h{int.from_bytes(b'Hello, wide!', 'big'):x}",
             int.from_bytes(b"Hello, wide!", "big"), 100),
            ("16'h41xx", 0x4100, 16),
            ("32'h0", 0, 32)]):
        st += [f"{pad}v{width} = {lit};", f"{pad}s = string'(v{width});"]
        text = int_to_string(value, width)
        line(f"itos{k}", "[%s] %0d", "s, s.len()", f"[{text}] {len(text)}")
    # String to integer: the last character in the low byte.
    for k, text in enumerate(["AB", "ABCDEFG", ""]):
        st += [f"{pad}s = \"{text}\";", f"{pad}v32 = w32_t'(s);", f"{pad}v100 = w100_t'(s);"]
        line(f"stoi{k}", "%h %h", "v32, v100",
             f"{string_to_int(text, 32):08x} {string_to_int(text, 100):025x}")
    # Concatenation and replication with run-time counts.
    st += [f"{pad}a = \"ab\"; b = \"\"; c = \"Cd\";"]
    line("cat", "[%s]", "{a, b, c, \"!\"}", "[abCd!]")
    for n in (0, 1, 3):
        st.append(f"{pad}n = {n};")
        line(f"rep{n}", "[%s]", "{n{c}}", f"[{'Cd' * n}]")
    # Indexing.
    st += [f"{pad}s = \"hello\";", f"{pad}i = 1;"]
    line("index", "%0d %0d %0d", "s[i], s[9], s[-1]", f"{ord('e')} 0 0")
    st += [f"{pad}s[0] = \"j\"; s[9] = \"z\"; s[2] = 8'd0;"]
    line("indexw", "[%s]", "s", "[jello]")
    # Methods.
    st += [f"{pad}s = \"Hello\";", f"{pad}s.putc(1, \"a\"); s.putc(7, \"x\"); s.putc(2, 8'd0);"]
    line("putc", "[%s] %0d %0d", "s, s.getc(0), s.getc(5)", f"[Hallo] {ord('H')} 0")
    line("case", "[%s] [%s]", "s.toupper(), s.tolower()", "[HALLO] [hallo]")
    st += [f"{pad}a = \"abc\"; b = \"abd\"; c = \"ABC\";"]
    line("compare", "%0d %0d %0d %0d %0d", "a.compare(b), b.compare(a), a.compare(a), "
         "a.icompare(c), a.compare(c)", f"-1 1 0 0 {compare('abc', 'ABC')}")
    line("relational", "%b%b%b%b%b%b", "a < b, a <= a, a > c, a >= b, a == a, a != c",
         "111011")
    st += [f"{pad}s = \"substring\";"]
    line("substr", "[%s] [%s] [%s] [%s]", "s.substr(3, 5), s.substr(0, 0), s.substr(4, 2), "
         "s.substr(5, 9)", "[str] [s] [] []")
    for k, (text, fn, base, chars) in enumerate([
            ("123_45ab", "atoi", 10, "0123456789"), ("-5", "atoi", 10, "0123456789"),
            ("1fZz", "atohex", 16, "0123456789abcdef"), ("178", "atooct", 8, "01234567"),
            ("1012", "atobin", 2, "01"), ("", "atoi", 10, "0123456789")]):
        st.append(f"{pad}s = \"{text}\";")
        line(f"{fn}{k}", "%0d", f"s.{fn}()", str(digits(text, base, chars)))
    st.append(f"{pad}s = \"2.5e3x\";")
    line("atoreal", "%f", "s.atoreal()", "2500.000000")
    st += [f"{pad}s.itoa(-42); a = s;", f"{pad}s.hextoa(255); b = s;", f"{pad}s.octtoa(8); c = s;",
           f"{pad}s.bintoa(5);"]
    line("toa", "[%s] [%s] [%s] [%s]", "a, b, c, s", "[-42] [ff] [10] [101]")
    st += [f"{pad}s.realtoa(2.5);"]
    line("realtoa", "[%s]", "s", "[2.5]")
    return st, ex


DECLS = ["string s, a, b, c;", "int n, i;", "logic [31:0] v32;", "logic [15:0] v16;",
         "logic [99:0] v100;"]


def files():
    head = [
        "/*",
        f":name: {NAME}",
        ":description: strings at run time: integer and string conversions at one and two "
        "words, concatenation and replication with run-time counts, indexing and the string "
        "methods at and past their ends, comparisons, in module and automatic variables",
        ":tags: values string",
        ":type: simulation",
        ":lrm: 1800-2023 6.16 11.4.12.2",
        ":oracle: spec",
        ":spec_check: ai",
        ":fourstate: yes",
        ":nondeterministic: no",
        f":generator: gen_string_matrix.py@{VERSION}",
        f":expected_stdout: @{NAME}.expected",
        "*/",
        "// Generated by tools/gen_string_matrix.py; change the generator, not this file.",
        "// Icarus Verilog 14.0 (devel) f190dbb rejects a string replication with a count known",
        "// only at run time and the method putc, which table 6-9 and 6.16.2 allow.",
        "module top;",
        "  typedef bit [31:0] w32_t;",
        "  typedef bit [99:0] w100_t;",
    ] + ["  " + d for d in DECLS]
    mst, mex = body("module", "    ")
    fst, fex = body("function", "    ")
    func = ["  function automatic void auto();"] + ["    " + d for d in DECLS] + fst + ["  endfunction"]
    sv = head + func + ["  initial begin"] + mst + ["    auto();", "  end", "endmodule", ""]
    return {f"{NAME}.sv": "\n".join(sv), f"{NAME}.expected": "\n".join(mex + fex) + "\n"}


def main():
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("--check", action="store_true")
    args = ap.parse_args()
    bad = 0
    for name, text in files().items():
        path = OUT / name
        if args.check:
            if not path.exists() or path.read_text(encoding="utf-8") != text:
                print(f"gen_string_matrix: {path.relative_to(OUT.parent.parent.parent)} is out of "
                      "date; run tools/gen_string_matrix.py", file=sys.stderr)
                bad += 1
        else:
            path.write_text(text, encoding="utf-8")
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main())
