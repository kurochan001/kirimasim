#!/usr/bin/env python3
# Copyright (c) 2026 The kirimasim Authors
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
"""Original execution/boundary matrix for approved whole-net contract 1-8."""
import argparse
import json
from pathlib import Path

from vpi_net_write_probe import cases as inventory, op


def record(name, source, expected, *, category='execution', status=0, options=()):
    if not source.startswith('`timescale'):
        source = '`timescale 1ns/1ns\n' + source
    return dict(name=name, source=source, expected_stdout=expected,
                expected_exit=status, category=category, options=list(options))


def steps(width, mode, bits=None, named=False):
    zero, one = '0'*width, bits or '1'*width
    flag = 1 if mode == 'deposit' else 5
    call = (f'$net_named("top.n",{width}\'b{one},{flag});' if named
            else f'$net_op(n,{width}\'b{one},{flag});')
    expected = ('named error=0 now='+one+'\n') if named else op(flag, one)
    body = '#1;' + call + '#1;$display("held %b",n);a=1;#1;a=0;#1;'
    expected += 'held '+one+'\n'
    if mode == 'force':
        body += '$net_named("top.n",0,6);' if named else '$net_op(n,0,6);'
        expected += ('named returned='+zero+'\nnamed error=0 now='+zero+'\n') if named else op(6,zero)
    body += '$display("after %b",n);'
    return body, expected+'after '+zero+'\n'


def cases():
    for old in inventory():
        if old['expected_stdout'] is not None:
            yield record(old['name'],old['source'],old['expected_stdout'])
        elif old['name']=='passive_deposit_boundary':
            yield record(old['name'],old['source'],
                         'op 1 error=3 event=0 now=0\ndeposited 0 0\nafter 0 0\n',
                         category='expected-api-rejection')
        else:
            pair = 'x 1' if 'tran_' in old['name'] and 'rtran_' not in old['name'] else '0 1'
            yield record('contract_'+old['name'],old['source'],
                         op(5,'1')+'held '+pair+'\n'+op(6,'0')+'after 0 0\n')
    for context in ('static_task','automatic_task','function','fork','named','package_function','class_static'):
        for width in (1,32,33,65,129,257):
            for mode in ('deposit','force'):
                named = context in ('named','package_function','class_static')
                body,expected=steps(width,mode,named=named)
                declaration=f'logic [{width-1}:0] a=0;wire [{width-1}:0] n;assign n=a;\n'
                prefix=''
                if context in ('static_task','automatic_task'):
                    lifetime='automatic' if context=='automatic_task' else 'static'
                    declaration+=f'task {lifetime} act();{body}endtask\n';body='act();'
                elif context=='function':
                    declaration+=f'function void act(input logic [{width-1}:0] data,input int mode);$net_op(n,data,mode);endfunction\n'
                    body=body.replace('$net_op(n,','act(')
                elif context=='fork':
                    body='fork begin '+body+' end join'
                elif context in ('package_function','class_static'):
                    home='package helpers;' if context=='package_function' else 'class helpers;'
                    method='function automatic void' if context=='package_function' else 'static function void'
                    prefix=(home+f'{method} act(input logic [{width-1}:0] data,input int mode);'
                            '$net_named("top.n",data,mode);endfunction '+
                            ('endpackage\n' if context=='package_function' else 'endclass\n'))
                    body=body.replace('$net_named("top.n",','helpers::act(')
                yield record(f'{context}_{width}_{mode}',prefix+'module top;'+declaration+'initial begin '+body+' end endmodule\n',expected)
    for width in (32,65,129,257):
        for logic in ('x','z'):
            for mode in ('deposit','force'):
                body,expected=steps(width,mode,logic*width)
                yield record(f'extended_{width}_{logic}_{mode}',
                             f'module top;logic [{width-1}:0] a=0;wire [{width-1}:0] n;assign n=a;initial begin {body}end endmodule\n',expected)
    for width in (1,33,129):
        source=f'module top;logic [{width-1}:0] a=0;wire [{width-1}:0] n;assign n=a;'
        yield record(f'callback_reentry_{width}',source+'initial begin #1;$net_reenter(n);$net_op(n,0,6);end endmodule\n',
                     'reentry calls=2 errors=0 now=0\n'+op(6,'0'*width))
        yield record(f'ignored_time_{width}',source+'initial begin #1;$net_ignored_time(n);end endmodule\n',
                     'time mode=1 error=0 now=1\ntime mode=5 error=0 now=0\ntime mode=6 error=0 now=0\n')
        for flag in (1,5,6):
            yield record(f'index_rejection_{width}_{flag}',source+f'initial begin #1;$net_index(n,0,{flag});end endmodule\n',
                         'indexed error=3 now=0\n',category='expected-api-rejection')
        for flag in (2,3,4,999):
            yield record(f'flag_rejection_{width}_{flag}',source+f'initial begin #1;$net_op(n,1\'b1,{flag});end endmodule\n',
                         f'op {flag} error=3 event=0 now='+('0'*width)+'\n',category='expected-api-rejection')
    yield record('selected_one_bit_declaration_rejected',
                 'module top;logic a=0;wire [0:0] n;assign n=a;initial begin #1;$net_op(n[0],1\'b1,5);end endmodule\n',
                 'op 5 error=3 event=0 now=0\n',category='expected-api-rejection')
    yield record('release_format_preflight',
                 'module top;wire n=0;initial begin #1;$net_bad_release(n);$net_op(n,0,6);end endmodule\n',
                 'bad-release error=3 format=999 payload=123 now=1\n'+op(6,'0'),category='expected-api-rejection')
    yield record('readonly_phase',
                 'module top;wire n=0;initial begin #1;$net_readonly(n);#1;$display("unreached");end endmodule\n',
                 'readonly error=3 now=0\n',category='expected-runtime-error',status=2)
    for kind,options in [('parse',('parse',)),('dump',('--dump-sir',)),('run',())]:
        yield record('compile_phase_'+kind,'module top;wire n=0;initial begin $net_compile(n);#1;$display("after %b",n);end endmodule\n',
                     'compile error=3\n'+('runtime\nafter 0\n' if kind=='run' else ''),
                     category='compile-phase' if kind!='dump' else 'dump-phase', options=options)


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output',type=Path,required=True)
    args=parser.parse_args();args.output.mkdir(parents=True,exist_ok=True)
    rows=list(cases());assert len(rows)==len({r['name'] for r in rows})
    for row in rows:(args.output/(row['name']+'.sv')).write_text(row['source'])
    (args.output/'manifest.json').write_text(json.dumps(rows,indent=2)+'\n')
    print(f'whole-net VPI: {len(rows)} original execution/boundary sources')


if __name__=='__main__':main()
