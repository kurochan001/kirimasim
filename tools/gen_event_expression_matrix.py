#!/usr/bin/env python3
# Copyright (c) 2026 The kirimasim Authors
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

"""Generates the matrix test of event controls on expressions of
TEST_SUITE.md ("Mechanism matrix") in tests/suite/sched/.

  tools/gen_event_expression_matrix.py           # writes the files
  tools/gen_event_expression_matrix.py --check   # exit 1 if a file differs from what it would write

An event control on an expression wakes when the expression's value
changes, not when an operand changes alone; posedge, negedge and edge are
judged on the value's least significant bit (9.4.2, SCHEDULER_SPEC.md
9.1, VALUE_SEMANTICS.md 4.3). kirimasim evaluates the expression again
whenever something it reads is written, so each write of the driver is
an occasion: a waiter wakes at the write that makes its event, and waits
again from the value after the driver's step. An iff is read when
the item's event occurs (9.4.2.3); an `or` list wakes at its first item.

The axes: the expression (s & t, s | t, s == t, s === t, !s, an element
m[i] and a bit v[i] at a run-time index, a function call, and v + 1, four
bits wide); the kind of wait (posedge, negedge, edge, any change,
posedge with iff, and `expr or negedge u`); the container (an always
procedure, a forever loop in an initial procedure, in an automatic task
and in a fork branch, and an intra-assignment event control). Each
waiter counts its events; for each step the driver notes the counts,
writes one or more variables in a row, and prints the counts since.

The expected outputs follow from the rules above, computed below. A
person checks them once for all the tests made from them (TEST_SUITE.md,
"Sampling by people").
"""

from __future__ import annotations

import argparse
import sys
from pathlib import Path

OUT = Path(__file__).resolve().parent.parent / "tests" / "suite" / "sched"
NAME = "event_expression_matrix"
# The version of the rules below (TEST_SUITE.md, :generator:). Raise it
# whenever they change.
VERSION = 1


def and4(a, b):
    if "0" in (a, b):
        return "0"
    return "1" if (a, b) == ("1", "1") else "x"


def or4(a, b):
    if "1" in (a, b):
        return "1"
    return "0" if (a, b) == ("0", "0") else "x"


def known(*bits):
    return all(c in "01" for c in bits)


# (name, the expression, its value in a state: a string of bits, most
# significant first)
EXPRS = [
    ("and", "(s & t)", lambda st: and4(st["s"], st["t"])),
    ("or", "(s | t)", lambda st: or4(st["s"], st["t"])),
    ("eq", "(s == t)",
     lambda st: str(int(st["s"] == st["t"])) if known(st["s"], st["t"]) else "x"),
    ("ceq", "(s === t)", lambda st: str(int(st["s"] == st["t"]))),
    ("not", "(!s)", lambda st: {"0": "1", "1": "0"}.get(st["s"], "x")),
    ("index", "m[i]", lambda st: st["m"][st["i"]] if 0 <= st["i"] < 4 else "x"),
    ("bit", "v[i]", lambda st: st["v"][3 - st["i"]] if 0 <= st["i"] < 4 else "x"),
    ("call", "f(s, t)",
     lambda st: str(int(st["s"] != st["t"])) if known(st["s"], st["t"]) else "x"),
    ("sum", "(v + 4'd1)",
     lambda st: f"{(int(st['v'], 2) + 1) % 16:04b}" if known(*st["v"]) else "xxxx"),
]
KINDS = [("pos", "posedge {e}"), ("neg", "negedge {e}"), ("edge", "edge {e}"), ("any", "{e}"),
         ("iff", "posedge {e} iff en"), ("or2", "{e} or negedge u")]
CONTAINERS = ["always", "initial", "task", "fork", "intra"]
# The driver's steps: the writes of each, in order. Within a step, no
# waiter has more than one write that could wake it (counts() checks):
# the standard lets a simulator run another process between two of them
# (4.7), and a waiter that ran there would wait again and could wake
# twice. kirimasim runs a process until it waits (SCHEDULER_SPEC.md 4.3).
STEPS = [
    [("s", "1")],
    [("t", "1")],
    [("s", "0")],
    [("t", "0")],
    [("en", "1"), ("t", "1")],
    [("s", "1")],
    [("s", "x")],
    [("t", "z")],
    [("s", "0"), ("en", "0")],
    [("t", "0")],
    [("i", 1)],
    [("m[1]", "1")],
    [("m[2]", "1")],
    [("i", 2), ("m[1]", "0")],
    [("i", 5)],
    [("v", "0101"), ("i", 0)],
    [("v", "0110")],
    [("v", "01x0")],
    [("v", "0000"), ("u", "1")],
    [("u", "0")],
    [("i", 3), ("v", "1000")],
    [("u", "1"), ("s", "1")],
]
INITIAL = {"s": "0", "t": "0", "en": "0", "u": "0", "i": 0, "v": "0000", "m": "0000"}


def edges(a, b):
    """posedge and negedge of a change of a bit from a to b."""
    pos = (a == "0" and b in "1xz") or (a in "xz" and b == "1")
    neg = (a == "1" and b in "0xz") or (a in "xz" and b == "0")
    return pos, neg


