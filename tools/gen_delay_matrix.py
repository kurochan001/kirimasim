#!/usr/bin/env python3
"""Generates the continuous assignment and gate delay matrix test of
TEST_SUITE.md ("Mechanism matrix") in tests/suite/cont/.

  tools/gen_delay_matrix.py           # writes the files
  tools/gen_delay_matrix.py --check   # exit 1 if a file differs from what it would write

The axes: the delayed driver (continuous assignments to a scalar net with
one, two and three delays, to a vector net with three, to a variable; a
net declaration assignment; and, gates, a not gate and a three-state gate
whose L and H take the x delay; a combinational UDP; a #0 assignment; an
assignment in a child module and in a generate block); and the changes
of its inputs (to 0, 1, x and z, pulses shorter than the delay, a change
back before the update, a change to another value before it). The
changes never fall on the time of an update, and every line is read
half a step after the changes, so no line depends on an order the
standard leaves open.

The expected outputs follow from SCHEDULER_SPEC.md 12 (10.3.3, 28.16,
table 28-9), modelled below without reading kirimasim's code: when the
right side changes, an update pending with another value is dropped; no
update is scheduled when the value is the one the target has; otherwise
one is, after the delay for the change: for one bit, to 1 rise, to 0
fall, to z turn-off, to x the least; for a vector, fall from a value
that is not 0 to 0, turn-off to all z, else rise. Until its first update,
a driver drives x (VALUE_SEMANTICS.md 5.3), so a net reads x until then. A
person checks the model once for all the tests made from it (TEST_SUITE.md,
"Sampling by people").
"""

import argparse
import sys
from pathlib import Path

from gen_gate_matrix import logic_gate, tri_gate

OUT = Path(__file__).resolve().parent.parent / "tests" / "suite" / "cont"
NAME = "delay_matrix"
# The version of the rules below (TEST_SUITE.md, :generator:). Raise it
# whenever they change.
VERSION = 2

FIRST = 0  # the first time read
END = 240  # the last time read

# The inputs: (time, variable, value). a, b and en are one bit, v four.
# The changes come in groups 20 apart, longer than every delay; inside a
# group, no two changes are a delay (2, 3, 5 or 7) apart.
CHANGES = [
    (0, "a", "0"), (0, "b", "0"), (0, "en", "0"), (0, "v", "0000"),
    (20, "a", "1"), (21, "a", "0"),      # a pulse of 1: shorter than every delay
    (40, "a", "1"), (44, "a", "0"),      # a pulse of 4: longer than 2 and 3 only
    (60, "b", "1"), (60, "en", "1"), (60, "a", "1"),
    (80, "v", "0101"), (86, "v", "0000"),
    (100, "v", "zzzz"),
    (106, "v", "0011"), (107, "v", "0111"),  # another value before the update
    (120, "a", "x"),
    (140, "a", "1"),
    (150, "en", "x"),                    # bufif1 gives H for a 1, L for a 0
    (160, "a", "0"),
    (180, "a", "z"), (180, "en", "0"),
    (200, "b", "0"),
    (220, "v", "1x00"), (226, "v", "0000"),
]


def and2(a, b):
    return logic_gate("and", [a, b])


def udp_and(a, b):
    """The UDP of the test: 0 if either is 0, 1 if both are 1, else x."""
    a = "x" if a == "z" else a
    b = "x" if b == "z" else b
    if "0" in (a, b):
        return "0"
    return "1" if a == b == "1" else "x"


def bufif1(a, en):
    v = tri_gate("bufif1", a, en)
    return "x" if v in ("L", "H") else v


# Drivers: (name, declaration, width, initial value, inputs, function,
# (rise, fall, off)). A target of one bit takes the scalar rule.
TARGETS = [
    ("s1", "  wire s1;\n  assign #5 s1 = a;\n", 1, "x", ["a"], lambda i: i["a"], (5, 5, 5)),
    ("s2", "  wire s2;\n  assign #(3, 7) s2 = a;\n", 1, "x", ["a"], lambda i: i["a"], (3, 7, 3)),
    ("s3", "  wire s3;\n  assign #(2, 5, 3) s3 = a;\n", 1, "x", ["a"], lambda i: i["a"],
     (2, 5, 3)),
    ("vv", "  wire [3:0] vv;\n  assign #(2, 5, 3) vv = v;\n", 4, "xxxx", ["v"],
     lambda i: i["v"], (2, 5, 3)),
    ("dw", "  wire #5 dw = a;\n", 1, "x", ["a"], lambda i: i["a"], (5, 5, 5)),
    ("lv", "  logic lv;\n  assign #2 lv = a;\n", 1, "x", ["a"], lambda i: i["a"], (2, 2, 2)),
    ("gn", "  wire gn;\n  and #(2, 3) ga (gn, a, b);\n", 1, "x", ["a", "b"],
     lambda i: and2(i["a"], i["b"]), (2, 3, 2)),
    ("gt", "  wire gt;\n  bufif1 #(3, 5, 2) gb (gt, a, en);\n", 1, "x", ["a", "en"],
     lambda i: bufif1(i["a"], i["en"]), (3, 5, 2)),
    ("gi", "  wire gi;\n  not #3 gnt (gi, a);\n", 1, "x", ["a"],
     lambda i: {"0": "1", "1": "0"}.get(i["a"], "x"), (3, 3, 3)),
    ("ud", "  wire ud;\n  uand #(2, 5) ug (ud, a, b);\n", 1, "x", ["a", "b"],
     lambda i: udp_and(i["a"], i["b"]), (2, 5, 2)),
    ("z0", "  wire z0;\n  assign #0 z0 = a;\n", 1, "x", ["a"], lambda i: i["a"], (0, 0, 0)),
    ("cy", "  wire cy;\n  child u (cy, a);\n", 1, "x", ["a"], lambda i: i["a"], (2, 2, 2)),
    ("gen.gy", "  generate\n    if (1) begin : gen\n      wire gy;\n      assign #(3, 5) gy = b;\n"
     "    end\n  endgenerate\n", 1, "x", ["b"], lambda i: i["b"], (3, 5, 3)),
]


