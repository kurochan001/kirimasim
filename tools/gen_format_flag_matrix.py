#!/usr/bin/env python3
# Copyright (c) 2026 The kirimasim Authors
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

"""Generates the matrix test of the format flags - and 0, which the
standard does not define (SYSTEM_TASKS.md 3.12), in tests/cli/.

  tools/gen_format_flag_matrix.py           # writes the files
  tools/gen_format_flag_matrix.py --check   # exit 1 if a file differs from what it would write

kirimasim gives the flags these meanings in %d, %h, %o, %b, %c, %s and %t
(3.12): - puts the value, as printed in the smallest width, at the left
of its field, the written width or else the automatic one (3.3, 3.5,
3.7), and spaces fill the rest; a written width that starts with 0 and is
not 0 pads %d with 0 after the sign, a lone x or z too, and changes
nothing in the other formats; - wins over 0. Each format that uses them
gets one warning at elaboration, at the format, naming the first such
flag, once however many instances lower it. The flags +, space and #, and
a precision, outside %e, %f and %g, are unsupported (exit code 2); slang
reports them itself in string literals, so they are tried in constant
formats of $sformat and $sformatf that are not literals.

The results of these flags are not in the test suite's expected values
(3.12), so the test lives here: tests/cli/run_cli_tests.py runs
format_flag_matrix.sv and checks code 0, standard output and the warning
lines, and runs format_flag_unsupported.sv and checks code 2 and the error
lines.

The axes: the format (%d, %h, %o, %b, %s, %t, and %c in formats that are
not literals, as slang rejects a width on %c in a literal); the flags and
width (none, -, a width, - with it, 0 with it, - and 0 with it, a width
narrower than the value with each flag, and - with the width 0); the value
(a positive and a negative int, an 8-bit unsigned, all x, all z, a partial
x, characters with and without a leading zero byte, a string, a real with
an integer format, the time as an integer and as a real); where the
format comes from (a literal of $display, $write, $strobe,
$sformat, $swrite and $sformatf, and a localparam of $sformatf); the
container (an initial procedure, an automatic task, a function, a fork
branch, and two instances of a module).

The expected outputs follow from the rules above, computed below. A
person checks them once for all the tests made from them (TEST_SUITE.md,
"Sampling by people").
"""

from __future__ import annotations

import argparse
import math
import sys
from pathlib import Path

OUT = Path(__file__).resolve().parent.parent / "tests" / "cli"
NAME = "format_flag_matrix"
UNSUPPORTED = "format_flag_unsupported"
# The version of the rules below. Raise it whenever they change.
VERSION = 1

TIME = 3  # every line prints at time 3ns, with the default $timeformat


class Value:
    """A value: its bits from the most significant, in 0, 1, x and z."""

    def __init__(self, decl, bits, signed=False, kind="int"):
        self.decl, self.bits, self.signed, self.kind = decl, bits, signed, kind

    @property
    def width(self):
        return len(self.bits)

    def number(self):
        n = int(self.bits, 2)
        if self.signed and self.bits[0] == "1":
            n -= 1 << self.width
        return n


def bits_of(n, width):
    return format(n & ((1 << width) - 1), f"0{width}b")


def text_bits(text, width):
    return "".join(format(ord(c), "08b") for c in text).rjust(width, "0")


# (name, value): the variables of the matrix.
VALUES = [
    ("p", Value("int p = 5;", bits_of(5, 32), signed=True)),
    ("n", Value("int n = -3;", bits_of(-3, 32), signed=True)),
    ("u", Value("logic [7:0] u = 8'd200;", bits_of(200, 8))),
    ("ch", Value('logic [7:0] ch = "A";', text_bits("A", 8))),
    ("xa", Value("logic [7:0] xa = 'x;", "x" * 8)),
    ("za", Value("logic [7:0] za = 'z;", "z" * 8)),
    ("px", Value("logic [7:0] px = 8'b0000_x101;", "0000x101")),
    ("s2", Value('logic [15:0] s2 = "ab";', text_bits("ab", 16))),
    ("s3", Value('logic [23:0] s3 = "hi";', text_bits("hi", 24))),
    ("str", Value('string str = "abc";', "", kind="string")),
    ("r", Value("real r = -2.0;", bits_of(-2, 64), signed=True)),
    ("$time", Value("", bits_of(TIME, 64), kind="time")),
    ("$realtime", Value("", bits_of(TIME, 64), kind="time")),
]
STRING_TEXT = {"str": "abc"}

