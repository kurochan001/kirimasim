#!/usr/bin/env python3
# Copyright (c) 2026 The kirimasim Authors
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

"""Check original dynamic-container runtime errors and explicit limits."""

import argparse
import re
import sys
import tempfile
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / 'tools'))
import bounded_run  # noqa: E402


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--kirimasim', required=True, type=Path)
    args = parser.parse_args()
    binary = args.kirimasim.resolve()
    checked = 0
    with tempfile.TemporaryDirectory() as directory:
        work = Path(directory)
        def check(source, message, position):
            nonlocal checked
            path = work / f'case{checked}.sv'
            path.write_text(source)
            result = bounded_run.run([str(binary), str(path)], cwd=work, timeout=10)
            assert result.returncode == 2, (source, result.returncode, result.stderr)
            assert result.stdout == '', (source, result.stdout)
            assert message in result.stderr, (source, result.stderr)
            assert re.search(rf'{path.name}:{position}: error:', result.stderr), result.stderr
            assert 'internal error' not in result.stderr, result.stderr
            checked += 1

        for size in ('-1', "'x", "'z"):
            check('module top; int a[]; logic signed [63:0] n;\n'
                  f'initial begin n={size}; a=new[n]; end endmodule\n',
                  'dynamic-array size is negative or contains X/Z bits', '2')

        # SIR static trigger ranges cannot yet encode typed heap reads.
        for statement in ('always_comb n=q.size();', 'always_latch n=q.size();',
                          'always @* n=q.size();', 'assign n=q.size();',
                          'always_comb n=read_size();'):
            declaration = 'wire [31:0] n;' if statement.startswith('assign') else 'int n;'
            source = ('module top; int q[$]; '+declaration+'\n'
                      'function automatic int read_size(); return q.size(); endfunction\n'
                      +statement+'\nendmodule\n')
            check(source, 'unsupported: implicit sensitivity to dynamic container values', r'3:\d+')

        # An enclosing automatic variable in a fork is still the explicit
        # SIR_SPEC 7.5 limit; do not silently alias or copy its storage.
        check('module top; task automatic run(); int q[$];\n'
              'fork q.push_back(1); join endtask initial run(); endmodule\n',
              "unsupported: automatic variable 'q' here", r'2:\d+')

    print(f'dynamic-container diagnostics: {checked} positioned errors checked')
    return 0


if __name__ == '__main__':
    sys.exit(main())
