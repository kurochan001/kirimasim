#!/usr/bin/env python3
# Copyright (c) 2026 The kirimasim Authors
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

"""Generate fixed-aggregate bit-query cases from a bit-character model.

SYSTEM_TASKS 8 / IEEE 1800-2023 20.9 interprets the first argument as its
streamed bits. Count characters directly, independently of lowering and the
kernel counter. Unknowns become zero in declared two-state fields. Duplicate
controls count a matching bit once and only a control's low bit is used.
"""

import argparse
from pathlib import Path

OUT = Path(__file__).resolve().parent.parent / "tests/suite/values"
VERSION = 1


def files():
    result = {}
    for width in (1, 5, 65, 129):
        for four in (False, True):
            for shape in ("array", "record", "record_array"):
                # Query lowering reuses existing aggregate storage. Automatic
                # arrays of records and record refs remain separate P1 gaps.
                contexts = ["initial", "task", "function", "fork"]
                if shape != "record_array":
                    contexts += ["automatic", "receiver"]
                if shape == "array":
                    contexts += ["ref"]
                for context in contexts:
                    name = f"aggregate_bit_query_{shape}_{context}_{'four' if four else 'two'}_{width}"
                    word = "logic" if four else "bit"
                    types = f"typedef {word} [{width - 1}:0] word_t;\n"
                    if shape == "array":
                        types += "typedef word_t payload_t[2:0];\n"
                        leaves = [f"data[{i}]" for i in (2, 1, 0)]
                    elif shape == "record":
                        types += "typedef struct {word_t a; word_t b; word_t c;} payload_t;\n"
                        leaves = ["data." + f for f in ("a", "b", "c")]
                    else:
                        types += ("typedef struct {word_t a; word_t b;} row_t;\n"
                                  "typedef row_t payload_t[-1:0];\n")
                        leaves = [f"data[{i}].{f}" for i in (-1, 0) for f in ("a", "b")]
                    body, expected = "", ""
                    argument = "receiver()" if context == "receiver" else "data"
                    for phase in ("one", "many", "mixed"):
                        bits = []
                        for i, leaf in enumerate(leaves):
                            value = ("0" * (width - 1) + ("1" if i == 0 else "0")
                                     if phase == "one" else "1" * width if phase == "many"
                                     else ("10xz" * ((width + 3) // 4))[-width:])
                            body += f"    {leaf} = {width}'b{value};\n"
                            bits += list(value if four else value.replace("x", "0").replace("z", "0"))
                        counts = [bits.count(c) for c in "01xz"]
                        zeros, ones, xs, zs = counts
                        numbers = counts + [zeros + ones, ones, int(ones == 1), int(ones <= 1), int(xs + zs > 0)]
                        body += (f'    $display("{phase} ' + " ".join(["%0d"] * 9) + '",\n'
                                 f"      $countbits({argument},1'b0), $countbits({argument},control()),\n"
                                 f"      $countbits({argument},1'bx), $countbits({argument},1'bz),\n"
                                 f"      $countbits({argument},1'b0,1'b0,4'b1001), $countones({argument}),\n"
                                 f"      $onehot({argument}), $onehot0({argument}), $isunknown({argument}));\n")
                        expected += phase + " " + " ".join(map(str, numbers)) + "\n"
                    body += '    $display("controls %0d", controls);\n'
                    expected += "controls 3\n"
                    if context in ("initial", "receiver"):
                        run = "payload_t data;\ninitial begin\n" + body + "end\n"
                        if context == "receiver":
                            run = ("payload_t data;\nint evaluations;\n"
                                   "function automatic payload_t receiver();\n"
                                   "  evaluations++; return data;\nendfunction\ninitial begin\n"
                                   + body + '    $display("evaluations %0d", evaluations);\nend\n')
                            expected += "evaluations 27\n"
                    elif context == "automatic":
                        run = "task automatic run; payload_t data;\n" + body + "endtask\ninitial run();\n"
                    elif context == "ref":
                        run = ("payload_t data;\ntask automatic run(ref payload_t data);\n"
                               + body + "endtask\ninitial run(data);\n")
                    elif context == "task":
                        run = "payload_t data;\ntask run;\n" + body + "endtask\ninitial run();\n"
                    elif context == "fork":
                        run = "payload_t data;\ninitial fork begin\n" + body + "end join\n"
                    else:
                        run = ("payload_t data;\nfunction void run();\n"
                               + body + "endfunction\ninitial run();\n")
                    source = f'''/*
:name: {name}
:description: Bit queries on {shape} in {context}, {width}-bit {word} leaves
:tags: values
:type: simulation
:lrm: 1800-2023 20.9 11.4.14.1 7.2.1
:oracle: spec
:spec_check: ai
:fourstate: {'yes' if four else 'no'}
:nondeterministic: no
:generator: gen_aggregate_bit_query_matrix.py@{VERSION}
:top_module: top
:expected_stdout: @{name}.expected
*/
// Generated by tools/gen_aggregate_bit_query_matrix.py.
module top;
{types}
int controls;
function logic [3:0] control();
  controls++;
  return 4'b1001;
endfunction
{run}
endmodule
'''
                    result[name + ".sv"] = source
                    result[name + ".expected"] = expected
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--check", action="store_true")
    args = parser.parse_args()
    failures = 0
    for name, text in files().items():
        target = OUT / name
        if args.check:
            if not target.exists() or target.read_text() != text:
                print(f"gen_aggregate_bit_query_matrix: out of date: {name}")
                failures += 1
        else:
            target.write_text(text)
    return bool(failures)


if __name__ == "__main__":
    raise SystemExit(main())
