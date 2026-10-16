#!/usr/bin/env python3
"""Generates the aggregate assignment matrix test of TEST_SUITE.md ("機構の
行列") in tests/suite/values/.

  tools/gen_aggregate_assign_matrix.py           # writes the files
  tools/gen_aggregate_assign_matrix.py --check   # exit 1 if a file differs from what it would write

An assignment to an unpacked array or structure computes the whole value
of its right side before it writes any element or member
(VALUE_SEMANTICS.md 3.13; 10.4.1, 10.4.2, 11.2.2), so a right side that
reads the target sees the old values. Each case starts the target from
known values and assigns it a pattern or a part of itself that reads it:
positional, keyed, default and replicated array patterns, a descending
range, sub-arrays of a two-dimensional array, nested patterns, a row of a
two-dimensional array; positional, named and default structure patterns,
members that are structures and arrays; with = and <=; in an initial
procedure, a static task, an always procedure and a branch of a fork.
Every case takes one time step, so the containers run one after another
and each line shows the target after the assignment.

The expected outputs follow from the old values by the rule above. A
person checks them once for all the tests made from them (TEST_SUITE.md,
"人の抜き取り").
"""

import argparse
import sys
from pathlib import Path

OUT = Path(__file__).resolve().parent.parent / "tests" / "suite" / "values"
NAME = "aggregate_self_assign"
# The version of the rules below (TEST_SUITE.md, :generator:). Raise it
# whenever they change.
VERSION = 1

# Targets: (declaration, initializing pattern, {spelling: old value}); the
# spellings are shown in this order.
TARGETS = {
    "a": ("int a [0:2];", "'{10, 11, 12}", {"a[0]": 10, "a[1]": 11, "a[2]": 12}),
    "r": ("int r [0:3];", "'{10, 11, 12, 13}",
          {"r[0]": 10, "r[1]": 11, "r[2]": 12, "r[3]": 13}),
    "d": ("int d [2:0];", "'{10, 11, 12}", {"d[2]": 10, "d[1]": 11, "d[0]": 12}),
    "m": ("int m [0:1][0:1];", "'{'{10, 11}, '{12, 13}}",
          {"m[0][0]": 10, "m[0][1]": 11, "m[1][0]": 12, "m[1][1]": 13}),
    "w": ("int w [0:1][0:2];", "'{'{10, 11, 12}, '{13, 14, 15}}",
          {"w[0][0]": 10, "w[0][1]": 11, "w[0][2]": 12, "w[1][0]": 13, "w[1][1]": 14,
           "w[1][2]": 15}),
    "p": ("point_t p;", "'{10, 11, 12}", {"p.x": 10, "p.y": 11, "p.z": 12}),
    "q": ("pair_t q;", "'{'{10, 11, 12}, '{13, 14, 15}}",
          {"q.a.x": 10, "q.a.y": 11, "q.a.z": 12, "q.b.x": 13, "q.b.y": 14, "q.b.z": 15}),
    "h": ("holder_t h;", "'{'{10, 11}, 12}", {"h.v[0]": 10, "h.v[1]": 11, "h.n": 12}),
}


def swap(o, pairs):
    """The old values with each (target, source) spelling pair copied."""
    n = dict(o)
    for t, src in pairs:
        n[t] = o[src]
    return n


# Cases: (label, target, left side, right side, the new values from the old).
CASES = [
    ("positional", "a", "a", "'{a[1], a[2], a[0]}",
     lambda o: swap(o, [("a[0]", "a[1]"), ("a[1]", "a[2]"), ("a[2]", "a[0]")])),
    ("keyed", "a", "a", "'{0: a[2], 1: a[0], 2: a[1]}",
     lambda o: swap(o, [("a[0]", "a[2]"), ("a[1]", "a[0]"), ("a[2]", "a[1]")])),
    ("default", "a", "a", "'{0: a[2], default: a[0]}",
     lambda o: swap(o, [("a[0]", "a[2]"), ("a[1]", "a[0]"), ("a[2]", "a[0]")])),
    ("replicated", "r", "r", "'{2{r[1], r[0]}}",
     lambda o: swap(o, [("r[0]", "r[1]"), ("r[1]", "r[0]"), ("r[2]", "r[1]"),
                        ("r[3]", "r[0]")])),
    # A pattern for [2:0] fills from the left bound, d[2] first (10.9.1).
    ("descending", "d", "d", "'{d[0], d[2], d[1]}",
     lambda o: swap(o, [("d[2]", "d[0]"), ("d[1]", "d[2]"), ("d[0]", "d[1]")])),
    ("rows", "m", "m", "'{m[1], m[0]}",
     lambda o: swap(o, [("m[0][0]", "m[1][0]"), ("m[0][1]", "m[1][1]"),
                        ("m[1][0]", "m[0][0]"), ("m[1][1]", "m[0][1]")])),
    ("nested", "m", "m", "'{'{m[1][1], m[0][0]}, '{m[0][1], m[1][0]}}",
     lambda o: swap(o, [("m[0][0]", "m[1][1]"), ("m[0][1]", "m[0][0]"),
                        ("m[1][0]", "m[0][1]"), ("m[1][1]", "m[1][0]")])),
    ("row", "w", "w[1]", "'{w[1][2], w[1][0], w[1][1]}",
     lambda o: swap(o, [("w[1][0]", "w[1][2]"), ("w[1][1]", "w[1][0]"),
                        ("w[1][2]", "w[1][1]")])),
    ("row copy", "w", "w[1]", "w[0]",
     lambda o: swap(o, [("w[1][0]", "w[0][0]"), ("w[1][1]", "w[0][1]"),
                        ("w[1][2]", "w[0][2]")])),
    ("struct positional", "p", "p", "'{p.y, p.z, p.x}",
     lambda o: swap(o, [("p.x", "p.y"), ("p.y", "p.z"), ("p.z", "p.x")])),
    ("struct named", "p", "p", "'{z: p.x, x: p.z, y: p.x}",
     lambda o: swap(o, [("p.x", "p.z"), ("p.y", "p.x"), ("p.z", "p.x")])),
    ("struct default", "p", "p", "'{y: p.z, default: p.y}",
     lambda o: swap(o, [("p.x", "p.y"), ("p.y", "p.z"), ("p.z", "p.y")])),
    ("struct members", "q", "q", "'{q.b, q.a}",
     lambda o: swap(o, [("q.a.x", "q.b.x"), ("q.a.y", "q.b.y"), ("q.a.z", "q.b.z"),
                        ("q.b.x", "q.a.x"), ("q.b.y", "q.a.y"), ("q.b.z", "q.a.z")])),
    ("struct array member", "h", "h", "'{'{h.n, h.v[0]}, h.v[1]}",
     lambda o: swap(o, [("h.v[0]", "h.n"), ("h.v[1]", "h.v[0]"), ("h.n", "h.v[1]")])),
]

