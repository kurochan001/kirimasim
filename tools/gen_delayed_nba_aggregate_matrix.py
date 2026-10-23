#!/usr/bin/env python3
# Copyright (c) 2026 The kirimasim Authors
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

"""Generate aggregate delayed-NBA snapshot tests (7.6, 10.4.2, 10.9).

Original cases combine arrays with differing bounds, nested structures,
patterns with side effects, self-overlap and dynamic struct-element targets.
Eight containers, four widths, and two/four-state storage use the same rules.
"""

import argparse
import sys
from pathlib import Path

OUT = Path(__file__).resolve().parent.parent/'tests'/'suite'/'sched'
VERSION = 1
HOMES = ('initial','task','function','fork','interface','package','program','generate')


def case(home,width,domain):
    name=f'delayed_nba_aggregate_{home}_{domain}_{width}'
    declarations=f'''  typedef {domain} [{width-1}:0] word_t;
  typedef struct {{ word_t value; string text; real ratio; word_t lanes[2]; }} record_t;
  word_t a[2:1][-1:0], b[0:1][2:3];
  record_t source, records[2];
  int i, calls; int delay_ticks;
  function automatic word_t next_value(); calls++; return word_t'(calls); endfunction
'''
    # Each issue body can also be called through a function without waits.
    issue=[
'''    foreach(a[x,y]) a[x][y]=0;
    calls=0; foreach(b[x,y]) b[x][y]=next_value();
    delay_ticks=2; a <= #delay_ticks b;
    delay_ticks=0; foreach(b[x,y]) b[x][y]=0;
''',
'''    a[2] <= #2 '{a[2][0],a[2][-1]};
    a[2][-1]=0; a[2][0]=0;
''',
'''    calls=0; a <= #2 '{'{next_value(),next_value()},'{next_value(),next_value()}};
''',
'''    source.value='1; source.text="captured"; source.ratio=1.5;
    source.lanes[0]=word_t'(5); source.lanes[1]=word_t'(6);
    foreach(records[x]) begin
      records[x].value=0; records[x].text=""; records[x].ratio=0;
      records[x].lanes[0]=0; records[x].lanes[1]=0;
    end
    i=1; records[i++] <= #2 source;
    source.value=0; source.text="changed"; source.ratio=0;
    source.lanes[0]=0; source.lanes[1]=0; i=0;
''',
'''    i=1; records[0] <= #2 records[i++];
    $display("source_index %0d",i);
    records[1].value=0; records[1].text="changed again"; records[1].ratio=0;
    records[1].lanes[0]=0; records[1].lanes[1]=0;
''',
'''    records[0] <= #2 '{value:'1,text:"pattern",ratio:2.5,lanes:'{word_t'(7),word_t'(8)}};
''',
'''    b[0][2]=word_t'(9); b[0][3]=word_t'(10);
    records[0] <= #2 '{value:0,text:"bounds",ratio:3.5,lanes:b[0]};
    b[0][2]=0; b[0][3]=0;
''',
'''    a <= #0 '{default:'{default:'1}};
''',
    ]
    observe=[
'''    #1; $display("before %0d %0d",a[2][-1],a[1][0]);
    #2; $display("copy %0d %0d %0d %0d",a[2][-1],a[2][0],a[1][-1],a[1][0]);
''',
'''    #3; $display("overlap %0d %0d",a[2][-1],a[2][0]);
''',
'''    $display("calls %0d",calls);
    #3; $display("pattern %0d %0d %0d %0d",a[2][-1],a[2][0],a[1][-1],a[1][0]);
''',
'''    #3; $display("record %b %s %0.1f %0d %0d",records[1].value,records[1].text,
      records[1].ratio,records[1].lanes[0],records[1].lanes[1]);
    $display("neighbor %0d %0d",records[0].value,records[0].lanes[0]);
''',
'''    #3; $display("record_copy %b %s %0.1f %0d %0d",records[0].value,records[0].text,
      records[0].ratio,records[0].lanes[0],records[0].lanes[1]);
''',
'''    #3; $display("record_pattern %b %s %0.1f %0d %0d",records[0].value,records[0].text,
      records[0].ratio,records[0].lanes[0],records[0].lanes[1]);
''',
'''    #3; $display("member_bounds %0d %0d",records[0].lanes[0],records[0].lanes[1]);
''',
'''    #0; $display("zero_before %0d",a[2][-1]);
    #1; $display("zero_after %b %b",a[2][-1],a[1][0]);
''',
    ]
    mask=(1<<width)-1
    vals=[x&mask for x in (1,2,3,4)]
    expected=["before 0 0",'copy '+' '.join(map(str,vals)),f'overlap {vals[1]} {vals[0]}',
              'calls 4','pattern '+' '.join(map(str,vals)),
              f'record {"1"*width} captured 1.5 {5&mask} {6&mask}', 'neighbor 0 0',
              'source_index 2', f'record_copy {"1"*width} captured 1.5 {5&mask} {6&mask}',
              f'record_pattern {"1"*width} pattern 2.5 {7&mask} {8&mask}',
              f'member_bounds {9&mask} {10&mask}',
              f'zero_before {vals[0]}', f'zero_after {"1"*width} {"1"*width}']
    funcs=''
    if home=='function':
        for k,text in enumerate(issue):
            funcs+=f'  function automatic void issue{k}();\n{text}  endfunction\n'
        body=''.join(f'    issue{k}();\n{text}' for k,text in enumerate(observe))
    else:
        body=''.join(a+b for a,b in zip(issue,observe))
    if home=='interface':
        source='interface worker;\n'+declarations+'  task automatic run();\n'+body
        source+='  endtask\nendinterface\nmodule top; worker w(); initial w.run(); endmodule\n'
    elif home=='package':
        source='package worker;\n'+declarations+'  task automatic run();\n'+body
        source+='  endtask\nendpackage\nmodule top; initial worker::run(); endmodule\n'
    elif home=='program':
        source='program worker;\n'+declarations+'  initial begin\n'+body
        source+='  end\nendprogram\nmodule top; worker w(); endmodule\n'
    elif home=='generate':
        source='module top; if (1) begin:g\n'+declarations+'  initial begin\n'+body
        source+='  end\nend endmodule\n'
    elif home=='task':
        source='module top;\n'+declarations+' task automatic run();\n'+body
        source+=' endtask\n initial run();\nendmodule\n'
    else:
        source='module top;\n'+declarations+funcs+'  initial begin\n'
        source+=('    fork begin\n'+body+'    end join\n' if home=='fork' else body)
        source+='  end\nendmodule\n'
    header=f'''/*
:name: {name}
:description: Aggregate delayed NBA snapshots arrays, structures and patterns before scheduling
:tags: sched nba aggregate delay
:type: simulation
:lrm: 1800-2023 7.6 9.4.5 10.4.2 10.9.1 10.9.2
:oracle: spec
:spec_check: ai
:fourstate: yes
:nondeterministic: no
:generator: gen_delayed_nba_aggregate_matrix.py@{VERSION}
:top_module: top
:expected_stdout: @{name}.expected
:expected_stderr: @{name}.stderr
*/
// Generated by tools/gen_delayed_nba_aggregate_matrix.py; edit the generator.
`timescale 1ns/1ns
'''
    return {name+'.sv':header+source,name+'.expected':'\n'.join(expected)+'\n',name+'.stderr':''}


def files():
    result={}
    for home in HOMES:
        for width in (1,32,65,129):
            for domain in ('bit','logic'):
                result.update(case(home,width,domain))
    return result


def main():
    parser=argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument('--check',action='store_true')
    args=parser.parse_args()
    bad=[]
    for name,text in files().items():
        path=OUT/name
        if args.check:
            if not path.exists() or path.read_text()!=text:
                bad.append(name)
        else:
            path.write_text(text)
    if bad:
        print('gen_delayed_nba_aggregate_matrix: out of date: '+', '.join(bad),file=sys.stderr)
    return int(bool(bad))


if __name__=='__main__':
    sys.exit(main())
