#!/usr/bin/env python3
# Copyright (c) 2026 The kirimasim Authors
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

"""Check construct-specific X/Z diagnostics without changing default execution."""

import argparse
from dataclasses import dataclass
import json
from pathlib import Path
import re
import subprocess
import sys
import tempfile

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE.parents[1] / "tools"))
import bounded_run  # noqa: E402


@dataclass
class Case:
    name: str
    source: str
    stdout: str
    # Executed unknown conditions, in order: (unique source spelling, construct, count).
    events: tuple = ()
    error_stdout: str = "final\n"
    final_only: bool = False


def cases():
    return [
        Case("basic", '''module top;
  logic [1:0] s = 2'b1x, x = 2'b0x, z = 2'b0z;
  int score;
  initial begin
    repeat (3) begin
      if (s) score += 1; else score += 100;
    end
    if (x) score += 100; else score += 4;
    if (z) score += 100; else score += 8;
    score += s ? 16 : 32;
    score += x ? 64 : 64;
    casez (s) 2'b1?: score += 128; default: score += 10000; endcase
    casex (x) 2'b00: score += 256; default: score += 10000; endcase
    case (x) 2'b0x: score += 512; default: score += 10000; endcase
    case (x) inside 2'b??: score += 1024; default: score += 10000; endcase
    $display("basic %0d", score);
  end
  final $display("final");
endmodule
''', "basic 2015\nfinal\n", (
            ("if (s)", "if", 3), ("if (x)", "if", 1), ("if (z)", "if", 1),
            ("? 16", "?:", 1), ("? 64", "?:", 1), ("casez (s)", "case", 1),
            ("casex (x)", "case", 1), ("case (x) 2'b0x", "case", 1),
            ("case (x) inside", "case", 1))),
        Case("wide", '''module top;
  logic [64:0] a = '0;
  logic [128:0] b = '0;
  logic z = 1'bz;
  bit known = 1;
  real r = 1.5;
  int score;
  initial begin
    a[64] = 1'bx; a[0] = 1;
    b[128] = 1'bz; b[0] = 1;
    if (a) score += 1;
    if (b) score += 2;
    if (z) score += 100; else score += 4;
    if (known) score += 8;
    if (r) score += 16;
    case (r) 1.5: score += 32; default: score += 10000; endcase
    $display("wide %0d", score);
  end
  final $display("final");
endmodule
''', "wide 63\nfinal\n", (("if (a)", "if", 1), ("if (b)", "if", 1),
                             ("if (z)", "if", 1))),
        Case("skipped", '''module top;
  int value, score;
  initial begin
    value = 1 ? 11 : (1'bx ? 22 : 33); score += value;
    value = 0 && (1'bx ? 1 : 1); score += value;
    value = 1 || (1'bx ? 0 : 0); score += value;
    value = 0 -> (1'bx ? 0 : 0); score += value;
    value = 1 ? (1'bx ? 7 : 7) : 88; score += value;
    value = (1'bx ? 1 : 1) ? 4 : 5; score += value;
    if (0) begin
      if (1'bx) score += 10000;
    end
    case (0)
      0: score += 2;
      default: if (1'bx) score += 10000;
    endcase
    $display("skipped %0d", score);
  end
  final $display("final");
endmodule
''', "skipped 26\nfinal\n", (("? 7", "?:", 1), ("? 1 : 1) ? 4", "?:", 1))),
        Case("nested", '''module top;
  int value;
  initial begin
    value = (1'bx ? 1'bx : 1'bx) ? 7 : 7;
    $display("nested %0d", value);
  end
  final $display("final");
endmodule
''', "nested 7\nfinal\n", (("? 1'bx", "?:", 1), ("? 7", "?:", 1))),
        Case("qualifiers", '''module top;
  logic [1:0] x = 2'b0x;
  int score;
  initial begin
    priority if (1) score += 1;
    else if (x) score += 100;
    else score += 100;
    unique if (x) score += 100;
    else if (1) score += 2;
    else score += 100;
    unique0 if (1) score += 4;
    else if (x) score += 200;
    else score += 100;
    priority if (x) score += 100;
    else if (1) score += 8;
    else score += 100;
    $display("qualifiers %0d", score);
  end
  final $display("final");
endmodule
''', "qualifiers 15\nfinal\n", (("unique if (x)", "if", 1),
                                   ("if (x) score += 200", "if", 1),
                                   ("priority if (x)", "if", 1))),
        Case("aggregates", '''module top;
  logic x = 1'bx;
  real ra = 1.5, rb = 2.5, rout;
  string sa = "a", sb = "b", sout;
  typedef bit [7:0] fixed_t [2];
  fixed_t a = '{11,22}, out;
  bit [7:0] q[$] = '{33,44}, copied[$];
  initial begin
    rout = x ? ra : rb;
    sout = x ? sa : sb;
    out = x ? a : a;
    copied = x ? q : q;
    copied[0] = 55;
    $display("aggregate %.1f [%s] %0d %0d %0d %0d %0d", rout, sout,
             out[0], out[1], q[0], copied[0], copied[1]);
  end
  final $display("final");
endmodule
''', "aggregate 0.0 [] 11 22 33 55 44\nfinal\n", (
            ("? ra", "?:", 1), ("? sa", "?:", 1), ("? a : a", "?:", 1),
            ("? q : q", "?:", 1))),
        Case("static_init", '''module top;
  parameter int W = 1'bx ? 8 : 8;
  logic [W-1:0] bits;
  int boot = 1'bx ? 9 : 9;
  initial $display("static %0d %0d", boot, $bits(1'bx ? bits : bits));
  final $display("final");
endmodule
''', "static 9 8\nfinal\n", (("? 9", "?:", 1),)),
        Case("functions", '''module top;
  int a, b;
  function automatic int pick(input logic [1:0] condition);
    if (condition) return 7;
    else return 8;
  endfunction
  initial begin
    a = pick(2'b1x); b = pick(2'b0x);
    $display("function %0d %0d", a, b);
  end
  final $display("final");
endmodule
''', "function 7 8\nfinal\n", (("if (condition)", "if", 2),)),
        Case("non_targets", '''module top;
  logic x = 1'bx;
  int score;
  initial begin
    while (x) score += 100;
    assert (x) score += 100; else score += 1;
    wait (x);
    score += 2;
    $display("non-target %0d", score);
  end
  initial #1 x = 1;
  final $display("final");
endmodule
''', "non-target 3\nfinal\n", error_stdout="non-target 3\nfinal\n"),
        Case("runtime_folds", '''module top;
  typedef enum int {A=10, B=20, C=30} enum_t;
  enum_t e = A, next_value;
  logic [7:0] bits = 1;
  int value;
  string formatted;
  initial begin
    #(1'bx ? 1 : 1);
    value = bits[1'bx ? 0 : 0];
    next_value = e.next(1'bx ? 1 : 1);
    $sformat(formatted, 1'bx ? "%0d" : "%0d", 7);
    $display("folds %0d %0d %s", value, next_value, formatted);
  end
  final $display("final");
endmodule
''', "folds 1 20 7\nfinal\n", (("? 1 : 1);\n    value", "?:", 1),
                                          ("? 0", "?:", 1),
                                          ("? 1 : 1);\n    $sformat", "?:", 1),
                                          ('? "%0d"', "?:", 1))),
        Case("event_ff", '''module top;
  bit [1:0] clocks = '0;
  int seen;
  always_ff @(posedge clocks[1'bx ? 0 : 0]) seen <= seen + 1;
  initial begin
    #1 clocks[0] = 1;
    #1 clocks[0] = 0;
    #1 clocks[0] = 1;
    #1 $display("events %0d", seen);
    $finish;
  end
  final $display("final");
endmodule
''', "events 2\nfinal\n", (("? 0", "?:", 6),)),
        Case("event_mixed", '''module top;
  bit [1:0] clocks = '0;
  int seen, index = 1;
  always @(posedge clocks[1'bx ? 0 : 0] or posedge clocks[index]) seen <= seen + 1;
  initial begin
    #1 clocks[0] = 1;
    #1 clocks[0] = 0;
    #1 clocks[0] = 1;
    #1 $display("events %0d", seen);
    $finish;
  end
  final $display("final");
endmodule
''', "events 2\nfinal\n", (("? 0", "?:", 6),)),
        Case("event_startup_ff", '''module top;
  bit [1:0] clocks = '0;
  int seen;
  initial begin
    $display("initial");
    clocks[0] = 1;
    #1 $display("startup %0d", seen);
    $finish;
  end
  always_ff @(posedge clocks[1'bx ? 0 : 0]) seen <= seen + 1;
  final $display("final");
endmodule
''', "initial\nstartup 1\nfinal\n", (("? 0", "?:", 3),)),
        Case("event_startup_plain", '''module top;
  bit [1:0] clocks = '0;
  int seen;
  initial begin
    $display("initial");
    clocks[0] = 1;
    #1 $display("startup %0d", seen);
    $finish;
  end
  always @(posedge clocks[1'bx ? 0 : 0]) seen <= seen + 1;
  final $display("final");
endmodule
''', "initial\nstartup 1\nfinal\n", (("? 0", "?:", 3),)),
        Case("constant_finish", '''module top;
  initial begin
    $display("before");
    $finish(1'bx ? 0 : 0);
    $display("unreachable");
  end
  final $display("final");
endmodule
''', "before\nfinal\n", (("? 0", "?:", 1),), error_stdout="before\nfinal\n"),
        Case("unevaluated", '''module top;
  localparam int N = 1'bx ? 2 : 2;
  int value;
  initial begin
    value = $bits(1'bx ? 8'hff : 8'h00);
    value += {(1'bx ? 2 : 2){1'b1}};
    $display("types %0d %0d", value, N);
  end
  final $display("final");
endmodule
''', "types 11 2\nfinal\n"),
        Case("final", '''module top;
  initial $display("body");
  final begin
    int value;
    value = 1'bx ? 5 : 5;
    $display("late %0d", value);
  end
endmodule
''', "body\nlate 5\n", (("? 5", "?:", 1),), error_stdout="body\n", final_only=True),
    ]


