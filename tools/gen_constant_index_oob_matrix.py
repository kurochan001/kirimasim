#!/usr/bin/env python3
# Copyright (c) 2026 The kirimasim Authors
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

"""Generates the matrix tests of a constant index that is outside the
bounds of an unpacked array (TEST_SUITE.md, "Mechanism matrix") in
tests/suite/values/.

  tools/gen_constant_index_oob_matrix.py           # writes the files
  tools/gen_constant_index_oob_matrix.py --check   # exit 1 if a file differs from what it would write

The rule: 7.4.5 of the standard with Table 7-1 (VALUE_SEMANTICS.md 3.11).
An index that is outside the bounds of the array, or that has an x or z
bit, is invalid. A read with an invalid index gives the value of Table
7-1 for the type of the element (0 for a 2-state integral type, x for a
4-state one, 0.0 for a real, the empty string, null for a class handle,
and for an enumeration the value of its base type), and a write with an
invalid index does nothing. A constant index is no exception: slang
warns about it (-Windex-oob), and the design is legal.

The axes: the type of the element (int, logic [7:0], logic [99:0] and
bit [99:0], which are kept wider than a word, real, string, an
enumeration on logic [2:0], a class handle), the shape of the array
([2], [1:3], [3:1], and the outer and the inner dimension of [2][2]),
the index (above the bounds, below them, 'x), the access (a read, a
blocking write, a nonblocking write, a compound assignment and an
increment, an output argument), and the container (an initial procedure
with static arrays, an automatic task and an automatic function with
arrays of their own, a class method with arrays that are properties).
A nonblocking write is made only to the static arrays (6.21).

The expected output is computed here from the values, without reading
kirimasim's code. A person checks it once for all the tests made from it
(TEST_SUITE.md, "Sampling by people").
"""

import argparse
import sys
from pathlib import Path

OUT = Path(__file__).resolve().parent.parent / "tests" / "suite" / "values"
# The version of the rules below (TEST_SUITE.md, :generator:). Raise it
# whenever they change.
VERSION = 1

CONTAINERS = ["initial", "task", "function", "method"]

# (key, the declared type, the format of one element, the value of the
# k-th valid element and the text it prints, the value written, the text
# a read with an invalid index prints, whether it takes += and ++).
TYPES = [
    ("int", "int", "%0d", lambda k: (f"{10 + k}", f"{10 + k}"), "99", "0", True),
    ("logic", "logic [7:0]", "%h", lambda k: (f"8'h{11 * (k + 1)}", f"{11 * (k + 1)}"), "8'hff",
     "xx", True),
    ("wide", "logic [99:0]", "%h",
     lambda k: (f"100'h{(k + 1) << 64:x}", f"{(k + 1) << 64:025x}"), "100'hf", "x" * 25, True),
    ("wide2", "bit [99:0]", "%h",
     lambda k: (f"100'h{(k + 1) << 64:x}", f"{(k + 1) << 64:025x}"), "100'hf", "0" * 25, True),
    ("real", "real", "%g", lambda k: (f"{k}.5", f"{k}.5"), "7.25", "0", False),
    ("string", "string", "[%s]", lambda k: (f'"s{k}"', f"[s{k}]"), '"zz"', "[]", False),
    ("enum", "num_t", "%0d", lambda k: (f"num_t'({k + 1})", f"{k + 1}"), "N5", "x", False),
    ("handle", "item", "%0d", lambda k: (f"new({k + 1})", f"{k + 1}"), "new(99)", "1", False),
]

# (key, the unpacked dimensions, the valid indices in the order they are
# printed, how an index i selects an element, the invalid indices).
SHAPES = [
    ("zero", "[2]", ["0", "1"], lambda i: f"[{i}]", {"above": "2", "below": "-1"}),
    ("ascending", "[1:3]", ["1", "2", "3"], lambda i: f"[{i}]", {"above": "4", "below": "0"}),
    ("descending", "[3:1]", ["1", "2", "3"], lambda i: f"[{i}]", {"above": "4", "below": "0"}),
    ("outer", "[2][2]", ["0][0", "0][1", "1][0", "1][1"], lambda i: f"[{i}][0]",
     {"above": "2", "below": "-1"}),
    ("inner", "[2][2]", ["0][0", "0][1", "1][0", "1][1"], lambda i: f"[0][{i}]",
     {"above": "2", "below": "-1"}),
]
INDICES = ["above", "below", "unknown"]


def invalid(shape, index):
    """The text of the invalid index."""
    return "'x" if index == "unknown" else shape[4][index]


def element(tkey, var, select):
    """The expression printed for the element var<select>."""
    if tkey == "handle":
        return f"{var}{select}.id"
    return f"{var}{select}"


