#!/usr/bin/env python3
# Copyright (c) 2026 The kirimasim Authors
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

"""Original source matrix for state-dependent scalar specify contract 1-8.

The original 40 review inputs/oracles remain unchanged. Timing expectations
below come from explicit source times and the independent endpoint delay table,
never from observed simulator output. No reference executable is needed in CI.
"""

import argparse
import json
from pathlib import Path

from gen_specify_matrix import contextual
from specify_feature_probe import DELAY_LISTS, TRANSITIONS, cell, expanded, state_cases, transition


def record(name, source, expected, **extra):
    return {'name': name, 'source': source, 'expected_stdout': expected, **extra}


def timed(path, stimulus, *, functional='assign q=a;', declarations='',
          ports='input wire a,b,output wire q'):
    return cell(path, declarations + '\n' + functional, ports) + '''module top;
reg a=0,b=0;wire q;bit armed=0;cell_under_test u(a,b,q);
always @(q)if(armed)$display("%0t %b",$time,q);
initial begin #20;armed=1;
''' + stimulus + '\n#20;$display("end %b",q);end\nendmodule\n'


def programs():
    yield from state_cases()
    for logic in '01xz':
        for delays in DELAY_LISTS:
            text = '(' + ','.join(map(str, delays)) + ')'
            path = (f"if(1'b{logic})(a=>q)=" + (text if logic != '0' else '41') +
                    ';ifnone(a=>q)=' + ('41' if logic != '0' else text) + ';')
            for pair, delay in zip(TRANSITIONS, expanded(delays)):
                yield record(f'state_transitions_{logic}_{len(delays)}_{pair}',
                             transition('assign q=a;', path, *pair),
                             f'{21 + delay} {pair[1]}\nend {pair[1]}\n')
    for context in ('initial', 'always', 'static_task', 'automatic_task', 'function',
                    'fork', 'generate', 'interface', 'program', 'package', 'ref'):
        source, expected = contextual(context)
        old = '(a => q)=3;'
        assert source.count(old) == 1
        yield record('state_context_' + context,
                     source.replace(old, "if(1'b1)(a=>q)=3;ifnone(a=>q)=7;"), expected)

    for logic in '01xz':
        yield record('state_unconditional_' + logic,
                     timed(f"if(1'b{logic})(a=>q)=3;(a=>q)=5;", '#1;a=1;'),
                     f'{26 if logic == "0" else 24} 1\nend 1\n')
    yield record('state_latest_slow_enabled',
                 timed('if(1)(a=>q)=3;if(1)(b=>q)=9;', '#1;a=1;#20;b=1;',
                       functional='assign q=a^b;'),
                 '24 1\n50 0\nend 0\n')
    for mode, ticks in (('min', 2), ('typ', 4), ('max', 6)):
        yield record('state_mintypmax_' + mode,
                     timed("if(1'b1)(a=>q)=(2:4:6);ifnone(a=>q)=13;", '#1;a=1;'),
                     f'{21+ticks} 1\nend 1\n', mtm=mode)

    for delay in (0, -3):
        source = timed(f"specparam D={delay};if(1'b1)(a=>q)=D;", '#1;a=1;#0;$display("inactive %b",q);')
        yield record('state_inactive_' + str(delay).replace('-', 'minus'), source,
                     '21 1\ninactive 1\nend 1\n')

    for readout, first, second in (('q', 7, 3), ('!q', 3, 7)):
        yield record('state_public_output_' + ('not' if readout == '!q' else 'direct'),
                     timed(f'if({readout})(a=>q)=3;ifnone(a=>q)=7;', '#1;a=1;#20;a=0;'),
                     f'{21+first} 1\n{41+second} 0\nend 0\n')

    collapsed = cell('if(b)(a=>q)=3;ifnone(a=>q)=7;', ports='input wire a,b,output wire q') + '''module top;
reg av=0,bv=1;wire a,b,q;assign a=av;assign b=bv;bit armed=0;
cell_under_test u(a,b,q);
always @(q)if(armed)$display("%0t %b",$time,q);
initial begin #20;armed=1;#1;av=1;#20;$display("end %b",q);end
endmodule
'''
    yield record('state_collapsed_inputs', collapsed, '24 1\nend 1\n')
    for logic in '01xz':
        aliases = cell('if(c)(a=>q)=9;ifnone(a=>q)=3;'
                       'if(!c)(b=>q)=11;ifnone(b=>q)=7;',
                       ports='input wire a,b,c,output wire q') + f'''module top;
reg value=0;wire shared,q;assign shared=value;bit armed=0;
cell_under_test u(shared,shared,1'b{logic},q);
always @(q)if(armed)$display("%0t %b",$time,q);
initial begin #20;armed=1;#1;value=1;#20;$display("end %b",q);end
endmodule
'''
        ticks = {'0': 3, '1': 7, 'x': 9, 'z': 9}[logic]
        yield record('state_aliased_source_' + logic, aliases, f'{21+ticks} 1\nend 1\n')
    for enabled in (0, 1):
        aliases = cell('if(c)(a=>q)=9;ifnone(a=>q)=3;(b=>q)=5;',
                       ports='input wire a,b,c,output wire q') + f'''module top;
reg value=0;wire shared,q;assign shared=value;bit armed=0;
cell_under_test u(shared,shared,1'b{enabled},q);
always @(q)if(armed)$display("%0t %b",$time,q);
initial begin #20;armed=1;#1;value=1;#20;$display("end %b",q);end
endmodule
'''
        yield record(f'state_aliased_unconditional_{enabled}', aliases,
                     f'{26 if enabled else 24} 1\nend 1\n')
    plain_aliases = aliases.replace('if(c)(a=>q)=9;ifnone(a=>q)=3;(b=>q)=5;',
                                    '(a=>q)=3;(b=>q)=9;')
    assert plain_aliases != aliases
    yield record('state_aliased_plain_paths', plain_aliases, '24 1\nend 1\n')
    isolated = cell('if(b)(a=>q)=3;ifnone(a=>q)=7;', ports='input wire a,b,output wire q') + '''module top;
reg av=0;wire a,q0,q1,b0,b1;assign a=av;assign b0=0;assign b1=1;
cell_under_test u0(a,b0,q0),u1(a,b1,q1);bit armed=0;
always @(q0)if(armed)$display("slow %0t %b",$time,q0);
always @(q1)if(armed)$display("fast %0t %b",$time,q1);
initial begin #20;armed=1;#1;av=1;#20;$display("end %b%b",q0,q1);end
endmodule
'''
    yield record('state_instance_isolation', isolated, 'fast 24 1\nslow 28 1\nend 11\n')
    force = cell('if(b)(a=>q)=9;ifnone(a=>q)=3;', ports='input wire a,b,output wire q') + '''module top;
reg a=0,b=1;wire q;cell_under_test u(a,b,q);
initial begin #21;a=1;#1;force u.q=0;#1;b=0;#8;$display("forced %b",q);
#1;release u.q;$display("released %b",q);end
endmodule
'''
    yield record('state_force_preserves_selected_deadline', force, 'forced 0\nreleased 1\n')
    before = force.replace('#21;a=1;#1;force u.q=0;#1;b=0;',
                           '#20;force u.q=0;#1;a=1;#2;b=0;')
    assert before != force
    yield record('state_force_before_functional_arrival', before, 'forced 0\nreleased 1\n')
    for mode, ticks in ((0, 7), (1, 3)):
        full = cell('if(en)(a,b*>q,r)=3;ifnone(a,b*>q,r)=7;',
                    'assign q=a;assign r=b;', ports='input wire a,b,en,output wire q,r') + f'''module top;
reg a=0,b=0;wire q,r;bit armed=0;cell_under_test u(a,b,1'b{mode},q,r);
always @(q)if(armed)$display("q %0t %b",$time,q);
always @(r)if(armed)$display("r %0t %b",$time,r);
initial begin #20;armed=1;#1;a=1;#10;b=1;#20;$display("end %b%b",q,r);end
endmodule
'''
        yield record(f'state_full_scalar_{mode}', full,
                     f'q {21+ticks} 1\nr {31+ticks} 1\nend 11\n')

    diagnostic = timed('if(b ? 1 : 0)(a=>q)=3;ifnone(a=>q)=7;', "b=1'bx;#1;a=1;")
    yield record('state_nested_condition_warn', diagnostic, '24 1\nend 1\n',
                 flags=['--cond-x=warn'], diagnostic='?: condition contains X/Z bits')
    yield record('state_nested_condition_error', diagnostic, '', flags=['--cond-x=error'],
                 expected_exit=2, diagnostic='?: condition contains X/Z bits', runtime_at='21ns')