# (flags, width): the forms of one specifier.
FORMS = [("", None), ("-", None), ("", 6), ("-", 6), ("0", 6), ("-0", 6), ("", 1), ("-", 1),
         ("0", 1), ("-", 0)]
# The values each format prints.
TABLE = {
    "d": ["p", "n", "u", "xa", "za", "px", "r"],
    "h": ["p", "n", "u", "xa", "px"],
    "o": ["p", "u", "za"],
    "b": ["u", "px"],
    "s": ["s2", "s3", "str"],
    "t": ["$time", "$realtime"],
}


def spec(conv, flags, width):
    w = "" if width is None else str(width)
    if "0" in flags:
        w = "0" + w
    return "%" + flags.replace("0", "") + w + conv


def digits(v, per):
    """%h, %o or %b digits of every group, most significant first (3.4)."""
    out = []
    for lo in range(0, v.width, per):
        g = v.bits[max(0, v.width - lo - per):v.width - lo]
        if set(g) == {"x"}:
            out.append("x")
        elif set(g) == {"z"}:
            out.append("z")
        elif "x" in g:
            out.append("X")
        elif "z" in g:
            out.append("Z")
        else:
            out.append("0123456789abcdef"[int(g, 2)])
    return "".join(reversed(out))


def smallest(conv, name, v):
    """The value as printed in the smallest width (%0d and the like)."""
    if conv == "d":
        if set(v.bits) <= {"x", "z"} and v.bits:
            return "x" if set(v.bits) == {"x"} else "z" if set(v.bits) == {"z"} else "X"
        if "x" in v.bits:
            return "X"
        if "z" in v.bits:
            return "Z"
        return str(v.number())
    if conv in "hob":
        d = digits(v, {"h": 4, "o": 3, "b": 1}[conv]).lstrip("0")
        return d or "0"
    if conv == "c":
        return chr(int(v.bits[-8:], 2))
    if conv == "s":
        if v.kind == "string":
            return STRING_TEXT[name]
        out = ""
        for k in range(0, v.width, 8):
            c = int(v.bits[k:k + 8], 2)
            if c == 0 and not out:
                continue
            out += " " if c == 0 else chr(c)
        return out
    return str(v.number())  # %t: ns with no decimals


def automatic(conv, v):
    """The automatic width (3.3, 3.5, 3.7)."""
    if conv == "d":
        if v.signed:
            return len(str(1 << (v.width - 1))) + 1
        return len(str((1 << v.width) - 1))
    if conv in "hob":
        return math.ceil(v.width / {"h": 4, "o": 3, "b": 1}[conv])
    if conv == "c":
        return 1
    if conv == "s":
        return 0 if v.kind == "string" else math.ceil(v.width / 8)
    return 20  # %t: the minimum width of the default $timeformat


def shown(conv, flags, width, name, v):
    small = smallest(conv, name, v)
    field = automatic(conv, v) if width is None else width
    if "-" in flags:
        return small.ljust(field)
    if conv == "d" and "0" in flags and width:
        if small.startswith("-"):
            return "-" + small[1:].rjust(field - 1, "0")
        return small.rjust(field, "0")
    if conv in "hob":
        if width is None:
            return digits(v, {"h": 4, "o": 3, "b": 1}[conv])
        return small.rjust(field, "0")
    return small.rjust(field)


def first_flag(conv_specs):
    """The warning's flag: the first - or 0 in the format."""
    for conv, flags, width in conv_specs:
        if "-" in flags:
            return f"'-' with %{conv}"
        if "0" in flags and width:
            return f"'0' with %{conv}"
    return None


class Writer:
    def __init__(self):
        self.lines, self.out, self.warn = [], [], []

    def add(self, text):
        self.lines.append(text)
        return len(self.lines)

    def warning(self, line, column, flag):
        if flag:
            self.warn.append(f"{NAME}.sv:{line}:{column}: warning: format flag {flag} is not in "
                             "IEEE 1800-2023")


