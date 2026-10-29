#!/usr/bin/env python3
# Copyright (c) 2026 The kirimasim Authors
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
"""Positioned diagnostics for clocking forms outside the approved subset."""
import argparse
from pathlib import Path
import re
import subprocess
import tempfile


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
        for body, diagnostic in cases:
            source.write_text("module top;\nbit clk; int d, index; logic [7:0] data; real r;\n"
                              "clocking cb @(posedge clk);\n" + body +
                              "\nendclocking\nendmodule\n", encoding="utf-8")
            result = subprocess.run([str(args.kirimasim.resolve()), str(source)],
                                    text=True, capture_output=True, timeout=30)
            assert result.returncode == 2, (body, result.stderr)
            pattern = r"clocking\.sv:4:\d+: error: unsupported: " + re.escape(diagnostic)
            assert re.search(pattern, result.stderr), (body, result.stderr)
            assert not result.stdout, (body, result.stdout)
    print(f"{len(cases)} clocking diagnostics passed")


if __name__ == "__main__":
    main()
