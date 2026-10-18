#!/usr/bin/env python3
"""Generates the matrix test of the math system functions on integral
arguments wider than 64 bits, of TEST_SUITE.md ("Mechanism matrix"), in
tests/suite/values/.

  tools/gen_math_wide_matrix.py           # writes the files
  tools/gen_math_wide_matrix.py --check   # exit 1 if a file differs from what it would write

$clog2 gives the ceiling of the base-2 logarithm of its argument, taken
as unsigned, and 0 for 0 and 1 (20.8.1), and x for an argument with x
or z (SYSTEM_TASKS.md 8). The real functions convert an integral
argument to real first, as an assignment does: signed or unsigned by its
type, x and z bits taken as 0, and a value a real cannot hold exactly
rounded to the nearest one, ties to the even one (20.8.2,
VALUE_SEMANTICS.md 1.5). All of it holds at any width.

The axes: the width (65, 100, 128, 200), two- and four-state, signed and
unsigned; the argument (0, 1, 2, 2**64, 2**64 + 1, the top bit alone,
every bit 1, 2**64 + 2**11 and 2**64 + 3 * 2**11, which lie halfway
between two reals, 2**64 + 2**11 + 1 just above halfway, and in a
four-state value x and z in the top bit over 1); the function ($clog2, $sqrt, $ln, $log10, $floor, $ceil, $pow with
0.5); the container (an initial procedure and an automatic task). Reals
print with %e, six digits, which no difference in the last bits of a
math library changes here; $floor prints once more with %0.1f, every
digit of the real the argument converts to.

The expected outputs follow from the rules above, computed below. For
reference (checked 2026-10-18): Icarus Verilog 13.0 prints every line as
expected, and Verilator 5.052 every line of the two-state variables but
those printing every digit of 2**64 + 2**11 + 1, which it rounds as if
the value were halfway.
"""

from __future__ import annotations

import argparse
import math
import sys
from pathlib import Path

OUT = Path(__file__).resolve().parent.parent / "tests" / "suite" / "values"
NAME = "math_wide_matrix"
# The version of the rules below (TEST_SUITE.md, :generator:). Raise it
# whenever they change.
VERSION = 2
WIDTHS = [65, 100, 128, 200]
TYPES = [(four, signed) for four in (True, False) for signed in (False, True)]
CONTAINERS = ["initial", "task"]
# (name, the call with V for the argument, the function, the format)
FUNCS = [
    ("sqrt", "$sqrt(V)", math.sqrt, "%e"),
    ("ln", "$ln(V)", math.log, "%e"),
    ("log10", "$log10(V)", math.log10, "%e"),
    ("floor", "$floor(V)", math.floor, "%e"),
    ("ceil", "$ceil(V)", math.ceil, "%e"),
    ("pow", "$pow(V, 0.5)", lambda r: r ** 0.5, "%e"),
    # Every digit of the real the argument converts to.
    ("floor_digits", "$floor(V)", math.floor, "%0.1f"),
]


def clog2(n):
    return 0 if n <= 1 else (n - 1).bit_length()


def to_real(n, w, signed):
    """The real that the w-bit value n converts to: the nearest, ties to
    the even one, as Python's conversion rounds."""
    if signed and n >> (w - 1):
        n -= 1 << w
    return float(n)


def args(w, four, signed):
    """(name, the literal, the value as unsigned, the real it converts to)."""
    top = 1 << (w - 1)
    every = (1 << w) - 1
    out = []
    for name, n in [("zero", 0), ("one", 1), ("two", 2), ("p64", 1 << 64),
                    ("p64p1", (1 << 64) + 1), ("top", top), ("ones", every),
                    ("tie", (1 << 64) + (1 << 11)), ("tie3", (1 << 64) + (3 << 11)),
                    ("above_tie", (1 << 64) + (1 << 11) + 1)]:
        out.append((name, f"{w}'h{n:x}", n, to_real(n, w, signed)))
    if four:
        # x and z in the top bit count as 0 for a real.
        out.append(("topx", f"{w}'b{'x' + '0' * (w - 2)}1", "x", 1.0))
        out.append(("topz", f"{w}'b{'z' + '0' * (w - 2)}1", "x", 1.0))
    return out


def real_text(f, r, fmt):
    """f(r) printed with fmt, or None where the function has no real
    value."""
    if r is None:
        return None
    try:
        v = f(r)
    except ValueError:
        return None
    if isinstance(v, int):
        v = float(v)
    if r < 0 and f is not math.floor and f is not math.ceil:
        return None
    if r == 0 and f in (math.log, math.log10):
        return None
    return fmt % v


def tname(w, four, signed):
    return f"{'logic' if four else 'bit'}{' signed' if signed else ''} [{w - 1}:0]"


def files():
    decls, initial, task, exp, task_exp = [], [], [], [], []
    for w in WIDTHS:
        for four, signed in TYPES:
            key = f"w{w}_{'4' if four else '2'}{'s' if signed else 'u'}"
            decls.append(f"  {tname(w, four, signed)} v_{key};")
            task.append(f"    begin\n      {tname(w, four, signed)} v;")
            for name, lit, unsigned, real in args(w, four, signed):
                for container in CONTAINERS:
                    var = f"v_{key}" if container == "initial" else "v"
                    ind = "    " if container == "initial" else "      "
                    lines = initial if container == "initial" else task
                    out = exp if container == "initial" else task_exp
                    lines.append(f"{ind}{var} = {lit};")
                    tag = f"{container} {key} {name} clog2"
                    lines.append(f'{ind}$display("{tag} %0d", $clog2({var}));')
                    out.append(f"{tag} {unsigned if unsigned == 'x' else clog2(unsigned)}")
                    for fname, text, f, fmt in FUNCS:
                        want = real_text(f, real, fmt)
                        if want is None:
                            continue
                        tag = f"{container} {key} {name} {fname}"
                        lines.append(f'{ind}$display("{tag} {fmt}", {text.replace("V", var)});')
                        out.append(f"{tag} {want}")
            task.append("    end")
    head = [
        "/*",
        f":name: {NAME}",
        ":description: $clog2 and the real math functions on integral arguments of 65, 100, "
        "128 and 200 bits, two- and four-state, signed and unsigned, with x and z taken as 0, "
        "in an initial procedure and an automatic task",
        ":tags: values wide",
        ":type: simulation",
        ":lrm: 1800-2023 20.8.1 20.8.2 6.12.1",
        ":oracle: spec",
        ":spec_check: ai",
        ":fourstate: yes",
        ":nondeterministic: no",
        f":generator: gen_math_wide_matrix.py@{VERSION}",
        ":top_module: top",
        f":expected_stdout: @{NAME}.expected",
        "*/",
        "// Generated by tools/gen_math_wide_matrix.py; change the generator, not this file.",
        "module top;",
    ]
    sv = head + decls + ["  task automatic run();"] + task + ["  endtask"]
    sv += ["  initial begin"] + initial + ["    run();", "  end", "endmodule", ""]
    return {f"{NAME}.sv": "\n".join(sv), f"{NAME}.expected": "\n".join(exp + task_exp) + "\n"}


def main():
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("--check", action="store_true")
    args_ = ap.parse_args()
    bad = 0
    for name, text in files().items():
        path = OUT / name
        if args_.check:
            if not path.exists() or path.read_text(encoding="utf-8") != text:
                print(f"gen_math_wide_matrix: {path.relative_to(OUT.parent.parent.parent)} is "
                      "out of date; run tools/gen_math_wide_matrix.py", file=sys.stderr)
                bad += 1
        else:
            path.write_text(text, encoding="utf-8")
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main())
