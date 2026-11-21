#!/usr/bin/env python3
# Copyright (c) 2026 The kirimasim Authors
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

"""Test sampled operands, clock history and the serialized bindings (16.5.1, 16.9.3)."""
import argparse
from pathlib import Path
import sys
import tempfile

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / "tools"))
import bounded_run  # noqa: E402


def cases():
    for width in [4, 65, 129]:
        for kind in ["logic", "bit"]:
            yield f"defaults_{kind}_{width}", f"""
module top;
  {kind} [{width-1}:0] d='h9;
  wire [{width-1}:0] net_value=d;
  initial begin
    d='1;
    if ($sampled(d)!==9 || $sampled(net_value)!=='x) $fatal(1,"time zero");
    #1; d='0; #0;
    if ($sampled(d)!=='1 || $sampled(net_value)!=='1 ||
        $sampled(d[{width-1} -: 2] + 1)!==4) $fatal(1,"delta");
    d<=3; @(d);
    if ($sampled(d)!=='1) $fatal(1,"NBA snapshot");
    #1; if ($sampled(d)!==3) $fatal(1,"next snapshot");
    $display("ok");
  end
endmodule""", None
    for edge, idle, active in [("posedge", 0, 1), ("negedge", 1, 0)]:
        yield f"history_{edge}", f"""
module top;
  bit clk={idle}, observe;
  logic [128:0] data=129'h1;
  int calls;
  always @({edge} clk) if (observe) begin
    calls++;
    if (!$stable(data) || $changed(data)) $fatal(1,"history despite skipped calls");
  end
  initial begin
    #1; observe=1; clk={active};
    #1; clk={idle}; data[128]=1'bx; observe=0;
    #1; clk={active};
    #1; clk={idle}; observe=1;
    #1; clk={active};
    #1; clk={idle}; data[128]=1'bz; observe=0;
    #1; clk={active};
    #0; clk={idle}; #0; clk={active}; #0;
    if (!$changed(data,@({edge} clk)) || $stable(data,@({edge} clk)))
      $fatal(1,"strictly prior tick and X/Z");
    #1;
    if (!$stable(data,@({edge} clk)) || calls!=2) $fatal(1,"between edges");
    $display("ok");
  end
endmodule""", None
    yield "two_clocks", """
module top;
  bit a,b;
  int d=6;
  initial begin
    #1; a=1; b=1;
    #1; a=0; b=0; d=7;
    #1; a=1;
    #1; if (!$stable(d,@(posedge a)) || !$changed(d,@(posedge b))) $fatal;
    $display("ok");
  end
endmodule""", None
    yield "clock_bit_select_and_resolved_net", """
module top;
  bit [1:0] drivers;
  wire [1:0] clocks = drivers;
  logic [7:0] payload = 'h1;
  initial begin
    #1; drivers[0] <= 1; @(posedge clocks[0]);
    #1; drivers = 0; payload = 'h3;
    #1; drivers[1] <= 1; @(posedge clocks[1]);
    #1; if (!$changed(payload & 3, @(posedge clocks[0])) ||
            !$stable(payload & 3, @(posedge clocks[1]))) $fatal(1, "distinct clock bits");
    $display("ok");
  end
endmodule""", None
    yield "procedural_containers", """
package p; int a=23; function automatic int read_sample(); return $sampled(a); endfunction endpackage
interface iface; int d=11; task automatic check_old(); if ($sampled(d)!=11) $fatal; endtask endinterface
module top; iface link(); initial begin #1; p::a=32; link.d=13; fork link.check_old(); join
  if (p::read_sample()!=23) $fatal; $display("ok"); end endmodule""", None
    for body, reason in [
        ("automatic int a=5; $display($sampled(a));", "without static packed storage"),
        ("$display($sampled(d[idx]));", "runtime selection"),
        ("$display($sampled(d++));", "effectful sampled operand"),
        ("$display($sampled(next()));", "sampled operand of this form"),
        ("$display($stable(d,@(posedge clk iff gate)));", "unguarded posedge or negedge"),
    ]:
        yield "unsupported", ("module top; int d,idx; bit clk,gate; "
                              "function int next(); return d; endfunction initial begin "
                              + body + " end endmodule"), reason


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--kirimasim", required=True)
    ap.add_argument("--roundtrip", required=True)
    args = ap.parse_args()
    with tempfile.TemporaryDirectory() as tmp:
        for i, (name, source, reason) in enumerate(cases()):
            path = Path(tmp) / f"{name}_{i}.sv"
            path.write_text(source, encoding="utf-8")
            result = bounded_run.run([args.kirimasim, str(path)], timeout=15)
            if reason:
                ok = (result.returncode == 2 and "error: unsupported:" in result.stderr
                      and reason in result.stderr and not result.stdout)
            else:
                ok = result.returncode == 0 and result.stdout == "ok\n"
                if ok:
                    result = bounded_run.run([args.roundtrip, str(path)], timeout=15)
                    ok = result.returncode == 0
            if not ok:
                sys.exit(f"{path.name}: {result.returncode}\n{source}\n{result.stdout}{result.stderr}")


if __name__ == "__main__":
    main()
