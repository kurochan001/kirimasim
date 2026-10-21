#!/usr/bin/env python3
# Copyright (c) 2026 The kirimasim Authors
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

"""Generates the user-defined primitive matrix tests of TEST_SUITE.md
("Mechanism matrix", "4-state matrix") in tests/suite/nets/.

  tools/gen_udp_matrix.py           # writes the files
  tools/gen_udp_matrix.py --check   # exit 1 if a file differs from what it would write

The axes: combinational UDPs with the symbols 0, 1, x, ? and b on every
combination of 0, 1, x and z, rows that do not match giving x, and a
drive strength against another driver; a level-sensitive latch; an
edge-sensitive flip-flop with r, f, (vw) edges, * and states, through
every change of its clock and data; p and n edges; a level row and an
edge row that disagree (29.9); the initial statement; and the containers
of an instance (a module body, a generate block, a child module and an
array of instances, whose connections slang slices). A second test
changes two inputs of one instance at once; the standard does not say in
which order they are seen, so it allows both (VALUE_SEMANTICS.md 5.9).

The expected outputs follow from the rules of VALUE_SEMANTICS.md 5.9,
written again below without reading kirimasim's code. A person checks
the rules against chapter 29 once for all the tests made from them
(TEST_SUITE.md, "Sampling by people").
"""

import argparse
import itertools
import sys
from pathlib import Path

from gen_resolution_matrix import resolve, value_of

OUT = Path(__file__).resolve().parent.parent / "tests" / "suite" / "nets"
# The version of the rules below (TEST_SUITE.md, :generator:). Raise it
# whenever they change.
VERSION = 1

VALUES = ["0", "1", "x", "z"]


# --- The rules of VALUE_SEMANTICS.md 5.9 ------------------------------------------

def known(v):
    return "x" if v == "z" else v


def level(sym, v):
    """Table 29-1: 0, 1 and x match themselves, ? any value, b 0 or 1."""
    return {"0": v == "0", "1": v == "1", "x": v == "x", "?": True, "b": v in "01"}[sym]


def fields(inputs):
    """One field per input: a symbol, or the two values of (vw)."""
    out, i = [], 0
    while i < len(inputs):
        if inputs[i] == "(":
            out.append(inputs[i + 1:i + 3])
            i += 4
        else:
            out.append(inputs[i])
            i += 1
    return out


EDGE_LETTERS = {"r": {("0", "1")}, "f": {("1", "0")},
                "p": {("0", "1"), ("0", "x"), ("x", "1")},
                "n": {("1", "0"), ("1", "x"), ("x", "0")}}


def is_edge(f):
    return len(f) == 2 or f in "rfpn*"


def edge(f, a, b):
    """Whether an edge field matches a change from a to b (table 29-1)."""
    if len(f) == 2:
        return level(f[0], a) and level(f[1], b)
    if f == "*":
        return True
    return (a, b) in EDGE_LETTERS[f]


def parse(table):
    """Rows (inputs, state, output) from 'inputs : output' or
    'inputs : state : output' lines with spaces between the symbols."""
    rows = []
    for line in table:
        parts = [p.replace(" ", "") for p in line.split(":")]
        if len(parts) == 2:
            rows.append((parts[0], None, parts[1]))
        else:
            rows.append((parts[0], parts[1], parts[2]))
    return rows


def comb(rows, inputs):
    """29.4: the output of a row that matches, x if none does."""
    vals = [known(v) for v in inputs]
    for ins, _, out in rows:
        fs = fields(ins)
        if all(not is_edge(f) and level(f, v) for f, v in zip(fs, vals)):
            return out
    return "x"


