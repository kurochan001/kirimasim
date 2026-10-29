#!/usr/bin/env python3
# Copyright (c) 2026 The kirimasim Authors
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

"""Check inertial-cancellation notes without changing execution or SIR."""

import argparse
from collections import Counter
from dataclasses import dataclass
import json
from pathlib import Path
import re
import subprocess
import sys
import tempfile

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / "tools"))
import bounded_run  # noqa: E402


PULSE_TEXT = "inertial delay removed a pulse at "
PULSE_NOTE = re.compile(
    r"(?m)^(.+?):(\d+):(\d+): note: inertial delay removed a pulse at "
    r"([^\n]+) \(--report-pulses\)$")


@dataclass
class Case:
    name: str
    source: str
    # A unique source spelling at the SIR delay operation's location, time, count.
    events: tuple = ()
    options: tuple = ()

    @property
    def stdout(self):
        return self.name + " passed\n"

    def notes(self):
        result = Counter()
        for spelling, time, count in self.events:
            if self.source.count(spelling) != 1:
                raise ValueError(f"{self.name}: nonunique source location {spelling!r}")
            offset = self.source.index(spelling)
            line = self.source.count("\n", 0, offset) + 1
            column = offset - self.source.rfind("\n", 0, offset)
            result[(self.name + ".sv", line, column, time)] += count
        return result


def source(name, declarations, body, prefix="", units="1ns", precision="1ps", final=False):
    printed = f'    $display("{name} passed");\n'
    return (prefix + "module top;\n" + f"  timeunit {units}; timeprecision {precision};\n"
            + declarations + "  initial begin\n" + body + ("" if final else printed)
            + "  end\n" + (f"  final begin\n{printed}  end\n" if final else "")
            + "endmodule\n")


UDP = '''primitive pulse_buffer(q, a);
  output q;
  input a;
  table
    0 : 0;
    1 : 1;
  endtable
endprimitive
'''


