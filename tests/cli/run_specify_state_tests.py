#!/usr/bin/env python3
# Copyright (c) 2026 The kirimasim Authors
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

"""Execute the approved state-dependent specify matrix with bounded I/O."""

import argparse
import hashlib
import json
from pathlib import Path
import re
import sys
import tempfile

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / 'tools'))
import bounded_run  # noqa: E402
from gen_specify_state_matrix import cases  # noqa: E402


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--kirimasim', required=True, type=Path)
    parser.add_argument('--report', type=Path)
    parser.add_argument('--case', action='append')
    args = parser.parse_args()
    binary = str(args.kirimasim.resolve())
    selected = [c for c in cases() if not args.case or c['name'] in args.case]
    assert selected and (not args.case or set(args.case) == {c['name'] for c in selected})
    assert len({c['name'] for c in selected}) == len(selected)
    records = []
    with tempfile.TemporaryDirectory(prefix='kirimasim-specify-state-') as temp:
        directory = Path(temp)
        for case in selected:
            path = directory / (case['name'] + '.sv')
            path.write_text(case['source'])
            command = [binary, '--mintypmax=' + case.get('mtm', 'typ'),
                       *case.get('flags', []), path.name]
            ran = bounded_run.run(command, cwd=directory, timeout=15)
            expected_exit = case.get('expected_exit', 0)
            diagnostic = case.get('diagnostic', '')
            good = (ran.returncode == expected_exit and ran.stdout == case['expected_stdout'] and
                    'internal error' not in ran.stderr and diagnostic in ran.stderr and
                    (expected_exit != 0 or 'error:' not in ran.stderr))
            if case.get('positioned'):
                good = good and re.search(re.escape(path.name) + r':\d+:\d+: error:', ran.stderr)
            if case.get('runtime_at'):
                good = good and ('@ ' + case['runtime_at'] + ']') in ran.stderr
            if not good:
                raise AssertionError(f"{case['name']}: exit {ran.returncode}, expected {expected_exit}\n"
                                     f"expected stdout {case['expected_stdout']!r}\n"
                                     f"stdout {ran.stdout[:4096]!r}\nstderr {ran.stderr[:4096]!r}")
            records.append({'case': case['name'], 'source_sha256': hashlib.sha256(case['source'].encode()).hexdigest(),
                            'status': 'passed' if expected_exit == 0 else 'expected_error',
                            'exit': ran.returncode})
    if args.report:
        args.report.write_text(json.dumps(records, indent=2) + '\n')
    print(f'state-dependent specify: {len(records)} source timing/context/boundary checks passed')


if __name__ == '__main__':
    main()
