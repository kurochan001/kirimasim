#!/usr/bin/env python3
"""Generates the matrix test of unique and priority violations reported from
static initializers, of TEST_SUITE.md ("Mechanism matrix"), in
tests/suite/proc/.

  tools/gen_static_init_report_matrix.py           # writes the files
  tools/gen_static_init_report_matrix.py --check   # exit 1 if a file differs from what it would write

The initial values of static variables are set before any procedure
starts (6.8, 10.5; SCHEDULER_SPEC.md 6.1), by static-init processes.
Like any process, one has a pending queue; it never waits, so it reaches
no flush point, and what it put there is settled in the Observed region
of time 0 (SCHEDULER_SPEC.md 15.3): unique and priority violations are
reported then (SYSTEM_TASKS.md 10.4). Deferred assertions are left out:
their actions run after the function returns, and how 13.4.4 applies to
a function that static initialization calls is not decided yet (not
confirmed).

The axes: what the function called by the initializer does (a unique
case that no item matches, one with two items that match, a unique0
case that no item matches, which is no violation, a priority case that
no item matches, a unique if with two true conditions, a priority if
with none); where the variable is (a module, a package, the
compilation unit, an initial procedure's static variable, a static
function's static variable, a generate block, a module the top
instantiates), one file each, so that the reports of a file come from one
scope in the order of its declarations (SCHEDULER_SPEC.md 3, step 1).
Each initializer's value is printed too. The reports of a package's and
of the compilation unit's functions are not compared: how SYSTEM_TASKS.md
3.8 writes their hierarchical names is not decided yet (not confirmed).

The expected outputs follow from the rules above, computed below. For
reference (checked 2026-10-18): Verilator 5.052 prints every value but
one: for the unique if with two true conditions it runs neither branch,
where 12.4.2 runs the first one.
"""

from __future__ import annotations

import argparse
import sys
from pathlib import Path

OUT = Path(__file__).resolve().parent.parent / "tests" / "suite" / "proc"
NAME = "static_init_report_matrix"
# The version of the rules below (TEST_SUITE.md, :generator:). Raise it
# whenever they change.
VERSION = 1

# (name, the body of a function automatic int of an int s, called with 5;
# the value it returns; the report, None, or (region, text) where REG marks
# the line of the reporting statement). Lines are indented by four.
KINDS = [
    ("unique_none", ["REGunique case (s) 0: return 1; 1: return 2; endcase", "return 10;"], 10,
     ("observed", "unique case violation: no item matches")),
    ("unique_two", ["REGunique case (s) 5: return 21; 4, 5: return 22; endcase", "return 20;"], 21,
     ("observed", "unique case violation: more than one item matches")),
    ("unique0_none", ["REGunique0 case (s) 0: return 1; endcase", "return 30;"], 30, None),
    ("priority_none", ["REGpriority case (s) 0: return 1; endcase", "return 40;"], 40,
     ("observed", "priority case violation: no item matches")),
    ("unique_if_two", ["REGunique if (s > 1) return 51; else if (s > 2) return 52;", "return 50;"],
     51, ("observed", "unique if violation: more than one condition is true")),
    ("priority_if_none", ["REGpriority if (s == 0) return 1; else if (s == 1) return 2;",
                          "return 60;"], 60,
     ("observed", "priority if violation: no condition is true")),
]
PLACES = ["module", "package", "unit", "initial", "function", "generate", "sub"]
REGIONS = ["observed"]


