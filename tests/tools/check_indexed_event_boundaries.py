#!/usr/bin/env python3
# Copyright (c) 2026 The kirimasim Authors
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

"""Indexed events preserve the approved implicit-sensitivity and slang boundaries."""
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
    with tempfile.TemporaryDirectory(prefix='kirimasim-indexed-event-') as folder:
        source = Path(folder, 'probe.sv')
        for call in ('seen()', 'nested()', 'recursive(2)'):
            for kind in ('always_comb', 'always_latch', 'always @*', 'assign'):
                decl = 'wire value;' if kind == 'assign' else 'bit value;'
                source.write_text(f'''module top;event events[1:3];int index=1;{decl}
function automatic bit seen();return events[index].triggered;endfunction
function automatic bit nested();return seen();endfunction
function automatic bit recursive(input int n);if(n>0)return recursive(n-1);return seen();endfunction
{kind} value={call};
initial begin #1;->events[index];#1;end endmodule
''')
                result = subprocess.run([binary, source.name], cwd=folder, capture_output=True,
                                        text=True, timeout=15, check=False)
                assert result.returncode == 2 and not result.stdout, result.stdout+result.stderr
                assert 'implicit sensitivity to named-event triggered values' in result.stderr, result.stderr
                assert re.search(r'probe\.sv:5:\d+: error:', result.stderr), result.stderr
                assert 'internal error' not in result.stderr, result.stderr
                checked += 1
        for statement in ('->events[0];', '->>events[0];', '@(events[0]);',
                          '$display("%0d",events[0].triggered);'):
            source.write_text('module top;event events[1:3];\ninitial begin '+statement+' end\nendmodule\n')
            result = subprocess.run([binary, source.name], cwd=folder, capture_output=True,
                                    text=True, timeout=15, check=False)
            assert result.returncode == 2 and not result.stdout, result.stdout+result.stderr
            assert result.stderr.count(': error:') == 1, result.stderr
            assert re.search(r'probe\.sv:2:\d+: error: cannot refer to element 0', result.stderr), result.stderr
            assert '[-Windex-oob]' in result.stderr, result.stderr
            assert 'internal error' not in result.stderr, result.stderr
            checked += 1
    print(f'{checked} positioned indexed-event policy boundaries checked')
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
