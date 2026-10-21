#!/usr/bin/env python3
# Copyright (c) 2026 The kirimasim Authors
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

"""Generates the process lifetime matrix test of TEST_SUITE.md ("Mechanism matrix")
in tests/suite/sched/.

  tools/gen_process_lifetime_matrix.py           # writes the files
  tools/gen_process_lifetime_matrix.py --check   # exit 1 if a file differs from what it would write

The record of a process is released once it has ended, its children's
records are released and its held reports are confirmed (SCHEDULER_SPEC.md
10.8), and releasing it changes nothing that can be seen. Each case repeats
one round n times (+n=<count>, 3 by default), one round every 6 time units.
In a round, a fork makes a child and a sibling; the child may fork a
grandchild that outlives it.

  time 1  the sibling counts (sb)
  time 3  the child counts (ch), unless it was ended before
  time 5  the grandchild counts (gc), unless it was ended before

The axes: how the round ends (normally; by disable fork, by disabling a
named block around the fork, or by disabling the task that holds the fork,
at time 2 while the child lives or at time 4 when only the grandchild
does); with or without the grandchild; how the child and the sibling
start (a fork with join, join_any, join_none then wait fork, or
join_none, or a function whose fork-join_none starts them, 13.4.4); the
container (an initial procedure, an automatic task, an always procedure,
a fork branch). A disable ends every process started in what it ends,
with their descendants, those of a finished child included (9.6.2, 9.6.3;
SCHEDULER_SPEC.md 10.4, 10.5), so the grandchild never counts after a
disable. wait fork waits only for the direct children (9.6.1).

Further cases. A child that holds a report and ends at once: a deferred
assertion, #0, whose action counts (rp), or final, whose action prints a
line at the time of the round, since nothing may be written from the
Postponed region where it runs (4.4.2.9, SCHEDULER_SPEC.md 2.2), or a
unique case violation, a warning line on stderr at the time of the round
(16.4, 12.5.3; SCHEDULER_SPEC.md 15); the report is still confirmed. With a
refork, the parent starts a new child in the same time slot, before the
report is confirmed (#0, then a fork whose child counts at time 2, rf):
a record released too early would be reused under the report. And a
child started before the block or the task call (an elder, counting at
time 5, el) is not started in it, so it goes on after a disable of the
block or the task; children are told apart by their birth number, not by
their place among the children kept (SCHEDULER_SPEC.md 10.8).

tests/cli/run_cli_tests.py also runs the design with two counts and checks
that proc.kept_peak (PERFORMANCE.md 3.2) does not grow with the count.

The expected outputs follow from the rules above. A person checks them
once for all the tests made from them (TEST_SUITE.md, "Sampling by people").
"""

from __future__ import annotations

import argparse
import itertools
import re
import sys
from pathlib import Path
from typing import NamedTuple

OUT = Path(__file__).resolve().parent.parent / "tests" / "suite" / "sched"
NAME = "process_lifetime"
# The version of the rules below (TEST_SUITE.md, :generator:). Raise it
# whenever they change.
VERSION = 4
DEFAULT_N = 3
ROUND = 6

# The joins, and the time from the start of the round at which the parent
# goes on after them: the sibling ends at 1, the child at 3. "function"
# calls a function whose fork-join_none starts the child and the sibling.
JOINS = {"join": 3, "join_any": 1, "join_none wait fork": 3, "join_none": 0, "function": 0}
CONTAINERS = ["initial", "task", "always", "branch"]


class Case(NamedTuple):
    container: str
    join: str
    end: str = "normal"
    time: int | None = None
    grandchild: bool = False
    report: str | None = None  # "#0", "final" or "unique"
    elder: bool = False
    refork: bool = False


def cases():
    out = []
    for c in CONTAINERS:
        ends = ["normal", "disable fork", "disable block"] + (["disable task"] if c == "task" else [])
        for end, join, gc in itertools.product(ends, JOINS, [False, True]):
            times = [None] if end == "normal" else [t for t in (2, 4) if t > JOINS[join]]
            for t in times:
                out.append(Case(c, join, end, t, gc))
        for report, join in itertools.product(["#0", "final"], ["join", "join_none"]):
            out.append(Case(c, join, report=report))
        # An elder child goes on after a disable of a block or a task (wait
        # fork would wait for it, so not with it).
        for end in ["disable block"] + (["disable task"] if c == "task" else []):
            for join in [j for j in JOINS if j != "join_none wait fork"]:
                for t in [t for t in (2, 4) if t > JOINS[join]]:
                    out.append(Case(c, join, end, t, elder=True))
        out += [Case(c, "join", report="unique"), Case(c, "join_none", report="unique")]
        for report in ["#0", "final", "unique"]:
            out.append(Case(c, "join_none", report=report, refork=True))
    return out