def apply(state, var, value):
    st = dict(state)
    if var.startswith("m["):
        k = int(var[2])
        m = list(st["m"])
        m[k] = value
        st["m"] = "".join(m)
    else:
        st[var] = value
    return st


def fires(kind, old, new, before, after):
    """Whether a wait of the kind fires on the write from state `before`
    to `after`, its expression going from old to new."""
    pos, neg = edges(old[-1], new[-1])
    if kind == "pos":
        return pos
    if kind == "neg":
        return neg
    if kind == "edge":
        return pos or neg
    if kind == "any":
        return old != new
    if kind == "iff":
        return pos and after["en"] == "1"
    upos, uneg = edges(before["u"], after["u"])
    return old != new or uneg


def counts(expr, kind):
    """The events a waiter counts in each step."""
    value = dict((n, f) for n, _, f in EXPRS)[expr]
    state = dict(INITIAL)
    last = value(state)
    out = []
    for k, step in enumerate(STEPS):
        armed = True
        n = 0
        could = 0  # writes that would wake it if it waited
        for var, val in step:
            after = apply(state, var, val)
            new = value(after)
            if fires(kind, last, new, state, after):
                could += 1
                if armed:
                    n += 1
                    armed = False
            last = new
            state = after
        assert could <= 1, f"step {k} could wake {expr} {kind} twice"
        last = value(state)  # it waits again after the driver's step
        out.append(n)
    return out


def lit(var, value):
    if var == "i":
        return str(value)
    if var == "v":
        return f"4'b{value}"
    return f"1'b{value}"


def waiter(cont, k, ctl):
    body = f"cnt[{k}]++;"
    if cont == "always":
        return [f"  always @({ctl}) {body}"]
    if cont == "initial":
        return [f"  initial forever @({ctl}) {body}"]
    if cont == "task":
        return [f"  task automatic t{k};\n    forever @({ctl}) {body}\n  endtask",
                f"  initial t{k}();"]
    if cont == "fork":
        return [f"  initial fork\n    forever @({ctl}) {body}\n  join_none"]
    return [f"  initial forever cnt[{k}] = @({ctl}) cnt[{k}] + 1;"]


def files():
    waiters, idx = [], {}
    k = 0
    for name, expr, _ in EXPRS:
        for cont in CONTAINERS:
            for kind, form in KINDS:
                idx[(name, cont, kind)] = k
                waiters += waiter(cont, k, form.format(e=expr))
                k += 1
    decl = [
        "  logic s = 1'b0, t = 1'b0, en = 1'b0, u = 1'b0;",
        "  logic [3:0] v = 4'b0000;",
        "  logic m [0:3] = '{1'b0, 1'b0, 1'b0, 1'b0};",
        "  int i = 0;",
        f"  int cnt[0:{k - 1}], base[0:{k - 1}];",
        "  function automatic logic f(logic a, logic b);",
        "    return a ^ b;",
        "  endfunction",
    ]
    lines, exp = [], []
    expected = {(name, kind): counts(name, kind) for name, _, _ in EXPRS for kind, _ in KINDS}
    for n, step in enumerate(STEPS):
        lines.append("    #10 base = cnt;")
        lines.append("    #2 " + " ".join(f"{var} = {lit(var, val)};" for var, val in step))
        lines.append("    #2;")
        for name, _, _ in EXPRS:
            for cont in CONTAINERS:
                ids = [idx[(name, cont, kind)] for kind, _ in KINDS]
                fmt = " ".join(f"{kind}=%0d" for kind, _ in KINDS)
                args = ", ".join(f"cnt[{c}] - base[{c}]" for c in ids)
                lines.append(f'    $display("{n} {name} {cont} {fmt}", {args});')
                exp.append(f"{n} {name} {cont} " + " ".join(
                    f"{kind}={expected[(name, kind)][n]}" for kind, _ in KINDS))
    head = [
        "/*",
        f":name: {NAME}",
        ":description: event controls on expressions: posedge, negedge, edge, any change, iff "
        "and an or list with a signal, on logic and equality operators, a function call, an "
        "element and a bit at a run-time index and a sum, through writes that change the value "
        "or only an operand, in always, initial, automatic task, fork and an intra-assignment "
        "event control",
        ":tags: sched edge",
        ":type: simulation",
        ":lrm: 1800-2023 9.4.2 9.4.2.1 9.4.2.3 9.4.5",
        ":oracle: spec",
        ":spec_check: ai",
        ":fourstate: yes",
        ":nondeterministic: no",
        f":generator: gen_event_expression_matrix.py@{VERSION}",
        ":top_module: top",
        f":expected_stdout: @{NAME}.expected",
        "*/",
        "// Generated by tools/gen_event_expression_matrix.py; change the generator, not this file.",
        "`timescale 1ns/1ns",
        "module top;",
    ]
    sv = head + decl + waiters + ["  initial begin"] + lines + ["    $finish;", "  end",
                                                              "endmodule", ""]
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
                print(f"gen_event_expression_matrix: {path.relative_to(OUT.parent.parent.parent)} "
                      "is out of date; run tools/gen_event_expression_matrix.py", file=sys.stderr)
                bad += 1
        else:
            path.write_text(text, encoding="utf-8")
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main())
