#!/usr/bin/env python3
# Copyright (c) 2026 The kirimasim Authors
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

"""Generates the matrix test of case statements on reals and strings, of
TEST_SUITE.md ("Mechanism matrix"), in tests/suite/values/.

  tools/gen_case_compare_matrix.py           # writes the files
  tools/gen_case_compare_matrix.py --check   # exit 1 if a file differs from what it would write

The case expression and the items take one common type (12.5; slang
gives it as for a comparison, 11.3.1, 11.8.1): real if any of them is
real, shortreal if any is shortreal and none real, string if the
expression is a string. When that type is real, shortreal or string, an
item matches when it is equal to the expression with == in that type
(VALUE_SEMANTICS.md 4.2): no rounding to an integer, -0.0 equal to 0.0,
an integral item converted to real first (x and z as 0, 1.5).

The axes: the types (a real, shortreal and realtime expression with real
items; an int expression with real items; a real expression with int
items; a shortreal expression with real items, compared as reals; a
string with string items); the value (equal to an item, near one but not
equal, negative, -0.0 against 0.0, and none); the statement (case, and
unique, unique0 and priority with exactly one item that matches); the
container (an initial procedure, a function, an automatic task and a
fork branch). Each line prints the item that was taken. A last part
checks that unique, unique0 and priority decide their violations with
the same comparison: with no default, a real or a string that matches no
item, or two, is reported (SYSTEM_TASKS.md 10.4), and a real that rounds
to an item's integer matches none.

The expected outputs follow from the rules above, computed below. For
reference (checked 2026-10-18): Verilator 5.052 prints every line as
expected but the 16 of a shortreal 1.1 against a real 1.1, which it
takes as equal, keeping a shortreal as a real; Icarus Verilog 13.0 does
the same without the strings, and stops with an internal error on them.
"""

from __future__ import annotations

import argparse
import struct
import sys
from pathlib import Path

OUT = Path(__file__).resolve().parent.parent / "tests" / "suite" / "values"
NAME = "case_compare_matrix"
# The version of the rules below (TEST_SUITE.md, :generator:). Raise it
# whenever they change.
VERSION = 1


def f32(x):
    """x rounded to a shortreal."""
    return struct.unpack("f", struct.pack("f", x))[0]


# (name, the type of the expression, its values as SystemVerilog text and as
# Python values, the items as text and values). The Python values are those
# of the common type.
GROUPS = [
    ("real", "real", [("2.0", 2.0), ("1.6", 1.6), ("2.4", 2.4), ("-3.5", -3.5),
                      ("-0.0", -0.0), ("7.25", 7.25)],
     [("2.0", 2.0), ("-3.5", -3.5), ("0.0", 0.0)]),
    ("realtime", "realtime", [("2.0", 2.0), ("1.6", 1.6), ("0.5", 0.5)],
     [("2.0", 2.0), ("0.5", 0.5)]),
    ("shortreal", "shortreal", [("2.5", 2.5), ("2.4", f32(2.4)), ("-1.25", -1.25)],
     [("shortreal'(2.5)", 2.5), ("shortreal'(-1.25)", -1.25), ("shortreal'(2.0)", 2.0)]),
    # A shortreal against real items: compared as reals, so 1.1 as a
    # shortreal is not the real 1.1.
    ("shortreal_vs_real", "shortreal", [("1.1", f32(1.1)), ("0.5", 0.5)],
     [("1.1", 1.1), ("0.5", 0.5)]),
    ("int_vs_real", "int", [("2", 2.0), ("3", 3.0), ("-1", -1.0)],
     [("1.9", 1.9), ("2.0", 2.0), ("-1.0", -1.0)]),
    ("real_vs_int", "real", [("2.0", 2.0), ("2.4", 2.4), ("-1.0", -1.0), ("0.0", 0.0)],
     [("2", 2.0), ("-1", -1.0), ("'x", 0.0)]),
    ("string", "string", [('"abc"', "abc"), ('"abd"', "abd"), ('"ab"', "ab"), ('""', "")],
     [('"abc"', "abc"), ('"ab"', "ab"), ('""', "")]),
]
CHECKS = ["", "unique ", "unique0 ", "priority "]
# Violations, decided with the same comparison (12.5.3, SCHEDULER_SPEC.md
# 15): (name, type, value, items, check, the report or None). No default.
VIOLATIONS = [
    ("real_none", "real", ("1.6", 1.6), [("2.0", 2.0), ("3.0", 3.0)], "unique",
     "no item matches"),
    ("real_two", "real", ("2.0", 2.0), [("2.0", 2.0), ("2.0", 2.0)], "unique",
     "more than one item matches"),
    ("real_priority", "real", ("2.4", 2.4), [("2.0", 2.0)], "priority", "no item matches"),
    ("real_unique0", "real", ("1.6", 1.6), [("2.0", 2.0)], "unique0", None),
    ("real_vs_int", "real", ("2.4", 2.4), [("2", 2.0), ("3", 3.0)], "unique",
     "no item matches"),
    ("shortreal_vs_real", "shortreal", ("1.1", f32(1.1)), [("1.1", 1.1)], "unique",
     "no item matches"),
    ("string_two", "string", ('"abc"', "abc"), [('"abc"', "abc"), ('"abc"', "abc")], "unique",
     "more than one item matches"),
    ("string_priority", "string", ('"x"', "x"), [('"abc"', "abc")], "priority",
     "no item matches"),
]
CONTAINERS = ["initial", "function", "task", "fork"]


