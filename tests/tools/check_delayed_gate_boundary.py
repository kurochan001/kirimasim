#!/usr/bin/env python3
# Copyright (c) 2026 The kirimasim Authors
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

"""Approved complete delayed state across entry and non-entry control-flow blocks."""

import argparse
from pathlib import Path
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
            delayed = source.replace('initial begin #5; $display("unexpected execution");end',
                                     'initial begin #5; $display("settled %0d", q === \'0); '
                                     'data=\'1; control=\'x; #1; $display("pending %0d", q === \'0); '
                                     '#2; $display("H %0d", q === \'1); '
                                     'data=\'0; #1; $display("L_pending %0d", q === \'1); '
                                     '#2; $display("L %0d", q === \'x); end')
            with tempfile.TemporaryDirectory() as folder:
                Path(folder, 'probe.sv').write_text(delayed)
                result = subprocess.run([binary, 'probe.sv'], cwd=folder, capture_output=True,
                                        text=True, timeout=15, check=False)
            expected = 'settled 1\npending 1\nH 1\nL_pending 1\nL 1\n'
            if result.returncode != 0 or result.stdout != expected or ': error:' in result.stderr:
                print(f'FAIL: delayed gate state width={width}, control={expression}: exit={result.returncode}')
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
    print('checked six complete delayed states and six unchanged undelayed acceptances, including non-entry blocks')
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