def matrix_lines(w):
    """One $display for each format and value, every form of it."""
    for conv, names in TABLE.items():
        for name in names:
            v = dict(VALUES)[name]
            specs = [(conv, f, wd) for f, wd in FORMS]
            fmt = f"{conv} {name.lstrip('$')}:" + "".join(f"[{spec(*s)}]" for s in specs)
            args = ", ".join([name] * len(specs))
            text = f'    $display("{fmt}", {args});'
            line = w.add(text)
            w.warning(line, text.index('"') + 1, first_flag(specs))
            w.out.append(f"{conv} {name.lstrip('$')}:"
                         + "".join(f"[{shown(c, f, wd, name, v)}]" for c, f, wd in specs))


# The format the forms below share, and its values.
SHARED = [("d", "-", 6), ("d", "0", 6), ("h", "-", 6), ("s", "-", 4)]
SHARED_ARGS = "p, n, p, s2"


def shared_text():
    return "".join(f"[{spec(*s)}]" for s in SHARED)


def shared_out():
    vals = ["p", "n", "p", "s2"]
    return "".join(f"[{shown(c, f, wd, n, dict(VALUES)[n])}]"
                   for (c, f, wd), n in zip(SHARED, vals))


def task_lines(w):
    """The shared format through every task that takes a format."""
    fmt = shared_text()
    flag = first_flag(SHARED)
    forms = [
        ("display", f'    $display("display {fmt}", {SHARED_ARGS});'),
        ("write", f'    $write("write {fmt}\\n", {SHARED_ARGS});'),
        ("strobe", f'    $strobe("strobe {fmt}", {SHARED_ARGS});'),
        ("sformat", f'    $sformat(s, "sformat {fmt}", {SHARED_ARGS});'),
        ("swrite", f'    $swrite(s, "swrite {fmt}", {SHARED_ARGS});'),
        ("sformatf", f'    s = $sformatf("sformatf {fmt}", {SHARED_ARGS});'),
        ("localparam", f"    s = $sformatf(F, {SHARED_ARGS});"),
    ]
    strobes = []
    for name, text in forms:
        line = w.add(text)
        column = text.index("F,") + 1 if name == "localparam" else text.index('"') + 1
        w.warning(line, column, flag)
        if name in ("sformat", "swrite", "sformatf", "localparam"):
            w.add('    $display("%s", s);')
        result = f"{name} {shared_out()}"
        if name == "strobe":
            strobes.append(result)
        else:
            w.out.append(result)
    return strobes


CHARS = [("-", None), ("", 3), ("-", 3), ("0", 3), ("-0", 3)]


def char_lines(w):
    """%c, only in a format that is not a literal."""
    specs = [("c", f, wd) for f, wd in CHARS]
    line = w.add("    s = $sformatf(C, ch, ch, ch, ch, ch);")
    w.warning(line, len("    s = $sformatf(") + 1, first_flag(specs))
    w.add('    $display("c %s", s);')
    v = dict(VALUES)["ch"]
    w.out.append("c " + "".join(f"[{shown(c, f, wd, 'ch', v)}]" for c, f, wd in specs))
    return "".join(f"[{spec(*s)}]" for s in specs)


