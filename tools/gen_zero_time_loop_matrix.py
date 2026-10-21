#!/usr/bin/env python3
# Copyright (c) 2026 The kirimasim Authors
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

"""Generates the matrix test of loops that never advance time, of
TEST_SUITE.md ("Mechanism matrix"), in tests/cli/zero_time_loop_matrix/.

  tools/gen_zero_time_loop_matrix.py           # writes the files
  tools/gen_zero_time_loop_matrix.py --check   # exit 1 if a file differs from what it would write

Processes that wake each other in zero time never let time advance. When
they wake each other inside one region, no event moves between regions, so
--max-zero-delay does not count them; --max-slot-events counts every event
run in one time slot, in any region, and stops them (CLI.md 2.6). The run
then ends with the reason zero_time_limit and exit code 5, the line of the
limit comes before the line of the reason on standard error, and final
procedures run (SCHEDULER_SPEC.md 16.2). The default of the limit takes
seconds to reach, so tests/cli/run_cli_tests.py runs each file with
--max-slot-events=1000 and checks exactly that, as the limit is a rule of
kirimasim's command line and not of the standard.

One file per loop. The axis is how the processes wake each other: slot
subscriptions (@(a)), event expressions (@(!a)), a wait statement,
named events, a ring of two continuous assignments, always_comb, always @*
(whose variables start at 0, as it does not run before its first change),
a continuous assignment and an always procedure together, a nonblocking
assignment, whose update comes back through the NBA region, and a process
that reads what it writes, which its own change evaluates again
(SCHEDULER_SPEC.md 5, 12): a continuous assignment on its net, on its
variable, on another part of its vector, with #0, and on a net that a
second assignment drives too; two assignments to parts of one vector; a
net declaration assignment; an assignment in an instance whose input and
output ports connect to one net; a gate; a UDP instance; a procedural
assign; and a force. Each loop is held back by en, or by an event, until
time 1, so the run stops at 1 ns. The Reactive side comes with programs,
which kirimasim reports as unsupported for now (tools/gen_program_matrix.py).
Loops that advance time, and processes that read what they write and
settle, are in tools/gen_cont_self_read_matrix.py.
"""

from __future__ import annotations

import argparse
import sys
from pathlib import Path

OUT = Path(__file__).resolve().parent.parent / "tests" / "cli" / "zero_time_loop_matrix"

