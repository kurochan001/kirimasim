#!/usr/bin/env python3
# Copyright (c) 2026 The kirimasim Authors
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

"""Generates the matrix test of streaming targets that contain streams, of
TEST_SUITE.md ("Mechanism matrix"), in tests/suite/values/.

  tools/gen_nested_stream_target_matrix.py           # writes the files
  tools/gen_nested_stream_target_matrix.py --check   # exit 1 if a file differs from what it would write

The rule: 11.4.14.3 of the standard and VALUE_SEMANTICS.md 8 (approved
2026-10-17): a streaming target is the inverse of packing. The target takes
the leftmost bits of the source, as many as it has; << k cuts them from the
left end into blocks of k bits, the last one possibly short, and reverses
their order (>> keeps it); then the operands take their bits from the left
like a concatenation. A stream among the operands takes its share the same
way, level by level. The axes: the outer and the inner operator (>> and
<< with blocks of 1 to 5 bits, short last blocks included), where the inner
stream stands (first, last, middle, and holding a third stream), what the
streams hold (packed vectors of 4, 5 and 8 bits, an unpacked array of
bytes and an unpacked structure), the assignment (blocking, NBA and
delayed NBA) and where the targets are (static variables and automatic
ones of a task). The source is three bits wider than the targets.

The expected outputs follow from the rule, worked out below as bit
strings without reading kirimasim's code. A person checks them once for
all the tests made from them (TEST_SUITE.md, "Sampling by people").
"""

import argparse
import sys
from pathlib import Path

OUT = Path(__file__).resolve().parent.parent / "tests" / "suite" / "values"
NAME = "nested_stream_target_matrix"
# The version of the rules below (TEST_SUITE.md, :generator:). Raise it
# whenever they change.
VERSION = 1

# The targets: their bits, from the left, as the stream takes them.
WIDTHS = {"a": 8, "b": 4, "c": 5, "d": 16, "st": 9}
# The leaves shown, and which bits of their target they are.
SHOWN = [("a", "a", 0, 8), ("b", "b", 0, 4), ("c", "c", 0, 5), ("d[0]", "d", 0, 8),
         ("d[1]", "d", 8, 8), ("st.x", "st", 0, 3), ("st.y", "st", 3, 6)]
OUTERS = [">>", "<<1", "<<4", "<<3"]
INNERS = [">>", "<<2", "<<4", "<<5"]


def patterns(outer, inner):
    """(name, tree): a stream is (operator, [operands]), a target a name."""
    return [
        ("first", (outer, [(inner, ["a", "b"]), "c"])),
        ("last", (outer, ["c", (inner, ["b", "d"])])),
        ("middle", (outer, ["b", (inner, ["a"]), "c"])),
        ("deep", (outer, [(inner, ["c", ("<<3", ["b", "st"])]), "a"])),
    ]


def width(node):
    if isinstance(node, str):
        return WIDTHS[node]
    return sum(width(n) for n in node[1])


def text(node, prefix=""):
    if isinstance(node, str):
        return prefix + node
    op = node[0]
    head = op if op == ">>" else "<< " + op[2:]
    return "{" + head + "{" + ", ".join(text(n, prefix) for n in node[1]) + "}}"


def reorder(op, bits):
    """The inverse of packing for one level: << k reverses the blocks of k
    bits cut from the left end, the last one possibly short."""
    if op == ">>":
        return bits
    k = int(op[2:])
    blocks = [bits[i:i + k] for i in range(0, len(bits), k)]
    return "".join(reversed(blocks))


def unpack(node, bits, out):
    if isinstance(node, str):
        out[node] = bits
        return
    bits = reorder(node[0], bits)
    at = 0
    for n in node[1]:
        w = width(n)
        unpack(n, bits[at:at + w], out)
        at += w


def source(c, w):
    """A source of w + 3 bits that differs from case to case."""
    x = (0x9E3779B97F4A7C15 * (c + 1)) & ((1 << (w + 3)) - 1)
    return format(x, f"0{w + 3}b")


