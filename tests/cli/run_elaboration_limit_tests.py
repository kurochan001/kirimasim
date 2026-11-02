#!/usr/bin/env python3
# Copyright (c) 2026 The kirimasim Authors
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

"""Exercise the four source elaboration budgets without changing runtime limits."""

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


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--kirimasim', required=True)
    parser.add_argument('--fail-fast', action='store_true', help='stop on the first failed check')
    args = parser.parse_args()
    exe = str(Path(args.kirimasim).resolve())
    fixtures = Path(__file__).resolve().parent / 'elaboration_limits'
    cases = json.loads((fixtures / 'cases.json').read_text())
    flags = list(dict.fromkeys(case['flag'] for case in cases['finite']))
    checked = 0
    failures = []

    with tempfile.TemporaryDirectory(prefix='kirimasim-elaboration-') as tmp:
        root = Path(tmp)
        for source in fixtures.glob('*.sv'):
            shutil.copyfile(source, root / source.name)

        def check(label, mode, source, options, code, reason='', transport='command', after=False):
            nonlocal checked
            checked += 1
            arguments = [source, *options] if after else [*options, source]
            if transport == 'file':
                (root / 'args.f').write_text('\n'.join(arguments) + '\n')
                arguments = ['-f', 'args.f']
            problems = []
            try:
                run = bounded_run.run([exe, *mode, *arguments], cwd=tmp, timeout=20)
                if run.returncode != code:
                    problems.append(f'exit={run.returncode}, expected={code}')
                if code == 0:
                    if not mode and run.stdout != '7\n':
                        problems.append(f'stdout={run.stdout!r}, expected="7\\n"')
                    elif mode == ['parse'] and run.stdout:
                        problems.append('parse produced stdout')
                    elif mode == ['--dump-sir'] and not run.stdout.startswith('; sir format 0\n'):
                        problems.append('missing SIR text')
                    if re.search(r'(?:fatal )?error:', run.stderr):
                        problems.append('unexpected error diagnostic')
                else:
                    if run.stdout:
                        problems.append(f'failed compilation produced stdout={run.stdout!r}')
                    if 'error:' not in run.stderr or reason not in run.stderr:
                        problems.append(f'missing error reason {reason!r}')
                    if reason and re.search(re.escape(source) + r':\d+:\d+: (?:fatal )?error:', run.stderr) is None:
                        problems.append('missing source-position diagnostic')
                if 'internal error' in run.stderr:
                    problems.append('internal error')
                if problems:
                    problems.append(f'stderr={run.stderr!r}')
            except (subprocess.TimeoutExpired, bounded_run.OutputLimit) as error:
                problems.append(str(error))
            if problems:
                detail = f'FAIL: {label} mode={mode} {transport} after={after}: ' + '; '.join(problems)
                failures.append(detail)
                if args.fail_fast:
                    print(detail)
                    raise SystemExit(1)

        for mode in ([], ['parse'], ['--dump-sir']):
            for transport in ('command', 'file'):
                for after in (False, True):
                    for case in cases['finite']:
                        source = case['name'] + '.sv'
                        for level in ('default', 'high', 'low'):
                            options = [] if level == 'default' else [f'--{case["flag"]}={case[level]}']
                            check(f'{case["name"]} {level}', mode, source, options,
                                  2 if level == 'low' else 0,
                                  case['reason'] if level == 'low' else '', transport, after)

            for transport in ('command', 'file'):
                for case in cases['finite'][::4]:
                    flag = case['flag']
                    source = case['name'] + '.sv'
                    # Zero is forwarded, not treated as the runtime "unlimited" spelling.
                    reason = case['reason'].replace('depth of 1', 'depth of 0')
                    check(flag + ' zero', mode, source, [f'--{flag}=0'], 2, reason, transport)
                    # The pinned frontend rejects this shallow hierarchy at UINT32_MAX.
                    # Preserve that observed upstream rejection, not a claim of support.
                    maximum_rejected = flag == 'max-hierarchy-depth'
                    check(flag + ' uint32 max upstream boundary', mode, source,
                          [f'--{flag}=4294967295'], 2 if maximum_rejected else 0,
                          'maximum depth of 4294967295' if maximum_rejected else '', transport)
                    # Slang rejects duplicate direct options, but a command file keeps
                    # its first occurrence. Preserve both frontend behaviors.
                    for values in ((1, 1024), (1024, 1)):
                        options = [f'--{flag}={value}' for value in values]
                        check(flag + f' repeated {values}', mode, source, options,
                              2 if transport == 'command' or values[0] == 1 else 0,
                              case['reason'] if transport == 'file' and values[0] == 1 else '',
                              transport)
                    # Numeric syntax and range belong to slang. Empty values can consume
                    # the next argument there; require an input error, not a specific text.
                    for value in ('', '-1', 'x', '1.5', '4294967296'):
                        check(flag + f' invalid {value!r}', mode, 'clean.sv', [f'--{flag}={value}'],
                              2, transport=transport)
                    others = [f'--{other}=1024' for other in flags if other != flag]
                    check(flag + ' independent low budget', mode, source,
                          [*others, f'--{flag}=1'], 2, case['reason'], transport)

                for case in cases['endless']:
                    check(case['source'] + ' bounded', mode, case['source'],
                          [f'--{case["flag"]}={case["value"]}'], 2, case['reason'], transport)
                # All four options coexist; a command must still compile and execute normally.
                check('all budgets', mode, 'clean.sv', [f'--{flag}=1024' for flag in flags], 0,
                      transport=transport)

            for flag in flags:
                # Unlisted spellings remain errors rather than gaining a new CLI alias.
                check(flag + ' bare', mode, 'clean.sv', [f'--{flag}'], 2)
                check(flag + ' unknown suffix', mode, 'clean.sv', [f'--{flag}-extra=1'], 2)

        checked += 1
        help_run = bounded_run.run([exe, '--help'], cwd=tmp, timeout=20)
        if help_run.returncode or any(f'--{flag}=<n>' not in help_run.stdout for flag in flags):
            failures.append('FAIL: help omits elaboration limits')

    print(f'elaboration limits: {checked} checks, {len(failures)} failures')
    for detail in failures[:10]:
        print(detail)
    return bool(failures)


if __name__ == '__main__':
    raise SystemExit(main())
