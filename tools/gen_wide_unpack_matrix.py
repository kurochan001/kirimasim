#!/usr/bin/env python3
# Copyright (c) 2026 The kirimasim Authors
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

"""Generate wide streaming assignment tests (11.4.14, VALUE_SEMANTICS.md 8).

Take the leftmost target-width bits of the source. For << k, split those
bits from the left into k-bit blocks and reverse their order, keeping a
short last block intact. >> keeps the bits. Assign leaves in foreach or
member order; two-state leaves turn X/Z into 0. Repacking with the same
operator recovers the source's selected bits after that conversion.

Axes: widths around 64/128 and 256 bits; two/four states; seven block
orders; scalars, part selects, concatenations, nested structures, arrays,
multidimensional arrays and a run-time element index; module variables,
automatic tasks/functions, ref arguments and NBAs. A side-effecting RHS
and index check single evaluation and its order. NBAs capture the RHS
before it changes. Expected bit strings are computed here, not by SIR.
"""

import argparse
import sys
from pathlib import Path

OUT = Path(__file__).resolve().parent.parent / "tests" / "suite" / "values"
VERSION = 1
WIDTHS = (63, 64, 65, 127, 128, 129, 256, 260)
CONTAINERS = ("module", "automatic", "function", "ref", "nba")
ORDERS = (("right", ">>", 0), ("bit", "<<", 1), ("nibble", "<<4", 4),
          ("byte", "<<8", 8), ("word", "<<64", 64), ("cross", "<<65", 65),
          ("large", "<<1024", 1024))


def unpack(bits, block):
    if not block:
        return bits
    return "".join(reversed([bits[i:i + block] for i in range(0, len(bits), block)]))