def cases():
    return [
        Case("continuous", source("continuous", '''  logic data = 0;
  wire q;
  assign #5 q = data;
''', '''    #10 data = 1;
    #1 data = 0;
    #9 data = 1;
    #1 data = 0;
    #9;
    if (q !== 0) $fatal(1, "short continuous pulses escaped");
'''), (("q = data;", "11 ns", 1), ("q = data;", "21 ns", 1))),
        Case("net_initializer", source("net_initializer", '''  logic data = 0;
  wire #5 q = data;
''', '''    #10 data = 1;
    #1 data = 0;
    #9;
    if (q !== 0) $fatal(1, "net initializer pulse escaped");
'''), (("q = data;", "11 ns", 1),)),
        Case("wide", source("wide", '''  logic [128:0] data = '0;
  wire [128:0] q;
  assign #5 q = data;
''', '''    #10 data = '1;
    #1 data = '0;
    #9;
    if (q !== 129'b0) $fatal(1, "wide pulse escaped");
'''), (("q = data;", "11 ns", 1),)),
        Case("array_element", source("array_element", '''  logic data = 0;
  wire q[2];
  assign #5 q[1] = data;
''', '''    #10 data = 1;
    #1 data = 0;
    #9;
    if (q[0] !== 1'bz || q[1] !== 0) $fatal(1, "array-element pulse escaped");
'''), (("q[1] = data;", "11 ns", 1),)),
        Case("gate", source("gate", '''  logic data = 0;
  wire q;
  buf #5 pulse_gate(q, data);
''', '''    #10 data = 1;
    #1 data = 0;
    #9;
    if (q !== 0) $fatal(1, "gate pulse escaped");
'''), (("pulse_gate(q, data)", "11 ns", 1),)),
        Case("gate_array", source("gate_array", '''  logic [2:0] data = 0;
  wire [2:0] q;
  buf #5 pulse_array[2:0](q, data);
''', '''    #10 data = '1;
    #1 data = '0;
    #9;
    if (q !== 0) $fatal(1, "gate-array pulse escaped");
'''), (("pulse_array[2:0]", "11 ns", 3),)),
        Case("udp", source("udp", '''  logic data = 0;
  wire q;
  pulse_buffer #5 pulse_udp(q, data);
''', '''    #10 data = 1;
    #1 data = 0;
    #9;
    if (q !== 0) $fatal(1, "UDP pulse escaped");
''', prefix=UDP), (("pulse_udp(q, data)", "11 ns", 1),)),
        Case("replace_pending", source("replace_pending", '''  logic [3:0] data = 0;
  wire [3:0] q;
  assign #5 q = data;
''', '''    #10 data = 4'h5;
    #1 data = 4'ha;
    #4;
    if (q !== 0) $fatal(1, "replaced value reached output");
    #2;
    if (q !== 4'ha) $fatal(1, "replacement did not propagate");
'''), (("q = data;", "11 ns", 1),)),
        Case("independent", source("independent", '''  logic data = 0;
  wire first_value, second_value;
  assign #5 first_value = data;
  assign #7 second_value = data;
''', '''    #10 data = 1;
    #1 data = 0;
    #9;
    if (first_value !== 0 || second_value !== 0)
      $fatal(1, "independent pulses escaped");
'''), (("first_value = data;", "11 ns", 1), ("second_value = data;", "11 ns", 1))),
        Case("strength_origin", source("strength_origin", '''  logic enable = 0;
  wire input_value, q;
  assign (supply1, supply0) input_value = 1'b1;
  nmos #5 pulse_mos(q, input_value, enable);
''', '''    #10 enable = 1;
    #1 enable = 0;
    #9;
    if (q !== 1'bz) $fatal(1, "MOS pulse escaped");
'''), (("pulse_mos(q, input_value, enable)", "11 ns", 1),)),
        Case("passive_edge", source("passive_edge", '''  logic enable = 0;
  wire input_value, q;
  assign input_value = 1'b1;
  tranif1 #5 pulse_path(input_value, q, enable);
''', '''    #10 enable = 1;
    #1 enable = 0;
    #9;
    if (q !== 1'bz) $fatal(1, "passive-edge pulse escaped");
'''), (("pulse_path(input_value, q, enable)", "11 ns", 1),)),
        Case("complete_state", source("complete_state", '''  logic data = 1, control = 1'bx;
  wire q;
  bufif1 #5 pulse_sides(q, data, control);
  assign (weak1, weak0) q = 1'b1;
''', '''    #10 data = 0;
    #1 data = 1;
    #9;
    if (q !== 1) $fatal(1, "complete-state pulse escaped");
'''), (("pulse_sides(q, data, control)", "11 ns", 1),)),
        Case("fractional_time", source("fractional_time", '''  logic data = 0;
  wire q;
  assign #(500ps) q = data;
''', '''    $timeformat(-3, 0, " milliseconds", 20);
    #600ps data = 1;
    #400ps data = 0;
    #200ps data = 1;
    #300ps data = 0;
    #700ps;
    if (q !== 0) $fatal(1, "fractional pulse escaped");
''', units="10ps"), (("q = data;", "1 ns", 1), ("q = data;", "1500 ps", 1))),
        Case("startup", source("startup", '''  logic data = 0;
  wire q;
  assign #5 q = data;
''', '''    data = 1;
    #10;
    if (q !== 1) $fatal(1, "startup replacement did not propagate");
'''), (("q = data;", "0 ps", 1),)),
        Case("same_pending", source("same_pending", '''  logic [1:0] data = 0;
  wire q;
  assign #5 q = |data;
''', '''    #10 data = 1;
    #1 data = 2;
    #5;
    if (q !== 1) $fatal(1, "identical pending value lost its deadline");
    data = 3;
    #1;
    if (q !== 1) $fatal(1, "same committed value changed");
''')),
        Case("same_complete_state", source("same_complete_state", '''  logic data = 1, control = 1'bx;
  wire q;
  bufif1 #5 identical_sides(q, data, control);
  assign (weak1, weak0) q = 1'b1;
''', '''    #10 data = 0;
    #1 control = 1'bz;
    #5;
    if (!$isunknown(q)) $fatal(1, "identical complete state lost its deadline");
''')),
        Case("long_pulse", source("long_pulse", '''  logic data = 0;
  wire q;
  assign #5 q = data;
''', '''    #10 data = 1;
    #6;
    if (q !== 1) $fatal(1, "long pulse did not rise");
    #2 data = 0;
    #6;
    if (q !== 0) $fatal(1, "long pulse did not fall");
''')),
        Case("transport_nba", source("transport_nba", '''  logic data = 0, q = 0;
  always @(data) q <= #5 data;
''', '''    #10 data = 1;
    #1 data = 0;
    #4.1;
    if (q !== 1) $fatal(1, "NBA pulse was removed");
    #1;
    if (q !== 0) $fatal(1, "NBA pulse did not finish");
''')),
        Case("zero_delay", source("zero_delay", '''  logic data = 0;
  wire q;
  assign #0 q = data;
''', '''    #1 data = 1;
    #1;
    if (q !== 1) $fatal(1, "zero delay rise");
    data = 0;
    #1;
    if (q !== 0) $fatal(1, "zero delay fall");
''')),
        Case("finish_pending", source("finish_pending", '''  logic data = 0;
  wire q;
  assign #5 q = data;
''', '''    #10 data = 1;
    #1;
    if (q !== 0) $fatal(1, "pending value committed too early");
    $finish;
''', final=True)),
        Case("limit_pending", source("limit_pending", '''  logic data = 0;
  wire q;
  assign #5 q = data;
''', '''    #10 data = 1;
    #10;
    $fatal(1, "time limit did not stop the design");
''', final=True), options=("--max-time=11ns", "--max-time-ok")),
        Case("program", '''program pulse_program;
  timeunit 1ns; timeprecision 1ps;
  logic data = 0;
  wire q;
  assign #5 q = data;
  initial begin
    #10 data = 1;
    #1 data = 0;
    #9;
    if (q !== 0) $fatal(1, "program pulse escaped");
    $display("program passed");
  end
endprogram
module top;
  timeunit 1ns; timeprecision 1ps;
  pulse_program test();
endmodule
''', (("q = data;", "11 ns", 1),)),
    ]


