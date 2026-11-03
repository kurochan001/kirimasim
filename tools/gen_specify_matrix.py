#!/usr/bin/env python3
# Copyright (c) 2026 The kirimasim Authors
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
"""Original bounded source matrix for the approved scalar specify contract.

The frozen reference probe supplies original sources and written traces, never
simulator output. Context and observation oracles below follow contract §§1–8.
No reference executable is required by native CI.
"""
import argparse
import json
from pathlib import Path

from specify_feature_probe import cases, cell

CONTEXTS = ('initial', 'always', 'static_task', 'automatic_task', 'function',
            'fork', 'generate', 'interface', 'program', 'package', 'ref')


def contextual(context):
    expected = 'early 0\nlate 1\n'
    declarations = 'logic a=0;wire q;cell_under_test u(a,q);\n'
    action = 'a=1;'
    setup = ''
    if context in ('static_task', 'automatic_task', 'function', 'ref'):
        setup = {'static_task': 'task change();a=1;endtask',
                 'automatic_task': 'task automatic change();a=1;endtask',
                 'function': 'function automatic void change();a=1;endfunction',
                 'ref': 'task automatic change(ref logic v);v=1;endtask'}[context]+'\n'
        action = 'change(a);' if context == 'ref' else 'change();'
    elif context == 'fork':
        action = 'fork begin a=1;end join'
    elif context == 'package':
        action = 'stimulus::change(a);'
    body = '#20;'+action+'#1;$display("early %b",q);#3;$display("late %b",q);'
    procedure = 'initial begin '+body+' end\n'
    if context == 'always':
        procedure = 'event go;initial #20 ->go;always @(go)begin '+body.replace('#20;', '', 1)+' end\n'
    elif context == 'generate':
        procedure = 'if(1)begin:g '+procedure+' end\n'
    prefix = ''
    if context == 'package':
        prefix = 'package stimulus;task automatic change(ref logic v);v=1;endtask endpackage\n'
    if context == 'interface':
        top = ('interface worker(output logic a,input wire q);initial a=0;'+procedure+
               'endinterface\nmodule top;wire a,q;cell_under_test u(a,q);worker w(a,q);endmodule\n')
    elif context == 'program':
        top = ('program worker(output logic a,input wire q);initial a=0;'+procedure+
               'endprogram\nmodule top;wire a,q;cell_under_test u(a,q);worker w(a,q);endmodule\n')
    else:
        top = 'module top;\n'+declarations+setup+procedure+'endmodule\n'
    return '`timescale 1ns/1ns\n'+prefix+cell()+top, expected


def programs():
    # These are exactly the admitted scalar cases with independently written
    # traces. Excluded vector/conditional forms have rejection checks below.
    for case in cases():
        if case['expected_stdout'] is None or case['name'].startswith('vector_') or case['area'] in ('state_dependent', 'condition_semantics'):
            continue
        yield 'table_'+case['name'], case['source'], case['expected_stdout'], case['mtm']
    for context in CONTEXTS:
        source, expected = contextual(context)
        yield 'context_'+context, source, expected, 'typ'
    directed = {
        'cascade_tracks_public_arrival': (
            cell('(a=>q)=3;')+"""module top;logic a=0;wire q0,q1;cell_under_test u0(a,q0),u1(q0,q1);
initial begin #21;a=1;#1;$display("early %b%b",q0,q1);#3;$display("middle %b%b",q0,q1);#3;$display("late %b%b",q0,q1);end endmodule
""", 'early 00\nmiddle 10\nlate 11\n'),
        'force_future_and_elapsed': (
            cell('(a=>q)=7;')+'''module top;logic a=0;wire q;cell_under_test u(a,q);
initial begin #20;force u.q=0;a=1;#3;release u.q;$display("future %b",q);
#1;force q=0;#5;$display("forced %b",u.q);release q;$display("elapsed %b",q);end
endmodule
''', 'future 0\nforced 0\nelapsed 1\n'),
        'inside_and_outside': (
            cell('(a=>q)=3;') .replace('endmodule', 'initial begin #22;$display("inside early %b",q);#4;$display("inside late %b",q);end\nendmodule', 1)+
            '''module top;logic a=0;wire q;cell_under_test u(a,q);
initial begin #21;a=1;#2;$display("outside early %b",q);#4;$display("outside late %b",q);end
endmodule
''', 'inside early 0\noutside early 0\ninside late 1\noutside late 1\n'),
        'monitor_strength_snapshot': (
            cell('(a=>q)=3;', 'assign(weak1,weak0)q=a;')+'''module top;logic a=0;wire q;cell_under_test u(a,q);
initial begin #10;$monitor("%0t %b %v",$time,q,q);#11;a=1;#10;$monitoroff;end
endmodule
''', '10 0 We0\n24 1 We1\n'),
        'strobe_strength_snapshot': (
            cell('(a=>q)=3;', 'assign(weak1,weak0)q=a;')+'''module top;logic a=0;wire q;cell_under_test u(a,q);
initial begin #21;a=1;#0;$strobe("early %b %v",q,q);#3;$strobe("due %b %v",q,q);end
endmodule
''', 'early 0 We0\ndue 1 We1\n'),
        'multiple_outputs': (
            cell('(a,b *> q,r)=(2,5);','assign q=a;assign r=b;', 'input wire a,b,output wire q,r')+'''module top;logic a=0,b=1;wire q,r;cell_under_test u(a,b,q,r);
initial begin #20;{a,b}=2\'b10;#1;$display("early %b%b",q,r);#2;$display("middle %b%b",q,r);#3;$display("late %b%b",q,r);end
endmodule
''', 'early 01\nmiddle 11\nlate 10\n'),
        'parameterized_generate_instances': (
            '''`timescale 1ns/1ns
module cell_under_test #(parameter D=3)(input wire a,output wire q);assign q=a;specify specparam T=D;(a=>q)=T;endspecify endmodule
module top;logic a=0;for(genvar i=0;i<2;i++)begin:g wire q;cell_under_test #(3+4*i) u(a,q);end
initial begin #20;a=1;#1;$display("early %b%b",g[1].q,g[0].q);#3;$display("middle %b%b",g[1].q,g[0].q);#4;$display("late %b%b",g[1].q,g[0].q);end endmodule
''', 'early 00\nmiddle 01\nlate 11\n'),
        'constant_input_epoch_zero': (
            cell('(a=>q)=5;')+'''module top;wire q;cell_under_test u(1\'b1,q);initial begin #1;$display("early %b",q);#5;$display("late %b",q);end endmodule
''', 'early x\nlate 1\n'),
        'functional_function': (
            cell('(a=>q)=3;', 'function automatic logic pass(input logic v);return v;endfunction assign q=pass(a);')+'''module top;logic a=0;wire q;cell_under_test u(a,q);initial begin #20;a=1;#1;$display("early %b",q);#3;$display("late %b",q);end endmodule
''', 'early 0\nlate 1\n'),
        'source_force_clocks_path': (
            cell('(a=>q)=3;')+'''module top;logic a=0;wire q;cell_under_test u(a,q);initial begin #20;force a=1;#1;$display("early %b",q);#3;$display("late %b",q);release a;end endmodule
''', 'early 0\nlate 1\n'),
        'final_observes_commit': (
            cell('(a=>q)=3;')+'''module top;logic a=0;wire q;cell_under_test u(a,q);initial begin #20;a=1;#4;end final $display("final %b",q);endmodule
''', 'final 1\n'),
    }
    for name, (source, expected) in directed.items():
        yield name, source, expected, 'typ'
    yield from approved_state_paths()