def label(case):
    parts = [case.container, case.join.replace(" ", "+"),
             case.end.replace(" ", "_") + (f"@{case.time}" if case.time else "")]
    parts.append("grandchild" if case.grandchild else "child")
    if case.report:
        parts.append(f"report{case.report}")
    if case.elder:
        parts.append("elder")
    if case.refork:
        parts.append("refork")
    return " ".join(parts)


def expected(case, n):
    ch = 0 if case.report or case.time == 2 else n
    gc = n if case.grandchild and case.end == "normal" else 0
    rp = n if case.report == "#0" else 0
    line = f"{label(case)} sb={n} ch={ch} gc={gc} rp={rp}"
    if case.elder:
        line += f" el={n}"
    if case.refork:
        line += f" rf={n}"
    return line


def elder_lines(k, pad):
    """A child started before the block or the task call."""
    return [f"{pad}fork", f"{pad}  #5 el[{k}]++;", f"{pad}join_none"]


def fork_lines(k, case, pad, join):
    """The fork that starts the child and the sibling."""
    lines = [f"{pad}fork", f"{pad}  begin"]
    if case.report == "unique":
        lines.append(f"{pad}    unique case (sb[{k}] < 0) 1'b1: ; endcase  // violation {k}")
    elif case.report == "final":
        lines.append(f"{pad}    assert final (sb[{k}] >= 0) "
                     f'$display("final report {k} at %0t", $time);')
    elif case.report:
        lines.append(f"{pad}    assert {case.report} (sb[{k}] >= 0) note({k});")
    else:
        if case.grandchild:
            lines += [f"{pad}    fork", f"{pad}      #5 gc[{k}]++;", f"{pad}    join_none"]
        lines.append(f"{pad}    #3 ch[{k}]++;")
    lines += [f"{pad}  end", f"{pad}  #1 sb[{k}]++;", f"{pad}{join}"]
    return lines


def round_body(k, case, pad):
    """One round of case k; in a task container with "disable task" the
    caller waits for the rest of the round."""
    join, end, t = case.join, case.end, case.time
    r = (0 if join == "join_none" else 1) if case.report else JOINS[join]
    lines = []
    inner = pad
    if case.elder and end == "disable block":
        lines += elder_lines(k, pad)
    if end == "disable block":
        lines.append(f"{pad}begin : blk{k}")
        inner = pad + "  "
    if join == "function":
        lines.append(f"{inner}start{k}();")
    else:
        lines += fork_lines(k, case, inner, join.split()[0])
    if join == "join_none wait fork":
        lines.append(f"{inner}wait fork;")
    if case.refork:
        lines += [f"{inner}#0;", f"{inner}fork", f"{inner}  #2 rf[{k}]++;", f"{inner}join_none"]
    if end == "disable fork":
        lines.append(f"{inner}#{t - r} disable fork;")
    elif end == "disable block":
        lines.append(f"{inner}#{t - r} disable blk{k};")
    elif end == "disable task":
        lines.append(f"{inner}#{t - r} disable round{k};")
    if end == "disable block":
        lines.append(f"{pad}end")
    if end != "disable task":
        lines.append(f"{pad}#{ROUND - (t if t else r)};")
    return lines


