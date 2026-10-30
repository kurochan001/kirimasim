#!/usr/bin/env python3
# Copyright (c) 2026 The kirimasim Authors
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

"""Generates the matrix tests of slices of fixed-size unpacked arrays of
TEST_SUITE.md ("Mechanism matrix") in tests/suite/proc/.

  tools/gen_array_slice_matrix.py           # writes the files
  tools/gen_array_slice_matrix.py --check   # exit 1 if a file differs from what it would write

The rule: VALUE_SEMANTICS.md 3.13 and SIR_SPEC.md 8 (added 2026-10-31,
approved), with 7.4.6, 7.6, 10.4.2, 13.5 and 4.9.7 of the standard. A slice
is the unpacked array of the elements it selects, in the order of the
array's own dimension, matched with the other side by position from the
left end. A position decided at run time is evaluated once, where the
slice is read or written: for an NBA when the statement runs, for an
output or inout when the call returns (and for an inout also at the
call). An element outside the array reads the default of its type and is
not written.

The axes: the array (static with ascending and descending bounds, a row
of a two-dimensional array, an array member of a structure, an automatic
array of the container, an array a ref argument refers to, and an array
of structures), the slice (a constant range, a constant b+:w, b+:w and
b-:w at run time, and b+:w at run time partly outside the array), what is
done with it (read into an array, written from a pattern and from an
array, compared, written by an NBA, and passed to an input, an output and
an inout, the position changed while the callee waits), and the container
(an initial procedure, a static task, an always procedure and a fork
branch; one file each). NBAs go only to static arrays (10.4.2).

The expected outputs follow from the rule, worked out below without
reading kirimasim's code. A person checks them once for all the tests made
from them (TEST_SUITE.md, "Sampling by people").
"""

import argparse
import sys
from pathlib import Path

OUT = Path(__file__).resolve().parent.parent / "tests" / "suite" / "proc"
NAME = "array_slice_matrix"
# The version of the rules below (TEST_SUITE.md, :generator:). Raise it
# whenever they change.
VERSION = 1

# The container of the cases, as the description names it.
CONTAINERS = {"initial": "an initial procedure", "task": "a static task",
              "always": "an always procedure", "fork": "a fork branch"}

# The array: (name, its expression, its bounds, whether its elements are
# structures, whether it is static, the first value of its elements).
PLACES = [
    ("ascending", "sa", (2, 5), False, True, 100),
    ("descending", "sd", (5, 2), False, True, 200),
    ("row", "m[1]", (2, 5), False, True, 300),
    ("member", "rec.arr", (5, 2), False, True, 400),
    ("automatic", "la", (2, 5), False, False, 500),
    ("ref", "r", (5, 2), False, False, 600),
    ("structures", "sp", (2, 5), True, True, 700),
]
# The slice: (name, its text with B for the array, the position set first).
FORMS = [
    ("range", None, None),
    ("up_constant", "[3+:2]", None),
    ("up_runtime", "[i+:2]", 3),
    ("down_runtime", "[i-:2]", 4),
    ("edge_runtime", "[i+:2]", 5),
]
USES = ["read", "pattern", "copy", "compare", "nba", "input", "output", "inout"]


def indices(bounds, form, i):
    """The slice's indices from its left end."""
    up = bounds[0] <= bounds[1]
    if form == "range" or form == "up_constant":
        chosen = [3, 4]
    elif form == "down_runtime":
        chosen = [i - 1, i]
    else:
        chosen = [i, i + 1]
    return chosen if up else chosen[::-1]


def slice_text(base, bounds, form, sv_form):
    if form == "range":
        return f"{base}[3:4]" if bounds[0] <= bounds[1] else f"{base}[4:3]"
    return base + sv_form


class Model:
    """The array's elements by index; a structure element is (n, t)."""

    def __init__(self, bounds, structs, first):
        lo, hi = sorted(bounds)
        self.structs = structs
        self.v = {k: ((first + k, k) if structs else first + k) for k in range(lo, hi + 1)}
        self.order = list(range(bounds[0], bounds[1] + (1 if bounds[0] <= bounds[1] else -1),
                                1 if bounds[0] <= bounds[1] else -1))

    def default(self):
        return (0, 0) if self.structs else 0

    def read(self, k):
        return self.v.get(k, self.default())

    def write(self, k, x):
        if k in self.v:
            self.v[k] = x

    def text(self, x):
        return f"{x[0]}/{x[1]}" if self.structs else str(x)

    def show(self):
        return " ".join(self.text(self.v[k]) for k in self.order)

    def literal(self):
        if self.structs:
            return "'{" + ", ".join(f"'{{n: {self.v[k][0]}, t: {self.v[k][1]}}}"
                                    for k in self.order) + "}"
        return "'{" + ", ".join(str(self.v[k]) for k in self.order) + "}"