def test(container):
    name = f"constant_index_oob_{container}"
    where = {"initial": "an initial procedure with static arrays",
             "task": "an automatic task with arrays of its own",
             "function": "an automatic function with arrays of its own",
             "method": "a class method with arrays that are properties"}[container]
    sv = ["/*", f":name: {name}",
          ":description: a constant index outside the bounds of an unpacked array, or with an x "
          "bit, reads the default value of the element type and writes nothing, for eight element "
          f"types and five shapes, in {where}",
          ":tags: values", ":type: simulation", ":lrm: 1800-2023 7.4.5 6.21",
          ":oracle: spec", ":spec_check: ai", ":fourstate: yes", ":nondeterministic: no",
          f":generator: gen_constant_index_oob_matrix.py@{VERSION}", ":top_module: top",
          f":expected_stdout: @{name}.expected", "*/",
          "// Generated by tools/gen_constant_index_oob_matrix.py; change the generator, not "
          "this file.", "`timescale 1ns/1ns",
          "package oob_pkg;",
          "  typedef enum logic [2:0] {N1 = 1, N2, N3, N4, N5} num_t;",
          "  class item; int id; function new(int i); id = i; endfunction endclass"]
    for tkey, decl, _, _, written, _, _ in TYPES:
        sv.append(f"  function automatic void put_{tkey}(output {decl} o); o = {written}; "
                  "endfunction")
    sv += ["endpackage"]
    decls, body, after, expected = [], [], [], []
    nonblocking = container == "initial"
    for tkey, decl, fmt, value, written, default, numeric in TYPES:
        for shape in SHAPES:
            skey, dims, valid, select, _ = shape
            var = f"{tkey}_{skey}"
            decls.append(f"{decl} {var} {dims};")
            for k, i in enumerate(valid):
                body.append(f"{var}[{i}] = {value(k)[0]};")
            for index in INDICES:
                at = select(invalid(shape, index))
                label = f"{tkey} {skey} {index}"
                if tkey == "handle":
                    body.append(f'$display("{label} read: %0d", {var}{at} == null);')
                else:
                    body.append(f'$display("{label} read: {fmt}", {var}{at});')
                expected.append(f"{label} read: {default}")
                body.append(f"{var}{at} = {written};")
                if nonblocking:
                    body.append(f"{var}{at} <= {written};")
                if numeric:
                    body.append(f"{var}{at} += 1;")
                    body.append(f"{var}{at}++;")
                body.append(f"put_{tkey}({var}{at});")
            fmts = " ".join([fmt] * len(valid))
            args = ", ".join(element(tkey, var, f"[{i}]") for i in valid)
            after.append(f'$display("{tkey} {skey} after: {fmts}", {args});')
    for tkey, _, _, value, _, _, _ in TYPES:
        for skey, _, valid, _, _ in SHAPES:
            expected.append(f"{tkey} {skey} after: " +
                            " ".join(value(k)[1] for k in range(len(valid))))
    # The reads come in the order of the types and shapes, then every array
    # is printed again; the writes are read back one time unit later, after
    # the nonblocking ones, where there is time to wait.
    reads = [e for e in expected if " read: " in e]
    afters = [e for e in expected if " after: " in e]
    expected = reads + afters
    if container == "initial":
        sv += ["module top;", "  import oob_pkg::*;"] + ["  " + d for d in decls]
        sv += ["  initial begin"] + ["    " + b for b in body] + ["    #1;"]
        sv += ["    " + a for a in after] + ["  end", "endmodule"]
    elif container == "task":
        sv += ["module top;", "  import oob_pkg::*;", "  task automatic run();"]
        sv += ["    " + d for d in decls] + ["    " + b for b in body] + ["    #1;"]
        sv += ["    " + a for a in after] + ["  endtask", "  initial run();", "endmodule"]
    elif container == "function":
        sv += ["module top;", "  import oob_pkg::*;", "  function automatic void run();"]
        sv += ["    " + d for d in decls] + ["    " + b for b in body]
        sv += ["    " + a for a in after] + ["  endfunction", "  initial run();", "endmodule"]
    else:
        sv += ["module top;", "  import oob_pkg::*;", "  class holder;"]
        sv += ["    " + d for d in decls] + ["    function void run();"]
        sv += ["      " + b for b in body] + ["      " + a for a in after]
        sv += ["    endfunction", "  endclass",
               "  initial begin", "    holder subject = new;", "    subject.run();", "  end",
               "endmodule"]
    return name, sv, expected


def files():
    out = {}
    for container in CONTAINERS:
        name, sv, expected = test(container)
        out[f"{name}.sv"] = "\n".join(sv) + "\n"
        out[f"{name}.expected"] = "\n".join(expected) + "\n"
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
                print("gen_constant_index_oob_matrix: "
                      f"{path.relative_to(OUT.parent.parent.parent)} is out of date; run "
                      "tools/gen_constant_index_oob_matrix.py", file=sys.stderr)
                bad += 1
        else:
            path.write_text(text, encoding="utf-8")
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main())
