#!/usr/bin/env python3
# Copyright (c) 2026 The kirimasim Authors
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

"""Generates the matrix test of %t with $timeformat, of TEST_SUITE.md
("Mechanism matrix"), in tests/suite/proc/.

  tools/gen_time_format_matrix.py           # writes the files
  tools/gen_time_format_matrix.py --check   # exit 1 if a file differs from what it would write

%t takes its value as a time in the time unit of the design element that
calls it, converts it to the unit $timeformat set, rounds it to that
many digits after the decimal point, exactly half away from zero, adds
the suffix and pads it on the left to the minimum width (SYSTEM_TASKS.md
3.7, 5.4; 20.4.3). Without $timeformat, the unit is the smallest
precision of the design, with no digits, no suffix and a width of 20
(table 20-3). A width written in the specifier replaces the minimum
width, and %0t has none (3.7, an interpretation).

The axes: the time unit of the calling module (10ns, 1us, 1ps and 1ns;
the design's precision is 1ps); the value (an integer constant, an int
variable, a 64-bit variable, two reals, %0t and %12t of the int); the
$timeformat in force (the default, then five calls, one of them with a
unit finer than the precision); the container (a task and a function of
the module, which the top calls by hierarchical name).

The expected outputs follow from the rules above, computed below with
exact fractions. A person checks them once for all the tests made from
them (TEST_SUITE.md, "Sampling by people").

Icarus Verilog (devel f190dbb) agrees on every line except two exact
halves, which SYSTEM_TASKS.md 3.7 rounds away from zero: the real 2.5 of
the 1ps module under the default format (Icarus prints 2), and the real
1.75 of the top (1ns) in units of 10ns with two digits, 0.175 (Icarus
prints 0.17).
"""

from __future__ import annotations

import argparse
import sys
from fractions import Fraction
from pathlib import Path

OUT = Path(__file__).resolve().parent.parent / "tests" / "suite" / "proc"
NAME = "time_format_matrix"
# The version of the rules below (TEST_SUITE.md, :generator:). Raise it
# whenever they change.
VERSION = 1

PRECISION = -12
# (module, `timescale text, unit exponent)
MODULES = [("m10ns", "10ns/1ns", -8), ("m1us", "1us/1ns", -6), ("m1ps", "1ps/1ps", -12),
           ("top", "1ns/1ps", -9)]
# (name, $timeformat call or None, unit, digits, suffix, minimum width)
FORMATS = [
    ("default", None, PRECISION, 0, "", 20),
    ("ns3", '$timeformat(-9, 3, " ns", 12);', -9, 3, " ns", 12),
    ("fs0", '$timeformat(-15, 0, " fs", 0);', -15, 0, " fs", 0),
    ("us4", '$timeformat(-6, 4, "us", 0);', -6, 4, "us", 0),
    ("ps1", '$timeformat(-12, 1, "", 6);', -12, 1, "", 6),
    ("x10ns2", '$timeformat(-8, 2, " x10ns", 15);', -8, 2, " x10ns", 15),
]
# (the expression, its value, the width in the specifier: None for %t)
VALUES = [("7", Fraction(7), None), ("n", Fraction(7), None), ("q", Fraction(7), None),
          ("r1", Fraction(5, 2), None), ("r2", Fraction(7, 4), None), ("n", Fraction(7), 0),
          ("n", Fraction(7), 12)]
CONTAINERS = ["task", "function"]


def shown(value, unit, tf_unit, digits, suffix, width):
    x = value * Fraction(10) ** (unit - tf_unit) * Fraction(10) ** digits
    n = int(x)
    if x - n >= Fraction(1, 2):
        n += 1  # exactly half goes away from zero (all values here are positive)
    text = str(n) if digits == 0 else f"{n // 10 ** digits}.{n % 10 ** digits:0{digits}d}"
    return (text + suffix).rjust(width)


def files():
    spec = " ".join("[%t]" if w is None else f"[%{w}t]" for _, _, w in VALUES)
    args = ", ".join(e for e, _, _ in VALUES)
    lines, exp = [], []
    for fname, call, tf_unit, digits, suffix, minw in FORMATS:
        if call:
            lines.append(f"    {call}")
        for module, _, unit in MODULES:
            for container in CONTAINERS:
                tag = f"{fname} {module} {container}"
                callee = "show" if container == "task" else "fshow"
                target = callee if module == "top" else f"{module}_i.{callee}"
                lines.append(f'    {target}("{tag}");')
                parts = [shown(v, unit, tf_unit, digits, suffix, minw if w is None else w)
                         for _, v, w in VALUES]
                exp.append(f"{tag} " + " ".join(f"[{p}]" for p in parts))
    body_of = [
        "  int n = 7;",
        "  logic [63:0] q = 64'd7;",
        "  real r1 = 2.5, r2 = 1.75;",
        "  task automatic show(input string tag);",
        f'    $display("%s {spec}", tag, {args});',
        "  endtask",
        "  function void fshow(input string tag);",
        f'    $display("%s {spec}", tag, {args});',
        "  endfunction",
    ]
    head = [
        "/*",
        f":name: {NAME}",
        ":description: %t of integers and reals called from modules of time units 10ns, 1us, 1ps "
        "and 1ns, under the default format and five $timeformat calls, with %0t and a written "
        "width, from a task and a function",
        ":tags: proc display time",
        ":type: simulation",
        ":lrm: 1800-2023 21.2.1.1 20.4.3",
        ":oracle: spec",
        ":spec_check: ai",
        ":fourstate: no",
        ":nondeterministic: no",
        f":generator: gen_time_format_matrix.py@{VERSION}",
        ":top_module: top",
        f":expected_stdout: @{NAME}.expected",
        "*/",
        "// Generated by tools/gen_time_format_matrix.py; change the generator, not this file.",
        "// SYSTEM_TASKS.md 3.7 rounds an exact half away from zero. Icarus Verilog (devel f190dbb)",
        "// does not for the real 2.5 of m1ps under the default format (it prints 2) and for the",
        "// real 1.75 of top under x10ns2 (it prints 0.17).",
    ]
    sv = list(head)
    for module, ts, _ in MODULES:
        if module == "top":
            continue
        sv += [f"`timescale {ts}", f"module {module};"] + body_of + ["endmodule", ""]
    sv += ["`timescale 1ns/1ps", "module top;"]
    sv += [f"  {m} {m}_i();" for m, _, _ in MODULES if m != "top"]
    sv += body_of + ["  initial begin"] + lines + ["  end", "endmodule", ""]
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
                print(f"gen_time_format_matrix: {path.relative_to(OUT.parent.parent.parent)} is "
                      "out of date; run tools/gen_time_format_matrix.py", file=sys.stderr)
                bad += 1
        else:
            path.write_text(text, encoding="utf-8")
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main())
