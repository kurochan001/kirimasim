#!/usr/bin/env python3
# Copyright (c) 2026 The kirimasim Authors
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

"""Check source event-NBA's single-control and language-process boundary."""

import argparse
from pathlib import Path
import re
import subprocess
import tempfile

SOURCE = '''module top;
typedef struct {int a; int b; logic [128:0] data;} record_t;
record_t q, src;
event tick;
function automatic bit gate(); int work=0; work++; return work==1; endfunction
task automatic issue(); q <= repeat(2) @(tick iff gate()) src; endtask
initial begin
q='{0,0,0}; src='{7,11,129'h10000000000000000000000000000000d};
issue(); src='{0,0,0};
->tick; ->tick; #1;
$display("value %0d %0d %h",q.a,q.b,q.data);
end
endmodule
'''


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--kirimasim', type=Path, required=True)
    binary = str(parser.parse_args().kirimasim.resolve())
    with tempfile.TemporaryDirectory(prefix='kirimasim-event-nba-') as directory:
        source = Path(directory, 'probe.sv')
        source.write_text(SOURCE)
        dump = subprocess.run([binary, '--dump-sir', source.name], cwd=directory,
                              capture_output=True, text=True, timeout=15, check=False)
        assert dump.returncode == 0, dump.stderr
        layouts = re.findall(r'^nba\.layout !n\d+ .*?^}', dump.stdout, re.M | re.S)
        assert len(layouts) == 1, dump.stdout
        assert len(re.findall(r'^  event\.', layouts[0], re.M)) == 1, layouts[0]
        assert len(re.findall(r'^  write\.', layouts[0], re.M)) == 3, layouts[0]
        assert len(re.findall(r'^process ', dump.stdout, re.M)) == 1, dump.stdout
        assert not re.search(r'^  (?:fork|wait\.event|wait\.repeat)', dump.stdout, re.M), dump.stdout
        # The positive and skipped-count branches select the same complete layout.
        instructions = re.findall(r'^  nba\.event (!n\d+),', dump.stdout, re.M)
        assert instructions and set(instructions) == {'!n0'}, dump.stdout
        callback = re.search(r'^subroutine .*"top\.issue\.iff\d+" ([^{]+)\{', dump.stdout, re.M)
        assert callback is not None, dump.stdout
        assert 'function automatic ' in callback[1], callback[0]
        assert 'frame-of=' not in callback[1] and 'may-wait' not in callback[1], callback[0]
        run = subprocess.run([binary, '--max-procs=1', source.name], cwd=directory,
                             capture_output=True, text=True, timeout=15, check=False)
        assert run.returncode == 0, run.stderr
        assert run.stdout == 'value 7 11 10000000000000000000000000000000d\n', run.stdout
        assert 'error:' not in run.stderr and 'warning:' not in run.stderr, run.stderr
    print('One event control owns three writes; callback is detached; no extra language process')
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