def files():
    body, subs, expected = [], [], []
    c = 0
    show_fmt = " ".join("%b" for _ in SHOWN)

    def run(tag, tree, kind, pad, prefix=""):
        nonlocal c
        w = width(tree)
        src = source(c, w)
        c += 1
        got = {k: "0" * v for k, v in WIDTHS.items()}
        unpack(tree, src[:w], got)
        lines = [f"{pad}// {tag}", f"{pad}{prefix}a = 0; {prefix}b = 0; {prefix}c = 0; "
                 f"{prefix}d = '{{0, 0}}; {prefix}st = '{{0, 0}};"]
        target = text(tree, prefix)
        value = f"{w + 3}'b{src}"
        if kind == "blocking":
            lines.append(f"{pad}{target} = {value};")
        elif kind == "nba":
            lines += [f"{pad}{target} <= {value};", f"{pad}#1;"]
        else:
            lines += [f"{pad}{target} <= #1 {value};", f"{pad}#2;"]
        args = ", ".join(prefix + s[0] for s in SHOWN)
        lines.append(f'{pad}$display("{tag} {show_fmt}", {args});')
        expected.append(f"{tag} " + " ".join(got[t][o:o + n] for _, t, o, n in SHOWN))
        return lines

    for outer in OUTERS:
        for inner in INNERS:
            for name, tree in patterns(outer, inner):
                body += run(f"{outer} {inner} {name} blocking", tree, "blocking", "    ")
    for inner in INNERS:
        for kind in ("nba", "delayed"):
            name, tree = patterns("<<4", inner)[0]
            body += run(f"<<4 {inner} {name} {kind}", tree, kind, "    ")
    # Automatic targets of a task.
    subs += ["  task automatic local_targets();", "    logic [7:0] la;", "    logic [3:0] lb;",
             "    logic [4:0] lc;", "    byte unsigned ld [0:1];", "    rec_t lst;"]
    for inner in INNERS:
        name, tree = patterns("<<3", inner)[1]
        subs += run(f"<<3 {inner} {name} automatic", tree, "blocking", "    ", "l")
    subs.append("  endtask")
    body.append("    local_targets();")
    head = [
        "/*",
        f":name: {NAME}",
        ":description: a streaming target that contains streams gives each of them its share "
        "of the source, which it reorders itself, by blocking assignments and NBAs, in static "
        "and automatic targets",
        ":tags: values",
        ":type: simulation",
        ":lrm: 1800-2023 11.4.14.3 11.4.14.1",
        ":oracle: spec",
        ":spec_check: ai",
        ":fourstate: no",
        ":nondeterministic: no",
        f":generator: gen_nested_stream_target_matrix.py@{VERSION}",
        f":expected_stdout: @{NAME}.expected",
        "*/",
        "// Generated by tools/gen_nested_stream_target_matrix.py; change the generator, not "
        "this file.",
        "`timescale 1ns/1ns",
        "module top;",
        "  typedef struct { logic [2:0] x; logic [5:0] y; } rec_t;",
        "  logic [7:0] a;",
        "  logic [3:0] b;",
        "  logic [4:0] c;",
        "  byte unsigned d [0:1];",
        "  rec_t st;",
    ]
    sv = head + subs + ["  initial begin"] + body + ["  end", "endmodule", ""]
    return {f"{NAME}.sv": "\n".join(sv), f"{NAME}.expected": "\n".join(expected) + "\n"}


def main():
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("--check", action="store_true")
    args = ap.parse_args()
    bad = 0
    for name, text_ in files().items():
        path = OUT / name
        if args.check:
            if not path.exists() or path.read_text(encoding="utf-8") != text_:
                where = path.relative_to(OUT.parent.parent.parent)
                print(f"gen_nested_stream_target_matrix: {where} is out of date; run "
                      "tools/gen_nested_stream_target_matrix.py", file=sys.stderr)
                bad += 1
        else:
            path.write_text(text_, encoding="utf-8")
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main())
