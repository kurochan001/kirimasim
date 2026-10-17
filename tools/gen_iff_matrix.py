#!/usr/bin/env python3
"""Generates the matrix test of event controls with iff of TEST_SUITE.md
("機構の行列") in tests/suite/sched/.

  tools/gen_iff_matrix.py           # writes the files
  tools/gen_iff_matrix.py --check   # exit 1 if a file differs from what it would write

An item `ev iff cond` of an event control fires when its event occurs and
cond is true then (9.4.2.3, SCHEDULER_SPEC.md 9): cond is evaluated when
the item's event occurs and only then, so a change of cond alone wakes
nothing, a change of cond after the event in the same time step is not
seen, and the other items of an `or` list do not read it. x and z are
false (12.4). The edges are those of tools/gen_edge_matrix.py.

The axes: the transition of what is waited on (0 to 1, 1 to 0, 0 to x, 1
to z, no change); the value of the condition's variable (0, 1, x); the
kind of item (posedge, negedge, edge, any change, and `posedge iff c or
negedge iff !(c)`); the condition (a variable, its negation, a call of an
automatic function); what is waited on (a variable, a bit-select, a net
driven by a continuous assignment); the container (always, always_ff,
an always that waits inside, a forever loop in an initial procedure, in
an automatic task whose condition reads a ref argument, in a fork branch,
and an intra-assignment event control). Each waiter counts its events;
for each step the driver sets the value from, notes the counts, sets the
condition, then sets the value to and the condition to another value with
a nonblocking assignment, and prints the counts since. A last part sets
the condition with blocking assignments just before and just after the
edge, in the same process.

The expected outputs follow from the rules above, computed below. A
person checks them once for all the tests made from them (TEST_SUITE.md,
"人の抜き取り").
"""

from __future__ import annotations

import argparse
import sys
from pathlib import Path

OUT = Path(__file__).resolve().parent.parent / "tests" / "suite" / "sched"
NAME = "iff_matrix"
# The version of the rules below (TEST_SUITE.md, :generator:). Raise it
# whenever they change.
VERSION = 1

TRANSITIONS = ["01", "10", "0x", "1z", "11"]
ENABLES = "01x"
# (name, the event control with {x} for what is waited on and {c} for the
# condition)
KINDS = [
    ("pos", "posedge {x} iff {c}"),
    ("neg", "negedge {x} iff {c}"),
    ("edge", "edge {x} iff {c}"),
    ("any", "{x} iff {c}"),
    ("split", "posedge {x} iff {c} or negedge {x} iff !({c})"),
]
# (name, the condition with {e} for the enable, its truth for the enable's
# value)
CONDS = [
    ("en", "{e}", lambda e: e == "1"),
    ("nen", "!{e}", lambda e: e == "0"),
    ("fn", "f({e})", lambda e: e == "1"),
]
# (name, the expression waited on, how a step drives it with {v})
SOURCES = [
    ("var", "s", "s = {v};"),
    ("bit", "b[2]", "b[2] = {v};"),
    ("net", "w", "n = {v};"),
]
CONTAINERS = ["always", "always_ff", "general", "initial", "task", "fork", "intra"]


def lit(v):
    return f"1'b{v}"


def after(e):
    """The enable's value set by the nonblocking assignment after the edge:
    one whose truth differs."""
    return "0" if e == "1" else "1"


def edges(a, b):
    """posedge and negedge of a change of the waited bit from a to b."""
    pos = (a == "0" and b in "1xz") or (a in "xz" and b == "1")
    neg = (a == "1" and b in "0xz") or (a in "xz" and b == "0")
    return pos, neg


def fires(kind, a, b, c, nc):
    """Whether an item of the kind fires on a change from a to b, with c
    the truth of the condition then and nc that of its negation."""
    pos, neg = edges(a, b)
    return {
        "pos": pos and c,
        "neg": neg and c,
        "edge": (pos or neg) and c,
        "any": a != b and c,
        "split": (pos and c) or (neg and nc),
    }[kind]


def waiter(cont, k, ctl):
    """The declarations of waiter k, counting in cnt[k]."""
    body = f"cnt[{k}]++;"
    if cont == "always":
        return [f"  always @({ctl}) {body}"]
    if cont == "always_ff":
        return [f"  always_ff @({ctl}) {body}"]
    if cont == "general":
        return [f"  always begin @({ctl}); {body} end"]
    if cont == "initial":
        return [f"  initial forever @({ctl}) {body}"]
    if cont == "task":
        return [f"  task automatic t{k}(ref logic e);\n    forever @({ctl}) {body}\n  endtask",
                f"  initial t{k}({{e}});"]
    if cont == "fork":
        return [f"  initial fork\n    forever @({ctl}) {body}\n  join_none"]
    return [f"  initial forever cnt[{k}] = @({ctl}) cnt[{k}] + 1;"]


