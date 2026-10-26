#!/usr/bin/env python3
# Copyright (c) 2026 The kirimasim Authors
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

"""Input sharing must not enable event rebinding, aliases or comparisons."""

import argparse
from pathlib import Path
import re
import subprocess
import tempfile


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--kirimasim', required=True, type=Path)
    binary = str(parser.parse_args().kirimasim.resolve())
    checked = 0
    with tempfile.TemporaryDirectory(prefix='kirimasim-event-input-') as directory:
        source = Path(directory, 'probe.sv')
        cases = []
        for home in ('initial', 'task', 'function'):
            for statement in ('a=b;', '$display("%0d",a==b);', 'automatic event shared=a;'):
                body = statement
                if home == 'initial':
                    body = 'initial begin ' + body + ' end'
                else:
                    ret = 'void ' if home == 'function' else ''
                    body = f'{home} automatic {ret}run();{body}end{home} initial run();'
                cases.append(('module top;event a,b;\n' + body + '\nendmodule\n',
                              'unsupported: event used as a value', r'probe\.sv:2:\d+: error:'))
        for direction in ('output', 'inout'):
            for dimensions in ('', '[0:1]'):
                cases.append((f'module top;event a{dimensions};\n'
                              f'task automatic run({direction} event value{dimensions});endtask\n'
                              'initial run(a);endmodule\n',
                              'unsupported: automatic event argument', r'probe\.sv:2:\d+: error:'))
        cases.append(('module top;\ntask automatic run(input event value);@(value);endtask\n'
                      'initial run(null);endmodule\n', 'wait on a null event', r'probe\.sv:2: error:'))
        for call in ('seen(root)', 'nested(root)'):
            for sensitivity in ('always_comb', 'always_latch', 'always @*', 'assign'):
                decl = 'wire result;' if sensitivity == 'assign' else 'bit result;'
                cases.append((f'module top;event root;{decl}\n'
                              'function automatic bit seen(input event value);return value.triggered;endfunction\n'
                              'function automatic bit nested(input event value);return seen(value);endfunction\n'
                              f'{sensitivity} result={call};\nendmodule\n',
                              'implicit sensitivity to named-event triggered values',
                              r'probe\.sv:4:\d+: error:'))
        for text, message, position in cases:
            source.write_text(text)
            result = subprocess.run([binary, source.name], cwd=directory, capture_output=True,
                                    text=True, timeout=15, check=False)
            assert result.returncode == 2 and not result.stdout, result.stdout + result.stderr
            assert message in result.stderr, result.stderr
            assert re.search(position, result.stderr), result.stderr
            assert 'internal error' not in result.stderr, result.stderr
            checked += 1
        for index, after in (('0', 0), ('index++', 1)):
            source.write_text(f'''module top;
typedef int row_t[3:2];typedef row_t matrix_t[-1:1];
int calls=0,index=0;
function automatic matrix_t make();
  matrix_t value;++calls;value[0][3]=17;value[0][2]=29;return value;
endfunction
task automatic take(input int value[0:1]);
  #1;$display("calls=%0d index=%0d values=%0d,%0d",calls,index,value[0],value[1]);
endtask
initial take(make()[{index}]);
endmodule
''')
            result = subprocess.run([binary, source.name], cwd=directory, capture_output=True,
                                    text=True, timeout=15, check=False)
            assert result.returncode == 0, result.stderr
            assert result.stdout == f'calls=1 index={after} values=17,29\n', result.stdout
            assert ': error:' not in result.stderr, result.stderr
    print(f'{checked} positioned event-input policy boundaries and 2 effectful aggregate bases checked')
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