def pair_literal(values, structs):
    if structs:
        return "'{" + ", ".join(f"'{{n: {n}, t: {t}}}" for n, t in values) + "}"
    return "'{" + ", ".join(str(x) for x in values) + "}"


SUBROUTINES = """\
  function automatic string fi(input int v [0:1]);
    return $sformatf("%0d %0d", v[0], v[1]);
  endfunction
  function automatic string fp(input pair_t v [0:1]);
    return $sformatf("%0d/%0d %0d/%0d", v[0].n, v[0].t, v[1].n, v[1].t);
  endfunction
  task automatic oi(output int v [0:1], input int x);
    v[0] = x;
    #2 v[1] = x + 1;
  endtask
  task automatic op(output pair_t v [0:1], input int x);
    v[0] = '{n: x, t: 1};
    #2 v[1] = '{n: x + 1, t: 2};
  endtask
  task automatic ii(inout int v [0:1]);
    v[0] = v[0] + 1;
    #2 v[1] = v[1] + 2;
  endtask
  task automatic ip(inout pair_t v [0:1]);
    v[0].n = v[0].n + 1;
    #2 v[1].t = v[1].t + 2;
  endtask
  function automatic string show4(input int v [0:3]);
    return $sformatf("%0d %0d %0d %0d", v[0], v[1], v[2], v[3]);
  endfunction
  function automatic string showp(input pair_t v [0:3]);
    return $sformatf("%0d/%0d %0d/%0d %0d/%0d %0d/%0d", v[0].n, v[0].t, v[1].n, v[1].t,
                     v[2].n, v[2].t, v[3].n, v[3].t);
  endfunction
"""


def case(container, place, form, use, c, pad, subs):
    """The statements of one case and the line it prints."""
    label, base, bounds, structs, static, first = place
    fname, sv_form, i0 = form
    tag = f"{container} {label} {fname} {use}"
    model = Model(bounds, structs, first)
    target = "r" if label == "ref" else base
    sl = slice_text(target, bounds, fname, sv_form)
    show = ("showp" if structs else "show4") + f"({target})"
    body = []
    i = i0 if i0 is not None else 0
    if i0 is not None:
        body.append(f"i = {i0};")
    w = [(1000 + c, 1), (2000 + c, 2)] if structs else [1000 + c, 2000 + c]
    src, dst = ("q2", "p2") if structs else ("s2", "b2")
    line = None
    if use == "read":
        body.append(f"{dst} = {sl};")
        got = [model.read(k) for k in indices(bounds, fname, i)]
        body.append(f'$display("{tag} %s", {"fp" if structs else "fi"}({dst}));')
        line = " ".join(model.text(x) for x in got)
    elif use in ("pattern", "copy"):
        if use == "pattern":
            body.append(f"{sl} = {pair_literal(w, structs)};")
        else:
            body.append(f"{src} = {pair_literal(w, structs)};")
            body.append(f"{sl} = {src};")
        for k, x in zip(indices(bounds, fname, i), w):
            model.write(k, x)
    elif use == "compare":
        got = [model.read(k) for k in indices(bounds, fname, i)]
        body.append(f"{src} = {pair_literal(got, structs)};")
        body.append(f'$display("{tag} %0d %0d", {sl} == {src}, {sl} != {src});')
        line = "1 0"
    elif use == "nba":
        # The position is taken when the statement runs; the update comes
        # in the NBA region, after the Inactive region where #0 resumes.
        body.append(f"{src} = {pair_literal(w, structs)};")
        body.append(f"{sl} <= {src};")
        body.append("i = 2;")
        body.append(f'#0 $display("{tag} before %s", {show});')
        before = model.show()
        for k, x in zip(indices(bounds, fname, i), w):
            model.write(k, x)
        body.append("#1;")
        line = None
        nba_before = before
    elif use == "input":
        got = [model.read(k) for k in indices(bounds, fname, i)]
        body.append(f'$display("{tag} %s", {"fp" if structs else "fi"}({sl}));')
        line = " ".join(model.text(x) for x in got)
    else:
        # The callee waits two steps; the position moves down one at the
        # first, so the copy back goes to the slice it names on return.
        callee = ("op" if structs else "oi") if use == "output" else ("ip" if structs else "ii")
        call = f"{callee}({sl}, {1000 + c});" if use == "output" else f"{callee}({sl});"
        body += ["fork", f"  {call}", "  #1 i = i - 1;", "join"]
        if use == "output":
            got = [(1000 + c, 1), (1001 + c, 2)] if structs else [1000 + c, 1001 + c]
        else:
            read = [model.read(k) for k in indices(bounds, fname, i)]
            if structs:
                got = [(read[0][0] + 1, read[0][1]), (read[1][0], read[1][1] + 2)]
            else:
                got = [read[0] + 1, read[1] + 2]
        after = i - 1 if i0 is not None else i
        for k, x in zip(indices(bounds, fname, after), got):
            model.write(k, x)
    lines = []
    if use == "nba":
        lines.append(f"{tag} before {nba_before}")
    if line is not None:
        lines.append(f"{tag} {line}")
    body.append(f'$display("{tag} after %s", {show});')
    lines.append(f"{tag} after {model.show()}")
    init = f"{target} = {Model(bounds, structs, first).literal()};"
    if label == "ref":
        # The case runs in a task given the static array sr through ref.
        name = f"ref_{container}_{c}"
        subs.append(f"  task automatic {name}(ref int r [5:2]);")
        subs += ["    " + s for s in [init] + body]
        subs.append("  endtask")
        return [f"{pad}// {tag}", f"{pad}{name}(sr);"], lines
    return [f"{pad}// {tag}"] + [pad + s for s in [init] + body], lines