def files():
    waiters = []
    idx = {}
    k = 0
    for src, expr, _ in SOURCES:
        for cont in CONTAINERS:
            for cname, cexpr, _ in CONDS:
                for kind, form in KINDS:
                    idx[(src, cont, cname, kind)] = k
                    e = "e" if cont == "task" else "en"
                    ctl = form.format(x=expr, c=cexpr.format(e=e))
                    waiters += [w.replace("{e}", "en") for w in waiter(cont, k, ctl)]
                    k += 1
    order = {}
    for cont in CONTAINERS:
        order[cont] = k
        ctl = "posedge o iff " + ("e" if cont == "task" else "oe")
        waiters += [w.replace("{e}", "oe") for w in waiter(cont, k, ctl)]
        k += 1
    decl = [
        "  logic s, n, o, en, oe;",
        "  logic [3:0] b;",
        "  wire w;",
        "  assign w = n;",
        f"  int cnt[0:{k - 1}], base[0:{k - 1}];",
        "  function automatic logic f(logic a);",
        "    return a;",
        "  endfunction",
    ]

    def drive(v):
        return " ".join(stmt.format(v=lit(v)) for _, _, stmt in SOURCES)

    def show(tag, rows):
        out = []
        for label, counts in rows:
            args = ", ".join(f"cnt[{c}] - base[{c}]" for c in counts)
            fmt = " ".join(f"{kind}=%0d" for kind, _ in KINDS[:len(counts)])
            out.append(f'    $display("{tag} {label} {fmt}", {args});')
        return out

    lines, exp = [], []
    for t in TRANSITIONS:
        a, b = t[0], t[1]
        for e in ENABLES:
            lines.append(f"    #10 en = 1'b0; {drive(a)}")
            lines.append("    #2 base = cnt;")
            lines.append(f"    #2 en = {lit(e)};")
            lines.append(f"    #2 {drive(b)} en <= {lit(after(e))};")
            lines.append("    #2;")
            rows = []
            for src, _, _ in SOURCES:
                for cont in CONTAINERS:
                    for cname, _, truth in CONDS:
                        counts = [idx[(src, cont, cname, kind)] for kind, _ in KINDS]
                        rows.append((f"{src} {cont} {cname}", counts))
                        c = truth(e)
                        nc = not c and e != "x"  # !(c) is true only when c is false
                        vals = [int(fires(kind, a, b, c, nc)) for kind, _ in KINDS]
                        exp.append(f"{t} e={e} {src} {cont} {cname} " +
                                   " ".join(f"{kind}={v}" for (kind, _), v in zip(KINDS, vals)))
            lines += show(f"{t} e={e}", rows)
    # The condition set in the same process just before the edge is seen;
    # one set just after it is not, and wakes nothing.
    for label, stmts, fired in [("before", "oe = 1'b1; o = 1'b1; oe = 1'b0;", 1),
                                ("after", "oe = 1'b0; o = 1'b1; oe = 1'b1;", 0)]:
        lines.append("    #10 oe = 1'b0; o = 1'b0;")
        lines.append("    #2 base = cnt;")
        lines.append(f"    #2 {stmts}")
        lines.append("    #2;")
        lines += show(f"order {label}", [(cont, [order[cont]]) for cont in CONTAINERS])
        exp += [f"order {label} {cont} pos={fired}" for cont in CONTAINERS]
    head = [
        "/*",
        f":name: {NAME}",
        ":description: event controls with iff: posedge, negedge, edge, any change and an or "
        "list of two items with their own conditions, the condition true, false or x when the "
        "event occurs, changed alone, or changed just before and after the event, on a "
        "variable, a bit-select and a net, in always, always_ff, an always that waits inside, "
        "initial, an automatic task reading a ref argument, fork and an intra-assignment "
        "event control",
        ":tags: sched edge iff",
        ":type: simulation",
        ":lrm: 1800-2023 9.2.2.4 9.4.2 9.4.2.3 9.4.5 12.4",
        ":oracle: spec",
        ":spec_check: ai",
        ":fourstate: yes",
        ":nondeterministic: no",
        f":generator: gen_iff_matrix.py@{VERSION}",
        ":top_module: top",
        f":expected_stdout: @{NAME}.expected",
        "*/",
        "// Generated by tools/gen_iff_matrix.py; change the generator, not this file.",
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
                print(f"gen_iff_matrix: {path.relative_to(OUT.parent.parent.parent)} is out of "
                      "date; run tools/gen_iff_matrix.py", file=sys.stderr)
                bad += 1
        else:
            path.write_text(text, encoding="utf-8")
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main())
