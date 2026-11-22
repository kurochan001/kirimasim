#!/usr/bin/env python3
# Copyright (c) 2026 The kirimasim Authors
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

"""Compile an emitted C++20 model independently and exercise both execution paths."""
import json
from pathlib import Path
import subprocess
import sys
import tempfile


def main():
    exe, compiler, project = sys.argv[1:]
    root = Path(project)
    with tempfile.TemporaryDirectory() as tmp:
        work = Path(tmp)
        source = work / "model.cpp"
        module = work / "model.so"

        known_module = work / "known.so"

        def build(output=module):
            subprocess.run([compiler, "-std=c++20", "-fno-exceptions", "-fno-rtti",
                            "-ffp-contract=off", "-O2", "-shared", "-fPIC",
                            "-I", root / "src/kernel/include", source, "-o", output], check=True)

        def evaluate(design, *args):
            p = subprocess.run([exe, design, *args], capture_output=True, text=True, timeout=30)
            if p.returncode:
                raise AssertionError((p.returncode, p.stdout, p.stderr))
            lines = p.stdout.rstrip().splitlines()
            counts = {k: v for k, v in json.loads(lines[-1]).items()
                      if k.startswith(("sched.", "proc.", "slot.", "nba.", "wait.", "sim."))}
            return lines[:-1], counts, p.stderr

        # The island starts in X, sees known inputs, returns to X and Z,
        # then sees known inputs again. The two-state path must be reversible.
        toggle = work / "switch.sv"
        toggle.write_text("""module top;
  logic [7:0] a, b;
  always_comb b = a + 8'd3;
  initial begin
    #1; $display("%h", b);
    a = 4; #1; $display("%h", b);
    a = 'x; #1; $display("%h", b);
    a = 'z; #1; $display("%h", b);
    a = 9; #1; $display("%h", b);
  end
endmodule
""")
        for design in (root / "bench/comb_chain.sv", root / "bench/nba_shift.sv", toggle):
            subprocess.run([exe, design, "stale"], check=True)
            generated = subprocess.check_output([exe, design, "emit"])
            assert generated == subprocess.check_output([exe, design, "emit"])
            source.write_bytes(generated)
            build()
            baseline = evaluate(design, "vm")
            assert evaluate(design, module) == baseline
            assert evaluate(design, module, "check") == baseline
            known = subprocess.check_output([exe, design, "emit-known"])
            assert known and known == subprocess.check_output([exe, design, "emit-known"])
            source.write_bytes(known)
            build(known_module)
            for mode in ("run", "check"):
                assert evaluate(design, module, mode, known_module) == baseline
            if design == toggle:
                assert baseline[0] == ["xx", "07", "xx", "xx", "0c"], baseline
                out = subprocess.check_output([exe, design, module, "run", known_module], text=True)
                counts = json.loads(out.rstrip().splitlines()[-1])
                assert counts["island.evals2"] == 2 and counts["island.evals4"] == 3, counts
                changed = known.replace(b".a + ", b".a - ", 1)
                assert changed != known
                source.write_bytes(changed)
                build(known_module)
                bad = subprocess.run([exe, design, module, "check", known_module],
                                     capture_output=True, text=True)
                assert bad.returncode == 3 and "tier-check island=" in bad.stderr, bad
            if design.stem == "comb_chain":
                changed = generated.replace(b"add4(", b"sub4(", 1)
                assert changed != generated
                source.write_bytes(changed)
                build()
                bad = subprocess.run([exe, design, module, "check"], capture_output=True, text=True)
                assert bad.returncode == 3 and "tier-check island=" in bad.stderr and "slot=" in bad.stderr, bad

        whole = work / "whole.sv"
        whole.write_text("""module top;
  logic [135:0] a, copy, q;
  logic [15:0] field; logic signed [8:0] index; bit clk;
  function automatic logic [15:0] pass(input logic [15:0] x);
    logic [15:0] temp; temp=x; pass=temp;
  endfunction
  always_comb copy=a;
  always @(posedge clk) q <= copy;
  always_comb field=pass(q[index+:16]);
  initial begin
    a={17{8'h9a}}; index=63; #1; clk=1; #1; $display("%h %h",q,field);
    index=-3; #1; $display("%h",field);
    index=132; #1; $display("%h",field);
    index='x; #1; $display("%h",field);
    a={17{8'b10xz0011}}; clk=0; #1; clk=1; index=61; #1; $display("%h %h",q,field);
  end
endmodule
""")
        generated = subprocess.check_output([exe, whole, "emit"])
        assert b"packed_select((w+" in generated and b"schedule_nba(ctx,f.process," in generated
        source.write_bytes(generated)
        build()
        baseline = evaluate(whole, "vm")
        assert evaluate(whole, module, "check") == baseline
        assert evaluate(whole, module, "run") == baseline

        # Synchronous event guards can update an input between two stores
        # of one generated process. Check both ownership and read timing.
        guards = work / "guards.sv"
        guards.write_text("""module top;
  bit clk, first, sampled;
  function static bit mark(input bit x); return x; endfunction
  always @(posedge first iff mark(first)) $display("woke");
  always @(posedge clk) begin first=1; sampled=mark.x; end
  initial begin #1; clk=1; #1; $display("%b %b",sampled,mark.x); end
endmodule
""")
        for guarded in (guards, root / "tests/suite/sched/iff_matrix.sv"):
            source.write_bytes(subprocess.check_output([exe, guarded, "emit"]))
            build()
            reference = evaluate(guarded, "vm")
            assert evaluate(guarded, module, "run") == reference
            assert evaluate(guarded, module, "check") == reference

        ordinary = work / "ordinary.sv"
        ordinary.write_text("""module top;
  logic [3:0] x, y, z;
  always @* begin
    if (x[0]) y = x - 1;
    else if (x[1]) y = 7;
    else y = 3;
  end
  always @* z = y + 2;
  initial begin
    #1; $display("%h %h",y,z);
    x=5; #1; $display("%h %h",y,z);
    x=2; #1; $display("%h %h",y,z);
    x=4; #1; $display("%h %h",y,z);
  end
endmodule
""")
        emitted = subprocess.check_output([exe, ordinary, "emit"])
        assert emitted.count(b"{{KIRIMA_MODEL_PROCESS,") == 2
        source.write_bytes(emitted)
        build()
        reference = evaluate(ordinary, "vm")
        assert reference[0] == ["x x", "4 6", "7 9", "3 5"], reference
        assert evaluate(ordinary, module, "run") == reference
        assert evaluate(ordinary, module, "check") == reference

        host = work / "host.sv"
        host.write_text("""module top(input bit clk, input logic [79:0] data, output logic [79:0] q);
  always_ff @(posedge clk) q <= data;
  initial #17 $finish;
  final $display("host final");
endmodule
""")
        source.write_bytes(subprocess.check_output([exe, host, "emit"]))
        build()
        for backend in ("vm", module):
            p = subprocess.run([exe, host, backend, "host"], capture_output=True, text=True)
            assert p.returncode == 0 and p.stdout.splitlines() == ["host final"] * 2, p

        reductions = work / "reductions.sv"
        reductions.write_text("""module top;
  logic [7:0] a; logic [5:0] b;
  always_comb b = {&a, ~&a, |a, ~|a, ^a, ~^a};
  initial begin
    a=0; #1; $display("%b",b); a='1; #1; $display("%b",b);
    a=8'b0xz10100; #1; $display("%b",b);
  end
endmodule
""")
        source.write_bytes(subprocess.check_output([exe, reductions, "emit"]))
        build()
        assert evaluate(reductions, module, "check") == evaluate(reductions, "vm")

        # Both fields straddle the 64-bit boundary. The overlapping blocking
        # writes must use an overlay for the whole physical element.
        fields = work / "fields.sv"
        fields.write_text("""module top;
  bit clk;
  logic [133:0] bank, next_bank;
  logic [33:0] part;
  always @(posedge clk) begin
    bank[79:46] = part;
    bank[90:57] = ~part;
    next_bank[78:45] <= part;
  end
  initial begin
    part = 34'h2abcdef01; #1; clk=1; #1;
    $display("%h %h", bank[90:46], next_bank[78:45]); clk=0;
    part = 34'hxz0000001; #1; clk=1; #1;
    $display("%h %h", bank[90:46], next_bank[78:45]);
  end
endmodule
""")
        for name, output in (("emit", module), ("emit-known", known_module)):
            generated = subprocess.check_output([exe, fields, name])
            assert generated
            source.write_bytes(generated)
            build(output)
        baseline = evaluate(fields, "vm")
        assert evaluate(fields, module, "check", known_module) == baseline
        assert evaluate(fields, module, "run", known_module) == baseline

        deep = work / "deep.sv"
        deep.write_text("module top; int i, v; always_comb begin v = i;\n"
                        + "v = v + 1;\n" * 250
                        + 'end initial begin i=3; #1; $display("%0d", v); end endmodule\n')
        generated = subprocess.check_output([exe, deep, "emit"])
        assert b"static void part" in generated
        source.write_bytes(generated)
        build()
        assert evaluate(deep, module, "check") == evaluate(deep, "vm")

        # Known inputs can still produce X from division by zero or an
        # out-of-range selector. Such islands keep normal four-state code.
        for expression in ("a / k", "a[k]", "a / 8'd0", "a + 8'hxz"):
            toggle.write_text(f"""module top;
  logic [7:0] a, k, b;
  always_comb b = {expression};
  initial begin a = 1; k = 2; #1; $display("%h", b); end
endmodule
""")
            assert subprocess.check_output([exe, toggle, "emit-known"]) == b"", expression


if __name__ == "__main__":
    main()
