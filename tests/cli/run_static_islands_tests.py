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
    assert "reason scc 0\n" in data and "members 3\n" in data, data
    rows = [line for line in data.splitlines() if " internal-predecessors=" in line]
    assert len(rows) == 2, data
    first, second = (row.split() for row in rows)
    assert first[1] == "0" and second[1] == "1", data
    assert first[-2] == second[-2] and first[-2].startswith("scc="), data
    assert first[-1] == "internal-predecessors=1" and second[-1] == "internal-predecessors=0", data


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


def ordinary_native(exe, work):
    source = work / "ordinary-cli.sv"
    source.write_text("""module top;
  logic [3:0] data, first, second;
  always @* first = (data ^ 4'hf);
  always @* second <= first + 1'b1;
  initial begin
    #1; $display("%h %h",first,second);
    data=3; #1; $display("%h %h",first,second);
    data='z; #1; $display("%h %h",first,second);
  end
endmodule
""")
    for state in (2, 4):
        cache = work / f"ordinary-cache-{state}"
        flags = (f"--state={state}", f"--cache-dir={cache}")
        reference = run(exe, work, "--tier=vm", *flags, "--stats=json", source)
        expected, counts = reference.stdout.rstrip().rsplit("\n", 1)
        counts = json.loads(counts)
        for checked in ((), ("--tier-check",)):
            got = run(exe, work, "--tier=native", *checked, *flags, "--stats=json", source)
            output, stats = got.stdout.rstrip().rsplit("\n", 1)
            stats = json.loads(stats)
            assert output == expected
            if checked:
                assert stats == counts
            else:
                assert stats.pop("vm.instrs") < counts["vm.instrs"]
                assert stats == {key: n for key, n in counts.items() if key != "vm.instrs"}
        modules = list(cache.glob("native-v1/*/model.cpp"))
        assert len(modules) == 1, modules
        assert modules[0].read_text().count("{{KIRIMA_MODEL_PROCESS,") == 2


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


def packed_logic(exe, work, width=129):
    source = work / "logic.sv"
    report = work / "logic.report"
    source.write_text("""module top;
  localparam W=WIDTH;
  logic [W-1:0] a,b;
  wire [W-1:0] conj=a&b, disj=a|b, xored=a^b, xn=a~^b, inverted=~a;
  wire [W-1:0] constant_xor=a ^ WIDTH'hz;
  wire [5:0] compared={a==b,a!=b,a===b,a!==b,a==?b,a!=?b};
  bit [W-1:0] clean;
  always_comb clean=b;
  task show;
    $display("%b %h %h %h %h %h %h %h",compared,conj,disj,xored,xn,inverted,constant_xor,clean);
  endtask
  initial begin
    a='0; b='0; #1; show;
    a[W-1]=1; b[63:59]=5'bxz10x; #1; show;
    a='z; b='x; #1; show;
    b=a; #1; show;
    a='1; b='1; #1; show;
  end
endmodule
""".replace("WIDTH", str(width)))
    saved = work / "logic.sir"
    saved.write_text(run(exe, work, "--dump-sir=1", source).stdout)
    for state in ("--state=4", "--state=2"):
        baseline = run(exe, work, state, "--disable-pass=islands", source).stdout
        assert len(baseline.splitlines()) == 5
        for design in ([source], [f"--load-sir={saved}"]):
            checked = run(exe, work, state, "--tier=native", "--tier-check", f"--cache-dir={work}/cache",
                          f"--report-islands={report}", *design)
            assert checked.stdout == baseline
            assert "reason codegen-unsupported 0\n" in report.read_text(), report.read_text()