def split_output(result):
    output, stats = [], []
    for line in result.stdout.splitlines():
        if line.startswith("{"):
            stats.append(json.loads(line))
        else:
            output.append(line)
    return "\n".join(output) + ("\n" if output else ""), stats


def notes(stderr):
    return Counter((Path(file).name, int(line), int(column), time)
                   for file, line, column, time in PULSE_NOTE.findall(stderr))


def has_unexpected_diagnostics(stderr):
    remainder = PULSE_NOTE.sub("", stderr)
    return bool(re.search(r"(?m)^.*: (?:warning|error|fatal|note):", remainder))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--kirimasim", type=Path, required=True)
    executable = str(parser.parse_args().kirimasim.resolve())
    failures = []
    executions = dumps = parses = invalids = 0
    with tempfile.TemporaryDirectory(prefix="kirimasim-report-pulses-") as directory:
        root = Path(directory)

        def run(path, *options):
            return bounded_run.run([executable, *options, str(path)], cwd=root, timeout=30)

        def fail(label, result, detail=""):
            failures.append(f"{label}: code={result.returncode}\n{result.stdout}\n{result.stderr}\n{detail}")

        paths = {}
        for case in cases():
            path = root / (case.name + ".sv")
            path.write_text(case.source)
            paths[case.name] = path
            baseline = None
            for flags in ((), ("--report-pulses",)):
                result = run(path, "--stats=json", *case.options, *flags)
                output, stats = split_output(result)
                wanted = case.notes() if flags else Counter()
                found = notes(result.stderr)
                if (result.returncode or output != case.stdout or len(stats) != 1 or
                        found != wanted or has_unexpected_diagnostics(result.stderr)):
                    fail(case.name + " " + ("on" if flags else "off"), result,
                         f"expected stdout={case.stdout!r}, notes={wanted!r}; got notes={found!r}")
                if not flags:
                    baseline = output, stats
                elif (output, stats) != baseline:
                    fail(case.name + " changed output or counters", result, repr(baseline))
                executions += 1

            baseline_sir = None
            for flags in ((), ("--report-pulses",)):
                result = run(path, "--dump-sir", *flags)
                if (result.returncode or not result.stdout or PULSE_TEXT in result.stderr or
                        has_unexpected_diagnostics(result.stderr)):
                    fail(case.name + " dump " + repr(flags), result)
                if not flags:
                    baseline_sir = result.stdout
                elif result.stdout != baseline_sir:
                    fail(case.name + " changed SIR", result)
                dumps += 1
                result = run(path, "parse", *flags)
                if (result.returncode or result.stdout or PULSE_TEXT in result.stderr or
                        has_unexpected_diagnostics(result.stderr)):
                    fail(case.name + " parse " + repr(flags), result)
                parses += 1

        basic = paths["continuous"]
        duplicate = run(basic, "--report-pulses", "--report-pulses")
        if (duplicate.returncode or duplicate.stdout != cases()[0].stdout or
                notes(duplicate.stderr) != cases()[0].notes() or
                has_unexpected_diagnostics(duplicate.stderr)):
            fail("duplicate flag", duplicate)
        executions += 1

        for value in ("", "0", "1", "off", "on", "false", "true"):
            for command in ((), ("parse",), ("--dump-sir",)):
                result = run(basic, *command, "--report-pulses=" + value)
                if (result.returncode != 2 or result.stdout or
                        "--report-pulses" not in result.stderr or "error:" not in result.stderr or
                        PULSE_TEXT in result.stderr):
                    fail("invalid option " + repr((command, value)), result)
                invalids += 1
        help_result = bounded_run.run([executable, "--help"], cwd=root, timeout=30)
        if help_result.returncode or "--report-pulses" not in help_result.stdout:
            fail("help", help_result)

    print(f"report-pulses: {executions} execution, {dumps} SIR, {parses} parse, "
          f"{invalids} invalid-option checks; {len(failures)} failures")
    for failure in failures:
        print(failure)
    return bool(failures)


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (bounded_run.OutputLimit, subprocess.TimeoutExpired, json.JSONDecodeError) as error:
        print(f"FAIL: {error}", file=sys.stderr)
        raise SystemExit(1)
