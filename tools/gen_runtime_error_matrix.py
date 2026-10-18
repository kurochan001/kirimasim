#!/usr/bin/env python3
"""Generates the matrix tests of run-time errors of the design, of
TEST_SUITE.md ("Mechanism matrix"), in tests/suite/sched/.

  tools/gen_runtime_error_matrix.py           # writes the files
  tools/gen_runtime_error_matrix.py --check   # exit 1 if a file differs from what it would write

An error the design makes while it runs ends the run with the reason
error and exit code 2: kirimasim writes one line on standard error in the
form of the severity tasks, <file>:<line>: error: <text> [<instance> @
<time>], at the statement's position, and final still runs
(SCHEDULER_SPEC.md 16.2, CLI.md 5). The errors: a delay that takes the
time past the largest time (8; a negative delay is a very large one,
9.4.1), a write or an event from the Postponed region (2.2, 4.4.2.9),
and a disable from a function of a block outside it (10.4).

One file for each error and container, since the first error ends the
run. The axes, for the delays: the statement (a wait, an NBA and a ->>
with the delay -1, after time 1) and the container (an initial
procedure, an automatic task, a fork branch and an always procedure).
For the Postponed region: how a function writes (a variable, an element
at a run-time index, a bit at a run-time position, through a ref
argument, a nonblocking assignment, a trigger) and what calls it in the
Postponed region (a $strobe argument, a $monitor argument, the action of
a final deferred assertion; the arguments of the action's own call are
evaluated with the assertion, 16.4). For disable: a function that
disables the block that called it.

What the Postponed region may write (SCHEDULER_SPEC.md 2.2): the
functions it calls may write their own variables, arguments, automatic
and static ones, and through a ref to them. One more file calls such
functions from the three callers, and ends with code 0.

The expected outputs follow from the rules above, computed below. A
person checks them once for all the tests made from them (TEST_SUITE.md,
"Sampling by people").
"""

from __future__ import annotations

import argparse
import sys
from pathlib import Path

OUT = Path(__file__).resolve().parent.parent / "tests" / "suite" / "sched"
PREFIX = "runtime_error_"
# The version of the rules below (TEST_SUITE.md, :generator:). Raise it
# whenever they change.
VERSION = 1

OVERFLOW = "the delay takes the time past the largest time"
POSTPONED = "a write or an event in the Postponed region"
DISABLE = "disable in a function of a block or task outside the function"

# (name, the statements at time 1 with ERR marking the line of the error,
# the text, the time)
DELAYS = [
    ("wait", ["#(-1); // ERR", '$display("not reached");'], OVERFLOW),
    ("nba", ["q <= #(-1) 1; // ERR", '$display("not reached");'], OVERFLOW),
    ("trigger_nb", ["->> #(-1) ev; // ERR", '$display("not reached");'], OVERFLOW),
]
CONTAINERS = ["initial", "task", "fork", "always"]


def body(container, stmts):
    """The module items that run stmts at time 1 in the container."""
    inner = ["#1;"] + stmts
    if container == "initial":
        return ["  initial begin"] + ["    " + s for s in inner] + ["  end"]
    if container == "task":
        return (["  task automatic run();"] + ["    " + s for s in inner] + ["  endtask",
                                                                           "  initial run();"])
    if container == "fork":
        return (["  initial fork", "    begin"] + ["      " + s for s in inner]
                + ["    end", "  join"])
    return (["  logic go = 0;", "  initial #0 go = 1;", "  always @(posedge go) begin"]
            + ["    " + s for s in inner] + ["  end"])


# (name, the statement that writes, with ERR marking the line of the
# error)
WRITES = [
    ("store", "y = y + 1; // ERR"),
    ("element", "arr[k] = 1; // ERR"),
    ("bit", "y[k] = 1'b1; // ERR"),
    ("ref", "inc(y);"),
    ("nba", "y <= 1; // ERR"),
    ("trigger", "-> ev; // ERR"),
]
CALLERS = {
    "strobe": '$strobe("%0d", side());',
    "monitor": '$monitor("%0d", side());',
    "final": "assert final (0) else touch();",
}


def postponed(write, caller):
    """A function that writes, called from caller in the Postponed region."""
    return ["  function automatic void inc(ref int r);",
            "    r++;" + (" // ERR" if write.startswith("inc(") else ""),
            "  endfunction",
            "  function void touch();",
            "    " + write,
            "  endfunction",
            "  function int side();",
            "    touch();",
            "    return 7;",
            "  endfunction",
            "  initial begin",
            "    #1;",
            "    " + CALLERS[caller],
            "  end"]


def disable_in_function():
    return ["  function void stop();",
            "    disable top.a.blk; // ERR",
            "  endfunction",
            "  initial begin : a",
            "    #1;",
            "    begin : blk",
            "      stop();",
            '      $display("not reached");',
            "    end",
            "  end"]


def cases():
    for name, stmts, text in DELAYS:
        for container in CONTAINERS:
            yield f"{name}_{container}", body(container, stmts), text
    for name, write in WRITES:
        for caller in CALLERS:
            yield f"postponed_{name}_{caller}", postponed(write, caller), POSTPONED
    yield "disable_in_function", disable_in_function(), DISABLE


