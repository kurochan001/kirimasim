#!/usr/bin/env python3
# Copyright (c) 2026 The kirimasim Authors
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

"""Check slang warning names, severity and ordering through both CLI paths."""

import argparse
import json
import re
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'tools'))
import bounded_run


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--kirimasim', required=True)
    args = parser.parse_args()
    exe = str(Path(args.kirimasim).resolve())
    inputs = Path(__file__).resolve().parent / 'slang_warnings'
    cases = json.loads((inputs / 'cases.json').read_text())
    failures = []
    checked = 0
    with tempfile.TemporaryDirectory(prefix='kirimasim-warning-') as tmp:
        root = Path(tmp)
        for source in inputs.glob('*.sv'):
            shutil.copyfile(source, root / source.name)
        for mode in ([], ['parse']):
            for transport in ('command', 'file'):
                for after in (False, True):
                    for case in cases:
                        flags = case['flags']
                        source = [case['source'] + '.sv']
                        arguments = source + flags if after else flags + source
                        if transport == 'file':
                            (root / 'args.f').write_text('\n'.join(arguments) + '\n')
                            arguments = ['-f', 'args.f']
                        label = f'{mode} {transport} after={after} {case["name"]}'
                        checked += 1
                        try:
                            run = bounded_run.run([exe, *mode, *arguments], cwd=root, timeout=20)
                        except (subprocess.TimeoutExpired, bounded_run.OutputLimit) as error:
                            failures.append(f'{label}: {error}')
                            continue
                        expected = '' if mode else case['stdout']
                        diagnostics = re.findall(r'(?m)^(?:.*?:\d+:\d+: )?(warning|error): (.*)$', run.stderr)
                        actual = [(level, text) for level, text in diagnostics]
                        wanted = case['severity']
                        ok = run.returncode == case['exit'] and run.stdout == expected
                        if wanted:
                            ok = ok and bool(actual) and all(
                                level == wanted and f'[-W{case["diagnostic"]}]' in text
                                for level, text in actual)
                        else:
                            ok = ok and not actual
                        if case['diagnostic'] == 'width-trunc':
                            ok = ok and bool(re.search(r'(?m)^width\.sv:8:14: (warning|error):', run.stderr))
                        if not ok:
                            failures.append(f'{label}: exit={run.returncode}, stdout={run.stdout!r}, stderr={run.stderr!r}')
            checked += 1
            run = bounded_run.run([exe, *mode, '-W', 'clean.sv'], cwd=root, timeout=20)
            if run.returncode != 2 or run.stdout or '-W needs a warning name' not in run.stderr:
                failures.append(f'{mode} empty warning name: {run.stderr!r}')
        checked += 1
        run = bounded_run.run([exe, '--help'], cwd=root, timeout=20)
        if run.returncode or any(x not in run.stdout for x in ('-W<name>', '-Wno-<name>', '-Werror[=<name>]')):
            failures.append('help omits source warning options')
    print(f'slang warning options: {checked} checks, {len(failures)} failures')
    for failure in failures:
        print(failure)
    return bool(failures)


if __name__ == '__main__':
    raise SystemExit(main())
