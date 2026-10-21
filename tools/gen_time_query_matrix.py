#!/usr/bin/env python3
# Copyright (c) 2026 The kirimasim Authors
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

"""Generates the matrix test of what a call learns of its design element
(TEST_SUITE.md, "Mechanism matrix") in tests/suite/proc/.

  tools/gen_time_query_matrix.py           # writes the files
  tools/gen_time_query_matrix.py --check   # exit 1 if a file differs from what it would write

The rules (SYSTEM_TASKS.md 3.8, 5.1, 5.2):

- $time is the time in the time unit of the design element the call is
  in, rounded to an integer with a half away from zero; $stime is its low
  32 bits; $realtime is the same time as a real.
- $timeunit and $timeprecision give the exponent of the unit and the
  precision: of the caller's design element with no argument, of the
  instance named, of the compilation unit for $unit, and of the
  simulation for $root, whose unit and precision are both the smallest
  precision of the design.
- %m prints the hierarchical name of the scope that calls the task, in
  which an unnamed fork is no scope; %l prints work.<module>.

The axes: the module the call is in (top, with 1us/10ns, and fine, with
1ns/1ps, an instance u of top), and the container of the call (an initial
procedure, a task, a function, a fork branch, a task in a generate
block). The compilation unit has 10ns/1ns. Each module's cells run at one
time of its own; at the end, fine shows $time and $stime past 2^32 of its
units, where $stime keeps the low 32 bits.

The expected outputs follow from the rules above, without reading
kirimasim's code. For reference, checked 2026-10-21: Icarus 14.0 devel and
13.0 do not have $timeunit and $timeprecision. Verilator 5.053 devel does
not take $unit or $root as their argument, gives every module the
design's smallest precision, and gave $timeunit(top.u) the caller's unit;
the rest of its output agrees.
"""

import argparse
import sys
from pathlib import Path

OUT = Path(__file__).resolve().parent.parent / "tests" / "suite" / "proc"
# The version of the rules below (TEST_SUITE.md, :generator:). Raise it
# whenever they change.
VERSION = 1
NAME = "time_query_matrix"

CONTAINERS = ["initial", "task", "function", "fork", "generate"]
UNIT_SCALE = (-8, -9)  # the compilation unit: 10ns / 1ns
# module: (unit, precision, the delay before its cells, in its unit, and
# that delay in ps)
MODULES = {"top": (-6, -8, "2.5", 2_500_000), "fine": (-9, -12, "3.7", 3_700)}
ROOT = min(p for _, p, _, _ in MODULES.values())  # the smallest precision


def time_in(ps, unit):
    """$time, $stime and $realtime at ps picoseconds in a unit 10^unit s."""
    scale = 10 ** (unit + 12)
    q, r = divmod(ps, scale)
    t = q + (1 if 2 * r >= scale else 0)
    return t, t % (1 << 32), ps / scale


def cell(module, container):
    """The statements of one cell and the line it prints."""
    stmt = ('$display("%m %l: %0d %0d %.1f %0d %0d %0d %0d %0d %0d %0d %0d", $time, $stime, '
            "$realtime, $timeunit, $timeprecision, $timeunit(top.u), $timeprecision(top.u), "
            "$timeunit($unit), $timeprecision($unit), $timeunit($root), $timeprecision($root));")
    unit, prec, _, ps = MODULES[module]
    t, st, rt = time_in(ps, unit)
    scope = {"top": "top", "fine": "top.u"}[module]
    where = {"initial": scope, "task": f"{scope}.t_{container}",
             "function": f"{scope}.f_{container}", "fork": scope,
             "generate": f"{scope}.g.t"}[container]
    line = (f"{where} work.{module}: {t} {st} {rt:.1f} {unit} {prec} -9 -12 "
            f"{UNIT_SCALE[0]} {UNIT_SCALE[1]} {ROOT} {ROOT}")
    return stmt, line


def module_text(module, sub):
    unit, prec, delay, _ = MODULES[module]
    names = {-6: "1us", -8: "10ns", -9: "1ns", -12: "1ps"}
    out = (f"module {module};\n  timeunit {names[unit]};\n  timeprecision {names[prec]};\n")
    if sub:
        out += "  fine u();\n"
    calls = []
    for container in CONTAINERS:
        stmt, _ = cell(module, container)
        if container == "initial":
            calls.append(stmt)
        elif container == "task":
            out += f"  task t_{container};\n    {stmt}\n  endtask\n"
            calls.append(f"t_{container};")
        elif container == "function":
            out += f"  function void f_{container};\n    {stmt}\n  endfunction\n"
            calls.append(f"f_{container}();")
        elif container == "fork":
            calls.append(f"fork {stmt} join")
        else:
            out += f"  if (1) begin : g\n    task t;\n      {stmt}\n    endtask\n  end\n"
            calls.append("g.t;")
    out += f"  initial begin\n    #{delay};\n"
    out += "".join(f"    {c}\n" for c in calls)
    if module == "fine":
        # Past 2^32 ns: $stime keeps the low 32 bits.
        out += ("    #(64'd4294967300 - 4);\n"
                '    $display("fine past 2^32: %0d %0d", $time, $stime);\n')
    out += "  end\nendmodule\n"
    return out


def files():
    sv = (f"/*\n:name: {NAME}\n:description: $time, $stime, $realtime, $timeunit and "
          "$timeprecision (with no argument, an instance, $unit and $root), %m and %l, called "
          "from an initial procedure, a task, a function, a fork branch and a task in a "
          "generate block of two modules with their own time units\n:tags: proc\n"
          ":type: simulation\n:lrm: 1800-2023 20.3 20.4.1 21.2.1.1 21.2.1.5 3.14.2\n"
          ":oracle: spec\n:spec_check: ai\n:fourstate: no\n:nondeterministic: no\n"
          f":generator: gen_time_query_matrix.py@{VERSION}\n:top_module: top\n"
          f":expected_stdout: @{NAME}.expected\n*/\n"
          "// Generated by tools/gen_time_query_matrix.py; change the generator, not this file.\n"
          "timeunit 10ns;\ntimeprecision 1ns;\n")
    sv += module_text("fine", False) + module_text("top", True)
    # fine's cells at 3.7ns, top's at 2.5us, then fine's line past 2^32 ns.
    exp = "".join(cell(m, c)[1] + "\n" for m in ("fine", "top") for c in CONTAINERS)
    t, st, _ = time_in(3_700 + (4294967300 - 4) * 1000, -9)
    exp += f"fine past 2^32: {t} {st}\n"
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
                print(f"gen_time_query_matrix: {path.relative_to(OUT.parent.parent.parent)} is "
                      "out of date; run tools/gen_time_query_matrix.py", file=sys.stderr)
                bad += 1
        else:
            path.write_text(text, encoding="utf-8")
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main())
