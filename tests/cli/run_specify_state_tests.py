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


def public_q_changes(text):
    codes = []
    changes = []
    tick = 0
    for line in text.splitlines():
        if line.startswith('$var '):
            fields = line.split()
            assert '$driver' not in fields[4]
            if fields[4] == 'q':
                codes.append(fields[3])
        elif line.startswith('#'):
            tick = int(line[1:])
        elif codes and line[0:1] in ('0', '1', 'x', 'z') and line[1:] == codes[0]:
            changes.append((tick, line[0]))
    assert len(codes) == 2 and len(set(codes)) == 1, codes
    return changes


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--kirimasim', required=True, type=Path)
    parser.add_argument('--report', type=Path)
    parser.add_argument('--case', action='append')
    parser.add_argument('--fst-reader', type=Path)
    args = parser.parse_args()
    binary = str(args.kirimasim.resolve())
    reader = str(args.fst_reader.resolve()) if args.fst_reader else None
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
        if reader and not args.case:
            for name, expected_changes in (
                ('state_public_output_direct', [(0, 'x'), (3, '0'), (28, '1'), (44, '0')]),
                ('state_force_before_functional_arrival', [(0, 'x'), (9, '0'), (32, '1')]),
            ):
                case = next(c for c in selected if c['name'] == name)
                path = directory / (name + '.sv')
                wave = directory / (name + '.fst')
                ran = bounded_run.run([binary, '--wave=' + wave.name, path.name],
                                      cwd=directory, timeout=15)
                assert ran.returncode == 0 and ran.stdout == case['expected_stdout'], ran.stderr
                decoded = bounded_run.run([reader, '--decode', str(wave)], timeout=15)
                assert decoded.returncode == 0 and not decoded.stderr, decoded.stderr
                observed = public_q_changes(decoded.stdout)
                assert observed == expected_changes, (name, observed, expected_changes)
                records.append({'case': name + '_fst', 'source_sha256': hashlib.sha256(case['source'].encode()).hexdigest(),
                                'status': 'passed', 'decoded_public_changes': observed})
    if args.report:
        args.report.write_text(json.dumps(records, indent=2) + '\n')
    print(f'state-dependent specify: {len(records)} source timing/context/boundary checks passed')


if __name__ == '__main__':
    main()