def step(rows, state, vals, k, frm):
    """29.5, 29.6, 29.9: a level row that matches decides; else an edge row
    on input k; else x. '-' keeps the state."""
    found_edge = None
    for ins, st, out in rows:
        if not level(st, state):
            continue
        fs = fields(ins)
        edges = [i for i, f in enumerate(fs) if is_edge(f)]
        others = all(level(f, v) for i, (f, v) in enumerate(zip(fs, vals)) if i not in edges)
        if not others:
            continue
        nxt = state if out == "-" else out
        if not edges:
            return nxt
        if edges == [k] and edge(fs[k], frm, vals[k]) and found_edge is None:
            found_edge = nxt
    return "x" if found_edge is None else found_edge


def change(rows, state, prev, cur, order):
    """The changed inputs one at a time, in `order` (VALUE_SEMANTICS.md 5.9
    takes port order)."""
    vals = [known(v) for v in prev]
    for k in order:
        to = known(cur[k])
        if to == vals[k]:
            continue
        frm, vals[k] = vals[k], to
        state = step(rows, state, vals, k, frm)
    return state


# --- The UDPs ----------------------------------------------------------------------

MUX = ["0 0 ? : 0", "0 1 ? : 1", "1 ? 0 : 0", "1 ? 1 : 1", "? 0 0 : 0", "? 1 1 : 1"]
MAJ = ["b 0 0 : 0", "0 b 0 : 0", "0 0 b : 0", "1 1 ? : 1", "1 ? 1 : 1", "? 1 1 : 1"]
AND2 = ["0 ? : 0", "? 0 : 0", "1 1 : 1"]
LATCH = ["1 0 : ? : 0", "1 1 : ? : 1", "0 ? : ? : -"]
DFF = ["0 r : ? : 0", "1 r : ? : 1", "? f : ? : -", "? (1x) : ? : -", "? (x0) : ? : -",
       "* ? : ? : -", "0 (0x) : 0 : -", "1 (0x) : 1 : -", "0 (x1) : 0 : -", "1 (x1) : 1 : -"]
TFF = ["1 p : 0 : 1", "1 p : 1 : 0", "0 p : ? : -", "? n : ? : -", "(??) ? : ? : -"]
PRIO = ["1 ? : ? : 1", "0 r : ? : 0", "1 r : ? : 0", "? f : ? : -", "(??) ? : ? : -"]


def primitive(name, ports, table, sequential=False, initial=None):
    """A UDP in the non-ANSI form of 29.3 (Icarus 13.0 does not take an
    initial statement in the ANSI form)."""
    out = "q" if sequential else "y"
    sv = f"primitive {name} ({out}, {', '.join(ports)});\n  output {out};\n"
    if sequential:
        sv += "  reg q;\n"
    sv += f"  input {', '.join(ports)};\n"
    if initial is not None:
        sv += f"  initial q = 1'b{initial};\n"
    sv += "  table\n" + "".join(f"    {row};\n" for row in table) + "  endtable\nendprimitive\n"
    return sv


def header(name, description, lrm, nondeterministic="no", note=""):
    return (f"/*\n:name: {name}\n:description: {description}\n:tags: nets\n:type: simulation\n"
            f":lrm: 1800-2023 {lrm}\n:oracle: spec iverilog@13.0\n:spec_check: ai\n"
            f":fourstate: yes\n:nondeterministic: {nondeterministic}\n"
            f":generator: gen_udp_matrix.py@{VERSION}\n:expected_stdout: @{name}.expected\n*/\n"
            "// Generated by tools/gen_udp_matrix.py; change the generator, not this file.\n"
            + note)


