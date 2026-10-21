#!/usr/bin/env python3
# Copyright (c) 2026 The kirimasim Authors
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

"""Generates the matrix tests of the warning on static initial values that
may read a variable before its initial value is set, of TEST_SUITE.md
("Mechanism matrix"), in tests/suite/proc/.

  tools/gen_static_init_order_matrix.py           # writes the files
  tools/gen_static_init_order_matrix.py --check   # exit 1 if a file differs from what it would write

The initial values of static variables are evaluated before time 0, and
the standard sets no order among them (6.8, 10.5). kirimasim runs them
in its own order: packages and compilation units first, then each
instance, in which the variables of the module and its generate blocks
come in declaration order, then the static variables of the blocks of
its procedures; a subroutine's static variables come at the point where
the subroutine is first lowered, before the initial value that names
them. An initial value gets the warning kirima-static-init-order when it
reads a static variable of another instance, or one of its own instance,
package or compilation unit set after it in that order. Only the
variables the expression names count, not those read inside the
functions it calls. The warning is given once per initial value, at its
position, naming the variables read (SCHEDULER_SPEC.md 3, step 1).

The axes: the scope of the variable read (a sibling instance placed
after and before the reader, the parent, a child, an interface instance,
a package, the compilation unit, a later and an earlier variable of the
same module, a generate block placed after and before, a static variable
of a named block of a procedure, a static variable of a subroutine of
the same instance, and a call of another instance's function); the
reader (a variable of the module, a static variable of a named block of
a procedure, a static variable of a function); and, for a sibling's
variable read by a module variable, the way it is read (alone, as an
operand, in a concatenation, as an index, as an element of an array's
initial value).

The value printed depends on whether the variable was set before it was
read, which the standard leaves open, so each test allows both
(:nondeterministic: yes); the warning lines are kirimasim's rule and are
compared exactly. The expected outputs follow from the rules above,
computed below.
"""

from __future__ import annotations

import argparse
import sys
from pathlib import Path

OUT = Path(__file__).resolve().parent.parent / "tests" / "suite" / "proc"
PREFIX = "static_init_order_"
# The version of the rules below (TEST_SUITE.md, :generator:). Raise it
# whenever they change.
VERSION = 1

# The readers: (name, the lines that declare it with {E} for the initial
# value, the expression that shows its value from top with {P} for the
# path to its instance, the path of the reader's variable for the
# message, its description)
READERS = {
    "module": (["  int x = {E};"], "{P}x", "a variable of the module"),
    "block": (["  initial begin : rb", "    static int x = {E};", "  end"], "{P}rb.x",
              "a static variable of a named block"),
    "function": (["  function int rf();", "    static int x = {E};", "    return x;",
                  "  endfunction"], "{P}rf()", "a static variable of a function"),
}

# The scopes: (name, description, the expression read, the path named in
# the warning, where the reader is ("top" or "ua"), the lines before
# module top, the items of top before and after the reader, the value of
# the variable read once set, the readers that get the warning)
ALL = {"module", "block", "function"}
SCOPES = [
    ("sibling_after", "a variable of a sibling instance placed after the reader",
     "top.ub.y", "top.ub.y", "ua", [], ["A ua();", "B ub();"], [], 5, ALL),
    ("sibling_before", "a variable of a sibling instance placed before the reader",
     "top.ub.y", "top.ub.y", "ua", [], ["B ub();", "A ua();"], [], 5, ALL),
    ("parent", "a variable of the parent, from a child", "top.v", "top.v", "ua", [],
     ["int v = 5;", "A ua();"], [], 5, ALL),
    ("child", "a variable of a child, from the parent", "ub.y", "top.ub.y", "top", [],
     ["B ub();"], [], 5, ALL),
    ("interface", "a variable of an interface instance, from the parent", "i.y", "top.i.y",
     "top", [], ["I i();"], [], 5, ALL),
    ("package", "a variable of a package", "p::y", None, "top", [], [], [], 5, set()),
    ("unit", "a variable of the compilation unit", "y_unit", None, "top", [], [], [], 5,
     set()),
    ("later", "a later variable of the same module", "top.w", "top.w", "top", [], [],
     ["int w = 5;"], 5, {"module"}),
    ("earlier", "an earlier variable of the same module", "w", None, "top", [],
     ["int w = 5;"], [], 5, set()),
    ("generate_after", "a variable of a generate block placed after the reader", "g.w",
     "top.g.w", "top", [], [], ["if (1) begin : g", "  int w = 5;", "end"], 5, {"module"}),
    ("generate_before", "a variable of a generate block placed before the reader", "g.w",
     None, "top", [], ["if (1) begin : g", "  int w = 5;", "end"], [], 5, set()),
    ("named_block", "a static variable of a named block of a procedure", "top.blk.w",
     "top.blk.w", "top", [], [], ["initial begin : blk", "  static int w = 5;", "end"], 5,
     {"module", "block"}),
    ("subroutine", "a static variable of a function of the same instance", "top.f.w", None,
     "top", [], [], ["function int f();", "  static int w = 5;", "  return w;", "endfunction"],
     5, set()),
    ("call", "a call of a function of another instance", "ub.get()", None, "top", [],
     ["B ub();"], [], 5, set()),
]

