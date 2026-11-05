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
    supported = 0
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

        def check_supported(source):
            nonlocal supported
            path = work / f'supported{supported}.sv'
            path.write_text(source)
            result = bounded_run.run([str(binary), str(path)], cwd=work, timeout=10)
            assert result.returncode == 0, (source, result.returncode, result.stderr)
            assert result.stdout == '', (source, result.stdout)
            assert ': error:' not in result.stderr, result.stderr
            sir = bounded_run.run([str(binary), '--dump-sir', str(path)], cwd=work, timeout=10)
            assert sir.returncode == 0, sir.stderr
            bindings = [line for line in sir.stdout.splitlines()
                        if line.startswith('process @') and ' implicit-container=' in line]
            assert len(bindings) == 1, sir.stdout
            assert ' trigger=' not in bindings[0], bindings[0]
            supported += 1

        for size in ('-1', "'x", "'z"):
            check('module top; int a[]; logic signed [63:0] n;\n'
                  f'initial begin n={size}; a=new[n]; end endmodule\n',
                  'dynamic-array size is negative or contains X/Z bits', '2')

        # Approved 2026-11-06: typed whole-value bindings replace the old
        # static-range-only rejection (SCHEDULER_SPEC 6.3/9.2/9.5).
        for statement in ('always_comb n=q.size();', 'always_latch n=q.size();',
                          'always @* n=q.size();', 'assign n=q.size();',
                          'always_comb n=read_size();'):
            declaration = 'wire [31:0] n;' if statement.startswith('assign') else 'int n;'
            source = ('module top; int q[$]; '+declaration+'\n'
                      'function automatic int read_size(); return q.size(); endfunction\n'
                      +statement+'\nendmodule\n')
            check_supported(source)

        for expression in ('top.holder.q.size()', 'read_nested()'):
            source = ('module top; int n;\n'
                      'function int holder(); int q[$]; return 0; endfunction\n'
                      'function automatic int read_nested(); return top.holder.q.size(); endfunction\n'
                      'always_comb n='+expression+';\n'
                      'initial begin #1; top.holder.q.push_back(1); end endmodule\n')
            check_supported(source)

        # A fork child shares the enclosing automatic queue (the approved
        # capture contract, SIR_SPEC 7 and 8), and runs silently.
        path = work / 'captured_queue.sv'
        path.write_text('module top; task automatic run(); int q[$];\n'
                        'fork q.push_back(1); join endtask initial run(); endmodule\n')
        result = bounded_run.run([str(binary), str(path)], cwd=work, timeout=10)
        assert result.returncode == 0, (result.returncode, result.stderr)
        assert result.stdout == '', result.stdout
        assert ': error:' not in result.stderr, result.stderr

    print(f'dynamic-container diagnostics: {checked} positioned errors checked')
    print(f'dynamic-container implicit bindings: {supported} unchanged sources execute')
    return 0


if __name__ == '__main__':
    sys.exit(main())
