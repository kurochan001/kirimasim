#!/usr/bin/env python3
# Copyright (c) 2026 The kirimasim Authors
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

"""Check the approved slang mode delegation and strict executable writer boundary."""

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
import bounded_run  # noqa: E402


MODES = {'run': [], 'parse': ['parse'], 'dump-sir': ['--dump-sir']}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--kirimasim', required=True)
    parser.add_argument('--section', choices=['modes', 'drivers'], required=True)
    parser.add_argument('--fail-fast', action='store_true')
    # One mode runs a third of the checks of a section, so that a slow
    # (sanitizer) build can run a section as several tests.
    parser.add_argument('--mode', choices=list(MODES), action='append',
                        help='the mode to check (repeatable); all three when omitted')
    args = parser.parse_args()
    exe = str(Path(args.kirimasim).resolve())
    fixtures = Path(__file__).resolve().parent / 'compatibility'
    cases = json.loads((fixtures / 'cases.json').read_text())
    failures = []
    checked = 0

    with tempfile.TemporaryDirectory(prefix='kirimasim-compat-') as tmp:
        root = Path(tmp)
        shutil.copytree(fixtures, root, dirs_exist_ok=True)

        def check(label, source, flags, mode, transport, code, stdout='7\n', after=False,
                  source_error=False, strict_error=False):
            nonlocal checked
            checked += 1
            argv = [source, *flags] if after else [*flags, source]
            if transport == 'file':
                (root / 'args.f').write_text('\n'.join(argv) + '\n')
                argv = ['-f', 'args.f']
            problems = []
            try:
                run = bounded_run.run([exe, *mode, *argv], cwd=tmp, timeout=20)
                if run.returncode != code:
                    problems.append(f'exit={run.returncode}, expected={code}')
                if code == 0:
                    if mode == ['parse']:
                        if run.stdout:
                            problems.append('parse produced stdout')
                    elif mode == ['--dump-sir']:
                        if not run.stdout.startswith('; sir format 0\n'):
                            problems.append('no SIR output')
                    elif run.stdout != stdout:
                        problems.append(f'stdout={run.stdout!r}, expected={stdout!r}')
                    if re.search(r'(?:fatal )?error:', run.stderr):
                        problems.append('unexpected source error')
                else:
                    if run.stdout:
                        problems.append('failed compilation produced stdout')
                    if 'error:' not in run.stderr:
                        problems.append('missing error diagnostic')
                    if source_error and re.search(r'[^\n]+\.sv:\d+:\d+: (?:fatal )?error:', run.stderr) is None:
                        problems.append('missing source-position error')
                    if strict_error and 'unsupported: conflicting source drivers' not in run.stderr:
                        problems.append('missing executable writer diagnostic')
                if 'internal error' in run.stderr:
                    problems.append('unexpected internal error')
                if problems:
                    problems.append(f'stdout={run.stdout!r}, stderr={run.stderr!r}')
            except (subprocess.TimeoutExpired, bounded_run.OutputLimit) as error:
                problems.append(str(error))
            if problems:
                detail = f'FAIL: {label} mode={mode} {transport} after={after}: ' + '; '.join(problems)
                failures.append(detail)
                if args.fail_fast:
                    print(detail)
                    raise SystemExit(1)

        for mode in (MODES[name] for name in (args.mode or list(MODES))):
            if args.section == 'modes':
                for transport in ('command', 'file'):
                    for after in (False, True):
                        for case in cases['behaviors']:
                            for selected in (None, 'default', 'vcs', 'all'):
                                effective = selected or 'default'
                                flags = [f'--compat={selected}'] if selected else []
                                flags += case.get('flags', [])
                                accepted = effective in case['accepted']
                                check(case['source'] + f' {selected}', case['source'], flags,
                                      mode, transport, 0 if accepted else 2,
                                      case['stdout'].get(effective, ''), after, source_error=not accepted)
                    for value in ('', 'unknown', '0'):
                        check('invalid compat ' + repr(value), 'clean.sv', ['--compat=' + value],
                              mode, transport, 2)
                    for first, second in [('default', 'vcs'), ('vcs', 'default')]:
                        valid = second == 'vcs'  # The pinned enum option keeps its last occurrence.
                        check('repeated modes ' + first + '/' + second, 'enum.sv',
                              ['--compat=' + first, '--compat=' + second], mode, transport,
                              0 if valid else 2, '1\n')
                for flags in (['--compat'], ['--compat-extra=all']):
                    check('unlisted option alias', 'clean.sv', flags, mode, 'command', 2)
                for source, stdout in cases['positives'].items():
                    for selected in (None, 'default', 'vcs', 'all'):
                        flags = [f'--compat={selected}'] if selected else []
                        check(source + f' {selected}', source, flags, mode, 'command', 0, stdout)
            else:
                for transport in ('command', 'file'):
                    for case in cases['drivers']:
                        for policy in ('default', 'suppressed', 'all'):
                            flags = ([] if policy == 'default' else ['--compat=all'] if policy == 'all'
                                     else [f'-Wno-{case["warning"]}'])
                            rejected = case['overlap'] and (policy == 'default' or mode != ['parse'])
                            own_error = rejected and policy != 'default' and mode != ['parse']
                            check(case['source'] + ' ' + policy, case['source'], flags, mode, transport,
                                  2 if rejected else 0, source_error=rejected, strict_error=own_error)
        if args.section == 'modes':
            checked += 1
            help_run = bounded_run.run([exe, '--help'], cwd=tmp, timeout=20)
            if help_run.returncode or '--compat=default|vcs|all' not in help_run.stdout:
                failures.append('FAIL: --help omits compatibility modes')

    modes = ', '.join(args.mode) if args.mode else 'all modes'
    print(f'compatibility {args.section} ({modes}): {checked} checks, {len(failures)} failures')
    for failure in failures[:12]:
        print(failure)
    return bool(failures)


if __name__ == '__main__':
    raise SystemExit(main())
