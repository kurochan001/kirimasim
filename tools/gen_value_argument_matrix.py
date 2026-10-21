#!/usr/bin/env python3
# Copyright (c) 2026 The kirimasim Authors
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

"""Generates the matrix test of output and inout arguments passed by value
of TEST_SUITE.md ("Mechanism matrix") in tests/suite/proc/.

  tools/gen_value_argument_matrix.py           # writes the files
  tools/gen_value_argument_matrix.py --check   # exit 1 if a file differs from what it would write

An argument passed by value is copied in when the subroutine is called
(input and inout) and copied back when it returns (output and inout), each
copy like a blocking assignment (13.5.1, 4.9.7, SIR_SPEC.md 8): a wider
source is truncated, a narrower one extended by its own signedness.

The axes: the direction (output, inout); the formal's type (logic [7:0],
and byte, which is signed); the subroutine (a static and an automatic
task, a static function called in void'(), an automatic function whose
result is used); the actual (a variable of the same type, a wider and a
narrower variable, an element of an array at a constant and at a run-time
index, a part-select, a concatenation, a member of a packed structure, and
in an automatic caller an automatic variable and an element of an
automatic array); the container of the call (an initial procedure, a fork
branch, an automatic task). The subroutine adds 8'h11 to an inout and
writes 8'hc3 to an output; each line shows the actual after the call.

The expected outputs follow from the rules above, computed below. A
person checks them once for all the tests made from them (TEST_SUITE.md,
"Sampling by people").
"""

from __future__ import annotations

import argparse
import random
import sys
from pathlib import Path

OUT = Path(__file__).resolve().parent.parent / "tests" / "suite" / "proc"
NAME = "value_argument_matrix"
# The version of the rules below (TEST_SUITE.md, :generator:). Raise it
# whenever they change.
VERSION = 1

FORMALS = [("u8", "logic [7:0]", 8, False), ("s8", "byte", 8, True)]
DIRS = ["output", "inout"]
KINDS = ["task", "atask", "func", "afunc"]
# (name, declaration (module or automatic), the actual, the variable shown
# and its width, where the actual's bits lie in it: (lsb, width), its
# signedness, automatic only)
PLACES = [
    ("var", "logic [7:0] {n};", "{n}", "{n}", 8, (0, 8), False, False),
    ("wider", "logic [15:0] {n};", "{n}", "{n}", 16, (0, 16), False, False),
    ("narrower", "logic [3:0] {n};", "{n}", "{n}", 4, (0, 4), False, False),
    ("elem", "logic [7:0] {n}[0:3];", "{n}[2]", "{n}[2]", 8, (0, 8), False, False),
    ("elem_rt", "logic [7:0] {n}[0:3];", "{n}[i]", "{n}[2]", 8, (0, 8), False, False),
    ("part", "logic [15:0] {n};", "{n}[11:4]", "{n}", 16, (4, 8), False, False),
    ("concat", "logic [3:0] {n}h, {n}l;", "{{{n}h, {n}l}}", "{{{n}h, {n}l}}", 8, (0, 8),
     False, False),
    ("member", "struct packed {{ logic [3:0] p; logic [7:0] f; logic [3:0] q; }} {n};", "{n}.f",
     "{n}", 16, (4, 8), False, False),
    ("auto", "logic [7:0] {n};", "{n}", "{n}", 8, (0, 8), False, True),
    ("auto_elem", "logic [7:0] {n}[0:3];", "{n}[2]", "{n}[2]", 8, (0, 8), False, True),
]
CONTAINERS = ["initial", "fork", "caller"]


def mask(w):
    return (1 << w) - 1


def convert(v, fw, signed, tw):
    """v (fw bits) assigned to tw bits: truncated, or extended by v's sign."""
    v &= mask(fw)
    if tw <= fw:
        return v & mask(tw)
    if signed and v >> (fw - 1):
        v |= mask(tw) & ~mask(fw)
    return v


def hexw(v, w):
    return f"{v:0{(w + 3) // 4}x}"