def files():
    out = {}
    for case, items, text in cases():
        name = PREFIX + case
        head = [
            "/*",
            f":name: {name}",
            f":description: a run-time error of the design ({case.replace('_', ' ')}) ends the "
            "run with code 2 and one error line at the statement, and final still runs",
            ":tags: sched",
            ":type: simulation",
            ":lrm: 1800-2023 9.4.1 4.4.2.9 9.6.2 16.4",
            ":oracle: spec",
            ":spec_check: ai",
            ":fourstate: no",
            ":nondeterministic: no",
            f":generator: gen_runtime_error_matrix.py@{VERSION}",
            ":top_module: top",
            ":expected_stdout: final ran",
            ":expected_exit: 2",
            f":expected_stderr: @{name}.stderr",
            "*/",
            "// Generated by tools/gen_runtime_error_matrix.py; change the generator, not this "
            "file.",
            "`timescale 1ns/1ns",
            "module top;",
            "  int k = 1;",
            "  int y;",
            "  int arr [0:2];",
            "  logic q;",
            "  event ev;",
            '  final $display("final ran");',
        ]
        lines = head + items + ["endmodule", ""]
        number = next(i for i, line in enumerate(lines, 1) if line.endswith("// ERR"))
        out[f"{name}.sv"] = "\n".join(lines)
        out[f"{name}.stderr"] = f"{name}.sv:{number}: error: {text} [top @ 1ns]\n"
    out.update(own_variables())
    return out


def own_variables():
    """Functions that write only their own variables, called from the three
    callers at times 1, 2 and 3: no error."""
    name = "postponed_own_variables"
    k = 1
    own = 5 + 5 + (1 << k)  # a static variable, an element and a bit of its own
    arg = 4 + 1  # its argument
    # automatic variables: through a ref to one, an element and a bit
    au = (3 * 2 + 1) + 3 * 2 + (1 << k)
    st = 1 + 1  # a static variable through a ref to it
    shown = f"{own} {arg} {au} {st}"
    lines = [
        "/*",
        f":name: {name}",
        ":description: functions called from $strobe, $monitor and a final deferred assertion's "
        "action in the Postponed region may write their own arguments, automatic and static "
        "variables, also through a ref",
        ":tags: sched",
        ":type: simulation",
        ":lrm: 1800-2023 4.4.2.9 16.4 21.2.2 21.2.3",
        ":oracle: spec",
        ":spec_check: ai",
        ":fourstate: no",
        ":nondeterministic: no",
        f":generator: gen_runtime_error_matrix.py@{VERSION}",
        ":top_module: top",
        f":expected_stdout: @{name}.expected",
        "*/",
        "// Generated by tools/gen_runtime_error_matrix.py; change the generator, not this file.",
        "`timescale 1ns/1ns",
        "module top;",
        f"  int k = {k};",
        "  function int own();",
        "    int t;",
        "    int a [0:2];",
        "    logic [7:0] b;",
        "    t = 5;",
        "    a[k] = t;",
        "    b = 0;",
        "    b[k] = 1'b1;",
        "    return t + a[k] + b;",
        "  endfunction",
        "  function int arg(int v);",
        "    v = v + 1;",
        "    return v;",
        "  endfunction",
        "  function automatic void inc(ref int r);",
        "    r++;",
        "  endfunction",
        "  function automatic int au(int v);",
        "    int t;",
        "    int c [0:2];",
        "    logic [7:0] e;",
        "    t = v * 2;",
        "    c[k] = t;",
        "    e = 0;",
        "    e[k] = 1'b1;",
        "    inc(t);",
        "    return t + c[k] + e;",
        "  endfunction",
        "  function int st();",
        "    int t;",
        "    t = 1;",
        "    inc(t);",
        "    return t;",
        "  endfunction",
        "  function void report();",
        "    int t;",
        "    t = own() + arg(4) + au(3) + st();",
        '    $display("final %0d", t);',
        "  endfunction",
        "  initial begin",
        "    #1;",
        '    $strobe("strobe %0d %0d %0d %0d", own(), arg(4), au(3), st());',
        "    #1;",
        '    $monitor("monitor %0d %0d %0d %0d", own(), arg(4), au(3), st());',
        "    #1;",
        "    assert final (0) else report();",
        "  end",
        "endmodule",
        "",
    ]
    expected = [f"strobe {shown}", f"monitor {shown}", f"final {own + arg + au + st}"]
    return {f"{name}.sv": "\n".join(lines), f"{name}.expected": "\n".join(expected) + "\n"}


def main():
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("--check", action="store_true")
    args = ap.parse_args()
    bad = 0
    for name, text in files().items():
        path = OUT / name
        if args.check:
            if not path.exists() or path.read_text(encoding="utf-8") != text:
                print(f"gen_runtime_error_matrix: {path.relative_to(OUT.parent.parent.parent)} is "
                      "out of date; run tools/gen_runtime_error_matrix.py", file=sys.stderr)
                bad += 1
        else:
            path.write_text(text, encoding="utf-8")
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main())
