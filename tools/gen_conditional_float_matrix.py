#!/usr/bin/env python3
# Copyright (c) 2026 The kirimasim Authors
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

"""Generates the matrix tests of the result type of a conditional
expression with shortreal and real operands (TEST_SUITE.md, "Mechanism
matrix") in tests/suite/values/.

  tools/gen_conditional_float_matrix.py           # writes the files
  tools/gen_conditional_float_matrix.py --check   # exit 1 if a file differs from what it would write

The rule: 11.4.11 of the standard. When one branch is real, the other
(shortreal or integral) is cast to real and the result is real; when one
is shortreal and the other integral, the integral one is cast to
shortreal and the result is shortreal. The type of the expression does
not depend on the left-hand side (11.8.1); an assignment then converts
the result to the target (a shortreal is a single-precision value,
6.12; a real becomes an integer by rounding, halves away from zero,
6.12.1). A $display argument is self-determined, so it prints the
conditional's own value. With an ambiguous condition and branches that
differ, the result is the value of Table 7-1 (7.4.5) for the result
type, 0.0.

The axes: the pair of branches (variables of shortreal, real and int
and an integer literal; 16777217 is 2**24 + 1, which a shortreal cannot
hold, and 16777217.5 needs a real), the condition (1, 0 and x), the
target (a real, a shortreal and an int variable, and the
self-determined $display argument), and the place: an initial
procedure, an automatic function, a class method, a fork branch, actual
arguments of functions, continuous assignments to variables, operands
that are all localparams, and operands and a condition that are all
literals (a shortreal one written as a cast), which slang folds into one
value when it binds the expression.

Left out, because the standard can be read two ways:
  - Branches that are expressions rather than simple operands. The real
    exception of 11.8.2 treats a context-determined operand that is not
    real as self-determined; whether a shortreal is "not real" there
    decides the precision of arithmetic inside a branch, or of a
    conditional inside real arithmetic.
  - An ambiguous condition whose branches are equal only after the
    conversion to the result type (16777216.0 and 16777217 against a
    shortreal): "logically equivalent" in 11.4.11 does not say whether
    it compares before or after it. The generator drops such rows.
  - A localparam that holds the conditional itself: slang evaluates it
    at elaboration, and kirimasim uses that value (issue 125).

The expected output is computed here from the rules, without reading
kirimasim's code. A person checks it once for all the tests made from
it (TEST_SUITE.md, "Sampling by people").
"""

import argparse
import math
import struct
import sys
from pathlib import Path

OUT = Path(__file__).resolve().parent.parent / "tests" / "suite" / "values"
# The version of the rules below (TEST_SUITE.md, :generator:). Raise it
# whenever they change.
VERSION = 1

# name: (SystemVerilog type, kind, value, the value as SystemVerilog).
OPERANDS = {
    "s": ("shortreal", "s", 16777216.0, "16777216.0"),
    "s2": ("shortreal", "s", 3.0, "3.0"),
    "r": ("real", "r", 16777217.5, "16777217.5"),
    "r2": ("real", "r", 0.5, "0.5"),
    "i": ("int", "i", 16777217, "16777217"),
    "i3": ("int", "i", 3, "3"),
}
LITERAL = "16777217"
PAIRS = [("s", "i"), ("i", "s"), ("s", "r"), ("r", "s"), ("r", "i"), ("i", "r"),
         ("s", "s2"), ("r", "r2"), ("s2", "i3"), ("i3", "s2"), ("s", LITERAL), (LITERAL, "s")]
CONDITIONS = [("1", "1'b1"), ("0", "1'b0"), ("x", "1'bx")]
PLACES = ["initial", "function", "class", "fork", "argument", "continuous", "constant",
          "literal"]


def f32(v):
    return struct.unpack("<f", struct.pack("<f", float(v)))[0]


def kind(name):
    return "i" if name == LITERAL else OPERANDS[name][1]


def value(name):
    return 16777217 if name == LITERAL else OPERANDS[name][2]


def to_kind(v, k):
    return f32(v) if k == "s" else float(v)


