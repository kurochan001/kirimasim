#!/usr/bin/env python3
# Copyright (c) 2026 The kirimasim Authors
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

"""Generates the matrix test of writes to packed selects whose parent is
chosen at run time, of TEST_SUITE.md ("Mechanism matrix"), in
tests/suite/values/.

  tools/gen_packed_runtime_parent_matrix.py           # writes the files
  tools/gen_packed_runtime_parent_matrix.py --check   # exit 1 if a file differs from what it would write

The rule: 7.4.5 and 11.5.1 of the standard, VALUE_SEMANTICS.md 3.11 and
SIR_SPEC.md 5.3. In t[i][j] (and t[i][j][k]) every index selects within
the element its parent selected: an index outside its own range makes the
whole select invalid, and a write through it changes nothing, not a bit of
a neighbouring element. NBAs keep the indices they had when the statement
ran. The axes: the packed type (two levels with each pair of directions,
and three levels), the state (logic and bit), the indices (all valid, the
last below and above its range, the parent below and above, and the
middle one outside for three levels), the assignment (blocking, NBA,
delayed NBA, NBA waiting for an event, and a compound ^=), and where the
vector is (a static variable, an element of an unpacked array, a member
of a structure, an automatic variable, and a ref static argument; NBAs
only to static storage, 10.4.2). Each case shows the whole vector after
the write.

The expected outputs follow from the rule, worked out below without
reading kirimasim's code. A person checks them once for all the tests made
from them (TEST_SUITE.md, "Sampling by people").
"""

import argparse
import sys
from pathlib import Path

OUT = Path(__file__).resolve().parent.parent / "tests" / "suite" / "values"
NAME = "packed_runtime_parent_matrix"
# The version of the rules below (TEST_SUITE.md, :generator:). Raise it
# whenever they change.
VERSION = 1

# The packed dimensions, outermost first.
SHAPES = {
    "aa": [(2, 3), (0, 7)],
    "ad": [(2, 3), (7, 0)],
    "da": [(3, 2), (0, 7)],
    "dd": [(3, 2), (7, 0)],
    "three": [(0, 1), (2, 0), (3, 0)],
}
STATES = ["logic", "bit"]
KINDS = ["blocking", "nba", "delayed", "event", "compound"]


def size(d):
    return abs(d[0] - d[1]) + 1


def width(dims):
    w = 1
    for d in dims:
        w *= size(d)
    return w


def inside(d, i):
    return min(d) <= i <= max(d)


def position(dims, path):
    """The lsb of the select and its width, or None when it is invalid."""
    pos = 0
    for level, i in enumerate(path):
        d = dims[level]
        if not inside(d, i):
            return None
        elem = width(dims[level + 1:])
        pos += (i - d[1] if d[0] >= d[1] else d[1] - i) * elem
    return pos, width(dims[len(path):])