def delay_for(width, now, new, delays):
    rise, fall, off = delays
    if width == 1:
        return {"1": rise, "0": fall, "z": off}.get(new, min(delays))
    if now != "0" * width and new == "0" * width:
        return fall
    if new == "z" * width:
        return off
    return rise


def simulate(target):
    """The value of a target at each read time, and the times of its
    updates, by the rule of SCHEDULER_SPEC.md 12."""
    name, _, width, init, ins, fn, delays = target
    inputs = {"a": "x", "b": "x", "en": "x", "v": "xxxx"}
    lhs, pending, updates, reads = init, None, [], {}
    times = sorted({t for t, _, _ in CHANGES})
    t = 0
    for step in range(0, END + 1):
        # Updates due at this time come before the reads half a step later;
        # no input changes at an update time (checked below).
        if pending is not None and pending[1] == step:
            lhs, pending = pending[0], None
            updates.append(step)
        if step in times:
            for when, var, val in CHANGES:
                if when == step:
                    inputs[var] = val
            new = fn(inputs)
            if pending is not None and pending[0] != new:
                pending = None
            if new != lhs and pending is None:
                d = delay_for(width, lhs, new, delays)
                pending = (new, step + d)
                if d == 0:
                    lhs, pending = new, None
                    updates.append(step)
        reads[step] = lhs
    # An update at the time of a change of the target's inputs would leave
    # their order to the simulator (4.7); a #0 update follows its change.
    changes = {when for when, var, _ in CHANGES if var in ins}
    for u in updates:
        if u in changes and delays != (0, 0, 0):
            raise SystemExit(f"gen_delay_matrix: {name} updates at {u}, when an input changes")
    return reads


def hexdigits(bits):
    """%h of a 4-bit value: its digit, x or z when all bits are, else X or Z."""
    if bits == "xxxx":
        return "x"
    if bits == "zzzz":
        return "z"
    if "x" in bits:
        return "X"
    if "z" in bits:
        return "Z"
    return format(int(bits, 2), "x")


def files():
    sv = (f"/*\n:name: {NAME}\n:description: Delays of continuous assignments, net declaration "
          "assignments, gates and UDPs: the rise, fall and turn-off delays chosen by the change, "
          "the x delay, pulses shorter than the delay, and updates dropped when the value "
          "changes again\n:tags: cont\n:type: simulation\n"
          ":lrm: 1800-2023 10.3.3 28.16 28.6 29.10 4.4.2.3\n:oracle: spec\n:spec_check: ai\n"
          ":fourstate: yes\n:nondeterministic: no\n"
          f":generator: gen_delay_matrix.py@{VERSION}\n:expected_stdout: @{NAME}.expected\n*/\n"
          "// Generated by tools/gen_delay_matrix.py; change the generator, not this file.\n"
          "// Icarus Verilog 13.0 takes the fall delay for vv going from all z to 4'h7 and from\n"
          "// 4'h7 to 4'b1x00; 10.3.3 takes the rise delay for a change that is neither from a\n"
          "// value that is not 0 to 0 nor to z (SCHEDULER_SPEC.md 12).\n"
          "`timescale 1ns/100ps\n"
          "primitive uand (y, a, b);\n  output y;\n  input a, b;\n  table\n    0 ? : 0;\n"
          "    ? 0 : 0;\n    1 1 : 1;\n  endtable\nendprimitive\n"
          "module child(output y, input i);\n  assign #2 y = i;\nendmodule\n"
          "module top;\n  logic a, b, en;\n  logic [3:0] v;\n")
    for t in TARGETS:
        sv += t[1]
    sv += "  initial begin\n"
    last = 0
    for when in sorted({t for t, _, _ in CHANGES}):
        if when > last:
            sv += f"    #{when - last};\n"
            last = when
        for w, var, val in CHANGES:
            if w == when:
                width = 4 if var == "v" else 1
                sv += f"    {var} = {width}'b{val};\n"
    sv += f"  end\n  initial begin\n    #{FIRST}.5;\n"
    names = [t[0] for t in TARGETS]
    fmt = " ".join(f"{n.replace('gen.', '')}=%{'h' if t[2] > 1 else 'b'}"
                   for n, t in zip(names, TARGETS))
    sv += (f"    for (int k = {FIRST}; k <= {END}; k++) begin\n      $display(\"%0d.5 {fmt}\", k, "
           f"{', '.join(names)});\n      #1;\n    end\n  end\nendmodule\n")
    reads = {t[0]: simulate(t) for t in TARGETS}
    exp = ""
    for step in range(FIRST, END + 1):
        cols = []
        for t in TARGETS:
            v = reads[t[0]][step]
            cols.append(f"{t[0].replace('gen.', '')}=" + (hexdigits(v) if t[2] > 1 else v))
        exp += f"{step}.5 " + " ".join(cols) + "\n"
    return {f"{NAME}.sv": sv, f"{NAME}.expected": exp}


def main():
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("--check", action="store_true")
    args = ap.parse_args()
    bad = 0
    for name, text in files().items():
        path = OUT / name
        if args.check:
            if not path.exists() or path.read_text(encoding="utf-8") != text:
                print(f"gen_delay_matrix: {path.relative_to(OUT.parent.parent.parent)} is out of "
                      "date; run tools/gen_delay_matrix.py", file=sys.stderr)
                bad += 1
        else:
            path.write_text(text, encoding="utf-8")
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main())