# (name, what the loop is, the items of top, the statement at time 1 that
# starts the loop)
LOOPS = [
    ("subscription", "two always procedures that wake each other through @(a) and @(b)",
     ["logic a = 0, b = 0;", "always @(a) b = ~b;", "always @(b) a = ~a;"], "a = 1;"),
    ("expression", "two always procedures that wake each other through event expressions",
     ["logic a = 0, b = 0;", "always @(!a) b = ~b;", "always @(~b) a = ~a;"], "a = 1;"),
    ("wait", "a wait statement and an always procedure that wake each other",
     ["logic a = 0, b = 0;", "always begin", "  wait (en && a == b);", "  b = ~b;", "end",
      "always @(b) a = ~a;"], "en = 1;"),
    ("named_event", "two always procedures that trigger each other's named event",
     ["event e1, e2;", "always @e1 ->e2;", "always @e2 ->e1;"], "->e1;"),
    ("assign_ring", "a ring of two continuous assignments",
     ["wire a, b;", "assign a = en ? ~b : 1'b0;", "assign b = a;"], "en = 1;"),
    ("always_comb", "a ring of two always_comb procedures",
     ["logic a, b;", "always_comb a = en ? ~b : 1'b0;", "always_comb b = a;"], "en = 1;"),
    ("always_star", "a ring of two always @* procedures",
     ["logic a = 0, b = 0;", "always @* a = en ? ~b : 1'b0;", "always @* b = a;"], "en = 1;"),
    ("assign_always", "a continuous assignment and an always procedure in a ring",
     ["wire b;", "logic a = 0;", "assign b = en ? ~a : 1'b0;", "always @(b) a = b;"], "en = 1;"),
    ("nba", "an always procedure woken by its own nonblocking assignment",
     ["logic a = 0;", "always @(a) if (en) a <= ~a;"], "begin en = 1; a = 1; end"),
    ("assign_self", "a continuous assignment that reads its own net",
     ["wire a;", "assign a = en ? ~a : 1'b0;"], "en = 1;"),
    ("assign_self_var", "a continuous assignment that reads its own variable",
     ["logic a;", "assign a = en ? ~a : 1'b0;"], "en = 1;"),
    ("assign_self_vector", "a continuous assignment that reads another part of its own vector",
     ["wire [1:0] v;", "assign v = en ? {v[0], ~v[1]} : 2'b00;"], "en = 1;"),
    ("assign_self_delay0", "a continuous assignment with delay #0 that reads its own net",
     ["wire a;", "assign #0 a = en ? ~a : 1'b0;"], "en = 1;"),
    ("assign_self_resolved", "a continuous assignment that reads its own net, which a second "
     "assignment drives too", ["wire a;", "assign a = en ? ~a : 1'b0;", "assign a = 1'bz;"],
     "en = 1;"),
    ("assign_parts", "two continuous assignments to parts of one vector, each reading the "
     "other's part", ["wire [1:0] v;", "assign v[0] = en ? ~v[1] : 1'b0;", "assign v[1] = v[0];"],
     "en = 1;"),
    ("net_decl_self", "a net declaration assignment that reads its own net",
     ["wire a = en ? ~a : 1'b0;"], "en = 1;"),
    ("port_self", "a continuous assignment in an instance whose input and output ports "
     "connect to one net", ["wire a;", "inv u(.e(en), .i(a), .o(a));"], "en = 1;"),
    ("gate_self", "a gate whose output is one of its inputs",
     ["wire a;", "nand g(a, en, a);"], "en = 1;"),
    ("udp_self", "a UDP instance whose output is one of its inputs",
     ["wire a;", "nand_udp u(a, en, a);"], "en = 1;"),
    ("proc_assign_self", "a procedural assign that reads the variable it holds",
     ["logic a = 0;"], "assign a = ~a;"),
    ("force_self", "a force that reads the net it holds",
     ["wire a;", "assign a = 1'b0;"], "force a = ~a;"),
]

# The lines before module top that some loops need.
BEFORE = {
    "port_self": ["module inv(input e, input i, output o);", "  assign o = e ? ~i : 1'b0;",
                  "endmodule"],
    "udp_self": ["primitive nand_udp(output o, input x, input y);", "  table", "    0 ? : 1;",
                 "    ? 0 : 1;", "    1 1 : 0;", "  endtable", "endprimitive"],
}


def design(name, what, items, start):
    lines = [
        "// Generated by tools/gen_zero_time_loop_matrix.py; change the generator, not this file.",
        f"// {what[0].upper()}{what[1:]}: the loop never advances time, and",
        "// --max-slot-events stops it (CLI.md 2.6).",
        *BEFORE.get(name, []),
        "module top;",
        "  logic en = 0;",
    ]
    lines += [f"  {x}" for x in items]
    lines += [
        "  initial begin",
        "    $display(\"started\");",
        f"    #1 {start}",
        "  end",
        "  final $display(\"final ran\");",
        "endmodule",
        "",
    ]
    return "\n".join(lines)


def files():
    return {f"{name}.sv": design(name, what, items, start) for name, what, items, start in LOOPS}


def main():
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("--check", action="store_true")
    args = ap.parse_args()
    bad = 0
    want = files()
    if args.check:
        have = {p.name for p in OUT.glob("*.sv")} if OUT.exists() else set()
        for name in sorted(have - set(want)):
            print(f"gen_zero_time_loop_matrix: {name} is not generated; remove it", file=sys.stderr)
            bad += 1
    else:
        OUT.mkdir(exist_ok=True)
    for name, text in want.items():
        path = OUT / name
        if args.check:
            if not path.exists() or path.read_text(encoding="utf-8") != text:
                print(f"gen_zero_time_loop_matrix: {path.relative_to(OUT.parent.parent.parent)} is "
                      "out of date; run tools/gen_zero_time_loop_matrix.py", file=sys.stderr)
                bad += 1
        else:
            path.write_text(text, encoding="utf-8")
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main())
