#!/usr/bin/env python3
# Copyright (c) 2026 The kirimasim Authors
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
"""Original source-context matrix for the approved P1 VPI bridge."""
import argparse
import json
from pathlib import Path
from vpi_feature_probe import WIDE

CONTEXTS=('initial','always','final','static_task','automatic_task','function','ref','fork','named_block','named_fork','generate','interface','program','package')
OPERATIONS=('task','integer_function','wide_function','wide_write','real_write')


def source(context,operation):
    declarations='logic[128:0] value; int scalar=7; real real_value=1.5;\n'
    body,expected={
        'task':('$k_task(scalar);','task 7\n'),
        'integer_function':('$display("function %0d",$k_add(20,22));','function 42\n'),
        'wide_function':(WIDE+'if($k_wide()===expected)$display("wide ok");else $display("BAD");','wide ok\n'),
        'wide_write':(WIDE+'$k_vector(value);if(value===expected)$display("hdl ok");else $display("BAD");','vector ok\nhdl ok\n'),
        'real_write':('$k_real(real_value);$display("hdl %.2f",real_value);','real 1.50\nhdl 2.25\n'),
    }[operation]
    lines=['`timescale 1ns/1ps']
    opening={'interface':'interface worker;','program':'program worker;','package':'package worker;'}
    lines.append(opening.get(context,'module top;'))
    automatic=context in ('automatic_task','function')
    if not automatic:lines.append(declarations)
    if context in ('static_task','automatic_task','function','ref','package'):
        signature={'automatic_task':'task automatic run();','function':'function automatic void run();','ref':'task automatic run(ref logic[128:0] value,ref int scalar,ref real real_value);'}.get(context,'task run();')
        lines.append(signature)
        if automatic:lines.append(declarations)
        lines.extend((body,'endfunction' if context=='function' else 'endtask'))
        if context!='package':lines.append('initial run(value,scalar,real_value);' if context=='ref' else 'initial run();')
    else:
        if context=='always':lines.extend(('event go;initial #1->go;','always @(go)begin'))
        elif context=='generate':lines.extend(('if(1)begin:g','initial begin'))
        else:lines.append('final begin' if context=='final' else 'initial begin : named' if context=='named_block' else 'initial begin')
        if context in ('fork','named_fork'):lines.extend(('fork : workers begin' if context=='named_fork' else 'fork begin',body,'end join'))
        else:lines.append(body)
        lines.append('end')
        if context=='generate':lines.append('end')
    if context=='interface':lines.extend(('endinterface','module top;worker u();endmodule'))
    elif context=='program':lines.extend(('endprogram','module top;worker u();endmodule'))
    elif context=='package':lines.extend(('endpackage','module top;initial worker::run();endmodule'))
    else:lines.append('endmodule')
    return '\n'.join(lines)+'\n',expected


def programs():
    for context in CONTEXTS:
        for operation in OPERATIONS:
            text,expected=source(context,operation)
            yield f'{context}_{operation}',text,expected


def main():
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--output',type=Path,required=True);args=parser.parse_args();args.output.mkdir(parents=True,exist_ok=True)
    records=[]
    for name,text,expected in programs():
        (args.output/(name+'.sv')).write_text(text);records.append({'name':name,'expected_stdout':expected})
    (args.output/'manifest.json').write_text(json.dumps(records,indent=2)+'\n');print(f'VPI matrix: {len(records)} original programs')


if __name__=='__main__':main()
