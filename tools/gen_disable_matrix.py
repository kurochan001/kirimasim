#!/usr/bin/env python3
"""Generates the disable matrix test of TEST_SUITE.md ("Mechanism matrix")
in tests/suite/sched/.

  tools/gen_disable_matrix.py           # writes the files
  tools/gen_disable_matrix.py --check   # exit 1 if a file differs from what it would write

disable of a named block ends every activation of the block: the
process goes on after the block, the processes the activation started
end, and the task calls it made end without copying their outputs back.
disable of a task ends every call of it the same way, and its callers go
on after the call (9.6.2, SCHEDULER_SPEC.md 10.4, 10.7; SIR_SPEC.md 7.2).

The axes for blocks: the container of the block (an initial procedure, a
static and an automatic task, a fork branch), where the disable comes
from (the block itself, a block inside it, a fork branch inside it,
another process, a task without waits called inside it, a task that
waits called inside it), and what the block is waiting for when another
process disables it (a delay, an event, a join, a task call, a wait
condition). Each block first starts a process with join_none that would
show a line later. For tasks: a call waiting in another process, a call
ended from a task it called, two calls at once, a task ending itself,
and a recursive task whose block ends at its deepest call.

The expected outputs follow from the rules above. A person checks them
once for all the tests made from them (TEST_SUITE.md, "Sampling by people").
"""

import argparse
import sys
from pathlib import Path

OUT = Path(__file__).resolve().parent.parent / "tests" / "suite" / "sched"
NAME = "disable_matrix"
# The version of the rules below (TEST_SUITE.md, :generator:). Raise it
# whenever they change.
VERSION = 1

WINDOW = 20
CONTAINERS = ["initial", "static task", "automatic task", "fork branch"]
SOURCES = ["itself", "inner block", "fork branch", "other process", "task", "waiting task"]
WAITS = ["delay", "event", "join", "task call", "wait condition"]