COMMON = [
    "package p;", "  int y = 5;", "endpackage", "int y_unit = 5;", "interface I;",
    "  int y = 5;", "endinterface", "module B;", "  int y = 5;",
    "  function int get();", "    return y;", "  endfunction", "endmodule",
]

# The ways of reading a sibling's variable by a module variable: (name,
# the initial value with {V} for the variable, the expression printed,
# the values printed when the variable is not set yet and when it is, the
# declarations it needs before the reader)
WAYS = [
    ("alone", "{V}", "{P}x", ("0", "5"), []),
    ("concatenation", "{{{V}[15:0], 16'h0001}}", "{P}x", ("1", "327681"), []),
    ("index", "K[{V}]", "{P}x", ("10", "15"),
     ["localparam int K[6] = '{10, 11, 12, 13, 14, 15};"]),
    ("aggregate", "'{{{V}, 7}}", "{P}x[0], {P}x[1]", ("0 7", "5 7"), []),
]


def warning(name, line, column, reader, path):
    return (f"{name}.sv:{line}:{column}: warning: the initial value of '{reader}' reads "
            f"'{path}', whose initial value may not be set yet")


def header(name, description):
    return [
        "/*",
        f":name: {name}",
        f":description: {description}",
        ":tags: proc static",
        ":type: simulation",
        ":lrm: 1800-2023 6.8 6.21 10.5 23.6",
        ":oracle: spec",
        ":spec_check: ai",
        ":fourstate: no",
        ":nondeterministic: yes",
        f":generator: gen_static_init_order_matrix.py@{VERSION}",
        ":issue: 16",
        ":top_module: top",
        f":expected_stdout: @{name}.expected",
        f":expected_stderr: @{name}.stderr",
        "*/",
        "// Generated by tools/gen_static_init_order_matrix.py; change the generator, not "
        "this file.",
    ]


def one(name, description, reader_lines, expr, where, before, items_before, items_after, show,
        values, warned_path, extra_decls=()):
    """A test: the reader's lines with {E} replaced by expr, in module A
    (instance ua) or in top."""
    body = [x.replace("{E}", expr) for x in reader_lines]
    lines = header(name, description) + COMMON + list(before)
    if where == "ua":
        lines += ["module A;"] + [f"  {d}" for d in extra_decls] + body + ["endmodule"]
        prefix = "ua."
    lines += ["module top;"] + [f"  {x}" for x in items_before]
    if where == "top":
        lines += [f"  {d}" for d in extra_decls] + body
        prefix = ""
    lines += [f"  {x}" for x in items_after]
    lines += [f"  initial #1 $display(\"{show_fmt(show)}\", {show.format(P=prefix)});",
              "endmodule"]
    stderr = []
    if warned_path is not None:
        # At the initial value, on the reader's line.
        at = next(i for i, x in enumerate(lines) if f"= {expr};" in x)
        column = lines[at].index(f"= {expr};") + 3
        stderr.append(warning(name, at + 1, column, "x", warned_path))
    expected = "\n---\n".join(values)
    return {f"{name}.sv": "\n".join(lines) + "\n", f"{name}.expected": expected + "\n",
            f"{name}.stderr": "".join(s + "\n" for s in stderr)}


def show_fmt(show):
    return " ".join("%0d" for _ in show.split(","))


def files():
    out = {}
    for sname, sdesc, read, path, where, before, items_before, items_after, value, warned \
            in SCOPES:
        for rname, (rlines, show, rdesc) in READERS.items():
            name = f"{PREFIX}{sname}_{rname}"
            description = (f"{rdesc} whose initial value reads {sdesc}: "
                           f"{'a warning' if rname in warned else 'no warning'}")
            out.update(one(name, description, rlines, f"{read} + 1", where, before,
                           items_before, items_after, show, (str(1), str(value + 1)),
                           path if rname in warned else None))
    for wname, init, show, values, decls in WAYS:
        name = f"{PREFIX}way_{wname}"
        description = (f"a variable of a sibling instance read {wname.replace('_', ' ')} "
                       "in the initial value of a module variable: a warning")
        reader = ["  int x[2] = {E};"] if wname == "aggregate" else ["  int x = {E};"]
        out.update(one(name, description, reader, init.format(V="top.ub.y"), "ua", [],
                       ["A ua();", "B ub();"], [], show, values, "top.ub.y", decls))
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
                print(f"gen_static_init_order_matrix: "
                      f"{path.relative_to(OUT.parent.parent.parent)} is out of date; run "
                      "tools/gen_static_init_order_matrix.py", file=sys.stderr)
                bad += 1
        else:
            path.write_text(text, encoding="utf-8")
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main())