def files():
    out = {}
    for container, where in CONTAINERS.items():
        name = f"{NAME}_{container}"
        pad = "      " if container == "fork" else "    "
        subs, stmts, exp = [], [f"{pad}automatic int la [2:5];"], []
        c = 0
        for place in PLACES:
            for form in FORMS:
                for use in USES:
                    if use == "nba" and not place[4]:
                        continue  # an NBA to an automatic variable is illegal (10.4.2)
                    s, e = case(container, place, form, use, c, pad, subs)
                    stmts += s
                    exp += e
                    c += 1
        head = [
            "/*",
            f":name: {name}",
            ":description: slices of fixed-size unpacked arrays read, written, compared, written "
            f"by NBAs and passed to input, output and inout arguments, in {where}",
            ":tags: proc",
            ":type: simulation",
            ":lrm: 1800-2023 7.4.6 7.6 10.4.2 13.5 4.9.7",
            ":oracle: spec",
            ":spec_check: ai",
            ":fourstate: no",
            ":nondeterministic: no",
            f":generator: gen_array_slice_matrix.py@{VERSION}",
            f":expected_stdout: @{name}.expected",
            "*/",
            "// Generated by tools/gen_array_slice_matrix.py; change the generator, not this "
            "file.",
            "`timescale 1ns/1ns",
            "module top;",
            "  typedef struct { int n; byte t; } pair_t;",
            "  typedef struct { int pad; int arr [5:2]; } rec_t;",
            "  int sa [2:5];",
            "  int sd [5:2];",
            "  int m [0:2][2:5];",
            "  rec_t rec;",
            "  int sr [5:2];",
            "  pair_t sp [2:5];",
            "  int b2 [0:1], s2 [0:1];",
            "  pair_t p2 [0:1], q2 [0:1];",
            "  int i;",
        ]
        sv = head + SUBROUTINES.rstrip("\n").split("\n") + subs
        if container == "initial":
            sv += ["  initial begin"] + stmts + ["  end"]
        elif container == "task":
            sv += ["  initial run();", "  task static run();"] + stmts + ["  endtask"]
        elif container == "always":
            sv += ["  logic go = 1'b0;", "  initial #1 go = 1'b1;",
                   "  always @(posedge go) begin"] + stmts + ["  end"]
        else:
            sv += ["  initial", "    fork", "      begin"] + stmts + ["      end", "    join"]
        sv += ["endmodule", ""]
        out[f"{name}.sv"] = "\n".join(sv)
        out[f"{name}.expected"] = "\n".join(exp) + "\n"
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
                where = path.relative_to(OUT.parent.parent.parent)
                print(f"gen_array_slice_matrix: {where} is out of date; run "
                      "tools/gen_array_slice_matrix.py", file=sys.stderr)
                bad += 1
        else:
            path.write_text(text, encoding="utf-8")
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main())