def aligned_clocked_writes(exe, work):
    source = work / "clocked_parts.sv"
    source.write_text("""module top;
  bit clk;
  bit [2:0] parent;
  int lane;
  logic [2:0][31:0] bank='0;
  logic [7:0] data;
  always_ff @(posedge clk) bank[parent][lane*8 +: 8] <= data;
  initial for (int n=0; n<18; n++) begin
    parent=(n/3)%4; lane=n%6-1; data=8'h51+n;
    #1; clk=1; #1; $display("%024h",bank); clk=0;
  end
endmodule
""")
    # Keep the reference in bytes. The DUT uses three packed 32-bit words.
    bank = bytearray(12)
    expected = []
    for n in range(18):
        parent, lane = n // 3 % 4, n % 6 - 1
        if parent < 3 and 0 <= lane < 4:
            bank[4 * parent + lane] = 0x51 + n
        expected.append(bank[::-1].hex())
    for state in (2, 4):
        result = run(exe, work, f"--state={state}", "--tier=native", "--tier-check",
                     "--max-scc-iter=65536", f"--cache-dir={work}/cache", "--stats=json", source)
        lines = result.stdout.splitlines()
        assert lines[:-1] == expected, result.stdout
        stats = json.loads(lines[-1])
        assert stats["island.members"] == 1 and stats[f"island.evals{state}"] == 18, stats