def point(source, spelling):
    # Match the declared occurrence independently of generated IR or diagnostics.
    start = source.find(spelling)
    if start < 0:
        raise ValueError(f"no source spelling {spelling!r}")
    line = source.count("\n", 0, start) + 1
    column = start - source.rfind("\n", 0, start)
    return line, column


def diagnostic(construct, mode):
    noun = "expression" if construct == "case" else "condition"
    return f"{construct} {noun} contains X/Z bits (--cond-x={mode})"


def output_and_stats(result):
    stats = []
    lines = []
    for line in result.stdout.splitlines():
        if line.startswith("{"):
            stats.append(json.loads(line))
        else:
            lines.append(line)
    return "\n".join(lines) + ("\n" if lines else ""), stats


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--kirimasim", type=Path, required=True)
    exe = str(parser.parse_args().kirimasim.resolve())
    failures = []
    executions = dumps = parses = 0
    with tempfile.TemporaryDirectory(prefix="kirimasim-cond-x-") as directory:
        root = Path(directory)
        paths = {}

        def run(path, *options):
            return bounded_run.run([exe, *options, str(path)], cwd=root, timeout=30)

        def fail(label, result, detail=""):
            failures.append(f"{label}: code {result.returncode}\n{result.stdout}\n"
                            f"{result.stderr}\n{detail}")

        for case in cases():
            path = root / (case.name + ".sv")
            path.write_text(case.source, encoding="utf-8")
            paths[case.name] = path
            sites = [(point(case.source, spelling), construct, count)
                     for spelling, construct, count in case.events]
            default_stats = None
            for mode in (None, "off", "warn", "error"):
                options = () if mode is None else ("--cond-x=" + mode,)
                result = run(path, "--stats=json", *options)
                design, stats = output_and_stats(result)
                warns = re.findall(r"(?m)^.+?:(\d+):(\d+): warning: ([^\n]+)$", result.stderr)
                errors = re.findall(r"(?m)^.+?:(\d+): error: ([^\n]+)$", result.stderr)
                summary = re.findall(r"condition X check: (\d+) evaluations at "
                                     r"(\d+) source positions", result.stderr)
                want_warnings = [(str(line), str(column), diagnostic(kind, "warn"))
                                 for (line, column), kind, _ in sites] if mode == "warn" else []
                want_summary = [(str(sum(n for _, _, n in sites)), str(len(sites)))] \
                    if mode == "warn" and sites else []
                stopping = mode == "error" and bool(sites)
                expected_output = case.error_stdout if stopping else case.stdout
                code = 2 if stopping and not case.final_only else 0
                expected_error = diagnostic(sites[0][1], "error") if stopping else None
                error_ok = (len(errors) == 1 and int(errors[0][0]) == sites[0][0][0]
                            and errors[0][1].startswith(expected_error + " [top @ ")) \
                    if stopping else not errors
                if (result.returncode != code or design != expected_output or
                        warns != want_warnings or summary != want_summary or
                        not error_ok or len(stats) != 1):
                    fail(f"{case.name} {mode}", result,
                         f"expected stdout {expected_output!r}, warnings {want_warnings!r}, "
                         f"summary {want_summary!r}")
                elif mode is None:
                    default_stats = stats
                elif mode == "off" and stats != default_stats:
                    fail(f"{case.name} off changes default counters", result)
                elif stopping:
                    reason = "quiescent" if case.final_only else "error"
                    if stats[0].get("sim.end_reason") != reason:
                        fail(f"{case.name} error end reason", result, reason)
                executions += 1

            implicit = run(path, "--dump-sir")
            off = run(path, "--dump-sir", "--cond-x=off")
            if (implicit.returncode or off.returncode or implicit.stdout != off.stdout or
                    "cond.x " in implicit.stdout):
                fail(case.name + " default/off SIR", off, implicit.stdout)
            dumps += 2
            if case.name in ("basic", "nested", "aggregates", "runtime_folds", "event_ff", "event_mixed",
                             "event_startup_ff", "event_startup_plain"):
                warn = run(path, "--dump-sir", "--cond-x=warn")
                error = run(path, "--dump-sir", "--cond-x=error")
                if (warn.returncode or error.returncode or warn.stdout != error.stdout or
                        "cond.x " not in warn.stdout or ": warning:" in warn.stderr or
                        ": error:" in warn.stderr):
                    fail(case.name + " enabled SIR", warn, error.stdout + error.stderr)
                if case.name.startswith("event_") and warn.stdout.count('cond.x "') != 1:
                    fail("event trigger metadata probe duplicated cond.x", warn)
                dumps += 2

        basic = paths["basic"]
        for mode in ("off", "warn", "error"):
            result = run(basic, "parse", "--cond-x=" + mode)
            if result.returncode or result.stdout or ": error:" in result.stderr:
                fail("parse " + mode, result)
            parses += 1
        for value in ("", "yes", "WARN", "warning", "0"):
            for command in ((), ("parse",)):
                result = run(basic, *command, "--cond-x=" + value)
                if result.returncode != 2 or result.stdout or "needs off, warn or error" not in result.stderr:
                    fail("invalid option " + repr((command, value)), result)
        result = run(basic, "--cond-x=warn", "--cond-x=off")
        if result.returncode or result.stdout != cases()[0].stdout or ": warning:" in result.stderr:
            fail("last option off", result)
        result = run(basic, "--cond-x=off", "--cond-x=warn")
        if result.returncode or "condition X check: 11 evaluations at 9 source positions" not in result.stderr:
            fail("last option warn", result)
        result = bounded_run.run([exe, "--help"], cwd=root, timeout=30)
        if result.returncode or "--cond-x=off|warn|error" not in result.stdout:
            fail("help", result)

    if failures:
        print("\n".join(failures), file=sys.stderr)
        return 1
    print(f"cond-x: {executions} execution, {dumps} SIR, {parses} parse checks; "
          "10 invalid options, option precedence and help passed")
    return 0


if __name__ == "__main__":
    try:
        sys.exit(main())
    except (bounded_run.OutputLimit, subprocess.TimeoutExpired) as error:
        print(f"FAIL: {error}", file=sys.stderr)
        sys.exit(1)