def rejections():
    common = 'module top;logic a=0,b=0;wire q;cell_under_test u(a,b,q);endmodule\n'
    for name, path, declarations, reason in (
        ('edge', 'if(b)(posedge a=>(q:a))=3;', '', 'edge-sensitive state-dependent specify path'),
        ('runtime_select', 'if(bits[b])(a=>q)=3;', 'wire [1:0] bits={a,b};',
         'runtime selection in a specify condition'),
        ('foreign', 'if(top.flag)(a=>q)=3;', '',
         'expression is not valid in a state-dependent path condition'),
        ('nonintegral', 'if(value)(a=>q)=3;', 'real value=1.25;',
         'specify condition other than a pure static integral expression'),
    ):
        source = cell(path, declarations + '\nassign q=a;', ports='input wire a,b,output wire q')
        top = common.replace('logic a=0,b=0;', 'logic a=0,b=0;logic flag=1;')
        yield record('state_reject_' + name, source + top, '', expected_exit=2,
                     diagnostic=reason, positioned=True)


def cases():
    yield from programs()
    yield from rejections()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=True)
    manifest = list(cases())
    for case in manifest:
        (args.output / (case['name'] + '.sv')).write_text(case['source'])
    (args.output / 'manifest.json').write_text(json.dumps(manifest, indent=2) + '\n')
    print(f'state-dependent specify: exported {len(manifest)} original cases')


if __name__ == '__main__':
    main()
