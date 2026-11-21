#!/usr/bin/env python3
# Copyright (c) 2026 The kirimasim Authors
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

"""Dynamic arrays of fixed rows, IEEE 1800-2023 7.4.5, 7.5 and 7.6."""
import argparse
from pathlib import Path
import subprocess
import tempfile

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--kirimasim", required=True)
    parser.add_argument("--roundtrip", required=True)
    args = parser.parse_args()
    with tempfile.TemporaryDirectory() as tmp:
        for left, right in [(-3, -1), (5, 2)]:
            for kind in ["bit", "logic"]:
                for width in [3, 65]:
                    default = "'x" if kind == "logic" else "'0"
                    source = f"""
module top;
  typedef {kind} signed [{width-1}:0] word_t;
  typedef word_t row_t [{left}:{right}];
  row_t original[], copied[];
  int i={left}, j={right};
  initial begin
    if (original.size() != 0) $fatal;
    original = new[2];
    if (original[1][j] !== {default}) $fatal;
    original[0][i] = -2; original[1][j] = 3;
    copied = original;
    original = new[3](original);
    copied[0][i] = 1;
    if (original[0][i] !== -2 || original[1][j] !== 3 ||
        original[2][j] !== {default} || copied.size() != 2) $fatal;
    i=100; original[1][i]=1;
    if (original[1][i] !== {default} || original[1][j] !== 3 ||
        original[-1][j] !== {default}) $fatal;
    original.delete();
    if (original.size()!=0 || copied[1][j] !== 3) $fatal;
    $display("ok");
  end
endmodule"""
                    path = Path(tmp)/"rows.sv"
                    path.write_text(source)
                    for exe in [args.kirimasim, args.roundtrip]:
                        proc = subprocess.run([exe, str(path)], capture_output=True, text=True, timeout=15)
                        if proc.returncode or (exe == args.kirimasim and proc.stdout != "ok\n"):
                            raise SystemExit(source + proc.stdout + proc.stderr)

if __name__ == "__main__":
    main()
