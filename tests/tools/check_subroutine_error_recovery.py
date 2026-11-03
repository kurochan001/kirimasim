#!/usr/bin/env python3
# Copyright (c) 2026 The kirimasim Authors
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
"""Original nested-block recovery matrix for disallowed function forks (13.4.4)."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT/'tools'))
import bounded_run

BODIES = {
    'declaration': 'begin integer parcel=19; scratch=parcel; end',
    'named': 'begin: chosen_branch scratch=23; end',
    'loop': 'for(integer lane=0;lane<2;lane++) scratch=lane;',
}


def cases():
    for home in ('module', 'unit', 'package', 'interface', 'program', 'class'):
        for join in ('join', 'join_any', 'join_none'):
            for position in ('before', 'after', 'both'):
                for body, nested in BODIES.items():
                    prefix = 'scratch=11;\n' if position != 'after' else ''
                    suffix = 'scratch=29;\n' if position != 'before' else ''
                    declaration = ('function automatic void examine();integer scratch;\n' +
                                   prefix + 'fork\n' + nested + '\n' + join + '\n' + suffix +
                                   'endfunction\n')
                    call = 'initial if(1\'b0) examine();\n'
                    if home == 'unit':
                        source = declaration + 'module top;\n' + call + 'endmodule\n'
                    elif home == 'package':
                        source = ('package recovery_scope;\n' + declaration +
                                  'endpackage\nmodule top;\n' +
                                  call.replace('examine()', 'recovery_scope::examine()') +
                                  'endmodule\n')
                    elif home in ('interface', 'program'):
                        source = (f'{home} holder;\n' + declaration + call + f'end{home}\n' +
                                  'module top;holder h();endmodule\n')
                    elif home == 'class':
                        source = ('class RecoveryCell;\nstatic ' + declaration +
                                  'endclass\nmodule top;\n' +
                                  call.replace('examine()', 'RecoveryCell::examine()') +
                                  'endmodule\n')
                    else:
                        source = 'module top;\n' + declaration + call + 'endmodule\n'
                    yield f'{home}_{join}_{position}_{body}', source, 0 if join == 'join_none' else 2


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--kirimasim', type=Path, required=True)
    parser.add_argument('--case', action='append')
    parser.add_argument('--report', type=Path)
    args = parser.parse_args()
    records = []
    with tempfile.TemporaryDirectory(prefix='subroutine-recovery-') as tmp:
        root = Path(tmp)
        for name, source, expected in cases():
            if args.case and name not in args.case:
                continue
            (root/'probe.sv').write_text(source)
            for mode in ('parse', 'run'):
                command = [str(args.kirimasim.resolve()), *(['parse'] if mode == 'parse' else []),
                           '--top=top', 'probe.sv']
                ran = bounded_run.run(command, cwd=root, timeout=10)
                positioned = bool(re.search(r'probe\.sv:\d+:\d+: error: statements that pass time '
                                            r'are not allowed in this context', ran.stderr))
                passed = (ran.returncode == expected and not ran.stdout and
                          ('error:' not in ran.stderr if expected == 0 else positioned) and
                          'internal compiler error' not in ran.stderr and
                          'AssertionException' not in ran.stderr)
                diagnostics = ran.stderr.replace(tmp, '<probe>').replace(str(Path.home()), '<home>')
                records.append(dict(name=name, mode=mode, expected_exit=expected,
                                    exit=ran.returncode, passed=passed,
                                    source_sha256=hashlib.sha256(source.encode()).hexdigest(),
                                    diagnostics=diagnostics))
                if not passed:
                    print(name, mode, 'FAILED', ran.returncode, diagnostics, flush=True)
    assert records, 'no selected cases'
    if args.report:
        args.report.write_text(json.dumps(records, indent=2)+'\n')
    good = sum(r['passed'] for r in records)
    print(f'subroutine recovery: {good}/{len(records)} passed')
    return 0 if good == len(records) else 1


if __name__ == '__main__':
    sys.exit(main())
