#!/usr/bin/env python3
# Copyright (c) 2026 The kirimasim Authors
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

"""Original checks for NBAs without a delay to fixed-array and packed
reference targets: an element or bits of the static variable a `ref static`
argument names (13.5.2) take a nonblocking assignment (10.4.2), applied in
the NBA region of the same time slot, or the Re-NBA region from a program."""

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
    # Elements: the selected element changes in the NBA region, not before;
    # the indices are taken when the NBA is issued.
    for width in (1, 32, 65, 129):
        for four in (False, True):
            dtype = f'{"logic" if four else "bit"} [{width-1}:0]'
            pattern = "'z" if four else "'1"
            for shape, indices, selected, other in (
                    ('[1:3]', '[i]', '[1]', '[3]'),
                    ('[3:1]', '[i]', '[1]', '[3]'),
                    ('[2:1][-1:0]', '[i][j]', '[1][0]', '[2][-1]')):
                source = module(f'''typedef {dtype} T;
T values{shape};
task automatic issue(ref static T r{shape});
  int i=1,j=0; T data={pattern};
  r{indices}<=data;
  i=3;j=-1;data='0;
  if(r{selected}!==T'(0)) $fatal(1,"write before the NBA region");
endtask
''', f'''values='{{default:'0}}; issue(values);
#0; if(values{selected}!==T'(0)) $fatal(1,"write before the NBA region");
#1; if(values{selected}!==T'({pattern}) || values{other}!==T'(0))
  $fatal(1,"captured element");''')
                result.append((f'array_{width}_{four}_{len(result)}', source))
    # Bits: every position from wholly below to wholly above the parent; the
    # valid bits change, the others keep their values (VALUE_SEMANTICS.md 3.11).
    for width in (1, 8, 65, 129):
        for four in (False, True):
            for parent in (0, 1):
                for part in sorted({1, min(13, width)}):
                    kind = 'logic' if four else 'bit'
                    result.append((f'bits_{width}_{four}_{parent}_{part}', module(f'''
typedef {kind} [1:0][{width-1}:0] T;
T value;
task automatic issue(ref static T r,input int position);
  {kind} [{part-1}:0] data='0;
  r[{parent}][position+:{part}]<=data;
  position=0;data='1;
endtask
''', f'''{kind} [{2*width-1}:0] expected;
for(int position=-{part}-1;position<={width}+1;position++) begin
  value='1;expected='1;
  for(int b=0;b<{part};b++)
    if(position+b>=0 && position+b<{width}) expected[{parent*width}+position+b]=0;
  issue(value,position);#0;if(value!=='1) $fatal(1,"early packed write");
  #1;if(value!==expected) $fatal(1,"bounded packed write");
end''')))
    base = result[-1][1]
    result.append(('program', base.replace('module top;', 'program runner;')
                   .replace('endmodule', 'endprogram\nmodule top; runner p(); endmodule')))
    result.append(('interface', base.replace('module top;', 'interface runner;')
                   .replace('endmodule', 'endinterface\nmodule top; runner p(); endmodule')))
    result.append(('generate', base.replace('initial begin', 'if (1) begin : generated\ninitial begin')
                   .replace('endmodule', 'end\nendmodule')))
    result.append(('function', base.replace('task automatic issue', 'function automatic void issue')
                   .replace('endtask', 'endfunction')))
    result.append(('subarray_forwarding', module('''
int values[2][3:1];
task automatic inner(ref static int r[-1:1]); r[1]<=7; endtask
task automatic outer(ref static int r[3:1]); inner(r); endtask
''', 'outer(values[1]);#1;if(values[1][1]!=7 || values[1][3]!=0 || values[0][1]!=0) $fatal(1,"formal bounds and base");')))
    result.append(('array_packed_parent_and_neighbours', module('''
typedef logic[1:0][64:0] T; T values[2][3:1];
task automatic inner(ref static T r[-1:1]);
  int index=1,position=61;
  r[index][1][position+:8]<=8'h5a;
  index=0;position=0;
endtask
task automatic outer(ref static T r[3:1]); inner(r); endtask
''', "values='{default:'{default:'0}};outer(values[1]);values[1][1][0]='1;#1;if(values[1][1][1]!== (65'ha<<61) || values[1][1][0]!=='1 || values[0][1]!=='0 || values[1][2]!=='0) $fatal(1,\"parent and current neighbours\");")))
    # The NBAs of one time slot apply in the order they were issued, those
    # through a ref with the others (SCHEDULER_SPEC.md 13).
    result.append(('issue_order', module('''
int values[3:1];
task automatic issue(ref static int r[3:1]);
  r[2]<=17;values[2]<=19;r[1]<=23;
endtask
''', 'values[1]<=29;issue(values);values[3]<=31;#1;if(values[2]!=19 || values[1]!=23 || values[3]!=31) $fatal(1,"issue order");')))
    result.append(('whole_array_and_element', module('''
int values[3:1];
task automatic issue(ref static int r[3:1]);
  int data[3:1]='{7,11,13};r<=data;data='{99,101,103};
  r[2]<=17;disable issue;
endtask
''', 'issue(values);#1;if(values[3]!=7 || values[2]!=17 || values[1]!=13) $fatal(1,"whole array and order");')))
    result.append(('invalid_indices_and_positions', module('''
logic[128:0] values[2];
task automatic issue(ref static logic[128:0] r[2]);
  logic[128:0] bad='x,huge=129'b1<<100;int outside=-1;
  r[bad]<='1;r[huge]<='1;r[outside]<='1;
  r[1][bad]<=1'b1;r[1][huge]<=1'b1;
endtask
''', "values='{default:'0};issue(values);#1;if(values[0]!=='0 || values[1]!=='0) $fatal(1,\"invalid selection\");")))
    result.append(('rhs_and_selector_once', module('''
int values[2],order;
function automatic int rhs();order=order*10+1;return 7;endfunction
function automatic int selected();order=order*10+3;return 1;endfunction
task automatic issue(ref static int r[2]);r[selected()]<=rhs();endtask
''', 'issue(values);if(order!=13) $fatal(1,"capture order");#1;if(values[1]!=7 || values[0]!=0 || order!=13) $fatal(1,"captured once");')))
    result.append(('container_values_and_roots', module('''
typedef int Q[$];typedef int D[];Q queues[2];D arrays[2];
task automatic putq(ref static Q r[2]);Q data={7,11};r[1]<=data;data[0]=99;endtask
task automatic putd(ref static D r[2]);D data=new[2];data[0]=13;data[1]=17;r[1]<=data;data[0]=101;endtask
task automatic churn();Q work={31,37};work.push_back(41);endtask
''', 'putq(queues);putd(arrays);for(int n=0;n<4000;n++) churn();#1;if(queues[1].size()!=2 || queues[1][0]!=7 || queues[1][1]!=11 || arrays[1].size()!=2 || arrays[1][0]!=13 || arrays[1][1]!=17 || queues[0].size()!=0 || arrays[0].size()!=0) $fatal(1,"typed roots");')))
    result.append(('nonintegral_elements', module('''
real numbers[2];shortreal small_numbers[2];string texts[2];chandle handles[2];
task automatic a(ref static real r[2]);r[1]<=1.5;endtask
task automatic b(ref static shortreal r[2]);r[1]<=2.25;endtask
task automatic c(ref static string r[2]);string text="saved";r[1]<=text;text="changed";endtask
task automatic d(ref static chandle r[2]);r[1]<=null;endtask
''', 'a(numbers);b(small_numbers);c(texts);d(handles);#1;if(numbers[1]!=1.5 || small_numbers[1]!=2.25 || texts[1]!="saved" || handles[1]!=null || texts[0]!="") $fatal(1,"nonintegral leaves");')))
    # From a program, the update applies in the Re-NBA region, after the NBA
    # region of the same slot (4.4.2.9).
    result.append(('program_origin_re_nba', '''`timescale 1ns/1ns
module top;
  logic[15:0] values[2];runner p();
  initial begin values='{default:'0};#1;values[1]<=16'hffff;#1;
    if(values[1]!==16'hff5a || values[0]!==16'b0) $fatal(1,"Re-NBA origin");$display("passed");end
endmodule
program runner;
  task automatic issue(ref static logic[15:0] r[2]);r[1][0+:8]<=8'h5a;endtask
  initial begin #1;issue(top.values);#1;end
endprogram
'''))
    result.append(('packed_structure_member', module('''
typedef struct packed {logic[7:0] tag;logic[64:0] payload;logic[7:0] suffix;} T;T value;
task automatic issue(ref static T r);int position=-2;r.payload[position+:13]<=13'h1a5b;endtask
''', "value='0;value.tag=17;value.suffix=34;issue(value);value.suffix=51;#1;if(value.payload!==65'h696 || value.tag!=17 || value.suffix!=51) $fatal(1,\"packed member offset\");")))
    for direction in ('+', '-'):
        result.append((f'ascending_parent_{direction}', module(f'''
typedef logic[0:1][0:64] T;T value;
task automatic issue(ref static T r,input int position);r[1][position{direction}:13]<=13'h1a5b;endtask
''', f'''T expected;automatic logic[12:0] data=13'h1a5b;
for(int position=-2;position<=67;position++) begin
  value='1;expected='1;
  for(int b=0;b<13;b++) begin
    automatic int index=position{direction}b;
    if(index>=0 && index<=64) expected[1][index]=data[{"12-b" if direction=="+" else "b"}];
  end
  issue(value,position);#1;if(value!==expected) $fatal(1,"ascending parent");
end''')))
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument('--kirimasim', required=True, type=Path)
    binary = str(parser.parse_args().kirimasim.resolve())
    failures, checks = [], 0
    with tempfile.TemporaryDirectory(prefix='kirimasim-ref-nba-selection-') as directory:
        root = Path(directory)
        for name, source in cases():
            path = root / (name + '.sv')
            path.write_text(source)
            for flags in (('parse',), ('--dump-sir',), (), ('--init-values=zero',),
                          ('--init-values=one',), ('--init-values=random', '--seed=61')):
                run = bounded_run.run([binary, *flags, '--max-steps=3000000', str(path)],
                                      cwd=root, timeout=20)
                if (run.returncode or re.search(r': (?:error|fatal|warning):', run.stderr) or
                        (flags[:1] not in (('parse',), ('--dump-sir',)) and run.stdout != 'passed\n') or
                        (flags == ('--dump-sir',) and
                         not re.search(r'nba\.ref\.(?:elem|bits) ', run.stdout))):
                    failures.append(f'{name} {flags}: exit={run.returncode}\n{run.stdout}\n{run.stderr}')
                checks += 1
    print(f'ref-nba-selection: {checks} checks; {len(failures)} failures')
    for failure in failures:
        print(failure)
    return bool(failures)


if __name__ == '__main__':
    try:
        raise SystemExit(main())
    except (bounded_run.OutputLimit, subprocess.TimeoutExpired) as error:
        print(f'FAIL: {error}', file=sys.stderr)
        raise SystemExit(1)