def approved_state_paths():
    # Exact two-case migration approved separately on 2026-11-03. Keep
    # historical source names/bytes; these execute rather than expect rejection.
    common_top = 'module top;logic a=0;wire q;cell_under_test u(a,q);endmodule\n'
    for name, path in (('conditional', 'if(a)(a=>q)=3;'),
                       ('ifnone', 'if(a)(a=>q)=3;ifnone(a=>q)=7;')):
        yield 'reject_' + name, cell(path) + common_top, '', 'typ'


def rejections():
    common_top = 'module top;logic a=0;wire q;cell_under_test u(a,q);endmodule\n'
    examples = {
        'vector': (cell('(a=>q)=3;', ports='input wire[1:0] a,output wire[1:0] q'), 'vector or non-four-state specify terminal'),
        'select': (cell('(a[0]=>q)=3;', ports='input wire[1:0] a,output wire q', functional='assign q=a[0];'), 'specify terminal other than a scalar port net'),
        'output_variable': (cell('(a=>q)=3;', ports='input wire a,output var logic q', functional='always_comb q=a;'), 'specify terminal other than a scalar port net'),
        'inout': (cell('(a=>q)=3;', ports='inout wire a,output wire q'), 'required input/output net direction'),
        'multiple_internal_drivers': (cell('(a=>q)=3;', 'assign q=a;assign q=~a;'), 'exactly one ordinary functional driver'),
        'three_state_driver': (cell('(a=>q)=3;', "bufif1 gate(q,a,1'b1);"), 'nonordinary or external functional driver'),
        'mos_driver': (cell('(a=>q)=3;', "nmos gate(q,a,1'b1);"), 'exactly one ordinary functional driver'),
        'passive': (cell('(a=>q)=3;', 'wire w;assign q=a;tran link(q,w);'), 'passive connectivity'),
        'pulse_style': (cell('(a=>q)=3;pulsestyle_ondetect q;'), 'specify pulse control or timing check'),
        'showcancelled': (cell('(a=>q)=3;showcancelled q;'), 'specify pulse control or timing check'),
        'pathpulse': (cell('(a=>q)=3;specparam PATHPULSE$=1;'), 'specify pulse control or timing check'),
        'pathpulse_pair': (cell('(a=>q)=3;specparam PATHPULSE$=(1,2);'), 'specify pulse control or timing check'),
        'timing_check': (cell('(a=>q)=3;$setup(a,posedge a,2);'), 'specify pulse control or timing check'),
        'external_driver': (cell('(a=>q)=3;'), 'exactly one ordinary functional driver'),
    }
    for name, (source, reason) in examples.items():
        top = common_top
        if name == 'external_driver': top = top.replace('endmodule', 'assign q=0;endmodule')
        if name == 'inout': top = top.replace('logic a=0;', 'wire a;')
        yield name, source+top, reason


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=True)
    manifest = []
    for name, source, expected, mode in programs():
        (args.output/(name+'.sv')).write_text(source)
        manifest.append({'source': name+'.sv', 'expected_stdout': expected, 'mintypmax': mode})
    for name, source, reason in rejections():
        name = 'reject_'+name
        (args.output/(name+'.sv')).write_text(source)
        manifest.append({'source': name+'.sv', 'unsupported_reason': reason})
    (args.output/'manifest.json').write_text(json.dumps(manifest, indent=2)+'\n')
    print(f'scalar specify: exported {len(manifest)} original programs')


if __name__ == '__main__':
    main()
