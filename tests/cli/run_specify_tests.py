#!/usr/bin/env python3
# Copyright (c) 2026 The kirimasim Authors
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
"""Execute admitted scalar specify timing and positioned boundary checks."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import sys
import tempfile

sys.path.insert(0, str(Path(__file__).resolve().parents[2]/'tools'))
import bounded_run
from gen_specify_matrix import programs, rejections


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--kirimasim', required=True)
    parser.add_argument('--report', type=Path)
    args = parser.parse_args()
    binary = str(Path(args.kirimasim).resolve())
    records = []
    with tempfile.TemporaryDirectory(prefix='kirimasim-specify-native-') as tmp:
        directory = Path(tmp)
        for name, text, expected, mode in programs():
            path = directory/(name+'.sv')
            path.write_text(text)
            ran = bounded_run.run([binary, '--mintypmax='+mode, path.name], cwd=tmp, text=True, timeout=15)
            errors = [line for line in ran.stderr.splitlines() if 'error:' in line or 'internal error' in line]
            if ran.returncode != 0 or errors or ran.stdout != expected:
                raise AssertionError(f'{name}: exit={ran.returncode}\nexpected={expected!r}\nactual={ran.stdout!r}\n{ran.stderr}')
            records.append({'case': name, 'status': 'passed', 'source_sha256': hashlib.sha256(text.encode()).hexdigest()})
        for name, text, reason in rejections():
            path = directory/('reject_'+name+'.sv')
            path.write_text(text)
            ran = bounded_run.run([binary, path.name], cwd=tmp, text=True, timeout=15)
            if ran.returncode != 2 or ran.stdout or reason not in ran.stderr or 'internal error' in ran.stderr or not re.search(re.escape(path.name)+r':\d+:\d+: error:', ran.stderr):
                raise AssertionError(f'reject_{name}: exit={ran.returncode}\n{ran.stdout}{ran.stderr}')
            records.append({'case': 'reject_'+name, 'status': 'rejected_with_position', 'reason': reason, 'source_sha256': hashlib.sha256(text.encode()).hexdigest()})
    if args.report:
        args.report.write_text(json.dumps(records, indent=2)+'\n')
    print(f'scalar specify: {len(records)} native timing/context/boundary cases passed')


if __name__ == '__main__':
    main()
