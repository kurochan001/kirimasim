#!/usr/bin/env python3
# Copyright (c) 2026 The kirimasim Authors
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

"""Live-process admission, completion, cancellation and CLI contracts (CLI 2.7)."""

import argparse
import json
from pathlib import Path
import subprocess
import tempfile

MATRIX = Path(__file__).resolve().parent / 'process_limit_matrix'


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--kirimasim', type=Path, required=True)
    binary = str(parser.parse_args().kirimasim.resolve())
    cases = json.loads((MATRIX / 'cases.json').read_text())
    checked = 0
    with tempfile.TemporaryDirectory(prefix='kirimasim-process-limit-') as directory:
        def run(arguments):
            return subprocess.run([binary, *arguments], cwd=directory, capture_output=True,
                                  text=True, timeout=20, check=False)

        for case in cases:
            arguments = [] if case['limit'] is None else [f'--max-procs={case["limit"]}']
            result = run([*arguments, '--stats=json', '--top=top', str(MATRIX / case['source'])])
            lines = result.stdout.splitlines(keepends=True)
            assert result.returncode == case['exit'], (case, result.stdout, result.stderr)
            assert ''.join(lines[:-1]) == case['stdout'], (case, result.stdout, result.stderr)
            stats = json.loads(lines[-1])
            assert stats['sim.end_reason'] == case['reason'], (case, stats)
            assert 'internal error' not in result.stderr, result.stderr
            if case['exit'] == 6:
                assert 'resource limit reached: 1 live processes made at run time (--max-procs=1)' in result.stderr, result.stderr
                assert 'time did not advance' not in result.stderr, result.stderr
                assert stats['sim.end_time'] == 0, (case, stats)
            checked += 1
        source = str(MATRIX / 'burst_module_join_1.sv')
        for value in ('', '-1', '+1', 'x', '1.5', ' 1', '18446744073709551616'):
            result = run([f'--max-procs={value}', source])
            assert result.returncode == 2 and not result.stdout, result.stdout + result.stderr
            assert f'--max-procs={value} needs a whole number' in result.stderr, result.stderr
            checked += 1
        for flag in ('--max-time-ok', '--max-wall-time=0'):
            result = run(['--max-procs=1', flag, source])
            assert result.returncode == 6 and result.stdout == 'final=0\n', result.stdout + result.stderr
            checked += 1
        for arguments in (['parse', '--max-procs=1'], ['--max-procs=1', '--dump-sir']):
            result = run([*arguments, source])
            assert result.returncode == 0, result.stderr
            assert 'final=0\n' not in result.stdout, result.stdout
            checked += 1
        for flags, output, status in ((['--max-procs=1', '--max-procs=2'], 'done=2\nfinal=2\n', 0),
                                      (['--max-procs=2', '--max-procs=1'], 'final=0\n', 6)):
            result = run([*flags, source])
            assert result.returncode == status and result.stdout == output, result.stdout + result.stderr
            checked += 1
        result = run(['--help'])
        assert result.returncode == 0 and '--max-procs=<n>' in result.stdout, result.stdout
    print(f'{len(cases)} original designs and {checked - len(cases)} CLI contracts checked; process limit advertised in help')
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
