#!/usr/bin/env python3
# Copyright (c) 2026 The kirimasim Authors
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

"""Generates the matrix test of subroutines called by hierarchical names,
of TEST_SUITE.md ("Mechanism matrix"), in tests/suite/hier/.

  tools/gen_hier_call_matrix.py           # writes the files
  tools/gen_hier_call_matrix.py --check   # exit 1 if a file differs from what it would write

A subroutine belongs to the instance whose body declares it, whatever
scope calls it: called by a hierarchical name, it is the same subroutine
with the same static variables, and their initial values are set once,
before time 0's procedures (6.21, 13.3.1, SIR_SPEC.md 3.3). Here each
static variable's initial value comes from a function that counts how
many times it runs, in a variable without an initial value of its own,
so that the count does not depend on the order of initializations.

The axes: what is called (a static function, a static task with an
output argument, an automatic function with a static variable); who
calls it (the instance itself, the top by a downward name from an
initial procedure, a task, a function (which calls no task, 13.4) and a
fork branch, every instance by an absolute name, and the instances calling the top's function by an
absolute name). The calls happen at different times, so that their
order is the order of time.

The expected outputs follow from the rules above, computed below. A
person checks them once for all the tests made from them (TEST_SUITE.md,
"Sampling by people").
"""

from __future__ import annotations

import argparse
import sys
from pathlib import Path

OUT = Path(__file__).resolve().parent.parent / "tests" / "suite" / "hier"
NAME = "hier_call_matrix"
# The version of the rules below (TEST_SUITE.md, :generator:). Raise it
# whenever they change.
VERSION = 1

START = 100
INSTANCES = [("m1", 1), ("m2", 2)]
SUBS = ["f", "tk", "af"]
TOP_CONTAINERS = [("initial", 10), ("task", 11), ("function", 12), ("fork", 13)]


def subs_in(container):
    """What a container may call: a function cannot call a task (13.4)."""
    return [s for s in SUBS if not (container == "function" and s == "tk")]


def call(target, sub, tag):
    if sub == "tk":
        return f'{target}tk(r); $display("{tag} %0d", r);'
    return f'$display("{tag} %0d", {target}{sub}());'


def files():
    count = {(inst, sub): START for inst, _ in INSTANCES for sub in SUBS}
    count[("top", "g")] = START
    exp = []

    def step(inst, sub, tag):
        count[(inst, sub)] += 1
        exp.append(f"{tag} {count[(inst, sub)]}")

    # In the order of time: the instances' own calls (1, 2), the top's
    # calls (10 to 13), the absolute names (21, 22), the calls of the
    # top's function (31, 32), then the counts of initializations (40, 41
    # and 42).
    for inst, d in INSTANCES:
        for sub in SUBS:
            step(inst, sub, f"{inst} own {sub}")
    for container, _ in TOP_CONTAINERS:
        for inst, _ in INSTANCES:
            for sub in subs_in(container):
                step(inst, sub, f"top {container} {inst}.{sub}")
    for inst, _ in INSTANCES:
        for sub in SUBS:
            step("m1", sub, f"{inst} absolute t.m1.{sub}")
    for inst, _ in INSTANCES:
        step("top", "g", f"{inst} absolute t.g")
    for inst, _ in INSTANCES:
        exp.append(f"{inst} initializations {len(SUBS)}")
    exp.append("top initializations 1")

    def own(target, sub, tag):
        if sub == "tk":
            return f'{target}tk(r); $display("%s {tag} %0d", name, r);'
        return f'$display("%s {tag} %0d", name, {target}{sub}());'

    # m1 runs at times 1, 21, 31 and 40; m2 at 2, 22, 32 and 41.
    module = [
        "module m;",
        "  parameter int D = 1;",
        "  int ninit;  // no initial value: the count starts at 0 before any",
        "  int r;",
        "  string name;",
        f"  function int note(); ninit++; return {START}; endfunction",
        "  function int f(); static int count = note(); count++; return count; endfunction",
        "  task tk(output int o); static int tcount = note(); tcount++; o = tcount; endtask",
        "  function automatic int af(); static int acount = note(); acount++; return acount;",
        "  endfunction",
        "  initial begin",
        "    #(D);",
    ]
    module += ["    " + own("", sub, f"own {sub}") for sub in SUBS]
    module += ["    #20;"]
    module += ["    " + own("t.m1.", sub, f"absolute t.m1.{sub}") for sub in SUBS]
    module += ["    #10;",
               '    $display("%s absolute t.g %0d", name, t.g());',
               "    #(9 - D);",
               "    #(D);",
               '    $display("%s initializations %0d", name, ninit);',
               "  end",
               "endmodule",
               ""]
    top = [
        "module t;",
        "  int ntop;",
        "  int r;",
        f"  function int ntop_note(); ntop++; return {START}; endfunction",
        "  function int g(); static int gcount = ntop_note(); gcount++; return gcount;",
        "  endfunction",
        "  m #(.D(1)) m1();",
        "  m #(.D(2)) m2();",
        "  initial begin",
        '    m1.name = "m1";',
        '    m2.name = "m2";',
        "  end",
    ]
    lines = {c: [] for c, _ in TOP_CONTAINERS}
    for container, _ in TOP_CONTAINERS:
        for inst, _ in INSTANCES:
            for sub in subs_in(container):
                lines[container].append(call(f"{inst}.", sub, f"top {container} {inst}.{sub}"))
    top += ["  task top_task();"] + ["    " + x for x in lines["task"]] + ["  endtask"]
    top += ["  function void top_function();"] + ["    " + x for x in lines["function"]]
    top += ["  endfunction"]
    top += ["  initial begin", "    #10;"] + ["    " + x for x in lines["initial"]]
    top += ["    #1;", "    top_task();", "    #1;", "    top_function();", "    #1;",
            "    fork", "      begin"] + ["        " + x for x in lines["fork"]]
    top += ["      end", "    join", "    #29;",
            '    $display("top initializations %0d", ntop);', "  end", "endmodule", ""]
    head = [
        "/*",
        f":name: {NAME}",
        ":description: a static function, a static task and an automatic function with a static "
        "variable, called by the instance itself and by hierarchical names from the top's "
        "procedures, task, function and fork branch and from every instance, are the declaring "
        "instance's: their static variables are shared and initialized once",
        ":tags: hier",
        ":type: simulation",
        ":lrm: 1800-2023 6.21 13.3.1 23.6",
        ":oracle: spec",
        ":spec_check: ai",
        ":fourstate: no",
        ":nondeterministic: no",
        f":generator: gen_hier_call_matrix.py@{VERSION}",
        ":top_module: t",
        f":expected_stdout: @{NAME}.expected",
        "*/",
        "// Generated by tools/gen_hier_call_matrix.py; change the generator, not this file.",
        "`timescale 1ns/1ns",
    ]
    sv = head + module + top
    return {f"{NAME}.sv": "\n".join(sv), f"{NAME}.expected": "\n".join(exp) + "\n"}


def main():
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("--check", action="store_true")
    args = ap.parse_args()
    bad = 0
    for name, text in files().items():
        path = OUT / name
        if args.check:
            if not path.exists() or path.read_text(encoding="utf-8") != text:
                print(f"gen_hier_call_matrix: {path.relative_to(OUT.parent.parent.parent)} is out "
                      "of date; run tools/gen_hier_call_matrix.py", file=sys.stderr)
                bad += 1
        else:
            path.write_text(text, encoding="utf-8")
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main())
