#!/usr/bin/env python3
"""Generates the edge matrix test of TEST_SUITE.md ("機構の行列") in
tests/suite/sched/.

  tools/gen_edge_matrix.py           # writes the files
  tools/gen_edge_matrix.py --check   # exit 1 if a file differs from what it would write

An event control waits for a posedge, a negedge, an edge or any change
(9.4.2, VALUE_SEMANTICS.md 4.3, SCHEDULER_SPEC.md 6): posedge is 0 to 1,
x or z and x or z to 1; negedge is 1 to 0, x or z and x or z to 0; edge is
either; a plain event control fires on any change of the value. An edge
is judged on the least significant bit of what is waited on. (iff is
checked by tools/gen_iff_matrix.py. Waiting on a compound expression such
as (s & en) or on an element of an unpacked array is not implemented yet;
tools/gen_unsupported_matrix.py checks that they are reported.)

The axes: the transition (every pair of 0, 1, x and z); the kind of wait
(posedge, negedge, edge, any change); what is waited on (a variable, the
least significant bit of a vector, a vector whose upper bits alone change,
a bit-select, and a net driven by a continuous assignment); the container (an always procedure, a forever
loop in an initial procedure, in an automatic task and in a fork branch).
Each waiter counts its events; for each transition the driver sets the
value from, clears the counts, sets the value to, and then prints them, so
no line depends on the order of processes in a time step.

The expected outputs follow from the rules above, computed below. A
person checks them once for all the tests made from them (TEST_SUITE.md,
"人の抜き取り").
"""

from __future__ import annotations

import argparse
import sys
from pathlib import Path

OUT = Path(__file__).resolve().parent.parent / "tests" / "suite" / "sched"
NAME = "edge_matrix"
# The version of the rules below (TEST_SUITE.md, :generator:). Raise it
# whenever they change.
VERSION = 1

VALUES = "01xz"
KINDS = [("pos", "posedge "), ("neg", "negedge "), ("edge", "edge "), ("any", "")]
CONTAINERS = ["always", "initial", "task", "fork"]


def lit(v):
    return f"1'b{v}"


# (name, the expression waited on, how a transition drives it: statements
# with {v} for the value)
SOURCES = [
    ("var", "s", ["s = {v};"]),
    ("lsb", "v", ["v = {{3'b101, {v}}};"]),
    ("upper", "u", ["u = {{{v}, {v}, {v}, 1'b0}};"]),
    ("bit", "b[2]", ["b[2] = {v};"]),
    ("net", "w", ["n = {v};"]),
]


def seen(src, v):
    """The value of the bit an edge is judged on when the source is driven
    to v."""
    if src == "upper":
        return "0"  # the least significant bit never changes
    return v


def expected(kind, src, a, b):
    """Whether the waiter fires on the transition from a to b."""
    if kind == "any" and src == "upper":
        return a != b  # any change of the whole value, the upper bits here
    return fires(kind, seen(src, a), seen(src, b))


def fires(kind, a, b):
    """Whether a change of the waited bit from a to b fires the kind."""
    pos = (a == "0" and b in "1xz") or (a in "xz" and b == "1")
    neg = (a == "1" and b in "0xz") or (a in "xz" and b == "0")
    return {"pos": pos, "neg": neg, "edge": pos or neg, "any": a != b}[kind]


def files():
    decl = [
        "  logic s, n;",
        "  logic [3:0] v, u, b;",
        "  wire w;",
        "  assign w = n;",
        f"  int cnt[0:{len(SOURCES) * len(CONTAINERS) * len(KINDS) - 1}];",
    ]
    waiters = []
    idx = {}
    k = 0
    for src, expr, _ in SOURCES:
        for cont in CONTAINERS:
            for kind, word in KINDS:
                idx[(src, cont, kind)] = k
                ctl = f"@({word}{expr})"
                body = f"cnt[{k}]++;"
                if cont == "always":
                    waiters.append(f"  always {ctl} {body}")
                elif cont == "initial":
                    waiters.append(f"  initial forever {ctl} {body}")
                elif cont == "task":
                    waiters.append(f"  task automatic t{k};\n    forever {ctl} {body}\n  endtask")
                    waiters.append(f"  initial t{k}();")
                else:
                    waiters.append(f"  initial fork\n    forever {ctl} {body}\n  join_none")
                k += 1
    drive = []
    exp = []
    for a in VALUES:
        for b in VALUES:
            def set_all(v):
                out = []
                for src, _, stmts in SOURCES:
                    out += [s.format(v=lit(v)) for s in stmts]
                return " ".join(out)
            drive.append(f"    #10 {set_all(a)}")
            drive.append("    #2 foreach (cnt[i]) cnt[i] = 0;")
            drive.append(f"    #2 {set_all(b)}")
            drive.append("    #2;")
            for src, _, _ in SOURCES:
                for cont in CONTAINERS:
                    counts = [idx[(src, cont, kind)] for kind, _ in KINDS]
                    args = ", ".join(f"cnt[{c}]" for c in counts)
                    drive.append(f'    $display("{a}{b} {src} {cont} pos=%0d neg=%0d edge=%0d '
                                 f'any=%0d", {args});')
                    vals = [str(int(expected(kind, src, a, b))) for kind, _ in KINDS]
                    exp.append(f"{a}{b} {src} {cont} pos={vals[0]} neg={vals[1]} edge={vals[2]} "
                               f"any={vals[3]}")
    head = [
        "/*",
        f":name: {NAME}",
        ":description: posedge, negedge, edge and any change on every transition among 0, 1, x "
        "and z, on a variable, a vector's least significant bit, upper bits alone, a bit-select, "
        "and a net, waited on in always, "
        "initial, automatic task and fork",
        ":tags: sched edge",
        ":type: simulation",
        ":lrm: 1800-2023 9.4.2 9.4.2.3",
        ":oracle: spec",
        ":spec_check: ai",
        ":fourstate: yes",
        ":nondeterministic: no",
        f":generator: gen_edge_matrix.py@{VERSION}",
        ":top_module: top",
        f":expected_stdout: @{NAME}.expected",
        "*/",
        "// Generated by tools/gen_edge_matrix.py; change the generator, not this file.",
        "`timescale 1ns/1ns",
        "module top;",
    ]
    sv = head + decl + waiters + ["  initial begin"] + drive + ["    $finish;", "  end",
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
                print(f"gen_edge_matrix: {path.relative_to(OUT.parent.parent.parent)} is out of "
                      "date; run tools/gen_edge_matrix.py", file=sys.stderr)
                bad += 1
        else:
            path.write_text(text, encoding="utf-8")
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main())