def subroutines():
    """The declarations of every subroutine, named by formal, direction and kind."""
    out = []
    for fname, ftype, _, _ in FORMALS:
        for d in DIRS:
            body = "x = x + 8'h11;" if d == "inout" else "x = 8'hc3;"
            for kind in KINDS:
                n = f"{kind}_{d}_{fname}"
                auto = " automatic" if kind.startswith("a") else ""
                if kind.endswith("task"):
                    out.append(f"  task{auto} {n}({d} {ftype} x);\n    {body}\n  endtask")
                else:
                    out.append(f"  function{auto} int {n}({d} {ftype} x);\n    {body}\n"
                               "    return 7;\n  endfunction")
    return out


def call(kind, d, fname, actual):
    n = f"{kind}_{d}_{fname}"
    if kind.endswith("task"):
        return f"{n}({actual});"
    if kind == "func":
        return f"void'({n}({actual}));"
    return f"r = {n}({actual});"


def files():
    rng = random.Random(1305)
    decls, autos, body, caller, exp = [], [], [], [], []
    k = 0
    for cont in CONTAINERS:
        for pname, decl, actual, shown, sw, (lsb, aw), _, automatic in PLACES:
            if automatic and cont != "caller":
                continue
            for fname, _, fw, fsigned in FORMALS:
                for d in DIRS:
                    for kind in KINDS:
                        n = f"a{k}"
                        k += 1
                        start = rng.getrandbits(sw)
                        (autos if automatic else decls).append(
                            ("    " if automatic else "  ") + decl.format(n=n))
                        tag = f"{cont} {pname} {fname} {d} {kind}"
                        st = [f"{shown.format(n=n)} = {sw}'h{hexw(start, sw)};"]
                        if pname == "elem_rt":
                            st.append("i = 2;")
                        st.append(call(kind, d, fname, actual.format(n=n)))
                        st.append(f'$display("{tag} %h", {shown.format(n=n)});')
                        # the actual's bits before the call, the formal after
                        cur = (start >> lsb) & mask(aw)
                        if d == "inout":
                            x = convert(cur, aw, False, fw)
                            x = (x + 0x11) & mask(fw)
                        else:
                            x = 0xC3
                        back = convert(x, fw, fsigned, aw)
                        after = (start & ~(mask(aw) << lsb)) | (back << lsb)
                        exp.append(f"{tag} {hexw(after, sw)}")
                        line = " ".join(st)
                        if cont == "caller":
                            caller.append("    " + line)
                        else:
                            body.append((cont, "    " + line))
    init = [s for c, s in body if c == "initial"]
    fork = [s for c, s in body if c == "fork"]
    head = [
        "/*",
        f":name: {NAME}",
        ":description: output and inout arguments passed by value: copied in and back like "
        "blocking assignments, with truncation and extension, to a variable, a wider and a "
        "narrower one, array elements, a part, a concatenation, a structure member and "
        "automatic variables, from static and automatic tasks and functions",
        ":tags: proc arguments",
        ":type: simulation",
        ":lrm: 1800-2023 4.9.7 10.7 13.4 13.5.1",
        ":oracle: spec",
        ":spec_check: ai",
        ":fourstate: yes",
        ":nondeterministic: no",
        f":generator: gen_value_argument_matrix.py@{VERSION}",
        ":top_module: top",
        f":expected_stdout: @{NAME}.expected",
        "*/",
        "// Generated by tools/gen_value_argument_matrix.py; change the generator, not this file.",
        "`timescale 1ns/1ns",
        "module top;",
        "  int i, r;",
    ]
    sv = head + decls + subroutines()
    sv += ["  task automatic caller;"] + autos + caller + ["  endtask"]
    sv += ["  initial begin"] + init + ["    #1;", "    fork"]
    sv += ["      begin"] + ["  " + s for s in fork] + ["      end"]
    sv += ["    join", "    #1;", "    caller();", "  end", "endmodule", ""]
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
                print(f"gen_value_argument_matrix: {path.relative_to(OUT.parent.parent.parent)} "
                      "is out of date; run tools/gen_value_argument_matrix.py", file=sys.stderr)
                bad += 1
        else:
            path.write_text(text, encoding="utf-8")
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main())