def place_file(place):
    """One file for one place, so that every report comes from one scope,
    in the order of its declarations (SCHEDULER_SPEC.md 3, step 1)."""
    name = f"{NAME}_{place}"
    unit, package, sub, module, initial, display, expected, marks = [], [], [], [], [], [], [], []
    fn_lines, gen_lines, h_lines = [], [], []
    for kname, body, value, report in KINDS:
        fn = f"f_{kname}"
        text = [f"function automatic int {fn}(input int s);"]
        text += [f"  {line}" for line in body] + ["endfunction"]
        var = f"v_{kname}"
        if place == "package":
            package += [f"  {x}" for x in text] + [f"  int {var} = {fn}(5);"]
            read, scope = f"p::{var}", f"p::{fn}"
        elif place == "unit":
            unit += text + [f"int {var} = {fn}(5);"]
            read, scope = var, fn
        elif place == "sub":
            sub += [f"  {x}" for x in text] + [f"  int {var} = {fn}(5);"]
            read, scope = f"s.{var}", f"top.s.{fn}"
        else:
            module.extend(f"  {x}" for x in text)
            scope = f"top.{fn}"
            if place == "module":
                module.append(f"  int {var} = {fn}(5);")
                read = var
            elif place == "generate":
                gen_lines.append(f"    int {var} = {fn}(5);")
                read = f"g.{var}"
            elif place == "initial":
                initial.append(f"    static int {var} = {fn}(5);")
                read = var
            else:
                h_lines.append(f"    static int {var} = {fn}(5);")
                read = f"h_{kname}()"
                fn_lines += [f"  function int h_{kname}();", f"    return {var};",
                             "  endfunction"]
        display.append(f'    $display("{kname} %0d", {read});')
        expected.append(f"{kname} {value}")
        if report:
            marks.append((fn, report[0], report[1], scope))
    if gen_lines:
        module += ["  if (1) begin : g"] + gen_lines + ["  end"]
    if h_lines:
        # One static function whose static variables call the functions,
        # and functions that read them.
        module += ["  function int h_all();"] + h_lines + ["    return 0;", "  endfunction"]
        module += [x.replace("    return v_", "    return h_all.v_") for x in fn_lines]
    checked = place not in ("package", "unit")
    lines = [
        "/*",
        f":name: {name}",
        ":description: unique and priority violations in functions that the initializers of "
        "static variables call are reported as from any process, at time 0: static variables "
        f"of {DESCRIBE[place]}",
        ":tags: proc unique",
        ":type: simulation",
        ":lrm: 1800-2023 6.8 10.5 12.4.2 12.4.2.1 12.5.3",
        ":oracle: spec",
        ":spec_check: ai",
        ":fourstate: yes",
        ":nondeterministic: no",
        f":generator: gen_static_init_report_matrix.py@{VERSION}",
        ":issue: 5",
        ":top_module: top",
        f":expected_stdout: @{name}.expected",
    ]
    if checked:
        lines.append(f":expected_stderr: @{name}.stderr")
    lines += ["*/",
              "// Generated by tools/gen_static_init_report_matrix.py; change the generator, not "
              "this file."]
    if not checked:
        lines += ["// The reports are not compared: SYSTEM_TASKS.md 3.8 does not say yet how the",
                  "// hierarchical name of a subroutine of a package or of the compilation unit",
                  "// is written (not confirmed)."]
    lines += ["`timescale 1ns/1ns"]
    if package:
        lines += ["package p;"] + package + ["endpackage", ""]
    if unit:
        lines += unit + [""]
    if sub:
        lines += ["module sub;"] + sub + ["endmodule", ""]
    lines += ["module top;"] + module + (["  sub s();"] if sub else [])
    lines += ["  initial begin"] + initial + display + ["  end", "endmodule", ""]
    # The report lines, from where REG marks each reporting statement: the
    # violations settle in Observed, in the order of the declarations.
    where, current = {}, None
    for number, line in enumerate(lines, 1):
        if "function automatic int f_" in line:
            current = line.split("function automatic int ")[1].split("(")[0]
        if "REG" in line:
            where[current] = number
    lines = [line.replace("REG", "") for line in lines]
    reports = []
    for region in REGIONS:
        for fn, rgn, text, scope in marks:
            if rgn == region:
                kind = "warning" if region == "observed" else "error"
                reports.append(f"{name}.sv:{where[fn]}: {kind}: {text} [{scope} @ 0ns]")
    out = {f"{name}.sv": "\n".join(lines), f"{name}.expected": "\n".join(expected) + "\n"}
    if checked:
        out[f"{name}.stderr"] = "\n".join(reports) + "\n"
    return out


DESCRIBE = {
    "module": "a module",
    "package": "a package",
    "unit": "the compilation unit",
    "initial": "an initial procedure",
    "function": "a static function",
    "generate": "a generate block",
    "sub": "a module that the top instantiates",
}


def files():
    out = {}
    for place in PLACES:
        out.update(place_file(place))
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
                print(f"gen_static_init_report_matrix: "
                      f"{path.relative_to(OUT.parent.parent.parent)} is out of date; run "
                      "tools/gen_static_init_report_matrix.py", file=sys.stderr)
                bad += 1
        else:
            path.write_text(text, encoding="utf-8")
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main())