def to_int(v):
    # 6.12.1: to the nearest integer, halves away from zero.
    return int(math.floor(abs(v) + 0.5)) * (1 if v >= 0 else -1)


def result(a, b, cond):
    """The conditional's value, or None where the standard reads two ways."""
    rk = "r" if "r" in (kind(a), kind(b)) else "s"
    va, vb = to_kind(value(a), rk), to_kind(value(b), rk)
    if cond == "1":
        return va
    if cond == "0":
        return vb
    before, after = value(a) == value(b), va == vb
    if before != after:
        return None
    return va if after else 0.0


def label(name):
    return "lit" if name == LITERAL else name


def rows():
    out = []
    for cl, _ in CONDITIONS:
        for a, b in PAIRS:
            v = result(a, b, cl)
            if v is not None:
                out.append((cl, a, b, v))
    return out


def expected():
    return [f"{cl} {label(a)}:{label(b)} dr={float(v):.1f} ds={f32(v):.1f} di={to_int(v)} "
            f"sd={v:.1f}" for cl, a, b, v in rows()]


def operand_text(name, mode):
    """The operand in a test: a variable, a localparam or a literal."""
    if name == LITERAL:
        return LITERAL
    if mode == "param":
        return name.upper()
    if mode == "literal":
        sv_type, _, _, text = OPERANDS[name]
        return f"shortreal'({text})" if sv_type == "shortreal" else text
    return name


def display(cl, a, b, cond_text, dr, ds, di, sd):
    return (f'$display("{cl} {label(a)}:{label(b)} dr=%.1f ds=%.1f di=%0d sd=%.1f", '
            f"{dr}, {ds}, {di}, {sd});")


def declarations(indent, constant=False):
    lines = []
    for name, (sv_type, _, _, text) in OPERANDS.items():
        if constant:
            lines.append(f"{indent}localparam {sv_type} {name.upper()} = {text};")
        else:
            lines.append(f"{indent}{sv_type} {name} = {text};")
    return lines


def body(indent, cond_text, cl, mode="var", through=None):
    """The statements of one condition value: three assignments and a display per pair."""
    lines = []
    for c2, a, b, _ in rows():
        if c2 != cl:
            continue
        e = f"{cond_text} ? {operand_text(a, mode)} : {operand_text(b, mode)}"
        if through:
            lines += [f"{indent}dr = fr({e});", f"{indent}ds = fs({e});", f"{indent}di = fi({e});"]
        else:
            lines += [f"{indent}dr = {e};", f"{indent}ds = {e};", f"{indent}di = {e};"]
        lines.append(indent + display(cl, a, b, cond_text, "dr", "ds", "di", e))
    return lines