def combinational():
    name = "udp_combinational"
    note = ("// Icarus Verilog 13.0 takes neither a drive strength nor an array of UDP\n"
            "// instances; with those two removed, it gives the other columns.\n")
    sv = header(name, "Combinational UDPs with the symbols 0, 1, x, ? and b on every combination "
                "of 0, 1, x and z, unmatched rows giving x, a drive strength, and instances in "
                "a generate block, a child module and an array", "29.3 29.4 29.3.6 28.3.6 23.3.3.5",
                note=note)
    sv += primitive("mux", ["s", "a", "b"], MUX)
    sv += primitive("maj", ["a", "b", "c"], MAJ)
    sv += primitive("and2", ["a", "b"], AND2)
    sv += ("module child(output y, input a, b);\n  and2 uc (y, a, b);\nendmodule\n"
           "module top;\n  logic a, b, c;\n  logic [3:0] va;\n"
           "  wire m, j, weak_and, from_child;\n  wire [3:0] arr;\n"
           "  mux um (m, a, b, c);\n  maj uj (j, a, b, c);\n"
           "  and2 (weak1, weak0) uw (weak_and, a, b);\n  assign (pull1, pull0) weak_and = c;\n"
           "  generate\n    if (1) begin : gen\n      wire o;\n      and2 ug (o, b, c);\n"
           "    end\n  endgenerate\n"
           "  child u (from_child, a, c);\n"
           "  and2 g [3:0] (arr, va, b);\n"
           "  initial begin\n")
    exp = ""
    fmt = "mux=%b maj=%b weak=%b gen=%b child=%b arr=%b"
    args = "m, j, weak_and, gen.o, from_child, arr"
    for x, y, z in itertools.product(VALUES, VALUES, VALUES):
        va = x + y + z + "1"  # va[3] = x, ..., va[0] = 1
        sv += (f"    a = 1'b{x}; b = 1'b{y}; c = 1'b{z}; va = 4'b{va};\n"
               f"    #1 $display(\"{x}{y}{z} {fmt}\", {args});\n")
        mux = comb(parse(MUX), [x, y, z])
        maj = comb(parse(MAJ), [x, y, z])
        a2 = comb(parse(AND2), [x, y])
        weak = value_of(resolve("wire", [(a2, 3, 3), (z, 5, 5)]))
        gen = comb(parse(AND2), [y, z])
        child = comb(parse(AND2), [x, z])
        arr = "".join(comb(parse(AND2), [va[3 - i], y]) for i in reversed(range(4)))
        exp += (f"{x}{y}{z} mux={mux} maj={maj} weak={weak} gen={gen} child={child} "
                f"arr={arr}\n")
    sv += "  end\nendmodule\n"
    return name, sv, exp


# Each step changes one input of one instance; the line after it shows
# every output.
SEQ = {
    # name: (udp, table, ports, initial)
    "latch": ("latch", LATCH, ["en", "d"], None),
    "dff": ("dff", DFF, ["d", "clk"], "0"),
    "tff": ("tff", TFF, ["t", "clk"], "1"),
    "prio": ("prio", PRIO, ["set", "clk"], None),
}
STEPS = [
    ("latch", "d", "0"), ("latch", "en", "1"), ("latch", "d", "1"), ("latch", "en", "0"),
    ("latch", "d", "0"), ("latch", "en", "x"), ("latch", "en", "1"), ("latch", "d", "z"),
    ("dff", "d", "0"), ("dff", "clk", "0"), ("dff", "clk", "1"), ("dff", "d", "1"),
    ("dff", "clk", "0"), ("dff", "clk", "1"), ("dff", "clk", "x"), ("dff", "clk", "1"),
    ("dff", "clk", "0"), ("dff", "d", "0"), ("dff", "clk", "x"), ("dff", "clk", "0"),
    ("dff", "clk", "1"), ("dff", "clk", "z"), ("dff", "d", "x"), ("dff", "clk", "0"),
    ("dff", "clk", "1"),
    ("tff", "clk", "0"), ("tff", "t", "1"), ("tff", "clk", "1"), ("tff", "clk", "0"),
    ("tff", "clk", "x"), ("tff", "clk", "1"), ("tff", "t", "0"), ("tff", "clk", "0"),
    ("tff", "clk", "1"),
    ("prio", "clk", "0"), ("prio", "set", "0"), ("prio", "clk", "1"), ("prio", "set", "1"),
    ("prio", "clk", "0"), ("prio", "clk", "1"), ("prio", "set", "0"), ("prio", "clk", "0"),
]


