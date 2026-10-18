#!/usr/bin/env python3
"""Generates the matrix test of comparing unpacked arrays and structures
of TEST_SUITE.md ("Mechanism matrix") in tests/suite/values/.

  tools/gen_aggregate_compare_matrix.py           # writes the files
  tools/gen_aggregate_compare_matrix.py --check   # exit 1 if a file differs from what it would write

==, !=, === and !== on unpacked arrays and structures compare the
corresponding elements and members with the same operator, nested ones
in turn; == is 0 when one of them is 0, else x when one is x, else 1;
=== is 1 when all are 1 (11.2.2, 7.4.6, VALUE_SEMANTICS.md 3.14).

The axes: the shape (an array, one with a descending range, a
two-dimensional array, a structure, a structure with an array member,
an array of structures); the right operand (a variable of the same
type, a typed assignment pattern); the values (equal, one element
different, an x where the other is known, an x and a difference, the
same x on both sides); the four operators.

The expected outputs follow from the rules above, computed below. A
person checks them once for all the tests made from them (TEST_SUITE.md,
"Sampling by people").
"""

from __future__ import annotations

import argparse
import sys
from pathlib import Path

OUT = Path(__file__).resolve().parent.parent / "tests" / "suite" / "values"
NAME = "aggregate_compare_matrix"
# The version of the rules below (TEST_SUITE.md, :generator:). Raise it
# whenever they change.
VERSION = 1

# A shape is a tree: ("leaf", width, four-state), ("array", [indices],
# shape) or ("struct", [(name, shape)]).
L4 = ("leaf", 4, True)
SHAPES = [
    ("array", "logic [3:0] {n} [0:2]", ("array", [0, 1, 2], L4)),
    ("array_down", "logic [3:0] {n} [2:0]", ("array", [2, 1, 0], L4)),
    ("array_2d", "logic [3:0] {n} [0:1][0:1]", ("array", [0, 1], ("array", [0, 1], L4))),
    ("struct", "struct { logic [3:0] a; int b; } {n}",
     ("struct", [("a", L4), ("b", ("leaf", 32, False))])),
    ("array_member", "struct { logic [3:0] a; logic [1:0] m [0:1]; } {n}",
     ("struct", [("a", L4), ("m", ("array", [0, 1], ("leaf", 2, True)))])),
    ("struct_array", "struct { logic [3:0] a; logic [3:0] b; } {n} [0:1]",
     ("array", [0, 1], ("struct", [("a", L4), ("b", L4)]))),
]
CASES = ["equal", "differ", "x_only", "x_and_differ", "same_x"]


def leaves(shape, path=""):
    """(path, width, four-state) of every leaf, in order."""
    kind = shape[0]
    if kind == "leaf":
        return [(path, shape[1], shape[2])]
    out = []
    if kind == "array":
        for i in shape[1]:
            out += leaves(shape[2], f"{path}[{i}]")
    else:
        for name, sub in shape[1]:
            out += leaves(sub, f"{path}.{name}")
    return out


def pattern(shape, values, at):
    """The assignment pattern of shape from values[at:]; returns (text, next)."""
    kind = shape[0]
    if kind == "leaf":
        return f"{shape[1]}'b{values[at]}", at + 1
    subs = shape[1] if kind == "struct" else [(None, shape[2]) for _ in shape[1]]
    parts = []
    for _, sub in subs:
        text, at = pattern(sub, values, at)
        parts.append(text)
    return "'{" + ", ".join(parts) + "}", at


def values_of(case, ls):
    """The left and right leaf values of a case."""
    left = [format((3 * k + 5) % (1 << w), f"0{w}b") for k, (_, w, _) in enumerate(ls)]
    right = list(left)
    first4 = next(k for k, (_, _, f) in enumerate(ls) if f)
    last = len(ls) - 1
    flip = lambda b: b[:-1] + ("1" if b[-1] == "0" else "0")
    xed = lambda b: "x" + b[1:]
    if case in ("differ", "x_and_differ"):
        right[last] = flip(right[last])
    if case in ("x_only", "x_and_differ", "same_x"):
        right[first4] = xed(right[first4])
    if case == "same_x":
        left[first4] = xed(left[first4])
    return left, right


def eq(a, b):
    if any(x in "01" and y in "01" and x != y for x, y in zip(a, b)):
        return "0"
    if any(c in "xz" for c in a + b):
        return "x"
    return "1"


def compare(left, right):
    elems = [eq(a, b) for a, b in zip(left, right)]
    e = "0" if "0" in elems else "x" if "x" in elems else "1"
    ne = {"0": "1", "1": "0", "x": "x"}[e]
    ceq = "1" if left == right else "0"
    return e, ne, ceq, "0" if ceq == "1" else "1"


def files():
    decl, body, exp = [], [], []
    # A typedef of an unpacked array: the dimensions follow the name.
    for name, d, _ in SHAPES:
        head, _, dims = d.partition(" {n}")
        decl.append(f"  typedef {head} {name}_t{dims};")
        decl.append(f"  {name}_t l_{name}, r_{name};")
    for name, _, shape in SHAPES:
        ls = leaves(shape)
        for case in CASES:
            left, right = values_of(case, ls)
            sets = [f"l_{name}{p} = {w}'b{v};" for (p, w, _), v in zip(ls, left)]
            expect = " ".join(compare(left, right))
            for form in ("variable", "pattern"):
                st = list(sets)
                if form == "variable":
                    st += [f"r_{name}{p} = {w}'b{v};" for (p, w, _), v in zip(ls, right)]
                    rhs = f"r_{name}"
                else:
                    rhs = f"{name}_t{pattern(shape, right, 0)[0]}"
                ops = ", ".join(f"l_{name} {op} {rhs}" for op in ("==", "!=", "===", "!=="))
                tag = f"{name} {case} {form}"
                st.append(f'$display("{tag} %b %b %b %b", {ops});')
                body.append(" ".join(st))
                exp.append(f"{tag} {expect}")
    head = [
        "/*",
        f":name: {NAME}",
        ":description: ==, !=, === and !== on unpacked arrays (ascending, descending, two "
        "dimensions), structures, a structure with an array member and an array of structures, "
        "against a variable and a typed assignment pattern, for equal values, a difference, an "
        "x, an x and a difference, and the same x on both sides",
        ":tags: values aggregates",
        ":type: simulation",
        ":lrm: 1800-2023 7.4.6 11.2.2 11.4.5",
        ":oracle: spec",
        ":spec_check: ai",
        ":fourstate: yes",
        ":nondeterministic: no",
        f":generator: gen_aggregate_compare_matrix.py@{VERSION}",
        ":top_module: top",
        f":expected_stdout: @{NAME}.expected",
        "*/",
        "// Generated by tools/gen_aggregate_compare_matrix.py; change the generator, not this "
        "file.",
        "`timescale 1ns/1ns",
        "module top;",
    ]
    sv = head + decl + ["  initial begin"] + ["    " + s for s in body] + ["  end", "endmodule",
                                                                      ""]
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
                print(f"gen_aggregate_compare_matrix: "
                      f"{path.relative_to(OUT.parent.parent.parent)} is out of date; run "
                      "tools/gen_aggregate_compare_matrix.py", file=sys.stderr)
                bad += 1
        else:
            path.write_text(text, encoding="utf-8")
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main())
