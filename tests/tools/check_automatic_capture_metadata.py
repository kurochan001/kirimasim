#!/usr/bin/env python3
# Copyright (c) 2026 The kirimasim Authors
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

"""Check full lexical imports and strict ref boundaries from original source."""

import argparse
from pathlib import Path
import re
import subprocess
import tempfile


SOURCE = '''module top;
task automatic run(input int seed);
  string unread_text="kept";
  int unread_queue[$]='{7};
  real unread_real=2.5;
  shortreal unread_short=1.5;
  chandle unread_handle=null;
  begin: outer
    logic [128:0] unread_array[-1:0];
    begin
      fork
        begin
          fork begin #1;$display("leaf %0d",seed);end join_none
        end
      join_none
    end
  end
endtask
initial begin run(7);#4;end
endmodule
'''


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--kirimasim', required=True, type=Path)
    binary = str(parser.parse_args().kirimasim.resolve())
    with tempfile.TemporaryDirectory() as folder:
        path = Path(folder, 'probe.sv')
        path.write_text(SOURCE)
        def run(*options):
            return subprocess.run([binary, *options, path.name], cwd=folder,
                                  capture_output=True, text=True, timeout=15, check=False)
        result = run('--dump-sir')
        assert result.returncode == 0, result.stderr
        bodies = re.split(r'(?m)^(?=subroutine @|process @)', result.stdout)[1:]
        owner, _, child, leaf = bodies
        scopes = re.findall(r'^  activation #(\d+).*fields=\((.*?)\)$', owner, re.M)
        assert len(scopes) == 4, scopes
        assert sum(not fields for _, fields in scopes) == 2, scopes
        names = ['seed', 'unread_text', 'unread_queue', 'unread_real',
                 'unread_short', 'unread_handle', 'unread_array']
        for body in (child, leaf):
            for name in names:
                assert re.search(rf'^  frame .* "{name}" : .*home=capture%f\d+$', body, re.M), body
        assert 'capture.extents #0 #1 #2 #3\n' in child, child
        assert 'capture.extents #0 #1 #2 inherited\n' in leaf, leaf
        result = run()
        assert result.returncode == 0 and result.stdout == 'leaf 7\n', result.stdout+result.stderr
        rejected = []
        for join in ('join_any', 'join_none'):
            rejected.append(f'''module top;int actual;
task automatic run(ref int value);fork begin #1;value=7;end {join} endtask
initial begin run(actual);#3;end endmodule
''')
        rejected += ['''module top;
task automatic run(ref static int value);value=7;endtask
initial begin automatic int actual;run(actual);end endmodule
''', '''module top;int actual;
task automatic run(const ref static int value);fork begin #1;value=7;end join_none endtask
initial begin run(actual);#3;end endmodule
''', '''module top;
task automatic run();event unread;int value=7;
fork begin #1;$display("%0d",value);end join_none endtask
initial begin run();#3;end endmodule
''']
        for source in rejected:
            path.write_text(source)
            result = run()
            assert result.returncode == 2 and not result.stdout, result.stdout+result.stderr
            assert re.search(r'probe\.sv:\d+:\d+: error:', result.stderr), result.stderr
            assert 'internal error' not in result.stderr, result.stderr
    print('full typed lexical imports, empty extents, descendants and five strict boundaries checked')
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
