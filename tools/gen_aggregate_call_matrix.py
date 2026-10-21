#!/usr/bin/env python3
# Copyright (c) 2026 The kirimasim Authors
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

"""Generates the structure argument and aggregate return matrix test of
TEST_SUITE.md ("Mechanism matrix") in tests/suite/proc/.

  tools/gen_aggregate_call_matrix.py           # writes the files
  tools/gen_aggregate_call_matrix.py --check   # exit 1 if a file differs from what it would write

An unpacked structure is passed to a subroutine by value: an input is
copied in, an output copied back at the end, an inout both (13.5.1); a
function may return an unpacked structure or array, a value assigned like
any aggregate (13.4.1, 7.6, 10.9; SIR_SPEC.md 8: leaves as arguments,
$return). The axes: the subroutine (static and automatic functions,
static and automatic tasks, a task that waits), the direction (input,
output, inout), the actual (a variable, an automatic variable, a member
of a structure, an element of an array of structures, an assignment
pattern, a call), and how a returned value is used (assigned, an
initializer, compared, a member read, passed on, a function name assigned
in place of return, recursion, a dropped value).

The expected outputs follow from the rules above. A person checks them
once for all the tests made from them (TEST_SUITE.md, "Sampling by people").
"""

import argparse
import sys
from pathlib import Path

OUT = Path(__file__).resolve().parent.parent / "tests" / "suite" / "proc"
NAME = "aggregate_call_matrix"
# The version of the rules below (TEST_SUITE.md, :generator:). Raise it
# whenever they change.
VERSION = 1

KINDS = [("static function", "function", "static"), ("automatic function", "function", "automatic"),
         ("static task", "task", "static"), ("automatic task", "task", "automatic")]


