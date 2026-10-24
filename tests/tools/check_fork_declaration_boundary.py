#!/usr/bin/env python3
# Copyright (c) 2026 The kirimasim Authors
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

"""Parent declarations execute; unreviewed automatic fork capture stays rejected."""

import argparse
from pathlib import Path
import re
import subprocess
import tempfile


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--kirimasim', required=True)
    args = parser.parse_args()
    binary = str(Path(args.kirimasim).resolve())
    for join in ('join', 'join_any', 'join_none'):
        source = f'''module top;
task automatic run(input int value);
fork
  int payload=value;
  begin #1;$display("%0d",payload);end
{join}
endtask
initial begin run(7);#4;end
endmodule
'''
        with tempfile.TemporaryDirectory() as folder:
            Path(folder, 'probe.sv').write_text(source)
            result = subprocess.run([binary, 'probe.sv'], cwd=folder, capture_output=True,
                                    text=True, timeout=15, check=False)
        errors = [line for line in result.stderr.splitlines() if ': error:' in line]
        pattern = r"(?:.*[/\\])?probe\.sv:5:[0-9]+: error: unsupported: automatic variable 'payload' here \(not implemented yet\)"
        if result.returncode != 2 or result.stdout or len(errors) != 1 or not re.fullmatch(pattern, errors[0]):
            print(f'FAIL: automatic fork capture boundary for {join}')
            print(result.stdout + result.stderr)
            return 1
        accepted = source.replace('task automatic run(input int value);',
                                  'int calls;function int mark(input int value);calls++;return value;endfunction\n'
                                  'task automatic run(input int value);')
        accepted = accepted.replace('int payload=value;', 'int payload=mark(value);')
        accepted = accepted.replace('$display("%0d",payload)', '/* no capture */')
        accepted = accepted.replace('run(7);#4;', 'run(7);#4;$display("%0d",calls);')
        with tempfile.TemporaryDirectory() as folder:
            Path(folder, 'probe.sv').write_text(accepted)
            result = subprocess.run([binary, 'probe.sv'], cwd=folder, capture_output=True,
                                    text=True, timeout=15, check=False)
        if result.returncode != 0 or result.stdout != '1\n' or ': error:' in result.stderr:
            print(f'FAIL: uncaptured parent initialization for {join}')
            print(result.stdout + result.stderr)
            return 1
    print('checked three positioned automatic-capture rejections and three parent-initialization acceptances')
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