def taken(value, items):
    for k, (_, v) in enumerate(items):
        if v == value:
            return f"item{k}"
    return "default"


def case_text(check, var, items, indent):
    lines = [f"{indent}{check}case ({var})"]
    for k, (text, _) in enumerate(items):
        lines.append(f"{indent}  {text}: r = \"item{k}\";")
    lines.append(f"{indent}  default: r = \"default\";")
    lines.append(f"{indent}endcase")
    return lines


def files():
    funcs, tasks, initial, forks, exp, order = [], [], [], [], {}, []
    for gname, typ, values, items in GROUPS:
        for check in CHECKS:
            cname = check.strip() or "case"
            # unique and priority need one item at most to match: every
            # value here matches one item or none, and a default is given.
            fn = f"pick_{gname}_{cname}"
            funcs.append(f"  function automatic string {fn}({typ} v);")
            funcs.append("    string r;")
            funcs += case_text(check, "v", items, "    ")
            funcs += ["    return r;", "  endfunction"]
            tk = f"run_{gname}_{cname}"
            tasks.append(f"  task automatic {tk}();")
            tasks += [f"    {typ} v;", "    string r;"]
            for vtext, value in values:
                want = taken(value, items)
                for container in CONTAINERS:
                    tag = f"{container} {gname} {cname} {vtext.strip(chr(34)) or 'empty'}"
                    exp[tag] = f"{tag} {want}"
                tasks.append(f"    v = {vtext};")
                tasks += case_text(check, "v", items, "    ")
                tag = f"task {gname} {cname} {vtext.strip(chr(34)) or 'empty'}"
                tasks.append(f'    $display("{tag} %s", r);')
                order.append(("task", tag))
                var = f"v_{gname}"
                tag = f"initial {gname} {cname} {vtext.strip(chr(34)) or 'empty'}"
                initial.append(f"    {var} = {vtext};")
                initial += case_text(check, var, items, "    ")
                initial.append(f'    $display("{tag} %s", r);')
                order.append(("initial", tag))
                tag = f"function {gname} {cname} {vtext.strip(chr(34)) or 'empty'}"
                initial.append(f'    $display("{tag} %s", {fn}({vtext}));')
                order.append(("initial", tag))
                tag = f"fork {gname} {cname} {vtext.strip(chr(34)) or 'empty'}"
                forks.append(f"        {var} = {vtext};")
                forks += case_text(check, var, items, "        ")
                forks.append(f'        $display("{tag} %s", r);')
                order.append(("fork", tag))
            tasks.append("  endtask")
    decls = [f"  {typ} v_{gname};" for gname, typ, _, _ in GROUPS]
    # The violations come at 1ns in a process of their own, which waits for
    # nothing after them, so that each settles and is reported (12.4.2.1).
    later, viol_exp, reports = ["  initial begin", "    #1;"], [], []
    for vname, typ, (vtext, value), items, check, report in VIOLATIONS:
        var = f"w_{vname}"
        decls.append(f"  {typ} {var};")
        later += [f"    {var} = {vtext};", '    r = "none";']
        later.append(f"    {check} case ({var})  // CASE {check}:{report or ''}")
        for k, (text, _) in enumerate(items):
            later.append(f"      {text}: r = \"item{k}\";")
        later += ["    endcase", f'    $display("violation {vname} %s", r);']
        hit = [k for k, (_, v) in enumerate(items) if v == value]
        viol_exp.append(f"violation {vname} {'item%d' % hit[0] if hit else 'none'}")
    later.append("  end")
    head = [
        "/*",
        f":name: {NAME}",
        ":description: case statements on reals and strings compare with == in the common "
        "type: real, realtime and shortreal expressions and items, an int expression with "
        "real items and the reverse, a shortreal with real items, and strings; equal, near, "
        "negative, -0.0 and no match; case, unique, unique0 and priority; in an initial "
        "procedure, a function, an automatic task and a fork branch",
        ":tags: values",
        ":type: simulation",
        ":lrm: 1800-2023 12.5 11.3.1 6.12.1 6.16",
        ":oracle: spec",
        ":spec_check: ai",
        ":fourstate: yes",
        ":nondeterministic: no",
        f":generator: gen_case_compare_matrix.py@{VERSION}",
        ":issue: 10",
        ":top_module: top",
        f":expected_stdout: @{NAME}.expected",
        f":expected_stderr: @{NAME}.stderr",
        "*/",
        "// Generated by tools/gen_case_compare_matrix.py; change the generator, not this file.",
        "// Icarus Verilog 13.0 and Verilator 5.052 keep a shortreal as a real, so they",
        "// match a shortreal 1.1 with a real 1.1; a shortreal is C's float",
        "// (VALUE_SEMANTICS.md 1.5) and the two differ.",
        "`timescale 1ns/1ns",
        "module top;",
        "  string r;",
    ]
    tnames = [f"run_{g}_{(c.strip() or 'case')}" for g, _, _, _ in GROUPS for c in CHECKS]
    sv = head + decls + funcs + tasks
    sv += ["  initial begin"] + initial + [f"    {t}();" for t in tnames]
    sv += ["    fork", "      begin"] + forks + ["      end", "    join", "  end"]
    sv += later + ["endmodule", ""]
    # The reports: the position of each case, in order.
    for number, line in enumerate(sv, 1):
        if "  // CASE " in line:
            check, report = line.split("  // CASE ")[1].split(":", 1)
            if report:
                reports.append(f"{NAME}.sv:{number}: warning: {check} case violation: {report} "
                               "[top @ 1ns]")
    sv = [line.split("  // CASE ")[0] for line in sv]
    # The order of the lines: the initial procedure (its own and the
    # function's lines), then the tasks, then the fork branch, then the
    # violations at 1ns.
    lines = [exp[t] for c in ("initial", "task", "fork") for k, t in order if k == c]
    return {f"{NAME}.sv": "\n".join(sv), f"{NAME}.expected": "\n".join(lines + viol_exp) + "\n",
            f"{NAME}.stderr": "\n".join(reports) + "\n"}


def main():
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("--check", action="store_true")
    args = ap.parse_args()
    bad = 0
    for name, text in files().items():
        path = OUT / name
        if args.check:
            if not path.exists() or path.read_text(encoding="utf-8") != text:
                print(f"gen_case_compare_matrix: {path.relative_to(OUT.parent.parent.parent)} "
                      "is out of date; run tools/gen_case_compare_matrix.py", file=sys.stderr)
                bad += 1
        else:
            path.write_text(text, encoding="utf-8")
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main())