def files():
    helpers = []   # tasks and functions
    steps = []     # the main procedure's statements, scenario by scenario
    driver = []    # the other process's disables, by time
    exp = []
    n = 0

    def scenario(container, source, wait):
        """One block: the lines of its body and how its container runs it."""
        nonlocal n
        i = n
        n += 1
        t0 = WINDOW * (i + 1)
        tag = f"s{i}"
        label = f"{tag} {container}, from {source}, waiting on {wait}"
        blk = f"b{i}"
        # Where the block is, for a disable from outside its body.
        path = {"initial": f"main.{blk}", "static task": f"ts{i}.{blk}",
                "automatic task": f"ta{i}.{blk}", "fork branch": f"main.f{i}.g{i}.{blk}"}[container]
        body = [f"begin : {blk}",
                f"  fork\n    #3 $display(\"{tag} child not ended\");\n  join_none"]
        if source == "itself":
            body.append(f"  #1 disable {blk};")
        elif source == "inner block":
            body.append(f"  begin : in{i}\n    #1 disable {blk};\n    $display(\"{tag} inner "
                        "not ended\");\n  end")
        elif source == "fork branch":
            body.append(f"  fork\n    #1 disable {path};\n    #2 $display(\"{tag} sibling not "
                        "ended\");\n  join")
        elif source == "task":
            helpers.append(f"  task dis{i}();\n    disable {path};\n  endtask")
            body.append(f"  #1 dis{i}();\n  $display(\"{tag} after the task\");")
        elif source == "waiting task":
            helpers.append(f"  task wdis{i}();\n    #1 disable {path};\n    $display(\"{tag} "
                           "task not ended\");\n  endtask")
            body.append(f"  wdis{i}();\n  $display(\"{tag} after the task\");")
        else:
            driver.append((t0 + 1, f"disable {path};"))
            if wait == "delay":
                body.append("  #5;")
            elif wait == "event":
                body.append("  @never;")
            elif wait == "join":
                body.append(f"  fork\n    #5 $display(\"{tag} joined child not ended\");\n"
                            "  join")
            elif wait == "task call":
                helpers.append(f"  task automatic long{i}(output int o);\n    o = 9;\n    #5;\n"
                               "  endtask")
                body.append(f"  long{i}(out);\n  $display(\"{tag} task not ended\");")
            else:
                body.append("  wait (flag);")
        body.append(f"  $display(\"{tag} block not ended\");\nend")
        after = f"$display(\"{label}: after the block at %0d, out=%0d\", $time - t0, out);"
        code = "\n".join(body) + "\n" + after
        exp.append(f"{label}: after the block at 1, out=0")
        start = f"    #({t0} - $time);\n    t0 = $time;\n    out = 0;"
        if container == "initial":
            steps.append(start + "\n" + indent(code, 4))
        elif container == "static task":
            helpers.append(f"  task static ts{i}();\n{indent(code, 4)}\n  endtask")
            steps.append(start + f"\n    ts{i}();")
        elif container == "automatic task":
            helpers.append(f"  task automatic ta{i}();\n{indent(code, 4)}\n  endtask")
            steps.append(start + f"\n    ta{i}();")
        else:
            steps.append(start + f"\n    fork : f{i}\n      begin : g{i}\n{indent(code, 8)}\n"
                                 "      end\n    join")

    for c in CONTAINERS:
        for s in SOURCES:
            scenario(c, s, "delay")
    for w in WAITS[1:]:
        scenario("initial", "other process", w)

    # Tasks.
    t0 = WINDOW * (n + 1)
    task_steps = [
        # A call waiting in another process ends without its outputs.
        (f"    #({t0} - $time);\n    t0 = $time;\n    out = 5;\n    fork\n"
         f"      #1 disable sleeper;\n    join_none\n    sleeper(out);\n"
         "    $display(\"task from another process: after the call at %0d, out=%0d\", "
         "$time - t0, out);",
         ["task from another process: after the call at 1, out=5"]),
        # A task ended by a task it called; the callee ends with it.
        (f"    #({t0 + WINDOW} - $time);\n    t0 = $time;\n    out = 5;\n    outer_t(out);\n"
         "    $display(\"task from a task it called: after the call at %0d, out=%0d\", "
         "$time - t0, out);",
         ["task from a task it called: after the call at 2, out=5"]),
        # Two calls at once both end.
        (f"    #({t0 + 2 * WINDOW} - $time);\n    t0 = $time;\n    fork\n"
         "      begin sleeper(out); $display(\"two calls: first after the call at %0d\", "
         "$time - t0); end\n"
         "      begin #1 sleeper(out2); #1 $display(\"two calls: second after the call at %0d\", "
         "$time - t0 - 1); end\n"
         "      #2 disable sleeper;\n    join",
         ["two calls: first after the call at 2", "two calls: second after the call at 2"]),
        # A task ending itself goes back to its caller.
        (f"    #({t0 + 3 * WINDOW} - $time);\n    t0 = $time;\n    out = 5;\n    self_t(out);\n"
         "    $display(\"task ending itself: after the call at %0d, out=%0d\", $time - t0, out);",
         ["task ending itself: after the call at 1, out=5"]),
        # The block of a recursive task ends in every call at once.
        (f"    #({t0 + 4 * WINDOW} - $time);\n    t0 = $time;\n    rec(0);\n"
         "    $display(\"recursion: after the call at %0d\", $time - t0);",
         ["recursion: after the block of call 0 at 1", "recursion: after the call at 1"]),
    ]
    helpers.append("  task automatic sleeper(output int o);\n    o = 9;\n    #5 $display(\"sleeper "
                   "not ended\");\n  endtask")
    helpers.append("  task automatic inner_t();\n    #2 disable outer_t;\n    $display(\"inner_t "
                   "not ended\");\n  endtask")
    helpers.append("  task automatic outer_t(output int o);\n    o = 9;\n    inner_t();\n"
                   "    $display(\"outer_t not ended\");\n  endtask")
    helpers.append("  task automatic self_t(output int o);\n    o = 9;\n    #1 disable self_t;\n"
                   "    $display(\"self_t not ended\");\n  endtask")
    helpers.append("  task automatic rec(int k);\n    begin : rb\n      if (k < 3) begin\n"
                   "        rec(k + 1);\n        $display(\"recursion: back in call %0d\", k);\n"
                   "      end else begin\n        #1 disable rb;\n      end\n"
                   "      $display(\"recursion: end of block %0d\", k);\n    end\n"
                   "    $display(\"recursion: after the block of call %0d at %0d\", k, $time - t0);\n"
                   "  endtask")
    for code, lines in task_steps:
        steps.append(code)
        exp += lines
    end = t0 + 6 * WINDOW

    sv = (f"/*\n:name: {NAME}\n:description: disable ends every activation of a named block or "
          "every call of a task, with the processes they started and the calls they made, from "
          "the block itself, inner blocks, fork branches, other processes and tasks, in "
          "procedures, tasks and fork branches, while they wait on delays, events, joins, task "
          "calls and wait conditions\n:tags: sched\n:type: simulation\n:lrm: 1800-2023 9.6.2\n"
          ":oracle: spec\n:spec_check: ai\n:fourstate: no\n:nondeterministic: no\n"
          f":generator: gen_disable_matrix.py@{VERSION}\n"
          f":expected_stdout: @{NAME}.expected\n*/\n"
          "// Generated by tools/gen_disable_matrix.py; change the generator, not this file.\n"
          "// 9.6.2 leaves the outputs of a disabled task open; kirimasim does not copy them\n"
          "// back (SCHEDULER_SPEC.md 10.4). Icarus Verilog 13.0 copies them back, and gives the\n"
          "// other lines but those of the automatic task blocks, where it stops on an internal\n"
          "// assertion. Verilator 5.052 does not take disable of hierarchical names.\n"
          "module top;\n  time t0;\n  int out, out2;\n  bit flag;\n  event never;\n")
    sv += "\n".join(helpers) + "\n"
    sv += "  initial begin : main\n" + "\n".join(steps) + f"\n    #({end} - $time) $finish;\n"
    sv += "  end\n"
    sv += "  initial begin\n"
    now = 0
    for t, text in sorted(driver):
        sv += f"    #{t - now} {text}\n"
        now = t
    sv += "  end\nendmodule\n"
    return {f"{NAME}.sv": sv, f"{NAME}.expected": "\n".join(exp) + "\n"}


def indent(text, k):
    return "\n".join((" " * k + line) if line else line for line in text.split("\n"))


def main():
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("--check", action="store_true")
    args = ap.parse_args()
    bad = 0
    for name, text in files().items():
        path = OUT / name
        if args.check:
            if not path.exists() or path.read_text(encoding="utf-8") != text:
                print(f"gen_disable_matrix: {path.relative_to(OUT.parent.parent.parent)} is "
                      "out of date; run tools/gen_disable_matrix.py", file=sys.stderr)
                bad += 1
        else:
            path.write_text(text, encoding="utf-8")
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main())
