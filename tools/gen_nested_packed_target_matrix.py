#!/usr/bin/env python3
# Copyright (c) 2026 The kirimasim Authors
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

"""Generate bounded nested packed writes (7.4.1, 7.4.5, 11.5.1).

A runtime child selection is clipped to its statically selected parent,
including compound reads and NBA capture. The oracle modifies a bit list.
"""

import argparse
import sys
from pathlib import Path

OUT=Path(__file__).resolve().parent.parent/'tests'/'suite'/'values'
VERSION=1
HOMES=('module','element','member','automatic','ref_static','ref_automatic','interface',
       'program','function','fork','nba','nba_delay','nba_member','nba_delay_member')


def case(home,width,domain):
    name=f'nested_packed_target_{home}_{domain}_{width}'
    member='member' in home
    total=width+6 if member else 4*width
    low=3 if member else 2*width
    root_decl=(f'typedef struct packed {{ {domain} [2:0] hi; {domain} [{width-1}:0] field; {domain} [2:0] lo; }} root_t;'
               if member else f'typedef {domain} [3:0][{width-1}:0] root_t;')
    common=f'''  {root_decl}
  logic [{width-1}:0] data;
  logic signed [128:0] pos;
  int calls; int element_index=1;
  function automatic logic signed [128:0] next_position(); calls++; return pos; endfunction
'''
    variable='cells[element_index]' if home=='element' else 'value'
    read='cells[1]' if home=='element' else 'value'
    parent=variable+'.field' if member else variable+'[2]'
    initial=''.join('1' if i%3==0 else '0' for i in range(total))
    data=('10xz'*((width+3)//4))[:width]
    selectors=(('zero','0',0),('middle',str(width//2),width//2),('high',str(width-1),width-1),
               ('below','-1',-1),('outside',str(width+1),width+1),('x',"'x",None),
               ('z',"'z",None),('wide',"129'h10000000000000001",None))
    lines=[];expected=[]
    nba=home.startswith('nba')
    delayed=home.startswith('nba_delay')
    for tag,spelling,position in selectors:
        for compound in (False,True):
            mode='xor' if compound else 'write'
            setup=f'    element_index=1; {variable}={total}\'b{initial}; data={width}\'b{data}; calls=0; pos={spelling};\n'
            if home=='element':
                setup="    cells[0]='0; cells[2]='0;\n"+setup
            lines.append(setup)
            target=parent+'[next_position()+:$bits(data)]'
            if compound:
                lines.append(f'    {target} ^= data;\n')
            elif nba:
                lines.append(f'    {target} <= '+('#2 ' if delayed else '')+'data;\n')
                lines.append("    pos=0; data=0; element_index=0;\n"+('    #3;\n' if delayed else '    #1;\n'))
            else:
                lines.append(f'    {target} = data;\n')
            root=list(reversed(initial));incoming=list(reversed(data))
            if position is not None:
                for j,char in enumerate(incoming):
                    at=position+j
                    if not 0<=at<width:
                        continue
                    if compound:
                        old=root[low+at]
                        char='x' if char in 'xz' or old in 'xz' else str(int(old)^int(char))
                    if domain=='bit' and char in 'xz':
                        char='0'
                    root[low+at]=char
            want=''.join(reversed(root))
            lines.append(f'    $display("{tag} {mode} %b %0d",{read},calls);\n')
            expected.append(f'{tag} {mode} {want} 1')
            if home=='element':
                lines.append('    $display("neighbors %b %b",cells[0],cells[2]);\n')
                expected.append('neighbors '+('0'*total)+' '+('0'*total))
    body=''.join(lines)
    if home=='automatic':
        source='module top;\n'+common+' task automatic run(); root_t value;\n'+body+' endtask\n initial run(); endmodule\n'
    elif home.startswith('ref_'):
        source='module top;\n'+common+' task automatic run(ref root_t value);\n'+body+' endtask\n'
        source+=(' task automatic host(); root_t actual; run(actual); endtask\n initial host(); endmodule\n'
                 if home=='ref_automatic' else ' root_t actual; initial run(actual); endmodule\n')
    elif home=='interface':
        source='interface worker;\n'+common+' root_t value; task automatic run();\n'+body
        source+=' endtask\nendinterface\nmodule top; worker w(); initial w.run(); endmodule\n'
    elif home=='program':
        source='program worker;\n'+common+' root_t value; initial begin\n'+body
        source+=' end\nendprogram\nmodule top; worker w(); endmodule\n'
    elif home=='function':
        source='module top;\n'+common+' root_t value; function automatic void run();\n'+body
        source+=' endfunction\n initial run(); endmodule\n'
    else:
        source='module top;\n'+common+(' root_t cells[3];\n' if home=='element' else ' root_t value;\n')
        source+=' initial begin\n'+(' fork begin\n'+body+' end join\n' if home=='fork' else body)+' end\nendmodule\n'
    header=f'''/*
:name: {name}
:description: Runtime nested packed writes and compound reads stay inside the parent range
:tags: values select packed
:type: simulation
:lrm: 1800-2023 7.4.1 7.4.5 11.4.1 11.5.1
:oracle: spec
:spec_check: ai
:fourstate: yes
:nondeterministic: no
:generator: gen_nested_packed_target_matrix.py@{VERSION}
:top_module: top
:expected_stdout: @{name}.expected
:expected_stderr: @{name}.stderr
*/
// Generated by tools/gen_nested_packed_target_matrix.py; edit the generator.
`timescale 1ns/1ns
'''
    return {name+'.sv':header+source,name+'.expected':'\n'.join(expected)+'\n',name+'.stderr':''}


def files():
    result={}
    for home in HOMES:
        for width in (1,4,65,129):
            for domain in ('bit','logic'):
                result.update(case(home,width,domain))
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
    if bad:print('gen_nested_packed_target_matrix: out of date: '+', '.join(bad),file=sys.stderr)
    return int(bool(bad))


if __name__=='__main__':
    sys.exit(main())
