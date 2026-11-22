#!/usr/bin/env python3
# Copyright (c) 2026 The kirimasim Authors
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

"""Process sensitivity, bit ranges, time-zero and NBA ordering (4.6, 9.2.2, 10.4.2)."""
import argparse
import json
import re
from pathlib import Path
import subprocess
import tempfile


def run(exe, cwd, *args, code=0):
    result = subprocess.run([exe, *args], cwd=cwd, capture_output=True, text=True, timeout=30)
    if result.returncode != code:
        raise AssertionError((args, result.returncode, result.stdout, result.stderr))
    return result


def matrix(exe, work):
    source = work / "model.sv"
    report = work / "islands"
    for container in ("module", "generate", "interface"):
        for kind in ("assign", "always_comb", "always_latch", "always_ff", "plain_ff"):
            clocked = kind in ("always_ff", "plain_ff")
            keyword = {"always_ff": "always_ff @(posedge clk)",
                       "plain_ff": "always @(posedge clk)",
                       "always_latch": "always_latch if (clk)"}.get(kind, kind)
            assign = "<=" if clocked else "="
            body = f"{keyword} q {assign} a;\n{keyword} z {assign} " + ("a" if clocked else "q") + ";"
            declared = "wire" if kind == "assign" else "logic"
            signals = f"logic clk, a; {declared} q, z;"
            if container == "interface":
                wrapper = f"interface bus; {signals} {body} endinterface\nmodule top; bus b();"
                prefix = "b."
            else:
                if container == "generate":
                    body = "if (1) begin : g\n" + body + "\nend"
                wrapper = f"module top; {signals} {body}"
                prefix = ""
            source.write_text(wrapper + f"""
initial begin
  {prefix}clk=0; {prefix}a=0; #1;
  {prefix}clk=1; #1; $display("first %b %b", {prefix}q,{prefix}z);
  {prefix}clk=0; {prefix}a=1; #1; {prefix}clk=1;
  #1; $display("second %b %b", {prefix}q,{prefix}z);
end
endmodule
""")
            off = run(exe, work, "--disable-pass=islands", source)
            on = run(exe, work, "--enable-pass=islands", f"--report-islands={report}", source)
            assert off.stdout == on.stdout == "first 0 0\nsecond 1 1\n", (container, kind, on.stdout)
            first = report.read_bytes()
            saved = run(exe, work, "--dump-sir=1", source)
            (work / "model.sir").write_text(saved.stdout)
            run(exe, work, "--load-sir=model.sir", f"--report-islands={report}", "--dump-sir")
            assert report.read_bytes() == first, (container, kind)
            assert b"members 2\n" in first, (container, kind, first)


def boundaries(exe, work):
    # The middle waiter stays outside the ff island because it prints its sampled input.
    # It must still issue its NBA between the other two, on every re-arm.
    source = work / "order.sv"
    report = work / "order.report"
    source.write_text("""
module top;
bit clk; int q;
always @(posedge clk) q <= 1;
always @(posedge clk) begin $display("sample %0d", q); q <= 2; end
always @(posedge clk) q <= 3;
initial begin repeat(4) #1 clk=~clk; #1; $display("end %0d",q); end
endmodule
""")
    for flag in ("--enable-pass=islands", "--disable-pass=islands"):
        got = run(exe, work, flag, f"--report-islands={report}", source)
        assert got.stdout == "sample 0\nsample 3\nend 3\n", got.stdout

    source.write_text("""
module top;
wire [2:0] v;
assign v[0] = v[1];
assign v[1] = v[0];
assign v[2] = v[1];
initial #1 $display("%b", v);
endmodule
""")
    run(exe, work, f"--report-islands={report}", "--dump-sir", source)
    data = report.read_text()
    assert "reason scc 2\n" in data and "members 1\n" in data, data


def state_report(exe, work):
    source = work / "states.sv"
    source.write_text("""module top;
  logic [3:0] data, y; bit clk;
  always_comb y = data + clk;
  initial begin data=1; #1; clk=1; #1; $display("%d",y); end
endmodule
""")
    plain = run(exe, work, "--enable-pass=islands", source)
    got = run(exe, work, "--enable-pass=islands", "--report-state-opt", "--stats=json", source)
    stdout, counts = got.stdout.rstrip().rsplit("\n", 1)
    assert stdout == plain.stdout.rstrip()
    assert "top.clk storage=2 reason=declared-two-state" in got.stderr
    assert "top.data storage=4 reason=declared-four-state" in got.stderr
    pairs = re.findall(r"^state island [0-9]+ two=([0-9]+) four=([0-9]+) fraction=(?:[0-9]+/[0-9]+|n/a)$", got.stderr, re.M)
    stats = json.loads(counts)
    assert pairs and sum(int(a) for a, _ in pairs) == stats["island.evals2"]
    assert sum(int(b) for _, b in pairs) == stats["island.evals4"]


def counts(exe, work):
    root = Path(__file__).resolve().parents[2]
    for name in ("comb_chain", "nba_shift"):
        got = []
        for flag in ("--disable-pass=islands", "--enable-pass=islands"):
            p = run(exe, work, flag, "--stats=json", root / "bench" / (name + ".sv"), "+n=32")
            line, stats = p.stdout.rstrip().rsplit("\n", 1)
            got.append((line, json.loads(stats)))
        assert got[0][0] == got[1][0], got
        before, after = got[0][1], got[1][1]
        for key in ("proc.wakeups", "sched.events.active"):
            assert after[key] < before[key], (name, key, before, after)
        for key in ("vm.instrs", "slot.changes"):
            assert after[key] <= before[key], (name, key, before, after)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--kirimasim", type=Path, required=True)
    exe = parser.parse_args().kirimasim.resolve()
    with tempfile.TemporaryDirectory() as tmp:
        work = Path(tmp)
        matrix(exe, work)
        boundaries(exe, work)
        counts(exe, work)
        state_report(exe, work)
        run(exe, work, "--enable-pass=invalid", code=2)
        run(exe, work, "--report-islands=", code=2)


if __name__ == "__main__":
    main()
