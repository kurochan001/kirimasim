#!/usr/bin/env python3
# Copyright (c) 2026 The kirimasim Authors
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

"""Original source checks for bounded packed parents through event-NBA refs."""

import argparse
from pathlib import Path
import re
import subprocess
import sys
import tempfile

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / 'tools'))
import bounded_run  # noqa: E402


def module(declarations, body):
    return ('`timescale 1ns/1ns\nmodule top;\n' + declarations + '\ninitial begin\n'
            + body + '\n$display("passed"); end\nendmodule\n')


def cases():
    result = []
    for parent_width in (1, 8, 65, 129):
        for four in (False, True):
            for parent_index in (0, 1):
                for child_width in sorted({1, min(13, parent_width)}):
                    scalar = 'logic' if four else 'bit'
                    data = "'x" if four else "'0"
                    low = parent_index * parent_width
                    source = module(f'''
typedef {scalar} [1:0][{parent_width-1}:0] T;
T value;
event go;
task automatic issue(ref static T r,input int position);
  automatic {scalar} [{child_width-1}:0] data={data};
  r[{parent_index}][position+:{child_width}]<=@(go) data;
  position=0; data='1;
endtask
''', f'''
{scalar} [{2*parent_width-1}:0] expected;
automatic {scalar} [{child_width-1}:0] data={data};
for (int position=-{child_width}-2;position<={parent_width}+2;position++) begin
  value='1; expected='1;
  for (int bit_number=0;bit_number<{child_width};bit_number++)
    if (position+bit_number>=0 && position+bit_number<{parent_width})
      expected[{low}+position+bit_number]=data[bit_number];
  issue(value,position); ->go; #1;
  if (value!==expected) $fatal(1,"packed parent clipping");
end
''')
                    result.append((f'packed_{parent_width}_{four}_{parent_index}_{child_width}', source))

    base = result[-1][1]
    result.append(('program', base.replace('module top;', 'program runner;')
                   .replace('endmodule', 'endprogram\nmodule top; runner p(); endmodule')))
    result.append(('interface', base.replace('module top;', 'interface runner;')
                   .replace('endmodule', 'endinterface\nmodule top; runner p(); endmodule')))
    result.append(('generate', base.replace('initial begin', 'if (1) begin : generated\ninitial begin')
                   .replace('endmodule', 'end\nendmodule')))
    result.append(('function', base.replace('task automatic issue', 'function automatic void issue')
                   .replace('endtask', 'endfunction')))
    result.append(('unknown_and_wide_positions', module('''
typedef logic [1:0][7:0] T;
T value='1; event go;
task automatic issue(ref static T r);
  logic [128:0] unknown_position='z;
  logic [128:0] huge_position=129'b1<<100;
  r[0][unknown_position]<=repeat(2) @(go) 1'b0;
  r[1][huge_position]<=@(go) 1'b0;
endtask
''', "issue(value); ->go; ->go; #1; if (value!==16'hffff) $fatal(1,\"invalid position\");")))
    result.append(('structure_members_and_nested_dimensions', module('''
typedef struct packed {logic [16:0] leading; logic [1:0][64:0] words; logic [12:0] trailing;} T;
T value; event go;
task automatic issue(ref static T r,input int position);
  r.words[1][position+:13]<=@(go) 13'bzx01xz10zx01x;
endtask
''', '''
logic [159:0] expected;
automatic logic [12:0] data=13'bzx01xz10zx01x;
for (int position=-15;position<=68;position++) begin
  value='1; expected='1;
  for (int n=0;n<13;n++)
    if (position+n>=0 && position+n<65) expected[78+position+n]=data[n];
  issue(value,position); ->go; #1;
  if (value!==expected) $fatal(1,"structure window offset");
end
''')))
    result.append(('array_refs_forwarding_and_return', module('''
typedef logic [1:0][64:0] T;
T values[2][3:1]; event go;
task automatic inner(ref static T r[-1:1]);
  int index=1,position=-2;
  r[index][1][position+:13]<=@(go) 13'h1a5b;
  index=-1;position=0;
endtask
task automatic outer(ref static T r[3:1]); inner(r); endtask
''', '''
automatic logic [129:0] expected='1;
automatic logic [12:0] data=13'h1a5b;
values='{default:'{default:'1}};
for (int n=2;n<13;n++) expected[65-2+n]=data[n];
outer(values[1]); ->go; #1;
if (values[1][1]!==expected || values[0][1]!=={130{1'b1}} || values[1][3]!=={130{1'b1}})
  $fatal(1,"array base and window");
''')))
    result.append(('nested_parent_dimensions', module('''
typedef logic [2:1][-1:-3][5:0] T;
T value; event go;
task automatic issue(ref static T r,input int position); r[2][-2][position+:4]<=@(go) 4'bx10z; endtask
''', '''
logic [35:0] expected;
automatic logic [3:0] data=4'bx10z;
for (int position=-6;position<=8;position++) begin
  value='1;expected='1;
  for (int n=0;n<4;n++) if (position+n>=0 && position+n<6) expected[24+position+n]=data[n];
  issue(value,position); ->go; #1;
  if (value!==expected) $fatal(1,"nested packed dimensions");
end
''')))
    result.append(('shared_countdown_and_issue_order', module('''
typedef logic [1:0][7:0] T;
T value='1; event go;
task automatic issue(ref static T r);
  int position=2;
  {r[0][position],r[1][position]}<=repeat(2) @(go or go) 2'b00;
  position=7;
endtask
''', '''
issue(value); ->go; #1;
if (value!==16'hffff) $fatal(1,"shared countdown");
value<=16'hffff; ->go; #1;
if (value!==16'hffff) $fatal(1,"pending issue order");
''')))
    result.append(('ascending_packed_ranges', module('''
typedef logic [0:1][-3:61] T;
T value; event go;
task automatic issue(ref static T r,input int position); r[0][position+:13]<=@(go) 13'h1a5b; endtask
''', '''
T expected;
automatic logic [12:0] data=13'h1a5b;
for (int position=-18;position<=66;position++) begin
  value='1;expected='1;
  for (int n=0;n<13;n++)
    if (position+n>=-3 && position+n<=61) expected[0][position+n]=data[12-n];
  issue(value,position); ->go; #1;
  if (value!==expected) $fatal(1,"ascending packed ranges");
end
''')))
    result.append(('downward_indexed_part', module('''
typedef logic [1:0][64:0] T;
T value; event go;
task automatic issue(ref static T r,input int position); r[1][position-:13]<=@(go) 13'h1a5b; endtask
''', '''
logic [129:0] expected;
automatic logic [12:0] data=13'h1a5b;
for (int position=-15;position<=80;position++) begin
  value='1;expected='1;
  for (int n=0;n<13;n++)
    if (position-n>=0 && position-n<65) expected[65+position-n]=data[12-n];
  issue(value,position); ->go; #1;
  if (value!==expected) $fatal(1,"downward packed part");
end
''')))
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--kirimasim', required=True, type=Path)
    binary = str(parser.parse_args().kirimasim.resolve())
    failures = []
    checks = 0
    with tempfile.TemporaryDirectory(prefix='kirimasim-nba-ref-window-') as directory:
        root = Path(directory)
        for name, source in cases():
            path = root / (name + '.sv')
            path.write_text(source)
            for flags in (('parse',), ('--dump-sir',), (), ('--init-values=zero',),
                          ('--init-values=one',), ('--init-values=random', '--seed=53')):
                result = bounded_run.run([binary, *flags, '--max-steps=2000000', str(path)],
                                         cwd=root, timeout=20)
                if (result.returncode or re.search(r': (?:error|fatal|warning):', result.stderr) or
                        (flags[:1] not in (('parse',), ('--dump-sir',)) and result.stdout!='passed\n') or
                        (flags==('--dump-sir',) and ' parent=' not in result.stdout)):
                    failures.append(f'{name} {flags}: exit={result.returncode}\n{result.stdout}\n{result.stderr}')
                checks += 1
    print(f'event-nba-ref-window: {checks} checks; {len(failures)} failures')
    for failure in failures:
        print(failure)
    return bool(failures)


if __name__ == '__main__':
    try:
        raise SystemExit(main())
    except (bounded_run.OutputLimit, subprocess.TimeoutExpired) as error:
        print(f'FAIL: {error}', file=sys.stderr)
        raise SystemExit(1)
