#!/usr/bin/env python3
# Copyright (c) 2026 The kirimasim Authors
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

"""Generates the matrix test of waits through a ref argument that sits
among other variables of the task's frame, of TEST_SUITE.md ("Mechanism
matrix"), in tests/suite/sched/.

  tools/gen_ref_wait_layout_matrix.py           # writes the files
  tools/gen_ref_wait_layout_matrix.py --check   # exit 1 if a file differs from what it would write

A wait in a task on an expression that reads a ref argument depends on
the variable the ref names (13.5.2, 9.4.2, 9.4.3; SCHEDULER_SPEC.md 9.5),
whatever the task's other variables are: their number, widths and values
do not change what it waits on (SIR_SPEC.md 7.4: a frame variable is
found by its number, wherever its value is kept).

The axes: the other variables of the frame (none; a one-bit argument;
an argument of 64, 65, 100, 128 or 200 bits; two wide arguments; an
array argument of wide elements; a wide automatic variable of the task),
the ref before them or after them, the values of the other variables
(zero, or a different value in every word), the wait (wait on the ref,
any change, posedge, posedge with an iff that reads the ref), and how
the task is reached (from an initial procedure, from a fork branch, and
through another task that passes everything on). Every ref names a
variable of its own, which starts at 0; at time 10 + k the driver writes
1 to waiter k's; each waiter prints the time it goes on.

The expected outputs follow from the rules above, computed below.
"""

from __future__ import annotations

import argparse
import sys
from pathlib import Path

OUT = Path(__file__).resolve().parent.parent / "tests" / "suite" / "sched"
NAME = "ref_wait_layout_matrix"
# The version of the rules below (TEST_SUITE.md, :generator:). Raise it
# whenever they change.
VERSION = 1

# (name, the wait with {e} for the ref)
WAITS = [
    ("wait", "wait ({e});"),
    ("any", "@({e});"),
    ("posedge", "@(posedge {e});"),
    ("iff", "@(posedge {e} iff {e});"),
]
# (name, the other variables [(name, width or 0 for one bit, array
# elements or 0)], whether they are automatic variables of the task rather
# than arguments)
LAYOUTS = [
    ("none", [], False),
    ("bit", [("p", 0, 0)], False),
    ("w64", [("p", 64, 0)], False),
    ("w65", [("p", 65, 0)], False),
    ("w100", [("p", 100, 0)], False),
    ("w128", [("p", 128, 0)], False),
    ("w200", [("p", 200, 0)], False),
    ("two", [("p", 128, 0), ("q", 200, 0)], False),
    ("array", [("p", 128, 3)], False),
    ("local", [("p", 200, 0)], True),
]
PLACES = ["first", "last"]
VALUES = ["zero", "words"]
CALLERS = ["initial", "fork", "nested"]


def decl(name, width, elems):
    packed = f" [{width - 1}:0]" if width else ""
    unpacked = f" [0:{elems - 1}]" if elems else ""
    return f"logic{packed} {name}{unpacked}"


def value(width, values, salt):
    """A literal of the width: zero, or every 64-bit word different and
    odd, so that no word is 0 however few bits it has."""
    w = max(width, 1)
    if values == "zero":
        return f"{w}'h0"
    v = 0
    for word in range((w + 63) // 64):
        v |= (((0x0123456789abcdef * (word + 1)) ^ (salt << 32) | 1) & ((1 << 64) - 1)) << (64 * word)
    return f"{w}'h{v & ((1 << w) - 1):x}"


def actual(width, elems, values, salt):
    if elems:
        return "'{" + ", ".join(value(width, values, salt + i) for i in range(elems)) + "}"
    return value(width, values, salt)


def cases():
    for lname, others, local in LAYOUTS:
        for place in PLACES if others and not local else ["first"]:
            for values in VALUES if others else ["zero"]:
                yield lname, others, local, place, values


def files():
    decls, items, at10, exp = [], [], [], []
    k = 0
    for wname, wform in WAITS:
        for lname, others, local, place, values in cases():
            for caller in CALLERS:
                tag = f"{wname} {lname} {place} {values} {caller}"
                decls.append(f"  logic a{k} = 1'b0;")
                args = [] if local else [(n, w, el) for n, w, el in others]
                formals = [f"input {decl(n, w, el)}" for n, w, el in args]
                passed = [n for n, _, _ in args]
                given = [actual(w, el, values, 16 * k + i) for i, (n, w, el) in enumerate(args)]
                if place == "first":
                    formals.insert(0, "ref logic e")
                    passed.insert(0, "e")
                    given.insert(0, f"a{k}")
                else:
                    formals.append("ref logic e")
                    passed.append("e")
                    given.append(f"a{k}")
                body = []
                if local:
                    n, w, el = others[0]
                    body.append(f"    {decl(n, w, el)} = {actual(w, el, values, 16 * k)};")
                body.append(f"    {wform.format(e='e')}")
                body.append(f'    $display("{tag} %0t", $time);')
                items.append(f"  task automatic w{k}({', '.join(formals)});\n" + "\n".join(body) +
                             "\n  endtask")
                call = f"w{k}({', '.join(given)});"
                if caller == "initial":
                    items.append(f"  initial {call}")
                elif caller == "fork":
                    items.append(f"  initial fork\n    {call}\n  join_none")
                else:
                    items.append(f"  task automatic n{k}({', '.join(formals)});\n"
                                 f"    w{k}({', '.join(passed)});\n  endtask")
                    items.append(f"  initial n{k}({', '.join(given)});")
                at10.append(f"a{k} = 1'b1;")
                exp.append(f"{tag} {10 + k}")
                k += 1
    head = [
        "/*",
        f":name: {NAME}",
        ":description: waits in a task through a ref argument wake when the variable it names "
        "changes, whatever the task's other variables: none, one bit, 64 to 200 bits, two wide "
        "ones, an array of wide elements and a wide automatic variable, before or after the "
        "ref, zero or not; wait, any change, posedge and iff; from initial, fork and a task "
        "that passes its arguments on",
        ":tags: sched arguments",
        ":type: simulation",
        ":lrm: 1800-2023 9.4.2 9.4.3 13.5.2",
        ":oracle: spec",
        ":spec_check: ai",
        ":fourstate: yes",
        ":nondeterministic: no",
        f":generator: gen_ref_wait_layout_matrix.py@{VERSION}",
        ":issue: 1",
        ":top_module: top",
        f":expected_stdout: @{NAME}.expected",
        "*/",
        "// Generated by tools/gen_ref_wait_layout_matrix.py; change the generator, not this "
        "file.",
        "`timescale 1ns/1ns",
        "module top;",
    ]
    driver = ["  initial begin", "    #10;"]
    driver += [f"    {w} #1;" for w in at10] + ["    $finish;", "  end"]
    sv = head + decls + items + driver + ["endmodule", ""]
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
                print(f"gen_ref_wait_layout_matrix: {path.relative_to(OUT.parent.parent.parent)} "
                      "is out of date; run tools/gen_ref_wait_layout_matrix.py", file=sys.stderr)
                bad += 1
        else:
            path.write_text(text, encoding="utf-8")
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main())
