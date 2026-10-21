#!/usr/bin/env python3
# Copyright (c) 2026 The kirimasim Authors
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

"""Generates the deferred assertion matrix test of TEST_SUITE.md ("Mechanism
matrix") in tests/suite/proc/.

  tools/gen_deferred_matrix.py           # writes the files
  tools/gen_deferred_matrix.py --check   # exit 1 if a file differs from what it would write

A deferred immediate assertion evaluates its expression when it runs.
Its action, one subroutine call, gets the values passed by value then,
and goes on the process's queue of pending reports. An Observed one
(#0) is confirmed in the Observed region and its action runs in the
Reactive region; a final one is confirmed and run in the Postponed
region. A process that resumes from an event control or a wait drops its
queue (a flush point); a delay does not. An assert or assume that fails
without an action calls $error; a cover has no failure (16.4, 16.4.1,
16.4.2, SCHEDULER_SPEC.md 15.2, 15.3).

The axes: assert, assume and cover, #0 and final, passing and failing;
the container (an initial procedure, a static and an automatic task, a
void function, a fork branch, an always procedure); the action (a system
task, a task, a void function, an expression argument); what follows in
the time slot (an event control resumed, a wait resumed, a #0 delay, a
resume after the Observed region caused by an Observed action), and
the $error of an action left out. Each case has its own time window.

The expected outputs follow from the rules above. A person checks them
once for all the tests made from them (TEST_SUITE.md, "Sampling by people").
"""

import argparse
import sys
from pathlib import Path

OUT = Path(__file__).resolve().parent.parent / "tests" / "suite" / "proc"
NAME = "deferred_assertion_matrix"
# The version of the rules below (TEST_SUITE.md, :generator:). Raise it
# whenever they change.
VERSION = 1

WINDOW = 10


