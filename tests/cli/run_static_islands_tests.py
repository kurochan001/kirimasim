#!/usr/bin/env python3
# Copyright (c) 2026 The kirimasim Authors
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

"""Process sensitivity, bit ranges, time-zero and NBA ordering (4.6, 9.2.2, 10.4.2)."""
import argparse
from concurrent.futures import ThreadPoolExecutor
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


def approximate_state(exe, work):
    source = work / "two.sv"
    source.write_text("""module top;
  logic [3:0] a, q, left, divided;
  logic [139:0] wide, copied;
  logic [4:0] index;
  logic [7:0] mem[2];
  int edges=0;
  wire undriven;
  always @(posedge a[0]) edges++;
  always_comb begin
    q=a+4'b01xz;
    divided=a/4'd0;
    left=wide[index +: 4];
  end
  always_comb copied=wide;
  function automatic logic [3:0] read_local();
    logic [3:0] tmp; return tmp;
  endfunction
  initial begin
    #1; $display("initial %b %b %b %b %b",a,undriven,mem[0],read_local(),1'bx);
    a=4'bx1z0; wide={35{4'b1xz1}}; index=0; #1;
    $display("arith %b %b %b %b %b",a,q,divided,left,copied[69:64]);
    a[0]=1'bx; a[0]=1; #1; a[0]=1'bz; #1;
    $display("edges %0d oob %b",edges,mem[5]);
    force a=4'b1xxz; #1; $display("force %b",a); release a;
  end
endmodule
""")
    expected = {
        4: ["initial xxxx z xxxxxxxx xxxx x", "arith x1z0 xxxx xxxx 1xz1 z11xz1",
            "edges 1 oob xxxxxxxx", "force 1xxz"],
        2: ["initial 0000 0 00000000 0000 0", "arith 0100 1000 0000 1001 011001",
            "edges 1 oob 00000000", "force 1000"],
    }
    saved = work / "two.sir"
    saved.write_text(run(exe, work, "--dump-sir=1",source).stdout)
    for state, lines in expected.items():
        for tier in ("--tier=vm", "--tier=native", "--tier-check"):
            result = run(exe, work, tier, f"--state={state}",f"--cache-dir={work / 'cache'}",
                         f"--load-sir={saved}")
            assert result.stdout.splitlines() == lines, result
    run(exe, work, "--state=3",source,code=2)


def native_cache(exe, work):
    source = work / "native.sv"
    source.write_text("""module top;
  logic a, q;
  always_comb q = ~a;
  initial begin a=0; #1; $display("%b",q); a='x; #1; $display("%b",q); end
endmodule
""")
    cache = work / "cache with spaces"
    args = ["--tier=native","--tier-check",f"--cache-dir={cache}",source]
    with ThreadPoolExecutor(3) as workers:
        results = list(workers.map(lambda _: run(exe, work, *args), range(3)))
    assert all(r.stdout == "1\nx\n" for r in results)
    modules = {p:p.stat().st_mtime_ns for p in cache.glob("native-v1/*/model.so")}
    assert modules
    run(exe, work, *args)
    assert modules == {p:p.stat().st_mtime_ns for p in cache.glob("native-v1/*/model.so")}
    missing = f"--cxx={work / 'missing compiler'}"
    assert run(exe, work, "--tier=auto",missing,source).stdout == results[0].stdout
    run(exe, work, "--tier=native",missing,source,code=2)
    compiler = work / "broken compiler"
    compiler.write_text('#!/bin/sh\nif [ "$1" = --version ]; then echo test; exit 0; fi\nexit 7\n')
    compiler.chmod(0o700)
    run(exe, work, "--tier=auto",f"--cxx={compiler}",f"--cache-dir={cache}",source,code=2)
    assert set(cache.glob("native-v1/*/model.so")) == set(modules)
    assert not list(cache.glob("native-v1/.build-*"))
    run(exe, work, "--tier=invalid",source,code=2)


def preserve(exe, work):
    source = work / "ports.sv"
    source.write_text("""module dut(input wire d, output wire q);
  wire inner; assign inner=d; assign q=inner;
endmodule
module top(input wire x, output wire y);
  logic hidden; dut u(x,y);
endmodule
""")
    def marked(*args):
        dump = run(exe, work, *args, "--dump-sir").stdout
        return {line.split('"')[1] for line in dump.splitlines()
                if line.startswith("slot ") and "observable" in line}

    assert marked(source, "--preserve=none") == set()
    assert marked(source, "--preserve=ports") == {"top.x", "top.y"}
    assert marked(source, "--preserve=all") == {"top.x", "top.y", "top.hidden", "top.u.inner"}
    saved = work / "ports.sir"
    saved.write_text(run(exe, work, source, "--preserve=ports", "--dump-sir=1").stdout)
    assert marked(f"--load-sir={saved}", "--public=top.hidden") == {"top.x","top.y","top.hidden"}
    run(exe, work, "--preserve=bad", source, code=2)