OPS = ["=", "<="]


def steps(container, indent):
    """The statements of every case, one time step each, and the expected lines."""
    sv, exp = [], []
    pad = " " * indent
    for label, tgt, lhs, rhs, rule in CASES:
        _, init, old = TARGETS[tgt]
        new = rule(old)
        shown = list(old)
        for op in OPS:
            fmt = " ".join(["%0d"] * len(shown))
            sv.append(f"{pad}{tgt} = {init};")
            sv.append(f"{pad}{lhs} {op} {rhs};")
            sv.append(f"{pad}#1 $display(\"{container} {label} {op} {fmt}\", {', '.join(shown)});")
            exp.append(f"{container} {label} {op} " + " ".join(str(new[s]) for s in shown))
    return sv, exp


def files():
    n = len(CASES) * len(OPS)
    sv = (f"/*\n:name: {NAME}\n:description: An assignment to an unpacked array or structure "
          "computes the whole right side before it writes an element or member, so a pattern "
          "or part that reads the target sees its old values, for each kind of pattern, with = "
          "and <=, in several containers\n:tags: values\n:type: simulation\n"
          ":lrm: 1800-2023 10.4.1 10.4.2 11.2.2 7.6 10.9.1 10.9.2\n:oracle: spec\n"
          ":spec_check: ai\n:fourstate: no\n:nondeterministic: no\n"
          f":generator: gen_aggregate_assign_matrix.py@{VERSION}\n"
          f":expected_stdout: @{NAME}.expected\n*/\n"
          "// Generated by tools/gen_aggregate_assign_matrix.py; change the generator, not "
          "this file.\n"
          "// Icarus Verilog 13.0 and Verilator 5.052 write element by element and give other\n"
          "// values here; VALUE_SEMANTICS.md 3.13 explains the reading taken.\n"
          "module top;\n"
          "  typedef struct { int x; int y; int z; } point_t;\n"
          "  typedef struct { point_t a; point_t b; } pair_t;\n"
          "  typedef struct { int v [2]; int n; } holder_t;\n")
    for decl, _, _ in TARGETS.values():
        sv += f"  {decl}\n"
    sv += "  logic go = 1'b0;\n"
    exp = []
    body, e = steps("initial", 4)
    exp += e
    sv += "  initial begin\n" + "\n".join(body) + "\n"
    sv += f"    run();\n    go = 1'b1;\n    #{n + 1};\n    fork\n      begin\n"
    fork_body, fork_exp = steps("fork", 8)
    sv += "\n".join(fork_body) + "\n      end\n    join\n  end\n"
    task_body, task_exp = steps("task", 4)
    sv += "  task static run();\n" + "\n".join(task_body) + "\n  endtask\n"
    always_body, always_exp = steps("always", 4)
    sv += "  always @(posedge go) begin\n" + "\n".join(always_body) + "\n  end\nendmodule\n"
    exp += task_exp + always_exp + fork_exp
    return {f"{NAME}.sv": sv, f"{NAME}.expected": "\n".join(exp) + "\n"}


def main():
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("--check", action="store_true")
    args = ap.parse_args()
    bad = 0
    for name, text in files().items():
        path = OUT / name
        if args.check:
            if not path.exists() or path.read_text(encoding="utf-8") != text:
                print(f"gen_aggregate_assign_matrix: {path.relative_to(OUT.parent.parent.parent)} "
                      "is out of date; run tools/gen_aggregate_assign_matrix.py", file=sys.stderr)
                bad += 1
        else:
            path.write_text(text, encoding="utf-8")
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main())
