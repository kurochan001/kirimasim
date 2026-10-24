#!/usr/bin/env python3
# Copyright (c) 2026 The kirimasim Authors
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

"""Unreviewed delayed-side timing stays unsupported in every control-flow block."""

import argparse
from pathlib import Path
import re
import subprocess
import tempfile


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--kirimasim', required=True)
    args = parser.parse_args()
    binary = str(Path(args.kirimasim).resolve())
    for width in (1, 65, 257):
        for expression in ('control', 'permit && read_control(control)'):
            source = f'''module top;
logic [{width-1}:0] data='0;logic control=1,permit=1;wire [{width-1}:0] q;
function automatic logic read_control(input logic v);return v;endfunction
bufif1 #(2,3,4) g[{width-1}:0](q,data,{expression});
assign (weak1,weak0)q='1;
initial begin #5; $display("unexpected execution");end
endmodule
'''
            with tempfile.TemporaryDirectory() as folder:
                Path(folder, 'probe.sv').write_text(source)
                result = subprocess.run([binary, 'probe.sv'], cwd=folder, capture_output=True,
                                        text=True, timeout=15, check=False)
            errors = [line for line in result.stderr.splitlines() if ': error:' in line]
            pattern = r"(?:.*[/\\])?probe\.sv:4:[0-9]+: error: unsupported: a three-state gate with a delay that drives the net 'top\.q' with other drivers \(not implemented yet\)"
            if result.returncode != 2 or result.stdout or len(errors) != 1 or not re.fullmatch(pattern, errors[0]):
                print(f'FAIL: delayed gate boundary width={width}, control={expression}: exit={result.returncode}')
                print(result.stdout + result.stderr)
                return 1
            accepted = source.replace('#(2,3,4) ', '').replace("assign (weak1,weak0)q='1;\n", '')
            accepted = accepted.replace('$display("unexpected execution")', '$display("%0d", q === \'0)')
            with tempfile.TemporaryDirectory() as folder:
                Path(folder, 'probe.sv').write_text(accepted)
                result = subprocess.run([binary, 'probe.sv'], cwd=folder, capture_output=True,
                                        text=True, timeout=15, check=False)
            if result.returncode != 0 or result.stdout != '1\n' or ': error:' in result.stderr:
                print(f'FAIL: undelayed unresolved gate width={width}, control={expression}')
                print(result.stdout + result.stderr)
                return 1
    print('checked six delayed-gate rejections and six undelayed acceptances, including non-entry blocks')
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