def race_replays(exe, work):
    racing = work / "racing.sv"
    racing.write_text("""module top;
  bit q;
  initial
    #1 q = 1;
  initial #1 q = 0;
  initial #2 $display("q=%b", q);
endmodule
""")
    options = ["--race-check=7", "--race-seed=4", "--public=top.q"]
    got = run(exe, work, *options, racing)
    assert got.stdout == run(exe, work, racing).stdout
    assert "time=1 ticks" in got.stderr and "top.initial0" in got.stderr and "top.initial1" in got.stderr
    assert "racing.sv:4:" in got.stderr and "racing.sv:5:" in got.stderr, got.stderr
    assert run(exe, work, *options, racing).stderr == got.stderr
    clean = work / "clean.sv"
    clean.write_text("""module top;
  integer fd, ch;
  logic [7:0] ram[0:0];
  initial begin
    ch=$fgetc(32'h80000000);
    fd=$fopen("append.txt","a"); $fwrite(fd,"%c",ch); $fclose(fd);
    ram[0]=8'hac; $writememh("saved.hex",ram);
    ram[0]=0; $readmemh("saved.hex",ram);
    $display("seen %c %h",ch,ram[0]);
  end
endmodule
""")
    (work / "append.txt").write_text("base")
    replay = subprocess.run([exe, *options, clean], cwd=work, input="Q", text=True,
                            capture_output=True, timeout=30)
    assert replay.returncode == 0 and replay.stdout == "seen Q ac\n", replay
    assert "no difference in 7 race replay(s)" in replay.stderr
    assert (work / "append.txt").read_text() == "baseQ"
    seek = work / "seek.sv"
    seek.write_text("""module top;
  integer pos, status, ch;
  initial begin
    pos=$ftell(32'h80000000); ch=$fgetc(32'h80000000);
    status=$fseek(32'h80000000,0,0);
    $display("%0d %c %0d %c",pos,ch,status,$fgetc(32'h80000000));
  end
endmodule
""")
    inp = work / "input.bin"
    inp.write_bytes(b"abc")
    with inp.open("rb") as stream:
        stream.seek(1)
        redirect = subprocess.run([exe, *options, seek], cwd=work, stdin=stream,
                                  text=True, capture_output=True, timeout=30)
    assert redirect.stdout == "1 b 0 a\n" and "no difference in 7" in redirect.stderr, redirect
    pipe = subprocess.run([exe, *options, seek], cwd=work, input="ab",
                          text=True, capture_output=True, timeout=30)
    assert pipe.stdout == "-1 a -1 b\n" and "no difference in 7" in pipe.stderr, pipe
    for flag in ("--race-check=0","--race-seed=-1","--public="):
        run(exe, work, flag, racing, code=2)


def scc_iterations(exe, work):
    source = work / "ring.sv"
    report = work / "ring.report"
    cache = f"--cache-dir={work / 'ring-cache'}"
    source.write_text("""module top;
  bit [31:0] a,b; bit enable; wire [31:0] out = b;
  always_comb a = enable ? (b << 1) | 1 : 0;
  always_comb b = a;
  initial begin #1; enable=1; #1; $display("%h",out);
    enable=0; #1; enable=1; #1; $display("%h",out); end
endmodule
""")
    for tier in ("--tier=vm", "--tier=native", "--tier-check"):
        args = ("--enable-pass=islands", tier, cache, f"--report-islands={report}")
        run(exe, work, *args, "--max-scc-iter=32", source, code=5)
        for limit in (33, 0):
            result = run(exe, work, *args, f"--max-scc-iter={limit}", source)
            assert result.stdout == "ffffffff\nffffffff\n", result
        graph = report.read_text().splitlines()
        members = [line for line in graph if "internal-predecessors=" in line]
        assert len(members) == 2 and all("level=0" in line for line in members), graph
        assert any("level=1 predecessors=" in line for line in graph), graph

    source.write_text("""module top;
  bit a,b;
  always_comb a = ~b;
  always_comb b = a;
endmodule
""")
    for tier in ("--tier=vm", "--tier=native", "--tier-check"):
        result = run(exe, work, "--enable-pass=islands", tier, cache,
                     "--max-scc-iter=4", source, code=5)
        assert "kirima-comb-not-converged" in result.stderr, result
        assert "previous=top.a,top.b last=top.a,top.b" in result.stderr, result


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
        scc_iterations(exe, work)
        state_report(exe, work)
        preserve(exe, work)
        native_cache(exe, work)
        approximate_state(exe, work)
        race_replays(exe, work)
        run(exe, work, "--enable-pass=invalid", code=2)
        run(exe, work, "--report-islands=", code=2)


if __name__ == "__main__":
    main()