def files():
    decls = []
    body = []
    exp = []
    # Subroutines for each kind: sum of an input, set an output, swap an
    # inout.
    for label, kw, life in KINDS:
        n = f"{kw[0]}{life[0]}"  # fs, fa, ts, ta
        if kw == "function":
            decls.append(f"  function {life} int sum_{n}(pair_t p);\n    return p.a + p.b;\n"
                         "  endfunction")
            decls.append(f"  function {life} void set_{n}(output pair_t o, input int x);\n"
                         "    o.a = x;\n    o.b = x + 1;\n  endfunction")
            decls.append(f"  function {life} void swap_{n}(inout pair_t p);\n    int t;\n"
                         "    t = p.a;\n    p.a = p.b;\n    p.b = t;\n  endfunction")
        else:
            decls.append(f"  task {life} sum_{n}(input pair_t p, output int s);\n"
                         "    s = p.a + p.b;\n  endtask")
            decls.append(f"  task {life} set_{n}(output pair_t o, input int x);\n    o.a = x;\n"
                         "    o.b = x + 1;\n  endtask")
            decls.append(f"  task {life} swap_{n}(inout pair_t p);\n    int t;\n    t = p.a;\n"
                         "    p.a = p.b;\n    p.b = t;\n  endtask")

        def sum_of(actual, value):
            if kw == "function":
                body.append(f"    $display(\"{label} input {actual_label(actual)}: %0d\", "
                            f"sum_{n}({actual}));")
            else:
                body.append(f"    sum_{n}({actual}, r);\n    $display(\"{label} input "
                            f"{actual_label(actual)}: %0d\", r);")
            exp.append(f"{label} input {actual_label(actual)}: {value}")

        def actual_label(a):
            return {"pv": "variable", "qv.p": "member", "ps[1]": "element",
                    "'{3, 4}": "pattern", "mk(5, 6)": "call"}[a]

        body.append("    pv = '{1, 2};\n    qv = '{'{10, 20}, 30};\n    ps[1] = '{7, 9};")
        sum_of("pv", 3)
        sum_of("qv.p", 30)
        sum_of("ps[1]", 16)
        sum_of("'{3, 4}", 7)
        sum_of("mk(5, 6)", 11)
        for target, show in [("pv", "pv.a, pv.b"), ("qv.p", "qv.p.a, qv.p.b"),
                             ("ps[1]", "ps[1].a, ps[1].b")]:
            body.append(f"    set_{n}({target}, 40);\n    $display(\"{label} output "
                        f"{actual_label(target)}: %0d %0d\", {show});")
            exp.append(f"{label} output {actual_label(target)}: 40 41")
            body.append(f"    swap_{n}({target});\n    $display(\"{label} inout "
                        f"{actual_label(target)}: %0d %0d\", {show});")
            exp.append(f"{label} inout {actual_label(target)}: 41 40")
    # An automatic variable of an automatic task as the actual.
    decls.append("  task automatic local_actual();\n    pair_t lp;\n    lp = '{2, 5};\n"
                 "    swap_ta(lp);\n    $display(\"automatic variable inout: %0d %0d\", lp.a, "
                 "lp.b);\n    $display(\"automatic variable input: %0d\", sum_fa(lp));\n"
                 "  endtask")
    body.append("    local_actual();")
    exp += ["automatic variable inout: 5 2", "automatic variable input: 7"]
    # A task that waits.
    decls.append("  task automatic slow_swap(inout pair_t p);\n    #1;\n    p = '{p.b, p.a};\n"
                 "  endtask")
    body.append("    pv = '{6, 8};\n    slow_swap(pv);\n    $display(\"task that waits inout: "
                "%0d %0d\", pv.a, pv.b);")
    exp.append("task that waits inout: 8 6")
    # Returns.
    decls.append("  function pair_t mk(int a, int b);\n    return '{a, b};\n  endfunction")
    decls.append("  function automatic pair_t mka(int a, int b);\n    pair_t r;\n    r.a = a;\n"
                 "    r.b = b;\n    return r;\n  endfunction")
    decls.append("  function quad_t mkq(int a, int b, int c);\n    mkq.p = mk(a, b);\n"
                 "    mkq.c = c;\n  endfunction")
    decls.append("  function int4_t seq(int base);\n    for (int i = 0; i < 4; i++)\n"
                 "      seq[i] = base + i;\n  endfunction")
    decls.append("  function automatic grid_t grid(int k);\n    grid_t g;\n"
                 "    foreach (g[i, j]) g[i][j] = k * 10 + i * 3 + j;\n    return g;\n"
                 "  endfunction")
    decls.append("  function automatic pair_t fibp(int n);\n    pair_t prev;\n"
                 "    if (n == 0) return '{0, 1};\n    prev = fibp(n - 1);\n"
                 "    return '{prev.b, prev.a + prev.b};\n  endfunction")
    decls.append("  function automatic pair_t first_over(int limit);\n"
                 "    for (int i = 0; i < 10; i++)\n      if (i * i > limit) return '{i, i * i};\n"
                 "    return '{-1, -1};\n  endfunction")
    body.append("    pv = mk(3, 4);\n    $display(\"return assigned: %0d %0d\", pv.a, pv.b);")
    exp.append("return assigned: 3 4")
    body.append("    pv = mka(5, 7);\n    $display(\"automatic return: %0d %0d\", pv.a, pv.b);")
    exp.append("automatic return: 5 7")
    body.append("    qv = mkq(1, 2, 3);\n    $display(\"nested return by name: %0d %0d %0d\", "
                "qv.p.a, qv.p.b, qv.c);")
    exp.append("nested return by name: 1 2 3")
    body.append("    a4 = seq(10);\n    $display(\"array return: %0d %0d %0d %0d\", a4[0], a4[1], "
                "a4[2], a4[3]);")
    exp.append("array return: 10 11 12 13")
    body.append("    g23 = grid(2);\n    $display(\"two-dimensional array return: %0d %0d\", "
                "g23[0][0], g23[1][2]);")
    exp.append("two-dimensional array return: 20 25")
    body.append("    $display(\"compared: %0d %0d\", mk(1, 2) == p12, mk(1, 3) == p12);")
    exp.append("compared: 1 0")
    body.append("    $display(\"member of a call: %0d\", mk(7, 8).b);")
    exp.append("member of a call: 8")
    body.append("    $display(\"passed on: %0d\", sum_fs(mk(2, 3)));")
    exp.append("passed on: 5")
    body.append("    pv = fibp(6);\n    $display(\"recursion: %0d %0d\", pv.a, pv.b);")
    exp.append("recursion: 8 13")
    body.append("    pv = first_over(20);\n    $display(\"early return: %0d %0d\", pv.a, pv.b);")
    exp.append("early return: 5 25")
    body.append("    void'(mk(9, 9));\n    $display(\"dropped value: done\");")
    exp.append("dropped value: done")
    body.append("    $display(\"initializer: %0d %0d\", init_a4[0], init_a4[3]);")
    exp.append("initializer: 4 7")

    sv = (f"/*\n:name: {NAME}\n:description: Unpacked structures as input, output and inout "
          "arguments of static and automatic functions and tasks, from variables, members, "
          "elements, patterns and calls, and functions returning unpacked structures and arrays, "
          "assigned, compared, read by member, passed on, returned by name, early and "
          "recursively\n:tags: proc\n:type: simulation\n:lrm: 1800-2023 13.4.1 13.5.1 7.6 10.9\n"
          ":oracle: spec verilator@5.052\n:spec_check: ai\n:fourstate: no\n:nondeterministic: no\n"
          f":generator: gen_aggregate_call_matrix.py@{VERSION}\n"
          f":expected_stdout: @{NAME}.expected\n*/\n"
          "// Generated by tools/gen_aggregate_call_matrix.py; change the generator, not this file.\n"
          "// Verilator 5.052 gives these lines; Icarus Verilog 13.0 does not take unpacked\n"
          "// structures.\n"
          "module top;\n"
          "  typedef struct {int a; int b;} pair_t;\n"
          "  typedef struct {pair_t p; int c;} quad_t;\n"
          "  typedef int int4_t[4];\n"
          "  typedef int grid_t[2][3];\n"
          "  pair_t pv, p12 = '{1, 2};\n  quad_t qv;\n  pair_t ps[3];\n  int4_t a4;\n"
          "  grid_t g23;\n  int r;\n")
    sv += "\n".join(decls) + "\n"
    sv += "  int4_t init_a4 = seq(4);\n"
    sv += "  initial begin\n" + "\n".join(body) + "\n  end\nendmodule\n"
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
                print(f"gen_aggregate_call_matrix: {path.relative_to(OUT.parent.parent.parent)} "
                      "is out of date; run tools/gen_aggregate_call_matrix.py", file=sys.stderr)
                bad += 1
        else:
            path.write_text(text, encoding="utf-8")
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main())