def files():
    w = Writer()
    head = [
        f"// Generated by tools/gen_format_flag_matrix.py (version {VERSION}); do not edit.",
        "`timescale 1ns/1ns",
        "module sub;",
        "  initial begin",
        "    #2;",
        '    $display("sub [%-4d]", 7);',
        "  end",
        "endmodule",
        "",
        "module top;",
    ]
    for line in head:
        w.add(line)
    w.warning(6, len('    $display(') + 1, "'-' with %d")
    sub_out = [f"sub [{'7'.ljust(4)}]"] * 2  # two instances, one warning
    for _, v in VALUES:
        if v.decl:
            w.add("  " + v.decl)
    w.add("  string s;")
    w.add(f'  localparam string F = "localparam {shared_text()}";')
    c_line = w.add("  localparam string C = \"\";")
    # The containers: each runs the shared format once.
    containers = []
    w.add("  task automatic in_task();")
    line = w.add(f'    $display("task {shared_text()}", {SHARED_ARGS});')
    containers.append((line, "task"))
    w.add("  endtask")
    w.add("  function automatic void in_function();")
    line = w.add(f'    $display("function {shared_text()}", {SHARED_ARGS});')
    containers.append((line, "function"))
    w.add("  endfunction")
    w.add("  sub i1();")
    w.add("  sub i2();")
    w.add("  initial begin")
    w.add("    #3;")
    w.out.extend(sub_out)
    matrix_lines(w)
    # A 0 before any -: the warning names it.
    zero_first = [("d", "0", 5), ("d", "-", 5)]
    text = f'    $display("zero first {"".join(f"[{spec(*z)}]" for z in zero_first)}", p, p);'
    w.warning(w.add(text), text.index('"') + 1, first_flag(zero_first))
    w.out.append("zero first " + "".join(f"[{shown(c, f, wd, 'p', dict(VALUES)['p'])}]"
                                         for c, f, wd in zero_first))
    strobes = task_lines(w)
    c_specs = char_lines(w)
    w.lines[c_line - 1] = f'  localparam string C = "{c_specs}";'
    w.add("    in_task();")
    w.out.append(f"task {shared_out()}")
    w.add("    in_function();")
    w.out.append(f"function {shared_out()}")
    w.add("    fork")
    line = w.add(f'      $display("fork {shared_text()}", {SHARED_ARGS});')
    containers.append((line, "fork"))
    w.out.append(f"fork {shared_out()}")
    w.add("    join")
    w.add("  end")
    w.add("endmodule")
    w.add("")
    for line, _ in containers:
        text = w.lines[line - 1]
        w.warning(line, text.index('"') + 1, first_flag(SHARED))
    w.out.extend(strobes)
    # In the order of the source; the test compares them in any order, as
    # the order of lowering is not a rule.
    warn = sorted(w.warn, key=lambda x: int(x.split(":")[1]))
    return {
        f"{NAME}.sv": "\n".join(w.lines),
        f"{NAME}.expected": "\n".join(w.out) + "\n",
        f"{NAME}.stderr": "\n".join(warn) + "\n",
    }


# (the flag or precision, the specifier, the message's subject)
BAD = [("+", "d", "format flag '+'"), (" ", "d", "format flag ' '"), ("#", "h", "format flag '#'"),
       ("+", "s", "format flag '+'"), (".3", "d", "format precision"),
       (".2", "t", "format precision")]


def unsupported_files():
    lines = [f"// Generated by tools/gen_format_flag_matrix.py (version {VERSION}); do not edit.",
             "`timescale 1ns/1ns", "module top;", "  string s;", "  int v = 5;"]
    err = []
    for k, (flag, conv, what) in enumerate(BAD):
        lines.append(f'  localparam string B{k} = "%{flag}{conv}";')
    for k, (flag, conv, what) in enumerate(BAD):
        task = "sformatf" if k % 2 == 0 else "sformat"
        text = (f"  initial s = $sformatf(B{k}, v);" if task == "sformatf"
                else f"  initial $sformat(s, B{k}, v);")
        lines.append(text)
        column = text.index(f"B{k},") + 1
        err.append(f"{UNSUPPORTED}.sv:{len(lines)}:{column}: error: unsupported: {what} with "
                   f"%{conv} (not implemented yet)")
    lines += ["endmodule", ""]
    return {f"{UNSUPPORTED}.sv": "\n".join(lines), f"{UNSUPPORTED}.stderr": "\n".join(err) + "\n"}


def main():
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("--check", action="store_true")
    args = ap.parse_args()
    bad = 0
    for name, text in {**files(), **unsupported_files()}.items():
        path = OUT / name
        if args.check:
            if not path.exists() or path.read_text(encoding="utf-8") != text:
                print(f"gen_format_flag_matrix: {path.relative_to(OUT.parent.parent)} is out of "
                      "date; run tools/gen_format_flag_matrix.py", file=sys.stderr)
                bad += 1
        else:
            path.write_text(text, encoding="utf-8")
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main())