def test(width, four, container):
    domain = "four" if four else "two"
    base = "logic" if four else "bit"
    name = f"wide_unpack_{width}_{domain}_{container}"
    low = width // 3
    high = width - low
    source_width = 4 * width + 17
    # An asymmetric pattern, including X/Z across the machine-word boundaries.
    pattern = "10x001z10110100z011x0101"
    source = (pattern * (source_width // len(pattern) + 1))[:source_width]
    declarations = ["  word_t value;", "  guard_t guard;", "  left_t left;", "  right_t right;",
                    "  row_t words;", "  grid_t cells;", "  record_t record;"]
    shapes = [("scalar", "value", width),
              ("part", f"guard[{width + 3}:4]", width),
              ("concat", "left, right", width),
              ("struct", "record", width),
              ("array", "words", 2 * width),
              ("array2d", "cells", 4 * width),
              ("dynamic", "words[choose()]", width)]
    if container == "ref":
        shapes = [s for s in shapes if s[0] != "struct"]
    body, expected = [], []
    for order_name, operator, block in ORDERS:
        for shape, target, bits in shapes:
            tag = f"{order_name} {shape}"
            payload = source[:bits]
            laid_out = unpack(payload, block)
            if not four:
                laid_out = laid_out.replace("x", "0").replace("z", "0")
                payload = payload.replace("x", "0").replace("z", "0")
            body += ["    value = '0; guard = '1; left = '0; right = '0;",
                     "    words = '{default:'0}; cells = '{default:'{default:'0}};",
                     "    source = SEED; trace = 0;"]
            op = "<=" if container == "nba" else "="
            body += [f"    {{{operator}{{{target}}}}} {op} sampled(source);"]
            if container == "nba":
                body += ["    source = '0;", "    #1;"]
            observed = "words[1]" if shape == "dynamic" else target
            body += [f'    $display("{tag} %b %b %0d", check{bits}_t\'({{>>{{{observed}}}}}), '
                     f'check{bits}_t\'({{{operator}{{{observed}}}}}), trace);']
            expected += [f"{tag} {laid_out} {payload} {12 if shape == 'dynamic' else 1}"]
            if shape == "part":
                body += [f'    $display("guard %b %b", guard[{width + 7}:{width + 4}], guard[3:0]);']
                expected += ["guard 1111 1111"]
            if shape == "dynamic":
                body += ['    $display("other %b", words[0]);']
                expected += ["other " + "0" * width]
    # A read of the target on the RHS must finish before any of its parts
    # is written. Unequal parts also cross the 64-bit boundary.
    initial = source[:width]
    initial = initial if four else initial.replace("x", "0").replace("z", "0")
    body += [f"    value = {width}'b{initial};",
             f"    {{>>{{value[{low - 1}:0], value[{width - 1}:{low}]}}}} "
             f"{'<=' if container == 'nba' else '='} value;"]
    if container == "nba":
        body += ["    value = '0;", "    #1;"]
    body += ['    $display("overlap %b", value);']
    expected += ["overlap " + initial[low:] + initial[:low]]
    lines = ["/*", f":name: {name}",
             f":description: Wide streaming targets of {width} bits, {domain} states, {container}",
             ":tags: values streaming", ":type: simulation", ":lrm: 1800-2023 11.4.14 11.4.14.3",
             ":oracle: spec", ":spec_check: ai", ":fourstate: yes", ":nondeterministic: no",
             f":generator: gen_wide_unpack_matrix.py@{VERSION}", ":top_module: top",
             f":expected_stdout: @{name}.expected", f":expected_stderr: @{name}.stderr", "*/",
             "// Generated by tools/gen_wide_unpack_matrix.py; edit the generator.",
             "`timescale 1ns/1ns", "module top;",
             f"  typedef {base} [{width - 1}:0] word_t;",
             f"  typedef {base} [{width + 7}:0] guard_t;",
             f"  typedef {base} [{low - 1}:0] left_t;",
             f"  typedef {base} [{high - 1}:0] right_t;",
             "  typedef word_t row_t [1:0];", "  typedef word_t grid_t [1:0][-2:-1];",
             "  typedef struct { left_t a; struct { right_t b; } nested; } record_t;",
             f"  typedef logic [{source_width - 1}:0] source_t;",
             *[f"  typedef logic [{bits - 1}:0] check{bits}_t;"
               for bits in (width, 2 * width, 4 * width)],
             f"  localparam source_t SEED = {source_width}'b{source};",
             "  source_t source;", "  int trace;",
             "  function automatic source_t sampled(input source_t data);",
             "    trace = trace * 10 + 1;", "    return data;", "  endfunction",
             "  function automatic int choose();", "    trace = trace * 10 + 2;",
             "    return 1;", "  endfunction"]
    if container in ("module", "nba"):
        lines += declarations + ["  initial begin"] + body + ["  end"]
    elif container == "ref":
        lines += declarations + ["  task automatic run(ref word_t value, ref guard_t guard,",
                                 "      ref left_t left, ref right_t right,",
                                 "      ref row_t words, ref grid_t cells);"]
        lines += body + ["  endtask", "  initial run(value, guard, left, right, words, cells);"]
    else:
        function = container == "function"
        lines += ["  function automatic void run();" if function else "  task automatic run();"]
        lines += declarations + body
        lines += ["  endfunction" if function else "  endtask", "  initial run();"]
    lines += ["endmodule"]
    return {name + ".sv": "\n".join(lines) + "\n",
            name + ".expected": "\n".join(expected) + "\n", name + ".stderr": ""}


def files():
    return {name: text for width in WIDTHS for four in (False, True) for container in CONTAINERS
            for name, text in test(width, four, container).items()}


def main():
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--check", action="store_true")
    args = parser.parse_args()
    bad = False
    for name, text in files().items():
        path = OUT / name
        if args.check:
            if not path.exists() or path.read_text(encoding="utf-8") != text:
                print(f"{name}: run tools/gen_wide_unpack_matrix.py", file=sys.stderr)
                bad = True
        else:
            path.write_text(text, encoding="utf-8")
    return int(bad)


if __name__ == "__main__":
    sys.exit(main())
