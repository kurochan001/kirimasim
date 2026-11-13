#!/usr/bin/env python3
# Copyright (c) 2026 The kirimasim Authors
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

"""Generates the matrix tests of the flush points of deferred immediate
assertions (TEST_SUITE.md, "Mechanism matrix") in tests/suite/proc/.

  tools/gen_deferred_flush_point_matrix.py           # writes the files
  tools/gen_deferred_flush_point_matrix.py --check   # exit 1 if a file differs from what it would write

The rule: 16.4.1 and 16.4.2 of the standard (SCHEDULER_SPEC.md 15.3 and
15.4). The report of a deferred assertion goes on the pending queue of
the process that runs it, also from a subroutine the process calls. It
is settled in the Observed region of the time slot (Postponed for
final), unless the process reaches a flush point first, which discards
the whole queue. The flush points are: resuming from an event control
or a wait statement, an always_comb or always_latch procedure starting
again, and a disable that ends the outermost scope of the process.
Resuming after a delay (#0 included), after a join, after wait fork, or
after the end of an inner block that is disabled is not a flush point;
nor is resuming in a later time slot, as the report was settled before.

The axes: the kind of assertion (assert #0, assume #0, cover #0, assert
final), the container of the assertion statement (the process itself, a
function, a task, a static class method), and the scenario: what the
process does in the same time slot after its first report, and the
glitches of always_comb, always_latch and an always procedure with an
event control at its top, each beside a control without the glitch. A
task is not called from always_comb or always_latch (9.2.2.2.2), so
those scenarios use the function in the task tests.

Each scenario runs in a time slot of its own and settles at most one
report there: after a flush point the second statement reports, and
where the first report is kept the second statement does not report
(its condition is false), so the output does not depend on the order in
which the reports of one time slot are settled. The action prints the
time and the label; for final it runs in the Postponed region, where it
may print but not write a variable. The expected output is computed
here from the rule above, without reading kirimasim's code. A person
checks it once for all the tests made from it (TEST_SUITE.md, "Sampling
by people").
"""

import argparse
import sys
from pathlib import Path

OUT = Path(__file__).resolve().parent.parent / "tests" / "suite" / "proc"
# The version of the rules below (TEST_SUITE.md, :generator:). Raise it
# whenever they change.
VERSION = 1

KINDS = ["assert", "assume", "cover", "final"]
CONTAINERS = ["process", "function", "task", "method"]

# (key, whether the first report is kept). The second statement reports
# only where the first is discarded, and in later_slot, where it runs in
# the next time slot.
SEQUENCES = [("event", False), ("wait", False), ("delay0", True), ("join", True),
             ("wait_fork", True), ("later_slot", True), ("disable_outer", False),
             ("disable_inner", True)]
# (key, the procedure, whether the value changes twice in one time slot).
GLITCHES = [("comb_glitch", "always_comb", True), ("comb_control", "always_comb", False),
            ("latch_glitch", "always_latch", True), ("latch_control", "always_latch", False),
            ("at_glitch", "always", True), ("at_control", "always", False)]


def assertion(kind, cond, label):
    """The statement that reports when cond holds; label and cond are SV text."""
    action = f'$display("%0t %s", $time, {label});'
    if kind == "cover":
        return f"cover #0 ({cond}) {action}"
    word = {"assert": "assert #0", "assume": "assume #0", "final": "assert final"}[kind]
    return f"{word} (!({cond})) else {action}"


def report(kind, container, label, cond, timed):
    """The statement of the process that reports label when cond holds.
    timed: the caller may not call a task (always_comb, always_latch)."""
    if container == "process":
        return assertion(kind, cond, f'"{label}"')
    if container == "function" or (container == "task" and timed):
        return f'check_f("{label}", {cond});'
    if container == "task":
        return f'check_t("{label}", {cond});'
    return f'probe_c::check("{label}", {cond});'


