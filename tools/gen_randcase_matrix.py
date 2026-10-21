#!/usr/bin/env python3
# Copyright (c) 2026 The kirimasim Authors
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

"""Generates the matrix tests of randcase (TEST_SUITE.md, "Mechanism
matrix") in tests/suite/proc/.

  tools/gen_randcase_matrix.py           # writes the files
  tools/gen_randcase_matrix.py --check   # exit 1 if a file differs from what it would write

The rules (18.16, SYSTEM_TASKS.md 13):

- The weights are evaluated in the order of the branches, each at its own
  width, and added as unsigned values at the width of the widest.
- A sum of 0 takes no branch and draws nothing.
- Otherwise r = $urandom_range(sum - 1) from the generator of the process
  (tools/gen_urandom_matrix.py gives its rules), and the first branch whose
  weight, added to those before it, is greater than r is taken.

The axes: the weights (the cells below: constants, a zero weight,
a signed weight narrower than the others, expressions of other widths, a
sum of 0, one branch, a branch that is a block) and the container of the statement (an initial procedure, a task,
a function, a fork branch, a task in a generate block). Each cell runs in
a loop, so that every branch with a weight is taken at some point.

The expected outputs come from the rules above, computed here, without
reading kirimasim's code. No other simulator was compared: the standard
leaves the generator to each implementation.
"""

import argparse
import sys
from pathlib import Path

OUT = Path(__file__).resolve().parent.parent / "tests" / "suite" / "proc"
# The version of the rules below (TEST_SUITE.md, :generator:). Raise it
# whenever they change.
VERSION = 1

CONTAINERS = ["initial", "task", "function", "fork", "generate"]
MASK = (1 << 64) - 1
ROUNDS = 12


class Rng:
    """SplitMix64 (SYSTEM_TASKS.md 13.1)."""

    def __init__(self, state):
        self.state = state & MASK

    def next(self):
        self.state = (self.state + 0x9e3779b97f4a7c15) & MASK
        z = self.state
        z = ((z ^ (z >> 30)) * 0xbf58476d1ce4e5b9) & MASK
        z = ((z ^ (z >> 27)) * 0x94d049bb133111eb) & MASK
        return z ^ (z >> 31)

    def draw(self):
        return self.next() >> 32

    def child(self):
        return Rng(self.next())

    def range(self, maxval, minval=0):
        if maxval < minval:
            maxval, minval = minval, maxval
        return minval + ((self.draw() * (maxval - minval + 1)) >> 32)


def init(seed, name):
    h = 0xcbf29ce484222325
    for b in name.encode():
        h = ((h ^ b) * 0x100000001b3) & MASK
    return Rng(seed ^ h)


# Each cell: (name, the branches as (weight, its value as the model sees
# it, the widest width it makes), the width of the sum).
# a is byte 3, b is byte 0, n is byte -1 (255 as an unsigned weight), c is
# bit [3:0] 4'hf.
CELLS = [
    ("constants", [("3", 3), ("0", 0), ("1", 1)], 32),
    ("signed_narrow", [("n", 255), ("1", 1)], 32),
    ("narrow", [("c", 15), ("a", 3), ("b", 0)], 8),
    ("zero_sum", [("0", 0), ("b", 0)], 32),
    ("one_branch", [("5", 5)], 32),
]


def choose(g, weights, width):
    m = (1 << width) - 1
    s = sum(w & m for w in weights) & m
    if s == 0:
        return None
    r = g.range(s - 1)
    acc = 0
    for i, w in enumerate(weights):
        acc = (acc + (w & m)) & m
        if r < acc:
            return i
    return None


def statements():
    body = ["for (int k = 0; k < %d; ++k) begin" % ROUNDS]
    for n, (key, branches, _) in enumerate(CELLS):
        items = " ".join(f"{w}: h{n} = {{h{n}, 8'd{i + 1}}};" for i, (w, _) in enumerate(branches))
        body.append(f"  randcase {items} endcase")
    body.append("end")
    body += [f'$display("{key}: %h", h{n});' for n, (key, _, _) in enumerate(CELLS)]
    body.append("randcase 1: begin y = 1; y = y + 1; end endcase")
    body.append('$display("block: %0d", y);')
    return body


def expected(g):
    picks = [[] for _ in CELLS]
    for _ in range(ROUNDS):
        for n, (_, branches, width) in enumerate(CELLS):
            i = choose(g, [v for _, v in branches], width)
            if i is not None:
                picks[n].append(i + 1)
    out = []
    for n, (key, _, _) in enumerate(CELLS):
        h = 0
        for p in picks[n]:
            h = ((h << 8) | p) & ((1 << 128) - 1)
        out.append(f"{key}: {h:032x}")
    choose(g, [1], 32)
    out.append("block: 2")
    return out


def test(container):
    name = f"randcase_{container}"
    lines = ["/*", f":name: {name}",
             ":description: randcase picks a branch by its weights from the generator of the "
             f"process, in {'an' if container[0] in 'aeiou' else 'a'} {container}",
             ":tags: proc", ":type: simulation", ":lrm: 1800-2023 18.16 18.14", ":oracle: spec",
             ":spec_check: ai", ":fourstate: yes", ":nondeterministic: no",
             f":generator: gen_randcase_matrix.py@{VERSION}", ":top_module: top",
             f":expected_stdout: @{name}.expected", f":expected_stderr: @{name}.stderr", "*/",
             "// Generated by tools/gen_randcase_matrix.py; change the generator, not this file.",
             "`timescale 1ns/1ns", "module top;", "  byte a = 3, b = 0, n = -1;",
             "  bit [3:0] c = 4'hf;",
             "  int y = 0;"]
    lines += [f"  bit [127:0] h{n} = 0;" for n in range(len(CELLS))]
    body = statements()
    if container == "task":
        lines += ["  task pick;"] + ["    " + b for b in body] + ["  endtask"]
        calls = ["pick;"]
    elif container == "function":
        lines += ["  function void pick();"] + ["    " + b for b in body] + ["  endfunction"]
        calls = ["pick();"]
    elif container == "generate":
        lines += ["  if (1) begin : g", "    task pick;"] + ["      " + b for b in body] + [
            "    endtask", "  end"]
        calls = ["g.pick;"]
    elif container == "fork":
        calls = ["fork begin"] + ["  " + b for b in body] + ["end join"]
    else:
        calls = body
    lines.append("  initial begin")
    lines += ["    " + c for c in calls]
    lines += ["  end", "endmodule"]
    main = init(0, "top").child()
    return name, lines, expected(main.child() if container == "fork" else main)


def files():
    out = {}
    for container in CONTAINERS:
        name, lines, stdout = test(container)
        out[f"{name}.sv"] = "\n".join(lines) + "\n"
        out[f"{name}.expected"] = "\n".join(stdout) + "\n"
        out[f"{name}.stderr"] = ""
    return out


def main():
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("--check", action="store_true")
    args = ap.parse_args()
    bad = 0
    for name, text in files().items():
        path = OUT / name
        if args.check:
            if not path.exists() or path.read_text(encoding="utf-8") != text:
                print(f"gen_randcase_matrix: {path.relative_to(OUT.parent.parent.parent)} is out "
                      "of date; run tools/gen_randcase_matrix.py", file=sys.stderr)
                bad += 1
        else:
            path.write_text(text, encoding="utf-8")
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main())