def indices(dims, case):
    """The indices of a case: valid ones in the middle of each range."""
    mid = [d[0] if size(d) < 3 else (d[0] + d[1]) // 2 for d in dims]
    mid[0] = dims[0][1]  # the parent's right element
    path = list(mid)
    if case == "last_low":
        path[-1] = min(dims[-1]) - 1
    elif case == "last_high":
        path[-1] = max(dims[-1]) + 1
    elif case == "parent_low":
        path[0] = min(dims[0]) - 1
    elif case == "parent_high":
        path[0] = max(dims[0]) + 1
    elif case == "middle":
        path[1] = max(dims[1]) + 1
    return path


def cases_of(shape):
    base = ["valid", "last_low", "last_high", "parent_low", "parent_high"]
    return base + (["middle"] if shape == "three" else [])


class Vector:
    def __init__(self, w):
        self.bits = [1 if k % 3 == 0 else 0 for k in range(w)]  # by lsb index

    def text(self):
        return "".join(str(b) for b in reversed(self.bits))

    def literal(self):
        return f"{len(self.bits)}'b{self.text()}"


def case_lines(tag, home, dims, state, case, kind, pad):
    """The statements of one case and the line it prints."""
    path = indices(dims, case)
    vec = Vector(width(dims))
    names = ["i", "j", "k"][:len(path)]
    sel = home["expr"] + "".join(f"[{n}]" for n in names)
    got = position(dims, path)
    # The value written: the opposite of the bit there, so that a write is
    # seen; any bit when the select is invalid.
    value = 1 - vec.bits[got[0]] if got else 1
    sv = [f"{pad}// {tag}", f"{pad}{home['expr']} = {vec.literal()};"]
    sv += [f"{pad}{n} = {v};" for n, v in zip(names, path)]
    # After an NBA the indices move to the parent's left element and the
    # low end of the others, which the update must not follow.
    other = [dims[0][0]] + [min(d) for d in dims[1:]]
    moved = [f"{pad}{n} = {v};" for n, v in zip(names, other)]
    if kind == "blocking":
        sv.append(f"{pad}{sel} = 1'b{value};")
    elif kind == "compound":
        sv.append(f"{pad}{sel} ^= 1'b1;")
        value = 1 - vec.bits[got[0]] if got else 0
    elif kind == "nba":
        sv += [f"{pad}{sel} <= 1'b{value};"] + moved + [f"{pad}#1;"]
    elif kind == "delayed":
        sv += [f"{pad}{sel} <= #1 1'b{value};"] + moved + [f"{pad}#2;"]
    else:
        sv += [f"{pad}{sel} <= @(e) 1'b{value};"] + moved + [f"{pad}#1 -> e;",
                                                                    f"{pad}#1;"]
    if got:
        vec.bits[got[0]] = value
    sv.append(f'{pad}$display("{tag} %b", {home["expr"]});')
    return sv, f"{tag} {vec.text()}"


def files():
    sv_types, statics, subs, body, expected = [], [], [], [], []
    tasks = 0

    def emit(tag, shape, state, case, kind, home_name):
        nonlocal tasks
        dims = SHAPES[shape]
        tname = f"{state}_{shape}_t"
        if home_name in ("automatic", "ref"):
            name = f"case_{tasks}"
            tasks += 1
            if home_name == "automatic":
                subs.append(f"  task automatic {name}();")
                subs.append(f"    {tname} v;")
                lines, exp = case_lines(tag, {"expr": "v"}, dims, state, case, kind, "    ")
            else:
                subs.append(f"  task automatic {name}(ref static {tname} v);")
                lines, exp = case_lines(tag, {"expr": "v"}, dims, state, case, kind, "    ")
            subs.extend(lines + ["  endtask"])
            body.append(f"    {name}({'' if home_name == 'automatic' else 'r_' + tname});")
        else:
            expr = {"variable": f"s_{tname}", "element": f"a_{tname}[1]",
                    "member": f"m_{tname}.f"}[home_name]
            lines, exp = case_lines(tag, {"expr": expr}, dims, state, case, kind, "    ")
            body.extend(lines)
        expected.append(exp)

    for shape, dims in SHAPES.items():
        for state in STATES:
            tname = f"{state}_{shape}_t"
            packed = "".join(f"[{d[0]}:{d[1]}]" for d in dims)
            sv_types.append(f"  typedef {state} {packed} {tname};")
            sv_types.append(f"  typedef struct {{ int pad; {tname} f; }} {tname}_r;")
            statics.append(f"  {tname} s_{tname}, r_{tname};")
            statics.append(f"  {tname} a_{tname} [0:1];")
            statics.append(f"  {tname}_r m_{tname};")
            # Every index case with a blocking write to a static variable.
            for case in cases_of(shape):
                emit(f"{shape} {state} variable {case} blocking", shape, state, case,
                     "blocking", "variable")
            # Every assignment with valid indices and with the last one out.
            for kind in KINDS[1:]:
                for case in ("valid", "last_high"):
                    emit(f"{shape} {state} variable {case} {kind}", shape, state, case, kind,
                         "variable")
        # The other places, with logic: a write, an invalid parent, an NBA.
        for home in ("element", "member", "automatic", "ref"):
            for case, kind in (("valid", "blocking"), ("parent_high", "blocking"),
                               ("valid", "nba"), ("middle" if shape == "three" else
                                                  "last_low", "compound")):
                if kind == "nba" and home == "automatic":
                    continue  # an NBA to an automatic variable is illegal (10.4.2)
                emit(f"{shape} logic {home} {case} {kind}", shape, "logic", case, kind, home)
    head = [
        "/*",
        f":name: {NAME}",
        ":description: a write to a packed select whose parent is chosen at run time changes "
        "only the bit its valid indices select, by blocking assignments, NBAs and compound "
        "assignments, in variables, array elements, members, automatic variables and refs",
        ":tags: values",
        ":type: simulation",
        ":lrm: 1800-2023 7.4.5 11.5.1 10.4.2 13.5.2",
        ":oracle: spec",
        ":spec_check: ai",
        ":fourstate: no",
        ":nondeterministic: no",
        f":generator: gen_packed_runtime_parent_matrix.py@{VERSION}",
        f":expected_stdout: @{NAME}.expected",
        "*/",
        "// Generated by tools/gen_packed_runtime_parent_matrix.py; change the generator, not "
        "this file.",
        "`timescale 1ns/1ns",
        "module top;",
    ]
    sv = head + sv_types + statics + ["  int i, j, k;", "  event e;"] + subs
    sv += ["  initial begin"] + body + ["  end", "endmodule", ""]
    return {f"{NAME}.sv": "\n".join(sv), f"{NAME}.expected": "\n".join(expected) + "\n"}


def main():
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("--check", action="store_true")
    args = ap.parse_args()
    bad = 0
    for name, text in files().items():
        path = OUT / name
        if args.check:
            if not path.exists() or path.read_text(encoding="utf-8") != text:
                where = path.relative_to(OUT.parent.parent.parent)
                print(f"gen_packed_runtime_parent_matrix: {where} is out of date; run "
                      "tools/gen_packed_runtime_parent_matrix.py", file=sys.stderr)
                bad += 1
        else:
            path.write_text(text, encoding="utf-8")
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main())