def files():
    all_cases = cases()
    nc = len(all_cases)
    sv = [
        "/*",
        f":name: {NAME}",
        ":description: forked processes, their descendants and their held reports behave the "
        "same however long the records of finished processes are kept: normal ends, disable "
        "fork, disable of a block and of a task while the child lives or after it ended with a "
        "grandchild left, with each join and with a function that forks, children that end "
        "holding reports with and without a new child in the same time slot, in an initial and "
        "an always procedure, a task and a fork branch",
        ":tags: sched fork",
        ":type: simulation",
        ":lrm: 1800-2023 9.3.2 9.6.1 9.6.2 9.6.3 12.5.3 13.4.4 16.4",
        ":oracle: spec",
        ":spec_check: ai",
        ":fourstate: no",
        ":nondeterministic: no",
        f":generator: gen_process_lifetime_matrix.py@{VERSION}",
        f":expected_stdout: @{NAME}.expected",
        f":expected_stderr: @{NAME}.stderr",
        "*/",
        "// Generated by tools/gen_process_lifetime_matrix.py; change the generator, not this file.",
        "`timescale 1ns/1ns",
        "module top;",
        "  int n;",
        f"  int sb[{nc}], ch[{nc}], gc[{nc}], rp[{nc}];",
        f"  int el[{nc}], rf[{nc}];",
        "  function void note(int k);",
        "    rp[k] = rp[k] + 1;",
        "  endfunction",
    ]
    main = ["  initial begin",
            f"    if (!$value$plusargs(\"n=%d\", n)) n = {DEFAULT_N};",
            "    #1;"]
    for k, case in enumerate(all_cases):
        c, end = case.container, case.end
        if case.join == "function":
            sv.append(f"  function automatic void start{k}();")
            sv += fork_lines(k, case, "    ", "join_none")
            sv.append("  endfunction")
        if c == "task":
            sv.append(f"  task automatic round{k}();")
            sv += round_body(k, case, "    ")
            sv.append("  endtask")
            main.append("    for (int i = 0; i < n; i++) begin")
            if case.elder and end == "disable task":
                main += elder_lines(k, "      ")
            main.append(f"      round{k}();")
            if end == "disable task":
                main.append(f"      #{ROUND - case.time};")
            main.append("    end")
        elif c == "always":
            sv.append(f"  event go{k}, done{k};")
            sv.append(f"  always @(go{k}) begin")
            sv.append("    for (int i = 0; i < n; i++) begin")
            sv += round_body(k, case, "      ")
            sv.append("    end")
            sv.append(f"    ->done{k};")
            sv.append("  end")
            main += [f"    ->go{k};", f"    @(done{k});"]
        elif c == "branch":
            main += ["    fork", "      for (int i = 0; i < n; i++) begin"]
            main += round_body(k, case, "        ")
            main += ["      end", "    join"]
        else:
            main.append("    for (int i = 0; i < n; i++) begin")
            main += round_body(k, case, "      ")
            main.append("    end")
    for k, case in enumerate(all_cases):
        fmt, args = "", ""
        if case.elder:
            fmt, args = fmt + " el=%0d", args + f", el[{k}]"
        if case.refork:
            fmt, args = fmt + " rf=%0d", args + f", rf[{k}]"
        main.append(f"    $display(\"{label(case)} sb=%0d ch=%0d gc=%0d rp=%0d{fmt}\", "
                    f"sb[{k}], ch[{k}], gc[{k}], rp[{k}]{args});")
    main.append("  end")
    sv += main
    sv.append("endmodule")
    # Each round of a case starts at 1 + ROUND * (n * k + i); the unique case
    # violation is reported with the scope of the statement (SYSTEM_TASKS.md
    # 10.4): the task in a task container, else the module.
    where = {int(m.group(1)): number for number, line in enumerate(sv, 1)
             if (m := re.search(r"// violation (\d+)$", line))}
    err = []
    for k, case in enumerate(all_cases):
        if case.report != "unique":
            continue
        scope = f"top.round{k}" if case.container == "task" else "top"
        for i in range(DEFAULT_N):
            err.append(f"{NAME}.sv:{where[k]}: warning: unique case violation: no item matches "
                       f"[{scope} @ {1 + ROUND * (DEFAULT_N * k + i)}ns]")
    # The final reports print in the Postponed region of their rounds, before
    # the counts at the end; the cases run one after another.
    exp = [f"final report {k} at {1 + ROUND * (DEFAULT_N * k + i)}"
           for k, case in enumerate(all_cases) if case.report == "final"
           for i in range(DEFAULT_N)]
    exp += [expected(case, DEFAULT_N) for case in all_cases]
    return {f"{NAME}.sv": "\n".join(sv) + "\n", f"{NAME}.expected": "\n".join(exp) + "\n",
            f"{NAME}.stderr": "\n".join(err) + "\n"}


def main():
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("--check", action="store_true")
    args = ap.parse_args()
    bad = 0
    for name, text in files().items():
        path = OUT / name
        if args.check:
            if not path.exists() or path.read_text(encoding="utf-8") != text:
                print(f"gen_process_lifetime_matrix: {path.relative_to(OUT.parent.parent.parent)} "
                      "is out of date; run tools/gen_process_lifetime_matrix.py", file=sys.stderr)
                bad += 1
        else:
            path.write_text(text, encoding="utf-8")
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main())
