#!/usr/bin/env python3
# Copyright (c) 2026 The kirimasim Authors
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

"""Generate original delayed streaming NBA capture matrices (11.4.14.3).

Eight execution containers, 1/32/65/129-bit leaves, two/four-state targets,
right/bit/byte/65-bit orders, runtime selectors, whole arrays and #0.
The oracle reorders strings of bits independently of the SIR implementation.
"""

import argparse
import sys
from pathlib import Path

OUT=Path(__file__).resolve().parent.parent/'tests'/'suite'/'sched'
VERSION=1
HOMES=('initial','task','function','fork','interface','package','program','generate')
ORDERS=(('right','>>',0),('bit','<<',1),('byte','<<8',8),('cross','<<65',65))


def reorder(bits,block):
    if not block:
        return bits
    return ''.join(reversed([bits[i:i+block] for i in range(0,len(bits),block)]))


def case(home,width,domain):
    name=f'delayed_nba_stream_{home}_{domain}_{width}'
    source_width=2*width+5
    seed=('10x001z10110100z011x0101'*((source_width//22)+2))[:source_width]
    declarations=f'''  typedef {domain} [{width-1}:0] word_t;
  typedef logic [{source_width-1}:0] source_t;
  word_t words[0:1]; {domain} [{width+7}:0] guard;
  source_t payload;
  int i,pos,trace,ticks;
  function automatic source_t sampled(); trace=trace*10+1; return payload; endfunction
  function automatic int latency(); trace=trace*10+2; return ticks; endfunction
  function automatic int choose(); trace=trace*10+3; return i++; endfunction
  function automatic int position(); trace=trace*10+4; return pos++; endfunction
'''
    issue=[];observe=[];expected=[]
    for order,operator,block in ORDERS:
        data=reorder(seed[:2*width],block)
        if domain=='bit':
            data=data.replace('x','0').replace('z','0')
        high,low=data[:width],data[width:]
        for shape in ('selected','array'):
            tag=f'{order}_{shape}'
            target='words[choose()],guard[position()+:$bits(word_t)]' if shape=='selected' else 'words'
            text=f'''    words='{{default:0}}; guard=0; payload={source_width}'b{seed};
    trace=0; i=1; pos=4; ticks=2;
    {{{operator}{{{target}}}}} <= #(latency()) sampled();
'''
            if shape=='selected':
                text+=f'    $display("{tag} capture %0d %0d %0d",trace,i,pos);\n'
                expected.append(f'{tag} capture 1234 2 5')
            else:
                text+=f'    $display("{tag} capture %0d",trace);\n'
                expected.append(f'{tag} capture 12')
            text+="    payload=0; i=0; pos=0; ticks=0; guard[1:0]=2;\n"
            issue.append(text)
            observed=f'''    #1; $display("{tag} before %b %b",words[0],words[1]);
    #2; $display("{tag} after %b %b %b",words[0],words[1],guard);
'''
            observe.append(observed)
            expected.append(f'{tag} before '+('0'*width)+' '+('0'*width))
            if shape=='selected':
                expected.append(f'{tag} after '+('0'*width)+' '+high+' '+'0000'+low+'0010')
            else:
                expected.append(f'{tag} after '+high+' '+low+' '+('0'*(width+6))+'10')
    issue.append(f'''    words='{{default:0}}; payload='1; trace=0; ticks=0;
    {{>>{{words}}}} <= #(latency()) sampled(); payload=0;
''')
    observe.append('''    #0; $display("zero_before %b %b",words[0],words[1]);
    #1; $display("zero_after %b %b",words[0],words[1]);
''')
    expected+=['zero_before '+('0'*width)+' '+('0'*width), 'zero_after '+('1'*width)+' '+('1'*width)]
    funcs=''
    if home=='function':
        for k,text in enumerate(issue):
            funcs+=f'  function automatic void issue{k}();\n{text}  endfunction\n'
        body=''.join(f'    issue{k}();\n{text}' for k,text in enumerate(observe))
    else:
        body=''.join(a+b for a,b in zip(issue,observe))
    if home=='interface':
        source='interface worker;\n'+declarations+' task automatic run();\n'+body
        source+=' endtask\nendinterface\nmodule top; worker w(); initial w.run(); endmodule\n'
    elif home=='package':
        source='package worker;\n'+declarations+' task automatic run();\n'+body
        source+=' endtask\nendpackage\nmodule top; initial worker::run(); endmodule\n'
    elif home=='program':
        source='program worker;\n'+declarations+' initial begin\n'+body
        source+=' end\nendprogram\nmodule top; worker w(); endmodule\n'
    elif home=='generate':
        source='module top; if (1) begin:g\n'+declarations+' initial begin\n'+body
        source+=' end\nend endmodule\n'
    elif home=='task':
        source='module top;\n'+declarations+' task automatic run();\n'+body
        source+=' endtask\n initial run();\nendmodule\n'
    else:
        source='module top;\n'+declarations+funcs+' initial begin\n'
        source+=('  fork begin\n'+body+'  end join\n' if home=='fork' else body)
        source+=' end\nendmodule\n'
    header=f'''/*
:name: {name}
:description: Delayed streaming NBA captures values, delay and runtime targets once
:tags: sched nba streaming delay
:type: simulation
:lrm: 1800-2023 9.4.5 10.4.2 11.4.14 11.4.14.3
:oracle: spec
:spec_check: ai
:fourstate: yes
:nondeterministic: no
:generator: gen_delayed_nba_stream_matrix.py@{VERSION}
:top_module: top
:expected_stdout: @{name}.expected
:expected_stderr: @{name}.stderr
*/
// Generated by tools/gen_delayed_nba_stream_matrix.py; edit the generator.
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
        print('gen_delayed_nba_stream_matrix: out of date: '+', '.join(bad),file=sys.stderr)
    return int(bool(bad))


if __name__=='__main__':
    sys.exit(main())
