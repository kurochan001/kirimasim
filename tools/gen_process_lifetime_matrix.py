#!/usr/bin/env python3
"""Generates the process lifetime matrix test of TEST_SUITE.md ("機構の行列")
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
disable. wait fork waits only for the direct children (9.6.1). Further
cases: a child that holds a deferred assertion report (#0 or final) and
ends at once; the report is still confirmed and its action runs (16.4;
SCHEDULER_SPEC.md 15). And a child started before the block or the task
call (an elder, counting at time 5, el) is not started in it, so it goes
on after a disable of the block or the task; children are told apart by
their birth number, not by their place among the children kept
(SCHEDULER_SPEC.md 10.8).

tests/cli/run_cli_tests.py also runs the design with two counts and checks
that proc.kept_peak (PERFORMANCE.md 3.2) does not grow with the count.

The expected outputs follow from the rules above. A person checks them
once for all the tests made from them (TEST_SUITE.md, "人の抜き取り").
"""

import argparse
import itertools
import sys
from pathlib import Path

OUT = Path(__file__).resolve().parent.parent / "tests" / "suite" / "sched"
NAME = "process_lifetime"
# The version of the rules below (TEST_SUITE.md, :generator:). Raise it
# whenever they change.
VERSION = 2
DEFAULT_N = 3
ROUND = 6

# The joins, and the time from the start of the round at which the parent
# goes on after them: the sibling ends at 1, the child at 3. "function"
# calls a function whose fork-join_none starts the child and the sibling.
JOINS = {"join": 3, "join_any": 1, "join_none wait fork": 3, "join_none": 0, "function": 0}
CONTAINERS = ["initial", "task", "always", "branch"]


def cases():
    """(container, join, end, time, grandchild, report, elder) for every case."""
    out = []
    for c in CONTAINERS:
        ends = ["normal", "disable fork", "disable block"] + (["disable task"] if c == "task" else [])
        for end, join, gc in itertools.product(ends, JOINS, [False, True]):
            times = [None] if end == "normal" else [t for t in (2, 4) if t > JOINS[join]]
            for t in times:
                out.append((c, join, end, t, gc, None, False))
        for report, join in itertools.product(["#0", "final"], ["join", "join_none"]):
            out.append((c, join, "normal", None, False, report, False))
        # An elder child goes on after a disable of a block or a task (wait
        # fork would wait for it, so not with it).
        for end in ["disable block"] + (["disable task"] if c == "task" else []):
            for join in [j for j in JOINS if j != "join_none wait fork"]:
                for t in [t for t in (2, 4) if t > JOINS[join]]:
                    out.append((c, join, end, t, False, None, True))
    return out


def label(case):
    c, join, end, t, gc, report, elder = case
    parts = [c, join.replace(" ", "+"), end.replace(" ", "_") + (f"@{t}" if t else "")]
    parts.append("grandchild" if gc else "child")
    if report:
        parts.append(f"report{report}")
    if elder:
        parts.append("elder")
    return " ".join(parts)


def expected(case, n):
    c, join, end, t, gc, report, elder = case
    ch = 0 if report or t == 2 else n
    return (f"{label(case)} sb={n} ch={ch} gc={n if gc and end == 'normal' else 0} "
            f"rp={n if report else 0}" + (f" el={n}" if elder else ""))


def elder_lines(k, pad):
    """A child started before the block or the task call."""
    return [f"{pad}fork", f"{pad}  #5 el[{k}]++;", f"{pad}join_none"]


def fork_lines(k, case, pad, join):
    """The fork that starts the child and the sibling."""
    c, _, end, t, gc, report, elder = case
    lines = [f"{pad}fork", f"{pad}  begin"]
    if report:
        lines.append(f"{pad}    assert {report} (sb[{k}] >= 0) note({k});")
    else:
        if gc:
            lines += [f"{pad}    fork", f"{pad}      #5 gc[{k}]++;", f"{pad}    join_none"]
        lines.append(f"{pad}    #3 ch[{k}]++;")
    lines += [f"{pad}  end", f"{pad}  #1 sb[{k}]++;", f"{pad}{join}"]
    return lines


def round_body(k, case, pad):
    """One round of case k; in a task container with "disable task" the
    caller waits for the rest of the round."""
    c, join, end, t, gc, report, elder = case
    r = 0 if report and join == "join_none" else 1 if report else JOINS[join]
    lines = []
    inner = pad
    if elder and end == "disable block":
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
        "grandchild left, with each join and with a function that forks, in an initial and an "
        "always procedure, a task and a fork branch",
        ":tags: sched fork",
        ":type: simulation",
        ":lrm: 1800-2023 9.3.2 9.6.1 9.6.2 9.6.3 13.4.4 16.4",
        ":oracle: spec",
        ":spec_check: ai",
        ":fourstate: no",
        ":nondeterministic: no",
        f":generator: gen_process_lifetime_matrix.py@{VERSION}",
        f":expected_stdout: @{NAME}.expected",
        "*/",
        "// Generated by tools/gen_process_lifetime_matrix.py; change the generator, not this file.",
        "`timescale 1ns/1ns",
        "module top;",
        "  int n;",
        f"  int sb[{nc}], ch[{nc}], gc[{nc}], rp[{nc}];",
        f"  int el[{nc}];",
        "  function void note(int k);",
        "    rp[k] = rp[k] + 1;",
        "  endfunction",
    ]
    main = ["  initial begin",
            f"    if (!$value$plusargs(\"n=%d\", n)) n = {DEFAULT_N};",
            "    #1;"]
    for k, case in enumerate(all_cases):
        c, join, end, t, gc, report, elder = case
        if join == "function":
            sv.append(f"  function automatic void start{k}();")
            sv += fork_lines(k, case, "    ", "join_none")
            sv.append("  endfunction")
        if c == "task":
            sv.append(f"  task automatic round{k}();")
            sv += round_body(k, case, "    ")
            sv.append("  endtask")
            main.append(f"    for (int i = 0; i < n; i++) begin")
            if elder and end == "disable task":
                main += elder_lines(k, "      ")
            main.append(f"      round{k}();")
            if end == "disable task":
                main.append(f"      #{ROUND - t};")
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
        el = (" el=%0d", f", el[{k}]") if case[6] else ("", "")
        main.append(f"    $display(\"{label(case)} sb=%0d ch=%0d gc=%0d rp=%0d{el[0]}\", "
                    f"sb[{k}], ch[{k}], gc[{k}], rp[{k}]{el[1]});")
    main.append("  end")
    sv += main
    sv.append("endmodule")
    exp = [expected(case, DEFAULT_N) for case in all_cases]
    return {f"{NAME}.sv": "\n".join(sv) + "\n", f"{NAME}.expected": "\n".join(exp) + "\n"}


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
