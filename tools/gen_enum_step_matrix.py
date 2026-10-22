#!/usr/bin/env python3
# Copyright (c) 2026 The kirimasim Authors
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

"""Generate runtime enum next/prev tests (6.19.5.3-4, VALUE_SEMANTICS.md 7.7).

Movement uses declaration order, with the count converted to int unsigned,
and wraps at both ends. A nonmember gives the base type's default value.
The Python oracle below uses lists of bit strings, independently of SIR.

Axes: sparse two/four-state and signed enums, one member, X/Z members,
65/260-bit values; zero, whole cycles, large, negative, wide and X/Z counts;
scalar, array element and struct member; initial, always, final, static and
automatic task, function, ref, fork, generate and interface. Receiver and count calls also check exactly
one evaluation each, in left-to-right order (VALUE_SEMANTICS.md 3.1).
"""

import argparse
import sys
from pathlib import Path

OUT = Path(__file__).resolve().parent.parent / "tests" / "suite" / "values"
VERSION = 1
CONTAINERS = ("initial", "always", "final", "task", "static_task", "function", "ref",
              "fork", "generate", "interface")
# name, width, four-state, signed, member bit strings, nonmember bit strings
TYPES = [
    ("two", 8, False, False, ["00001101", "00000010", "01100011"], ["00000000"]),
    ("signed", 8, True, True, ["11111001", "00101010", "11111111"],
     ["00000000", "xxxxxxxx"]),
    ("xz", 3, True, False, ["000", "1x0", "zz1"], ["111", "xxx"]),
    ("single", 1, False, False, ["1"], ["0"]),
    ("four", 2, True, False, ["00", "01", "11"], ["10", "xx"]),
    ("wide65", 65, True, False,
     [format(1 << 64 | 1, "065b"), format(2, "065b"), format((1 << 65) - 2, "065b")],
     ["0" * 65, "x" * 65]),
    ("wide260", 260, True, False,
     [format(1, "0260b"), format(1 << 259, "0260b"), format((1 << 259) + 7, "0260b")],
     ["0" * 260, "x" * 260]),
]
# The low 32 bits after conversion to int unsigned; X and Z bits become 0.
COUNTS = [("128'd0", 0), ("128'd1", 1), ("128'd2", 2), ("128'd3", 3),
          ("128'd6", 6), ("128'd7", 7), ("128'h80000000", 0x80000000),
          ("128'hffffffff", 0xFFFFFFFF), ("-128'sd1", 0xFFFFFFFF),
          ("128'h100000002", 2), ("128'hxxxxxxxx", 0),
          ("128'hzzzzzzz5", 5), ("128'hx0000003", 3)]


def moved(value, members, count, default):
    if value not in members:
        return default
    return members[(members.index(value) + count) % len(members)]