def files():
    cases = []  # (tag, container, statements, stdout lines in order, stderr builder or None)

    def add(container, stmts, out, err=None):
        cases.append((container, stmts, out, err))

    # Kind, timing and outcome, in an initial procedure.
    for kind in ["assert", "assume", "cover"]:
        for timing in ["#0", "final"]:
            for passes in [True, False]:
                i = len(cases)
                cond = f"v == {i}" if passes else f"v != {i}"
                if kind == "cover":
                    stmt = f"cover {timing} ({cond}) $display(\"s{i} covered v=%0d\", v);"
                    action = [f"s{i} covered v={i}"] if passes else []
                else:
                    stmt = (f"{kind} {timing} ({cond}) $display(\"s{i} pass v=%0d\", v);\n"
                            f"else $display(\"s{i} fail v=%0d\", v);")
                    action = [f"s{i} {'pass' if passes else 'fail'} v={i}"]
                add("initial", [f"v = {i};", stmt, "v = v + 100;",
                                f"$display(\"s{i} after the statement\");"],
                    [f"s{i} after the statement"] + action)
    # Containers.
    for container in ["static task", "automatic task", "function", "fork branch", "always"]:
        i = len(cases)
        add(container, [f"v = {i};",
                        f"assert #0 (v != {i}) else $display(\"s{i} fail v=%0d\", v);",
                        "v = v + 100;", f"$display(\"s{i} after the statement\");"],
            [f"s{i} after the statement", f"s{i} fail v={i}"])
    # Actions.
    for call, line in [("note(v)", "note {i}"), ("fnote(v)", "fnote {i}"),
                       ("$display(\"s{i} twice %0d\", v * 2)", "s{i} twice {d}")]:
        i = len(cases)
        add("initial", [f"v = {i};", f"assert #0 (v != {i}) else {call.format(i=i)};",
                        "v = v + 100;", f"$display(\"s{i} after the statement\");"],
            [f"s{i} after the statement", line.format(i=i, d=2 * i)])
    # What follows in the time slot.
    for timing in ["#0", "final"]:
        i = len(cases)
        add("initial", [f"v = {i};",
                        f"assert {timing} (v != {i}) else $display(\"s{i} not dropped\");",
                        "fork\n  #0 -> ev;\njoin_none", "@ev;",
                        f"$display(\"s{i} resumed from the event control\");"],
            [f"s{i} resumed from the event control"])
    # A final one is still pending after the Observed region: a resume
    # caused by an Observed action drops it.
    i = len(cases)
    add("initial", [f"v = {i};",
                    f"assume final (v != {i}) else $display(\"s{i} not dropped\");",
                    "assert #0 (0) else trig();", "@ev;",
                    f"$display(\"s{i} resumed after the Observed region\");"],
        [f"s{i} resumed after the Observed region"])
    i = len(cases)
    add("initial", [f"v = {i};", "flag = 0;",
                    f"assert #0 (v != {i}) else $display(\"s{i} not dropped\");",
                    "fork\n  #0 flag = 1;\njoin_none", "wait (flag);",
                    f"$display(\"s{i} resumed from the wait\");"],
        [f"s{i} resumed from the wait"])
    i = len(cases)
    add("initial", [f"v = {i};", f"assert #0 (v != {i}) else $display(\"s{i} kept v=%0d\", v);",
                    "#0;", f"$display(\"s{i} after #0\");"],
        [f"s{i} after #0", f"s{i} kept v={i}"])
    # $error for an action left out, in the Reactive and Postponed regions.
    for kind, timing in [("assert", "#0"), ("assume", "final")]:
        i = len(cases)
        add("initial", [f"v = {i};", f"{kind} {timing} (v != {i});",
                        f"{kind} {timing} (v == {i});",
                        f"$display(\"s{i} after the statements\");"],
            [f"s{i} after the statements"], err=1)

    head = (f"/*\n:name: {NAME}\n:description: Deferred immediate assertions: the expression and the "
            "action's values when the statement runs, the action in the Reactive region for #0 "
            "and in the Postponed region for final, dropped when the process resumes from an "
            "event control or a wait, kept over a delay, and $error without an action; assert, "
            "assume and cover in procedures, tasks, functions and fork branches\n"
            ":tags: proc assertion\n:type: simulation\n:lrm: 1800-2023 16.4 16.4.1 16.4.2\n"
            ":oracle: spec\n:spec_check: ai\n:fourstate: no\n:nondeterministic: no\n"
            f":generator: gen_deferred_matrix.py@{VERSION}\n"
            f":expected_stdout: @{NAME}.expected\n"
            f":expected_stderr: @{NAME}.stderr\n*/\n"
            "// Generated by tools/gen_deferred_matrix.py; change the generator, not this file.\n"
            "// Icarus Verilog 13.0 does not take deferred assertions, and Verilator 5.052 runs\n"
            "// their actions at once and stops at the first failure.\n"
            "`timescale 1ns/1ns\n"
            "module top;\n  int v;\n  bit flag;\n  event ev;\n  logic go = 0;\n"
            "  task automatic note(int x);\n    $display(\"note %0d\", x);\n  endtask\n"
            "  function automatic void fnote(int x);\n    $display(\"fnote %0d\", x);\n"
            "  endfunction\n  function automatic void trig();\n    -> ev;\n  endfunction\n")
    lines = head.split("\n")[:-1]
    main = []
    out = []

    def emit(where, text, pad):
        for t in text.split("\n"):
            where.append(" " * pad + t)

    for i, (container, stmts, expect, e) in enumerate(cases):
        t0 = WINDOW * (i + 1)
        main.append(f"    #({t0} - $time);")
        body = []
        for st in stmts:
            emit(body, st, 0)
        out += expect
        if container == "initial":
            main.append((body, t0 if e is not None else None))
        elif container in ("static task", "automatic task"):
            kw = "static" if container == "static task" else "automatic"
            lines.append(f"  task {kw} c{i}();")
            emit(lines, "\n".join(body), 4)
            lines.append("  endtask")
            main.append(f"    c{i}();")
        elif container == "function":
            lines.append(f"  function void c{i}();")
            emit(lines, "\n".join(body), 4)
            lines.append("  endfunction")
            main.append(f"    c{i}();")
        elif container == "fork branch":
            main.append("    fork\n      begin")
            main.append((["  " * 2 + b for b in body], None))
            main.append("      end\n    join")
        else:
            lines.append("  always @(posedge go) begin")
            emit(lines, "\n".join(body), 4)
            lines.append("  end")
            main.append("    go = 1;")
    end = WINDOW * (len(cases) + 1)
    lines.append("  initial begin")
    stderr = []
    for item in main:
        if isinstance(item, str):
            emit(lines, item, 0)
            continue
        body, t0 = item
        first = len(lines) + 1
        emit(lines, "\n".join(body), 4)
        # A case without actions: the assertion on its second line fails,
        # and $error names the line, the scope and the time.
        if t0 is not None:
            stderr.append(f"{NAME}.sv:{first + 1}: error:  [top @ {t0}ns]")
    lines.append(f"    #({end} - $time) $finish;")
    lines.append("  end")
    lines.append("endmodule")
    sv = "\n".join(lines) + "\n"
    return {f"{NAME}.sv": sv, f"{NAME}.expected": "\n".join(out) + "\n",
            f"{NAME}.stderr": "\n".join(stderr) + "\n"}


def main():
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("--check", action="store_true")
    args = ap.parse_args()
    bad = 0
    for name, text in files().items():
        path = OUT / name
        if args.check:
            if not path.exists() or path.read_text(encoding="utf-8") != text:
                print(f"gen_deferred_matrix: {path.relative_to(OUT.parent.parent.parent)} is "
                      "out of date; run tools/gen_deferred_matrix.py", file=sys.stderr)
                bad += 1
        else:
            path.write_text(text, encoding="utf-8")
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main())
