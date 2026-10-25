#!/usr/bin/env python3
# Copyright (c) 2026 The kirimasim Authors
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

"""Ordinary function event reads are supported; implicit object sensitivity is not."""

import argparse
from pathlib import Path
import re
import subprocess
import tempfile


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--kirimasim', type=Path, required=True)
    binary = str(parser.parse_args().kirimasim.resolve())
    checked = 0
    with tempfile.TemporaryDirectory() as folder:
        path = Path(folder, 'probe.sv')
        for call in ('seen()', 'nested()', 'recursive(2)'):
            for kind in ('always_comb', 'always_latch', 'always @*', 'assign'):
                decl = 'wire x;' if kind == 'assign' else 'bit x;'
                source = f'''module top;event ready;{decl}
function automatic bit seen();return ready.triggered;endfunction
function automatic bit nested();return seen();endfunction
function automatic bit recursive(input int n);if(n>0)return recursive(n-1);return seen();endfunction
{kind} x={call};
initial begin #1;->ready;#1;end endmodule
'''
                path.write_text(source)
                result = subprocess.run([binary, path.name], cwd=folder, capture_output=True,
                                        text=True, timeout=15, check=False)
                assert result.returncode == 2 and not result.stdout, result.stdout+result.stderr
                assert 'implicit sensitivity to named-event triggered values' in result.stderr, result.stderr
                assert re.search(r'probe\.sv:5:\d+: error:', result.stderr), result.stderr
                assert 'internal error' not in result.stderr, result.stderr
                checked += 1
    print(f'{checked} positioned implicit function-read boundaries checked')
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
