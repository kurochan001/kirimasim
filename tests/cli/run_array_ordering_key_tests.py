#!/usr/bin/env python3
# Copyright (c) 2026 The kirimasim Authors
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

"""Original diagnostics for the approved initial pure-ordering-key boundary."""

import argparse
import re
import subprocess
import tempfile
from pathlib import Path


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--kirimasim', type=Path, required=True)
    args = parser.parse_args()
    executable = args.kirimasim.resolve()
    count = 0
    with tempfile.TemporaryDirectory(prefix='kirimasim-order-key-') as directory:
        source = Path(directory) / 'key.sv'
        for shape in ('[2:0]', '[]', '[$]'):
            for context in ('initial', 'automatic', 'ref'):
                for method in ('sort', 'rsort'):
                    for effectful in (False, True):
                        action = f'a.{method}() with (key(item));'
                        declaration = f'int a{shape};'
                        if context == 'initial':
                            run = declaration + '\ninitial begin\n' + action + '\nend'
                        elif context == 'automatic':
                            run = 'task automatic run();\n' + declaration + '\n' + action + '\nendtask\ninitial run();'
                        else:
                            run = declaration + f'\ntask automatic run(ref int a{shape});\n' + action + '\nendtask\ninitial run(a);'
                        program = ('module top;\nint calls;\nfunction int key(input int value);\n'
                                   + ('calls++;\n' if effectful else '')
                                   + 'return value;\nendfunction\n' + run + '\nendmodule\n')
                        source.write_text(program)
                        parsed = subprocess.run([str(executable), 'parse', str(source)],
                                                capture_output=True, text=True, timeout=15)
                        assert parsed.returncode == 0, parsed.stderr
                        compiled = subprocess.run([str(executable), str(source)],
                                                  capture_output=True, text=True, timeout=15)
                        line = program.splitlines().index(action) + 1
                        message = (f'{re.escape(str(source))}:{line}:1: error: unsupported: '
                                   'ordering with an effectful or unverified-pure key')
                        assert compiled.returncode == 2, compiled.stderr
                        assert not compiled.stdout, compiled.stdout
                        assert re.search(message, compiled.stderr), compiled.stderr
                        count += 1
    print(f'array ordering keys: {count} explicit diagnostic cases passed')


if __name__ == '__main__':
    main()