def test(place):
    name = f"conditional_float_{place}"
    where = {"initial": "an initial procedure", "function": "an automatic function",
             "class": "a class method", "fork": "a fork branch",
             "argument": "actual arguments of functions",
             "continuous": "continuous assignments to variables",
             "constant": "operands that are all localparams",
             "literal": "operands and a condition that are all literals"}[place]
    sv = ["/*", f":name: {name}",
          ":description: a conditional with a real branch is real and one with a shortreal and an "
          "integral branch is shortreal, converted to real, shortreal and int targets, with "
          f"conditions 1, 0 and x, in {where}",
          ":tags: values", ":type: simulation", ":lrm: 1800-2023 11.4.11 11.8.1 6.12 6.12.1 7.4.5",
          ":oracle: spec", ":spec_check: ai", ":fourstate: yes", ":nondeterministic: no",
          f":generator: gen_conditional_float_matrix.py@{VERSION}", ":top_module: top",
          ":issue: 61", f":expected_stdout: @{name}.expected", "*/",
          "// Generated by tools/gen_conditional_float_matrix.py; change the generator, not "
          "this file.", "`timescale 1ns/1ns"]
    targets = ["real dr;", "shortreal ds;", "int di;"]
    if place in ("initial", "fork", "argument"):
        sv += ["module top;", "  logic c;"] + declarations("  ") + [f"  {t}" for t in targets]
        if place == "argument":
            sv += ["  function automatic real fr(real v); return v; endfunction",
                   "  function automatic shortreal fs(shortreal v); return v; endfunction",
                   "  function automatic int fi(int v); return v; endfunction"]
        sv += ["  initial begin"]
        for cl, ct in CONDITIONS:
            sv.append(f"    c = {ct};")
            if place == "fork":
                sv += ["    fork", "      begin"]
                sv += body("        ", "c", cl)
                sv += ["      end", "    join"]
            else:
                sv += body("    ", "c", cl, through=(place == "argument"))
        sv += ["  end", "endmodule"]
    elif place == "function":
        sv += ["module top;", "  function automatic void run(logic c);"]
        sv += declarations("    ") + [f"    {t}" for t in targets]
        sv += ["    case (c)"]
        for cl, ct in CONDITIONS:
            sv.append(f"      {ct}: begin")
            sv += body("        ", "c", cl)
            sv.append("      end")
        sv += ["    endcase", "  endfunction", "  initial begin"]
        sv += [f"    run({ct});" for _, ct in CONDITIONS]
        sv += ["  end", "endmodule"]
    elif place == "class":
        sv += ["class K;"] + declarations("  ") + [f"  {t}" for t in targets]
        sv += ["  function void run(logic c);", "    case (c)"]
        for cl, ct in CONDITIONS:
            sv.append(f"      {ct}: begin")
            sv += body("        ", "c", cl)
            sv.append("      end")
        sv += ["    endcase", "  endfunction", "endclass", "module top;", "  K k;",
               "  initial begin", "    k = new;"]
        sv += [f"    k.run({ct});" for _, ct in CONDITIONS]
        sv += ["  end", "endmodule"]
    elif place == "continuous":
        sv += ["module top;", "  logic c;"] + declarations("  ")
        for k, (a, b) in enumerate(PAIRS):
            e = f"c ? {operand_text(a, 'var')} : {operand_text(b, 'var')}"
            sv += [f"  real dr{k};", f"  shortreal ds{k};", f"  int di{k};",
                   f"  assign dr{k} = {e};", f"  assign ds{k} = {e};", f"  assign di{k} = {e};"]
        sv += ["  initial begin"]
        for cl, ct in CONDITIONS:
            sv += [f"    c = {ct};", "    #1;"]
            for c2, a, b, _ in rows():
                if c2 != cl:
                    continue
                k = PAIRS.index((a, b))
                e = f"c ? {operand_text(a, 'var')} : {operand_text(b, 'var')}"
                sv.append("    " + display(cl, a, b, "c", f"dr{k}", f"ds{k}", f"di{k}", e))
        sv += ["  end", "endmodule"]
    elif place == "constant":
        sv += ["module top;"] + declarations("  ", constant=True)
        sv += ["  localparam logic C1 = 1'b1, C0 = 1'b0, CX = 1'bx;"]
        sv += [f"  {t}" for t in targets] + ["  initial begin"]
        for cl, _ in CONDITIONS:
            sv += body("    ", {"1": "C1", "0": "C0", "x": "CX"}[cl], cl, mode="param")
        sv += ["  end", "endmodule"]
    else:  # literal
        sv += ["module top;"] + [f"  {t}" for t in targets] + ["  initial begin"]
        for cl, ct in CONDITIONS:
            sv += body("    ", ct, cl, mode="literal")
        sv += ["  end", "endmodule"]
    return name, sv


def files():
    out = {}
    lines = "\n".join(expected()) + "\n"
    for place in PLACES:
        name, sv = test(place)
        out[f"{name}.sv"] = "\n".join(sv) + "\n"
        out[f"{name}.expected"] = lines
    return out


def main():
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("--check", action="store_true")
    args = ap.parse_args()
    bad = 0
    for name, text in files().items():
        path = OUT / name
        if args.check:
            if not path.exists() or path.read_text(encoding="utf-8") != text:
                print("gen_conditional_float_matrix: "
                      f"{path.relative_to(OUT.parent.parent.parent)} is out of date; run "
                      "tools/gen_conditional_float_matrix.py", file=sys.stderr)
                bad += 1
        else:
            path.write_text(text, encoding="utf-8")
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main())
