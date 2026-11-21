#!/usr/bin/env python3
# Copyright (c) 2026 The kirimasim Authors
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
"""Check sampled values across an NBA update and diagnostics for unsupported clocking forms."""
import argparse
from pathlib import Path
import re
import subprocess
import tempfile


def check_sampled_source(binary, source):
    # SIR_SPEC 7 and 1800-2023 14.13: the clockvar keeps the #1step sample,
    # including X/Z, when a process awakened by the same edge writes q by NBA.
    for signal, samples in (("q", "01xz"), ("d", "1xz0")):
        source.write_text(f"""module top;
  logic clk = 0, q = 0, d = 1;
  clocking cb @(posedge clk);
    input #1step sampled = {signal};
  endclocking
  always @(posedge clk) q <= d;
  initial begin
    #1 clk = 1;
    #1 clk = 0; d = 1'bx;
    #1 clk = 1;
    #1 clk = 0; d = 1'bz;
    #1 clk = 1;
    #1 clk = 0; d = 1'b0;
    #1 clk = 1;
  end
  initial repeat (4) begin
    @(cb); $display("sample=%b q=%b", cb.sampled, q);
  end
endmodule
""", encoding="utf-8")
        result = subprocess.run([binary, str(source)], text=True, capture_output=True, timeout=30)
        assert result.returncode == 0, (signal, result.stderr)
        expected = [f"sample={before} q={after}" for before, after in zip(samples, "1xz0")]
        assert result.stdout.splitlines() == expected, (signal, result.stdout, result.stderr)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--kirimasim", required=True, type=Path)
    args = parser.parse_args()
    cases = [
        ("default input #0; input d;", "clocking input skew other than #1step"),
        ("input #2 d;", "clocking input skew other than #1step"),
        ("output d;", "output or inout clocking variable"),
        ("inout d;", "output or inout clocking variable"),
        ("input selected = data[index];", "clocking source without static packed storage"),
        ("input selected = d + 1;", "clocking source without static packed storage"),
        ("input selected = {data[1],data[3]};", "noncontiguous clocking source"),
        ("input r;", "nonintegral clocking input"),
    ]
    with tempfile.TemporaryDirectory(prefix="kirimasim-clocking-") as directory:
        source = Path(directory) / "clocking.sv"
        binary = str(args.kirimasim.resolve())
        check_sampled_source(binary, source)
        for body, diagnostic in cases:
            source.write_text("module top;\nbit clk; int d, index; logic [7:0] data; real r;\n"
                              "clocking cb @(posedge clk);\n" + body +
                              "\nendclocking\nendmodule\n", encoding="utf-8")
            result = subprocess.run([binary, str(source)],
                                    text=True, capture_output=True, timeout=30)
            assert result.returncode == 2, (body, result.stderr)
            pattern = r"clocking\.sv:4:\d+: error: unsupported: " + re.escape(diagnostic)
            assert re.search(pattern, result.stderr), (body, result.stderr)
            assert not result.stdout, (body, result.stdout)
    print(f"2 clocking sample sources and {len(cases)} clocking diagnostics passed")


if __name__ == "__main__":
    main()