def test(kind, container):
    tag, width, four, signed, members, invalid = kind
    name = f"enum_step_{tag}_{container}"
    default = ("x" if four else "0") * width
    values = members + invalid
    base = ("logic" if four else "bit") + (" signed" if signed else "")
    entries = ", ".join(f"E{i} = {width}'b{bits}" for i, bits in enumerate(members))
    declarations = [f"  typedef enum {base} [{width - 1}:0] {{{entries}}} enum_t;",
                    "  typedef struct { enum_t item; } record_t;",
                    "  enum_t value, items[2];", "  record_t record;", "  int trace;"]
    helpers = ["  function automatic enum_t receiver(input enum_t item);",
               "    trace = trace * 10 + 1;", "    return item;", "  endfunction",
               "  function automatic int unsigned distance();",
               "    trace = trace * 10 + 2;", "    return 2;", "  endfunction"]
    local = [f"    enum_t values[{len(values)}];", f"    logic [127:0] counts[{len(COUNTS)}];",
             "    enum_t forward, backward;"]
    if container in ("task", "function"):
        local += ["    enum_t value, items[2];", "    record_t record;"]
    body = [f"    values[{i}] = enum_t'({width}'b{v});" for i, v in enumerate(values)]
    body += [f"    counts[{j}] = {expr};" for j, (expr, _) in enumerate(COUNTS)]
    body += [f"    for (int i = 0; i < {len(values)}; i++) begin",
             f"      for (int j = 0; j < {len(COUNTS)}; j++) begin"]
    expected = []
    places = ("value", "items[1]", "record.item")
    for p, target in enumerate(places):
        body += [f"        {target} = values[i];",
                 f"        forward = {target}.next(counts[j]);",
                 f"        backward = {target}.prev(counts[j]);",
                 f'        $display("{p} %0d %0d %b %b %b", i, j, forward, backward, {target});']
    body += ["      end", "      trace = 0;",
             "      forward = receiver(values[i]).next(distance());",
             '      $display("next-call %0d %b %0d", i, forward, trace);',
             "      trace = 0;", "      backward = receiver(values[i]).prev(distance());",
             '      $display("prev-call %0d %b %0d", i, backward, trace);', "    end"]
    for i, v in enumerate(values):
        for j, (_, count) in enumerate(COUNTS):
            forward = moved(v, members, count, default)
            backward = moved(v, members, -count, default)
            expected += [f"{p} {i} {j} {forward} {backward} {v}" for p in range(len(places))]
        expected += [f"next-call {i} {moved(v, members, 2, default)} 12",
                     f"prev-call {i} {moved(v, members, -2, default)} 12"]
    lines = ["/*", f":name: {name}",
             f":description: Runtime enum next and prev with {tag} values in {container}",
             ":tags: values enum", ":type: simulation", ":lrm: 1800-2023 6.19.5.3 6.19.5.4",
             ":oracle: spec", ":spec_check: ai", ":fourstate: yes", ":nondeterministic: no",
             f":generator: gen_enum_step_matrix.py@{VERSION}", ":top_module: top",
             f":expected_stdout: @{name}.expected", f":expected_stderr: @{name}.stderr", "*/",
             "// Generated by tools/gen_enum_step_matrix.py; edit the generator."]
    lines.append("interface worker;" if container == "interface" else "module top;")
    lines += declarations + helpers
    if container == "generate":
        lines += ["  if (1) begin : generated", "  initial begin"] + local + body
        lines += ["  end", "  end"]
    elif container in ("task", "static_task", "function", "ref"):
        opening = {"task": "task automatic run();", "function": "function automatic void run();",
                   "static_task": "task run();",
                   "ref": "task automatic run(ref enum_t value);"}[container]
        lines += ["  " + opening] + local + body
        lines += ["  endfunction" if container == "function" else "  endtask",
                  "  initial run(value);" if container == "ref" else "  initial run();"]
    else:
        if container == "always":
            lines += ["  event go;", "  initial #1 ->go;", "  always @(go) begin"]
        else:
            lines += ["  final begin" if container == "final" else "  initial begin"]
        lines += local
        if container == "fork":
            lines += ["    fork", "      begin"] + body + ["      end", "    join"]
        else:
            lines += body
        lines += ["  end"]
    lines += (["endinterface", "module top;", "  worker u();", "endmodule"]
              if container == "interface" else ["endmodule"])
    return {name + ".sv": "\n".join(lines) + "\n",
            name + ".expected": "\n".join(expected) + "\n", name + ".stderr": ""}


def files():
    return {name: text for kind in TYPES for container in CONTAINERS
            for name, text in test(kind, container).items()}


def main():
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--check", action="store_true")
    args = parser.parse_args()
    bad = False
    for name, text in files().items():
        path = OUT / name
        if args.check:
            if not path.exists() or path.read_text(encoding="utf-8") != text:
                print(f"{name}: run tools/gen_enum_step_matrix.py", file=sys.stderr)
                bad = True
        else:
            path.write_text(text, encoding="utf-8")
    return int(bad)


if __name__ == "__main__":
    sys.exit(main())