def sequential():
    name = "udp_sequential"
    sv = header(name, "Sequential UDPs: a level-sensitive latch, an edge-sensitive flip-flop "
                "through every change of its clock and data, p and n edges, a level row over an "
                "edge row, and initial statements", "29.3 29.5 29.6 29.7 29.8 29.9")
    for key, (udp, table, ports, init) in SEQ.items():
        sv += primitive(udp, ports, table, True, init)
    sv += "module top;\n"
    for key, (udp, table, ports, init) in SEQ.items():
        sv += f"  logic {', '.join(f'{key}_{p}' for p in ports)};\n  wire {key}_q;\n"
        sv += f"  {udp} u_{key} ({key}_q, {', '.join(f'{key}_{p}' for p in ports)});\n"
    sv += "  initial begin\n"
    keys = list(SEQ)
    fmt = " ".join(f"{k}=%b" for k in keys)
    args = ", ".join(f"{k}_q" for k in keys)
    inputs = {k: ["x"] * len(SEQ[k][2]) for k in keys}
    states = {k: SEQ[k][3] or "x" for k in keys}
    exp = ""
    sv += f"    #1 $display(\"start {fmt}\", {args});\n"
    exp += "start " + " ".join(f"{k}={states[k]}" for k in keys) + "\n"
    for key, port, val in STEPS:
        udp, table, ports, init = SEQ[key]
        k = ports.index(port)
        cur = list(inputs[key])
        cur[k] = val
        states[key] = change(parse(table), states[key], inputs[key], cur, range(len(ports)))
        inputs[key] = cur
        sv += f"    {key}_{port} = 1'b{val};\n    #1 $display(\"{key}.{port}={val} {fmt}\", {args});\n"
        exp += f"{key}.{port}={val} " + " ".join(f"{k}={states[k]}" for k in keys) + "\n"
    sv += "  end\nendmodule\n"
    return name, sv, exp


def swapped(table):
    """The rows of a two-input table with the inputs in the other order."""
    out = []
    for row in table:
        ins, rest = row.split(" : ", 1)
        a, b = ins.split(" ")
        out.append(f"{b} {a} : {rest}")
    return out


def simultaneous():
    """Data and clock changed by one assignment: the standard does not say
    in which order the UDP sees them, so both orders are allowed."""
    name = "udp_simultaneous"
    note = ("// VALUE_SEMANTICS.md 5.9 takes the changes in port order (the data first\n"
            "// here); chapter 29 does not fix one, so the expected file allows both.\n")
    sv = header(name, "A sequential UDP whose data and clock change in one assignment; both "
                "orders of the changes are allowed", "29.6 4.7", "yes", note)
    sv += primitive("dff", ["d", "clk"], DFF, True, "0")
    sv += ("module top;\n  logic d, clk;\n  wire q;\n  dff a (q, d, clk);\n"
           "  initial begin\n    {d, clk} = 2'b00;\n    #1 {d, clk} = 2'b11;\n"
           "    #1 $display(\"%b\", q);\n  end\nendmodule\n")
    results = []
    for order in ([0, 1], [1, 0]):
        start = change(parse(DFF), "0", ["x", "x"], ["0", "0"], order)
        line = change(parse(DFF), start, ["0", "0"], ["1", "1"], order) + "\n"
        if line not in results:
            results.append(line)
    return name, sv, "---\n".join(results)


def files():
    out = {}
    for name, sv, exp in [combinational(), sequential(), simultaneous()]:
        out[f"{name}.sv"] = sv
        out[f"{name}.expected"] = exp
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
                print(f"gen_udp_matrix: {path.relative_to(OUT.parent.parent.parent)} is out of "
                      "date; run tools/gen_udp_matrix.py", file=sys.stderr)
                bad += 1
        else:
            path.write_text(text, encoding="utf-8")
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main())
