#!/usr/bin/env python3
# Copyright (c) 2026 The kirimasim Authors
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

"""Event initialization must preserve rebinding restrictions and constructor counts."""

import argparse
from pathlib import Path
import re
import subprocess
import tempfile


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--kirimasim', type=Path, required=True)
    binary = str(parser.parse_args().kirimasim.resolve())
    checked = 0
    with tempfile.TemporaryDirectory(prefix='kirimasim-event-init-') as directory:
        source = Path(directory, 'probe.sv')
        for home in ('initial', 'task', 'function'):
            statements = (
                'automatic event shared=a;shared=b;',
                'automatic event shared=a;$display("%0d",shared==b);',
                'automatic event shared=a;$display("%0d",shared);',
                "automatic event originals[0:1];automatic event shared[0:1]='{originals[0],originals[1]};shared='{originals[1],originals[0]};",
            )
            for statement in statements:
                ret = 'void ' if home == 'function' else ''
                body = (f'initial begin {statement} end' if home == 'initial'
                        else f'{home} automatic {ret}run();{statement}end{home} initial run();')
                source.write_text('module top;event a,b;\n' + body + '\nendmodule\n')
                result = subprocess.run([binary, source.name], cwd=directory, capture_output=True,
                                        text=True, timeout=15, check=False)
                assert result.returncode == 2 and not result.stdout, result.stdout + result.stderr
                message = ("value of type 'event' is invalid for '%d' format specifier"
                           if statement.endswith('"%0d",shared);') else
                           'unsupported: event used as a value')
                assert message in result.stderr, result.stderr
                assert re.search(r'probe\.sv:2:\d+: error:', result.stderr), result.stderr
                assert 'internal error' not in result.stderr, result.stderr
                checked += 1
        for home in ('initial', 'task'):
            statement = 'automatic event shared=null;@(shared);'
            body = ('initial begin ' + statement + ' end' if home == 'initial' else
                    'task automatic run();' + statement + 'endtask initial run();')
            source.write_text('module top;\n' + body + '\nendmodule\n')
            result = subprocess.run([binary, source.name], cwd=directory, capture_output=True,
                                    text=True, timeout=15, check=False)
            assert result.returncode == 2 and not result.stdout, result.stdout + result.stderr
            assert 'wait on a null event' in result.stderr, result.stderr
            assert re.search(r'probe\.sv:2: error:', result.stderr), result.stderr
            assert 'internal error' not in result.stderr, result.stderr
            checked += 1
        declarations = (
            ('automatic event original;automatic event shared=original;automatic event forwarded=shared;', 1),
            ('automatic event shared=null;', 0),
            ("automatic event originals[0:1];automatic event shared[1:0]='{originals[0],originals[1]};", 2),
            ("automatic event shared[-1:1]='{default:null};", 0),
        )
        for declaration, constructors in declarations:
            source.write_text('module top;initial begin ' + declaration + ' end endmodule\n')
            result = subprocess.run([binary, '--dump-sir', source.name], cwd=directory,
                                    capture_output=True, text=True, timeout=15, check=False)
            assert result.returncode == 0, result.stderr
            assert result.stdout.count('event.new') == constructors, result.stdout
    print(f'{checked} positioned event-initialization boundaries and 4 constructor-count designs checked')
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
