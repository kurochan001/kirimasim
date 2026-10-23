#!/usr/bin/env python3
# Copyright (c) 2026 The kirimasim Authors
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

"""Generate original mixed named-event/iff wait tests (9.4.2, 15.5).

Each waiter rearms within a time slot and across time slots. False/X iff
conditions keep the item waiting; another satisfied item cancels it.
Eight containers include an automatic-frame value read by the iff guard.
"""

import argparse
import re
import sys
from pathlib import Path

OUT=Path(__file__).resolve().parent.parent/'tests'/'suite'/'sched'
VERSION=1
HOMES=('initial','task','automatic','fork','interface','package','program','generate')
SHAPES=('named2','named_signal','named_watch','single_iff','named2_iff','mixed_iff','named2_nb','single_iff_late')


def case(home,shape):
    name=f'named_event_wait_{home}_{shape}'
    frame=' && local_enable' if home=='automatic' else ''
    expressions={
        'named2':'first or second',
        'named2_nb':'first or second',
        'named_signal':'first or posedge clk',
        'named_watch':'first or (left ^ right)',
        'single_iff':f'first iff (ena{frame})',
        'single_iff_late':f'first iff (ena{frame})',
        'named2_iff':f'first iff (ena{frame}) or second iff (enb{frame})',
        'mixed_iff':f'first iff (ena{frame}) or second iff (enb{frame}) or posedge clk iff (enb{frame}) or left ^ right iff (enc{frame})',
    }
    declarations='''  event first,second;
  bit clk=0,left=0,right=0;
  logic ena=0,enb=0,enc=0;
'''
    body=('    bit local_enable=1;\n' if home=='automatic' else '')
    body+=f'''    for(int round=1;round<=4;round++) begin
      @({expressions[shape]});
      $display("wake %0d %0t",round,$time);
    end
    #5;
'''
    if shape=='named2_nb':
        drive='''    #1; ->>first; ->>second;
    #1; ->>second;
    #1; ->>first;
    #1; ->>second;
'''
    elif shape=='named2':
        drive='''    #1; ->first; ->second;
    #0; ->first;
    #1; ->second;
    #1; ->first;
'''
    elif shape=='named_signal':
        drive='''    #1; ->first; clk=1;
    #0; clk=0; clk=1;
    #1; clk=0; clk=1;
    #1; ->first;
'''
    elif shape=='named_watch':
        drive='''    #1; ->first; left=1;
    #0; right=1;
    #1; left=0;
    #1; ->first;
'''
    elif shape=='single_iff_late':
        drive='''    #1; ena=1;
    #1; ->first;
    #1; ->first;
    #1; ->first;
    #1; ->first;
'''
    elif shape=='single_iff':
        drive='''    #1; ena=0; ->first; ena='x; ->first; ->second; ena=1; ->first;
    #0; ->first;
    #1; ->first;
    #1; ->first;
'''
    elif shape=='named2_iff':
        drive='''    #1; ena=0; enb=1; ->first; ena='x; ->first; ena=1; ->first; ->second;
    #0; ena=0; enb=1; ->first; ->second;
    #1; ena=1; enb=0; ->second; ->first;
    #1; ena=1; enb=1; ->first; ->second;
'''
    else:
        drive='''    #1; ena=0; enb=0; enc=0; ->first; ->second; clk=1; left=1;
    ena='x; ->first; ena=1; ->first; ->second;
    #0; ena=0; enb=0; enc=1; ->first; right=1;
    #1; enc=0; enb=1; left=0; clk=0; clk=1;
    #1; enb=0; ena=1; ->second; ->first;
'''
    drive+='    #1; ->first; ->second; clk=~clk; left=~left;\n    #1; $display("done");\n'
    if home in ('interface','package'):
        qualified=drive
        prefix='w.' if home=='interface' else 'worker::'
        for token in ('first','second','ena','enb','enc','clk','left','right'):
            qualified=re.sub(r'\b'+token+r'\b',prefix+token,qualified)
        source=f'{home} worker;\n'+declarations+' task automatic run();\n'+body+f' endtask\nend{home}\n'
        entry='worker w(); initial w.run();' if home=='interface' else 'initial worker::run();'
        source+='module top; '+entry+' initial begin\n'+qualified+' end\nendmodule\n'
    elif home=='program':
        qualified_body=body
        for token in ('first','second','ena','enb','enc','clk','left','right'):
            qualified_body=re.sub(r'\b'+token+r'\b','top.'+token,qualified_body)
        # Active #0 precedes Reactive; use separate time steps to rearm.
        qualified_drive=drive.replace('#0;','#1;')
        source='program worker; initial begin\n'+qualified_body+' end\nendprogram\n'
        source+='module top;\n'+declarations+' worker p(); initial begin\n'+qualified_drive+' end\nendmodule\n'
    elif home=='generate':
        source='module top; if(1) begin:g\n'+declarations+' initial begin\n'+body+' end\n initial begin\n'+drive+' end\nend endmodule\n'
    else:
        source='module top;\n'+declarations
        if home in ('task','automatic'):
            source+=' task automatic run();\n'+body+' endtask\n initial run();\n'
        elif home=='fork':
            source+=' initial fork begin\n'+body+' end join\n'
        else:
            source+=' initial begin\n'+body+' end\n'
        source+=' initial begin\n'+drive+' end\nendmodule\n'
    times=(1,2,3,4) if home=='program' or shape=='named2_nb' else (1,1,2,3)
    if shape=='single_iff_late':times=(2,3,4,5)
    expected=''.join(f'wake {i} {t}\n' for i,t in enumerate(times,1))+'done\n'
    header=f'''/*
:name: {name}
:description: Mixed named event and signal/watch waits retain false iff items and cancel stale items
:tags: sched event iff
:type: simulation
:lrm: 1800-2023 9.4.2 9.4.2.3 15.5
:oracle: spec
:spec_check: ai
:fourstate: yes
:nondeterministic: no
:generator: gen_named_event_wait_matrix.py@{VERSION}
:top_module: top
:expected_stdout: @{name}.expected
:expected_stderr: @{name}.stderr
*/
// Generated by tools/gen_named_event_wait_matrix.py; edit the generator.
`timescale 1ns/1ns
'''
    return {name+'.sv':header+source,name+'.expected':expected,name+'.stderr':''}


def files():
    result={}
    for home in HOMES:
        for shape in SHAPES:result.update(case(home,shape))
    return result


def main():
    parser=argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument('--check',action='store_true')
    args=parser.parse_args();bad=[]
    for name,text in files().items():
        path=OUT/name
        if args.check:
            if not path.exists() or path.read_text()!=text:bad.append(name)
        else:path.write_text(text)
    if bad:print('gen_named_event_wait_matrix: out of date: '+', '.join(bad),file=sys.stderr)
    return int(bool(bad))


if __name__=='__main__':
    sys.exit(main())