def native_level_udp(exe, work):
    source = work / "levels.sv"
    source.write_text("""primitive levels(y,a,b,c);
  output y; input a,b,c;
  table
    0 0 ? : 0;
    0 1 b : 1;
    1 ? 1 : 1;
    1 1 0 : 0;
    x ? 0 : 1;
    0 x 1 : x;
  endtable
endprimitive
module top;
  logic [2:0] inputs; wire y;
  levels u(y,inputs[0],inputs[1],inputs[2]);
  function automatic logic drive(int k);
    case(k%4) 0:return 0; 1:return 1; 2:return 1'bx; default:return 1'bz; endcase
  endfunction
  initial for(int n=0;n<64;n++) begin
    inputs={drive(n/16),drive(n/4),drive(n)};
    #1; $display("%b",y);
  end
endmodule
""")
    report = work / "levels.report"
    for state in (2, 4):
        expected = []
        for n in range(64):
            digits = "0100" if state == 2 else "01xx"
            a, b, c = (digits[n // scale % 4] for scale in (1, 4, 16))
            if (a == b == "0") or (a == b == "1" and c == "0"):
                y = "0"
            elif ((a == "0" and b == "1" and c != "x") or (a == "1" and c == "1")
                  or (a == "x" and c == "0")):
                y = "1"
            else:
                y = "0" if state == 2 else "x"
            expected.append(y)
        result = run(exe, work, f"--state={state}", "--tier=native", "--tier-check",
                     f"--cache-dir={work}/cache", f"--report-islands={report}", source)
        assert result.stdout.splitlines() == expected, result.stdout
        assert "members 1\n" in report.read_text() and "reason codegen-unsupported 0\n" in report.read_text()


def indexed_native_storage(exe, work):
    source = work / "index.sv"
    report = work / "index.report"
    source.write_text("""module top;
  bit clk; logic signed [4:0] i; int j; logic signed [5:0] pos;
  logic [7:0] mem[-2:1][4:3]; logic [6:0] x; logic [7:0] q; logic [3:0] d;
  always_ff @(posedge clk) begin
    mem[i][j][pos+:4] <= d;
    q <= mem[i][j];
  end
  always_comb begin x=0; x[pos+:4]=d; end
  initial begin
    i=-2; j=4; pos=-1; d=4'bx101; #1;
    repeat (10) begin clk=1; #1; clk=0; #1;
      $display("%0d,%0d,%0d %b %b %b",i,j,pos,q,x,mem[-2][4]);
      i= i==1 ? -2 : i+1; j=j==3?4:3; pos+=1; end
    i='x; pos='x; clk=1; #1; $display("%b %b",q,x);
  end
endmodule
""")
    saved = work / "index.sir"
    saved.write_text(run(exe, work, "--dump-sir=1", source).stdout)
    for state in (4, 2):
        args = (f"--state={state}", "--max-scc-iter=65536")
        reference = run(exe, work, "--disable-pass=islands", *args, source)
        for design in (source, f"--load-sir={saved}"):
            observed = run(exe, work, "--tier=native", "--tier-check",
                           f"--report-islands={report}", f"--cache-dir={work / 'cache'}",
                           *args, design)
            assert observed.stdout == reference.stdout and len(observed.stdout.splitlines()) == 11
            assert "reason codegen-unsupported 0" in report.read_text(), report.read_text()


def blocking_race_lint(exe, work):
    src = work / "clock-race.sv"
    src.write_text("""module top;
  bit clk, other; logic [3:0] data; int q, f, late, other_read, a[2], separate;
  function automatic logic [3:0] sample(); return data; endfunction
  always @(posedge clk) begin data=1; a[0]=2; end
  always @(posedge clk) q<=sample();
  always @(negedge clk) f<=data;
  always @(posedge other) other_read<=data;
  always @(posedge clk) separate<=a[1];
  initial begin #1; @(posedge clk); q=data; #0; late=data; end
  initial begin #2; clk=1; #1; clk=0; end
endmodule
""")
    flag = "-Wkirima-race-blocking"
    def reports(result):
        return [line for line in result.stderr.splitlines() if line.endswith("[" + flag + "]")]
    warning = reports(run(exe, work, flag, src))
    assert len(warning) == 2 and all("top.data" in line and "posedge of top.clk" in line
                                     for line in warning), warning
    assert any("top.initial0" in line for line in warning), warning
    saved = work / "clock-race.sir"
    saved.write_text(run(exe, work, "--dump-sir=1",src).stdout)
    assert reports(run(exe, work, flag, f"--load-sir={saved}")) == warning
    assert len(reports(run(exe, work, "-Werror=kirima-race-blocking",src,code=2))) == 2
    assert not reports(run(exe, work,flag,"-Wno-kirima-race-blocking",src))


def mixed_wait_order(exe, work):
    source = work / "mixed-wait.sv"
    source.write_text("""module top;
  bit clk, kick; int q;
  always @(posedge clk or posedge kick) q <= 1;
  always @(posedge clk) q <= 3;
  initial begin
    #4;
    repeat (4) begin @(posedge clk); q <= 2; end
  end
  initial begin
    repeat(8) begin
      #1; clk=1; #1; clk=0; $display("%0d",q);
      kick=1; #0; kick=0;
    end
  end
endmodule
""")
    reference = run(exe, work, "--disable-pass=islands", source).stdout
    assert len(reference.splitlines()) == 8 and len(set(reference.splitlines())) > 1
    for tier in ("--tier=vm", "--tier=native", "--tier-check"):
        observed = run(exe, work, "--enable-pass=islands", tier, f"--cache-dir={work / 'cache'}", source)
        assert observed.stdout == reference, (tier, reference, observed)

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
        default_limit = run(exe, work, tier, cache, source, code=5)
        assert "kirima-comb-not-converged" in default_limit.stderr, default_limit
        assert "--max-scc-iter=65536 previous=top.a,top.b last=top.a,top.b" in default_limit.stderr


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


def tier_report(exe, work):
    bench = Path(__file__).resolve().parents[2] / "bench"
    report = work / "tier.jsonl"
    option = f"--report-tier={report}"
    cache = f"--cache-dir={work / 'cache'}"

    def profile(*args):
        plain = run(exe, work, cache, "--stats=json", *args)
        measured = run(exe, work, cache, "--stats=json", option, *args)
        assert measured.stdout == plain.stdout
        stats = json.loads(measured.stdout.splitlines()[-1])
        rows = [json.loads(line) for line in report.read_text().splitlines()]
        assert [r["process_id"] for r in rows] == list(range(stats["sir.processes"]))
        assert sum(r["vm_instructions"] + r["check_vm_instructions"] for r in rows) == stats["vm.instrs"]
        return rows, stats

    comb = bench / "comb_chain.sv"
    for flags, field in [(("--tier=vm",), "vm_entries"),
                         (("--tier=native",), "native_calls"),
                         (("--tier-check",), "check_native_calls")]:
        rows, stats = profile(*flags, comb, "+n=7")
        members = [r for r in rows if r["island"] is not None]
        assert len(members) == 8 and all(r[field] == 8 for r in members)
        assert sum(r[field] for r in members) == stats["island.member_evals"]
        for r in members:
            assert not r["blocking_members"] and r["rejection"] is None
            assert r["generation"] == ("not-attempted" if field == "vm_entries" else "available")
            if field == "check_native_calls":
                assert r["check_vm_entries"] == 8 and r["check_vm_instructions"] > 0
                assert r["native_calls"] == r["vm_entries"] == r["vm_instructions"] == 0

    # Keep a runtime-bound loop to exercise rejection of one member and fallback of its peers.
    source = work / "runtime_bound_shift.sv"
    shift = (bench / "nba_shift.sv").read_text()
    assert "i < 64" in shift
    source.write_text(shift.replace("i < 64", "i < n", 1))
    saved = work / "tier.sir"
    saved.write_text(run(exe, work, "--dump-sir=1", source).stdout)
    args = ("--tier=native", "--state=2", "+n=7")
    rows, stats = profile(*args, source)
    reload_rows, reload_stats = profile(*args, f"--load-sir={saved}")
    assert reload_rows == rows and reload_stats == stats
    members = [r for r in rows if r["island"] is not None]
    blocked = [r for r in members if r["generation"] == "blocked-by-peer"]
    rejected, = (r for r in members if r["generation"] == "rejected")
    assert rejected["rejection"] == "process-frame"
    assert len(blocked) == 16 and all(r["rejection"] is None for r in blocked)
    assert all(r["blocking_members"] == [rejected["process_id"]] for r in members)
    assert stats["island.evals2"] == 7
    assert all(r["vm_entries"] == 7 and r["native_calls"] == 0 for r in members)
    static, = (r for r in rows if r["name"].endswith(".$static"))
    assert static["vm_entries"] == 1 and static["vm_instructions"] > 0

    # SIR format 1 admits byte escapes in process names. The report must
    # remain one parseable JSON line per row, including control bytes.
    escaped = work / "tier-names.sir"
    old_name = rejected["name"]
    new_name = r'quoted\"path\\tab\tnext\n\xe8\xa8\x88\xff'
    renamed = saved.read_text().replace(f'name="{old_name}"', f'name="{new_name}"')
    assert renamed != saved.read_text()
    escaped.write_text(renamed)
    escaped_rows, _ = profile("--tier=vm", "+n=1", f"--load-sir={escaped}")
    assert escaped_rows[rejected["process_id"]]["name"] == 'quoted"path\\tab\tnext\n計ÿ'

    ordinary = work / "tier-ordinary.sv"
    ordinary.write_text("""module top;
  logic a, q;
  always @* q = ~a;
  initial begin a=0; #1; a=1; #1; $display("%b",q); end
endmodule
""")
    rows, _ = profile("--tier=native", ordinary)
    generated, = (r for r in rows if r["native_calls"])
    assert generated["island"] is None and "kind" in generated["exclusions"]
    assert generated["generation"] == "available" and generated["vm_entries"] == 0
    rows, _ = profile("--tier=auto", f"--cxx={work/'missing-compiler'}", ordinary)
    fallback = rows[generated["process_id"]]
    assert fallback["generation"] == "compiler-unavailable" and fallback["vm_entries"] > 0
    rows, _ = profile("--disable-pass=islands", "--tier=native", comb, "+n=7")
    assert all(r["generation"] == "not-attempted" and r["native_calls"] == 0 for r in rows)

    # A generated body can be available without ever receiving its edge.
    idle = work / "tier-idle.sv"
    idle.write_text("""module top;
  bit clk, q;
  always_ff @(posedge clk) q <= ~q;
  initial #1 $finish;
endmodule
""")
    idle_rows, _ = profile("--tier=native", idle)
    unused, = (r for r in idle_rows if r["generation"] == "available")
    assert unused["native_calls"] == unused["vm_entries"] == 0

    for flags in [("parse",), ("--dump-sir=1",), ("--dump-sir",), ("--race-check=1",)]:
        assert "single simulation" in run(exe, work, *flags, option, comb, code=2).stderr
    assert "needs a file" in run(exe, work, "--report-tier=", comb, code=2).stderr
    assert "cannot write tier report" in run(exe, work, f"--report-tier={work}", comb, "+n=1", code=2).stderr


def bounded_slot_loops(exe, work):
    # An ordinary @* process exposes every counter write to the kernel. Its
    # four-state counter also controls indexing and a call inside the loop.
    source = work / "slot-loop.sv"
    text = """module top;
  logic [31:0] i;
  logic [3:0] a, q;
  function automatic logic echo(input logic value);
    return value;
  endfunction
  always @* begin
    for (i = 0; i < 4; i = i + 1)
      q[i] = echo(a[i]);
  end
  initial begin
    #1 a = 4'bx1z0;
    #1 $display("%b", q);
    a = 4'b1010;
    #1 $display("%b", q);
  end
endmodule
"""
    source.write_text(text)
    report = work / "slot-loop.jsonl"
    common = (f"--cache-dir={work / 'slot-loop-cache'}", f"--report-tier={report}")
    def execute(source_arg, tier, *extra, code=0):
        p = run(exe, work, *common, *extra, tier, source_arg, code=code)
        row = next(r for line in report.read_text().splitlines()
                   if (r := json.loads(line))["name"] == "top.always0")
        return p.stdout, row

    saved = work / "slot-loop.sir"
    saved.write_text(run(exe, work, "--dump-sir=1", source).stdout)
    for state in (2, 4):
        flags = (f"--state={state}",)
        ref, row = execute(source, "--tier=vm", *flags)
        assert row["vm_entries"] == 2
        for source_arg, tier, field in ((source, "--tier=native", "native_calls"),
                                        (f"--load-sir={saved}", "--tier-check", "check_native_calls")):
            output, row = execute(source_arg, tier, *flags)
            assert output == ref and row[field] == 2, row

    changes = []
    for tier in ("--tier=vm", "--tier=native"):
        wave = work / f"slot-loop-{tier[7:]}.vcd"
        _, row = execute(source, tier, "--wave-glitches", f"--wave={wave}")
        assert tier == "--tier=vm" or row["native_calls"] == 2
        header, events = wave.read_bytes().split(b"$enddefinitions $end", 1)
        counter = next(v.split()[3] for v in header.splitlines()
                       if v.startswith(b"$var ") and v.split()[4] == b"i")
        assert sum(line.endswith(b" " + counter) for line in events.splitlines()) >= 10
        changes.append(events)
    assert changes[0] == changes[1]

    # With 32 iterations the callee alone contributes more steps than one
    # extra pass over the loop. Stop one instruction before the VM can finish.
    source.write_text(text.replace("i < 4", "i < 32"))
    _, row = execute(source, "--tier=vm")
    assert row["vm_entries"] == 2 and row["vm_instructions"] % 2 == 0
    steps = row["vm_instructions"] // 2
    low = (f"--max-steps={steps - 1}", "--stats=json")
    ref, _ = execute(source, "--tier=vm", *low, code=5)
    output, row = execute(source, "--tier=native", *low, code=5)
    assert output == ref and row["rejection"] == "step-budget", row

    unsafe = (
        text.replace("logic [31:0] i;", "logic [31:0] i, limit = 4;").replace("i < 4", "i < limit"),
        text.replace("i < 4", "i[1:0] < 3"),
        text.replace("q[i] = echo(a[i]);", "begin q[i] = echo(a[i]); i[0] = i[0]; end"),
        text.replace("endmodule", "initial #5 i = 4; endmodule"),
        text.replace("endmodule", "initial begin #4; force i = 4; #1; release i; end endmodule"),
    )
    for variant in unsafe:
        source.write_text(variant)
        ref, _ = execute(source, "--tier=vm")
        output, row = execute(source, "--tier=native")
        assert output == ref and row["vm_entries"] and row["native_calls"] == 0, row
    source.write_text(text)
    ref, _ = execute(source, "--tier=vm", "--bounds-check=warn")
    output, row = execute(source, "--tier=native", "--bounds-check=warn")
    assert output == ref and row["vm_entries"] == 2 and row["native_calls"] == 0


def branching_comb_frame_loop(exe, work):
    # A four-state conditional produces joins with incoming values in the
    # loop body. The induction variable itself lives in the process frame.
    source = work / "comb-frame-loop.sv"
    text = """module top;
  logic [7:0] a, q;
  logic reverse;
  always_comb begin
    for (int unsigned i = 0; i < 8; i = i + 1)
      q[i] = reverse ? a[7-i] : a[i];
  end
  initial begin
    #1 a = 8'b10xz1101; reverse = 1;
    #1 $display("%b", q);
    reverse = 0; #1 $display("%b", q);
    reverse = 1'bx; #1 $display("%b", q);
  end
endmodule
"""
    report = work / "comb-frame-loop.jsonl"
    cache = f"--cache-dir={work / 'comb-frame-cache'}"
    def execute(arg, tier, *flags, code=0):
        result = run(exe, work, cache, f"--report-tier={report}", "--stats=json",
                     tier, *flags, arg, code=code)
        output = result.stdout.splitlines()
        row = next(r for line in report.read_text().splitlines()
                   if (r := json.loads(line))["name"] == "top.always_comb0")
        return output[:-1], json.loads(output[-1]), row

    source.write_text(text)
    saved = work / "comb-frame-loop.sir"
    saved.write_text(run(exe, work, "--dump-sir=1", source).stdout)
    for state in (2, 4):
        flags = (f"--state={state}",)
        ref, stats, vm = execute(source, "--tier=vm", *flags)
        assert len(ref) == 3 and vm["vm_entries"] == (3 if state == 2 else 4)
        for arg, tier, field in ((source, "--tier=native", "native_calls"),
                                 (f"--load-sir={saved}", "--tier-check", "check_native_calls")):
            output, got, row = execute(arg, tier, *flags)
            assert output == ref and row[field] == vm["vm_entries"], row
            assert all(got[k] == stats[k] for k in ("slot.writes", "slot.changes", "sim.end_time"))
    ref, stats, _ = execute(source, "--tier=vm", "--max-steps=25", code=5)
    result, got, row = execute(source, "--tier=native", "--max-steps=25", code=5)
    assert (result, got) == (ref, stats) and row["rejection"] == "step-budget", row

    # An exit from inside the body, a second counter write and a nested loop
    # each invalidate the single-entry region or the counted induction.
    statement = "q[i] = reverse ? a[7-i] : a[i];"
    for body in ("begin if (i == 5) break; " + statement + " end",
                 "begin " + statement + " i = i + 1; end",
                 "for (int unsigned j = 0; j < 2; j++) " + statement):
        source.write_text(text.replace(statement, body))
        ref, stats, _ = execute(source, "--tier=vm")
        output, got, row = execute(source, "--tier=native")
        assert output == ref and stats == got and row["vm_entries"] and not row["native_calls"], row


def bounded_frame_loops(exe, work, checkpoint_host):
    original = Path(__file__).resolve().parents[2] / "bench" / "nba_shift.sv"
    cache = f"--cache-dir={work / 'cache'}"
    report = work / "loops.jsonl"

    def profile(source, state, tier, *extra, code=0):
        result = run(exe, work, cache, f"--state={state}", tier, "--stats=json",
                     f"--report-tier={report}", *extra, source, "+n=16", code=code)
        output, stats = result.stdout.splitlines()[:-1], json.loads(result.stdout.splitlines()[-1])
        members = [r for line in report.read_text().splitlines()
                   if (r := json.loads(line))["island"] is not None]
        return output, stats, members

    # Run the exact generated module with a host that requests a stop at
    # every branch of each reset path and inspect the host frame and NBA prefix.
    probe_cache = work / "checkpoint-cache"
    run(exe, work, "--tier=native", "--state=2", f"--cache-dir={probe_cache}", original, "+n=1")
    model, = probe_cache.glob("native-v1/*/model.so")
    run(checkpoint_host, work, model)

    text = original.read_text()
    saved = work / "loops.sir"
    saved.write_text(run(exe, work, "--dump-sir=1", original).stdout)
    for state in (2, 4):
        expected, ref, _ = profile(original, state, "--tier=vm")
        for source, tier, field in ((original, "--tier=native", "native_calls"),
                                    (f"--load-sir={saved}", "--tier-check", "check_native_calls")):
            output, stats, rows = profile(source, state, tier)
            assert len(rows) == 17 and all(r[field] == 16 for r in rows), rows
            assert (output, stats["sim.end_time"], stats["nba.scheduled"]) == (
                expected, ref["sim.end_time"], 16 * 81)
        variants = (
            ("zero", "i < 64", "i < 0", True),
            ("one", "i < 64", "i < 1", True),
            ("signed-limit", "int i = 0; i < 64", "int i = 2147483646; i < 2147483647", True),
            ("unknown", "16'hace1", "16'haxz1", True),
            ("dynamic", "i < 64", "i < n", False),
            ("counter-write", "sr[i] <= '0;", "begin sr[i] <= '0; i++; end", False),
            ("step-two", "i++) sr", "i+=2) sr", False),
            ("too-many", "i < 64", "i < 257", False),
        )
        for name, before, after, eligible in variants:
            source = work / f"loop-{name}.sv"
            source.write_text(text.replace(before, after))
            expected, ref, _ = profile(source, state, "--tier=vm")
            output, stats, rows = profile(source, state, "--tier-check")
            assert len(rows) == 17 and all(r["check_native_calls"] == (16 if eligible else 0)
                                           for r in rows), (name, rows)
            assert output == expected and stats["nba.scheduled"] == ref["nba.scheduled"], name
            assert stats["vm.instrs"] == ref["vm.instrs"], name

    # Preserve the exact partial evaluation when the VM stops mid-loop.
    _, ref, _ = profile(original, 4, "--tier=vm", "--max-steps=407", code=5)
    _, stats, rows = profile(original, 4, "--tier=native", "--max-steps=407", code=5)
    assert ref == stats and any(r["rejection"] == "step-budget" for r in rows)

    deadline = work / "loop-deadline.sv"
    deadline.write_text(text.replace("endmodule", 'final $display("final-ran");\nendmodule'))
    for tier, field in (("--tier=native", "native_calls"), ("--tier-check", "check_native_calls")):
        result = run(exe, work, tier, cache, "--stats=json", "--max-wall-time=1",
                     f"--report-tier={report}", deadline, "+n=100000000", code=4)
        final, stats = result.stdout.splitlines()
        assert final == "final-ran" and json.loads(stats)["sim.end_reason"] == "wall_time_limit"
        rows = [json.loads(line) for line in report.read_text().splitlines()]
        assert sum(r[field] > 0 for r in rows) == 17


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
    parser.add_argument("--checkpoint-host", type=Path)
    args = parser.parse_args()
    exe = args.kirimasim.resolve()
    checkpoint_host = args.checkpoint_host or exe.parents[2] / "tests/cli/kirimasim_native_loop_probe"
    with tempfile.TemporaryDirectory() as tmp:
        work = Path(tmp)
        matrix(exe, work)
        boundaries(exe, work)
        counts(exe, work)
        scc_iterations(exe, work)
        mixed_wait_order(exe, work)
        blocking_race_lint(exe, work)
        indexed_native_storage(exe, work)
        aligned_clocked_writes(exe, work)
        native_level_udp(exe, work)
        packed_logic(exe, work)
        state_report(exe, work)
        tier_report(exe, work)
        bounded_frame_loops(exe, work, checkpoint_host.resolve())
        bounded_slot_loops(exe, work)
        branching_comb_frame_loop(exe, work)
        preserve(exe, work)
        native_cache(exe, work)
        ordinary_native(exe, work)
        approximate_state(exe, work)
        race_replays(exe, work)
        run(exe, work, "--enable-pass=invalid", code=2)
        run(exe, work, "--report-islands=", code=2)


if __name__ == "__main__":
    main()
