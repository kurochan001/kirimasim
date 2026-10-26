#!/usr/bin/env python3
# Copyright (c) 2026 The kirimasim Authors
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

"""Class-runtime contract 2: storage identities, strict bounds and initializer control."""

import argparse
from pathlib import Path
import re
import subprocess
import tempfile


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--kirimasim', type=Path, required=True)
    binary = str(parser.parse_args().kirimasim.resolve())
    with tempfile.TemporaryDirectory(prefix='kirimasim-class-static-') as directory:
        source = Path(directory, 'probe.sv')

        def run(text, dump=False):
            source.write_text(text + '\n')
            return subprocess.run([binary] + (['--dump-sir'] if dump else []) + [source.name],
                                  cwd=directory, capture_output=True, text=True, timeout=15,
                                  check=False)

        boundaries = (
            ('class Box;local static int data=3;endclass\n'
             'module top;initial $display("%0d",Box::data);endmodule', "a local member of 'Box'"),
            ('class Box;protected static int data=3;endclass\n'
             'module top;initial $display("%0d",Box::data);endmodule', "a protected member of 'Box'"),
            ('class Box;const static int data=3;endclass\n'
             'module top;initial Box::data=5;endmodule', "cannot assign to read-only variable 'data'"),
            ('class Box;int data=3;endclass\n'
             'module top;initial $display("%0d",Box::data);endmodule', 'non-static class property'),
            ('class Box;int data=3;endclass\nmodule top;Box b;initial b=new;endmodule',
             "unsupported: type 'Box'"),
            ('class Box#(int N=1);static int data=N;endclass\n'
             'module top;initial $display("%0d",Box#(3)::data);endmodule',
             'unsupported: static property of a parameterized class'),
            ('package history;int calls=0;function automatic int mark();++calls;return 7;endfunction endpackage\n'
             'class Box#(int N=1);static int data=history::mark();endclass\n'
             'typedef Box#(3) Bound;module top;initial $display("%0d",history::calls);endmodule',
             'unsupported: static storage of a parameterized class'),
            ('class Box;static int data=mark();static function int mark();'
             'fork #1 $display("later");join_none return 1;endfunction endclass\n'
             'module top;initial $display("unreached");endmodule', 'schedules work that runs after it returns'),
            ('class Box;static int data=3;endclass\nmodule top;initial $dumpvars(0,Box::data);endmodule',
             'expected module or variable name'),
        )
        for text, message in boundaries:
            result = run(text)
            assert result.returncode == 2 and not result.stdout, result.stdout + result.stderr
            assert message in result.stderr, result.stderr
            assert re.search(r'probe\.sv:\d+:\d+: error:', result.stderr), result.stderr
            assert 'internal error' not in result.stderr, result.stderr

        result = run('class Box#(int N=1);static function int value();return N;endfunction endclass\n'
                     'module top;initial $display("%0d",Box#(7)::value());endmodule')
        assert result.returncode == 0 and result.stdout == '7\n', result.stdout + result.stderr

        base = 'class Base;static int data=3;int per_object=9;endclass\n'
        for hidden in (False, True):
            text = (base + 'class Child extends Base;' + ('static int data=5;' if hidden else '') +
                    'endclass\ntypedef Child Alias;\n'
                    'module top;initial $display("%0d %0d",Alias::data,Base::data);endmodule')
            result = run(text, dump=True)
            assert result.returncode == 0, result.stderr
            slots = re.findall(r'^slot %s\d+ "([^"]+)"', result.stdout, re.MULTILINE)
            assert slots == (['Base::data', 'Child::data'] if hidden else ['Base::data']), result.stdout
            assert result.stdout.count('kind=static-init') == 1, result.stdout
            assert 'per_object' not in result.stdout, result.stdout

        result = run('class Box;static int first=Box::second;static int second=7;endclass\n'
                     'module top;initial $display("%0d %0d",Box::first,Box::second);endmodule')
        assert result.returncode == 0 and result.stdout == '0 7\n', result.stdout + result.stderr
        assert re.search(r'probe\.sv:1:\d+: warning:', result.stderr), result.stderr
        assert "the initial value of 'first' reads 'Box::second'" in result.stderr, result.stderr

        for stop, status in (('$fatal(1,"static-fail");', 1), ('$finish;', 0)):
            result = run('class Box;static int data=stop();static function int stop();' + stop +
                         'return 7;endfunction endclass\nmodule top;initial $display("unreached");endmodule')
            assert result.returncode == status and not result.stdout, result.stdout + result.stderr
            if status:
                assert 'probe.sv:1: fatal: static-fail' in result.stderr, result.stderr

    print('9 strict boundaries, generic method, 2 storage identities, order warning and 2 initializer stops checked')
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