def test(kind, container):
    name = f"deferred_flush_point_{kind}_{container}"
    what = {"assert": "assert #0", "assume": "assume #0", "cover": "cover #0",
            "final": "assert final"}[kind]
    where = {"process": "the process itself", "function": "a function", "task": "a task",
             "method": "a static class method"}[container]
    sv = ["/*", f":name: {name}",
          f":description: the reports of {what} in {where} are discarded at the flush points of "
          "the process (an event control or wait resuming, always_comb or always_latch starting "
          "again, a disable of its outermost scope) and kept otherwise",
          ":tags: proc", ":type: simulation", ":lrm: 1800-2023 16.4.1 16.4.2 9.2.2.2.2",
          ":oracle: spec", ":spec_check: ai", ":fourstate: no", ":nondeterministic: no",
          f":generator: gen_deferred_flush_point_matrix.py@{VERSION}", ":top_module: top",
          ":issue: 62", f":expected_stdout: @{name}.expected", "*/",
          "// Generated by tools/gen_deferred_flush_point_matrix.py; change the generator, not "
          "this file.", "`timescale 1ns/1ns", "module top;"]
    body, expected = [], []
    if container in ("function", "task"):
        body += ["function automatic void check_f(string label, bit r);",
                 "  " + assertion(kind, "r", "label"), "endfunction"]
    if container == "task":
        body += ["task automatic check_t(string label, bit r);",
                 "  " + assertion(kind, "r", "label"), "endtask"]
    if container == "method":
        body += ["class probe_c;", "  static function void check(string label, bit r);",
                 "    " + assertion(kind, "r", "label"), "  endfunction", "endclass"]
    body.append("event never;")
    slot = 0
    for key, kept in SEQUENCES:
        slot += 10
        a = report(kind, container, f"{key} first", "1'b1", False)
        later = key == "later_slot"
        b = report(kind, container, f"{key} second", "1'b0" if kept and not later else "1'b1",
                   False)
        if kept:
            expected.append(f"{slot} {key} first")
        if not kept and key != "disable_outer":
            expected.append(f"{slot} {key} second")
        if later:
            expected.append(f"{slot + 1} {key} second")
        if key == "event":
            body += [f"event e_{key};",
                     f"initial begin #{slot}; {a} @(e_{key}); {b} end",
                     f"initial begin #{slot}; #0 -> e_{key}; end"]
        elif key == "wait":
            body += [f"bit go_{key};",
                     f"initial begin #{slot}; {a} wait (go_{key}); {b} end",
                     f"initial begin #{slot}; #0 go_{key} = 1; end"]
        elif key == "delay0":
            body += [f"initial begin #{slot}; {a} #0; {b} end"]
        elif key == "join":
            body += [f"initial begin #{slot}; {a} fork #0; join {b} end"]
        elif key == "wait_fork":
            body += [f"initial begin #{slot}; {a} fork #0; join_none wait fork; {b} end"]
        elif key == "later_slot":
            body += [f"event e_{key};",
                     f"initial begin #{slot}; {a} @(e_{key}); {b} end",
                     f"initial begin #{slot + 1}; -> e_{key}; end"]
        elif key == "disable_outer":
            body += [f"initial begin : s_{key} #{slot}; {a} @(never); end",
                     f"initial begin #{slot}; #0 disable s_{key}; end"]
        else:
            body += [f"initial begin : s_{key} #{slot}; begin : inner {a} @(never); end {b} end",
                     f"initial begin #{slot}; #0 disable s_{key}.inner; end"]
    for key, proc, glitch in GLITCHES:
        slot += 10
        v = f"v_{key}"
        stmt = report(kind, container, key, f"{v} == 1", proc != "always")
        if not glitch:
            expected.append(f"{slot} {key}")
        body.append(f"logic [1:0] {v} = 0;")
        if proc == "always_comb":
            body.append(f"always_comb {stmt}")
        elif proc == "always_latch":
            body.append(f"always_latch if (1'b1) {stmt}")
        else:
            body.append(f"always @({v}) {stmt}")
        second_change = "#0" if glitch else "#1"
        body.append(f"initial begin #{slot}; {v} = 1; {second_change} {v} = 2; end")
    sv += ["  " + x for x in body] + ["endmodule"]
    return name, sv, expected


def files():
    out = {}
    for kind in KINDS:
        for container in CONTAINERS:
            name, sv, expected = test(kind, container)
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
                print("gen_deferred_flush_point_matrix: "
                      f"{path.relative_to(OUT.parent.parent.parent)} is out of date; run "
                      "tools/gen_deferred_flush_point_matrix.py", file=sys.stderr)
                bad += 1
        else:
            path.write_text(text, encoding="utf-8")
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main())
