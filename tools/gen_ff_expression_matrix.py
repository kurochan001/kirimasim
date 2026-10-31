#!/usr/bin/env python3
# Copyright (c) 2026 The kirimasim Authors
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
"""Original always_ff expression event matrix (9.2.2.4, 9.4.2).

Reference counts follow the four-state edge table, full-value change for
unqualified events, and iff sampling only when the event occurs. Each
stimulus changes one source; observations follow the NBA region.
"""
import argparse
import itertools
from pathlib import Path

OUT = Path(__file__).resolve().parent.parent / 'tests/suite/sched'
HOMES = ('module', 'interface', 'generate')
EDGES = ('posedge', 'negedge', 'both', 'any')


def logic_xor(a, b):
    return str(int(a) ^ int(b)) if a in '01' and b in '01' else 'x'


def value(state, shape):
    a, b = state['a'], state['b']
    if shape in ('xor', 'function'):
        return [logic_xor(x, y) for x, y in zip(a, b)]
    if shape == 'indexed':
        return [a[state['index']]]
    if shape == 'array':
        return state['row' + str(state['index'])][:]
    if shape == 'invert':
        return [{'0': '1', '1': '0'}.get(x, 'x') for x in a]
    if shape == 'reduction':
        return [str(sum(int(x) for x in a) % 2) if all(x in '01' for x in a) else 'x']
    pick = state['pick']
    return a[:] if pick == '1' else b[:] if pick == '0' else [x if x == y else 'x' for x, y in zip(a, b)]


def matches(old, new, edge):
    if edge == 'any':
        return old != new
    a, b = old[0], new[0]
    pos = (a == '0' and b in '1xz') or (a in 'xz' and b == '1')
    neg = (a == '1' and b in '0xz') or (a in 'xz' and b == '0')
    return pos if edge == 'posedge' else neg if edge == 'negedge' else pos or neg


def operations(width, shape, four):
    hi = width - 1
    index = 1 if shape == 'array' else hi
    ops = [('a', hi, '1'), ('a', 0, '1'), ('gate', None, '0'),
           ('b', 0, '1'), ('a', 0, '0'), ('gate', None, '1'),
           ('b', 0, '0'), ('index', None, index), ('a', hi, '0'),
           ('a', hi, '1'), ('index', None, 0), ('row1', 0, '1'),
           ('row0', 0, '1'), ('index', None, index), ('row1', hi, '1'),
           ('pick', None, '1'), ('a', 0, '1'), ('pick', None, '0'),
           ('b', hi, '1'), ('a', 0, '0'), ('b', 0, '1')]
    if four:
        ops += [('a', 0, 'z'), ('a', 0, 'x'), ('a', 0, '1'),
                ('b', 0, 'z'), ('b', 0, '0'), ('pick', None, 'x'),
                ('pick', None, 'z'), ('pick', None, '1'), ('gate', None, 'x'),
                ('a', 0, '0'), ('gate', None, '1'), ('a', 0, '1'),
                ('row1', 0, 'z'), ('row1', 0, 'x'), ('row1', 0, '1')]
    return ops


def exercise(width, shape, four, edge, gated):
    state = {x: ['0'] * width for x in ('a', 'b', 'row0', 'row1')}
    state.update(index=0, pick='0', gate='1')
    previous, hits = value(state, shape), 0
    lines = ['int errors=0;', '#0;', 'if(hits!=0)++errors;']
    for field, bit, changed in operations(width, shape, four):
        if bit is None:
            state[field] = changed
            text = str(changed) if field == 'index' else "1'b" + changed
            target = field
        else:
            state[field][bit] = changed
            target = ('rows[' + field[-1] + ']' if field.startswith('row') else field) + f'[{bit}]'
            text = "1'b" + changed
        current = value(state, shape)
        if matches(previous, current, edge) and (not gated or state['gate'] == '1'):
            hits += 1
        previous = current[:]
        lines += [f'#1;{target}={text};', f'#1;if(hits!={hits})++errors;']
    lines.append('$display("errors=%0d",errors);')
    return '\n'.join(lines)


def source(home, width, shape, four, edge, gated, spelling='local_auto'):
    kind = 'logic' if four else 'bit'
    value_type = f'{kind}[{width-1}:0]'
    declarations = f'''{value_type} a=0,b=0;
{value_type} rows[1:0]='{{default:'0}};
int index=0,hits=0;
logic pick=0,gate=1;
'''
    functions = ''
    prefix = ''
    expression = {'xor': 'a^b', 'indexed': 'a[index]', 'array': 'rows[index]',
                  'invert': '~a', 'reduction': '^a', 'conditional': 'pick?a:b'}.get(shape)
    if shape == 'function':
        lifetime = '' if spelling == 'local_static' else 'automatic '
        functions = f'''function {lifetime}{value_type} mix(input {value_type} left,right);
{value_type} work;
work=left^right;return work;
endfunction
'''
        expression = 'mix(a,b)'
        if spelling == 'package':
            prefix = 'package functions;\n' + functions + 'endpackage\n'
            functions, expression = '', 'functions::mix(a,b)'
        elif spelling == 'unit':
            prefix, functions = functions, ''
        elif spelling == 'class_static':
            prefix = 'class Utility;\nstatic ' + functions + 'endclass\n'
            functions, expression = '', 'Utility::mix(a,b)'
    guard = ' iff gate' if gated else ''
    event = ('posedge (' + expression + ')' + guard + ' or negedge (' + expression + ')' + guard
             if edge == 'both' else expression + guard if edge == 'any' else edge + ' (' + expression + ')' + guard)
    body = declarations + functions + 'always_ff @(' + event + ') hits<=hits+1;\ninitial begin\n'
    body += exercise(width, shape, four, edge, gated) + '\nend\n'
    if home == 'interface':
        return prefix + 'interface unit;\n' + body + 'endinterface\nmodule top;unit u();endmodule\n'
    if home == 'generate':
        return prefix + 'module top;if(1)begin:g\n' + body + 'end endmodule\n'
    return prefix + 'module top;\n' + body + 'endmodule\n'


def emit(name, body):
    name = 'ff_expression_' + name
    return name, f'''/*
:name: {name}
:description: always_ff watches expression values while preserving event and writer rules
:tags: sched always_ff expressions edges iff
:type: simulation
:lrm: 1800-2023 9.2.2.4 9.4.2 9.4.2.3
:oracle: spec
:spec_check: ai
:fourstate: yes
:nondeterministic: no
:generator: gen_ff_expression_matrix.py@1
:top_module: top
:expected_stdout: errors=0
:timeout: 15
*/
// Generated by tools/gen_ff_expression_matrix.py; edit the generator.
`timescale 1ns/1ns
{body}'''


def cases():
    for home, width, shape, four, edge, gated in itertools.product(
            HOMES, (1, 65, 129), ('xor', 'indexed', 'array', 'function'), (False, True), EDGES, (False, True)):
        name = f'{home}_{shape}_{width}_{"logic" if four else "bit"}_{edge}_{"iff" if gated else "plain"}'
        yield emit(name, source(home, width, shape, four, edge, gated))
    for home, shape, four, edge, gated in itertools.product(
            HOMES, ('invert', 'reduction', 'conditional'), (False, True), EDGES, (False, True)):
        name = f'{home}_{shape}_129_{"logic" if four else "bit"}_{edge}_{"iff" if gated else "plain"}'
        yield emit(name, source(home, 129, shape, four, edge, gated))
    for home, spelling, four, edge in itertools.product(
            HOMES, ('unit', 'package', 'class_static', 'local_static'), (False, True), ('posedge', 'negedge')):
        name = f'{home}_{spelling}_function_65_{"logic" if four else "bit"}_{edge}'
        yield emit(name, source(home, 65, 'function', four, edge, True, spelling))
    for home, edge in itertools.product(HOMES, ('posedge', 'negedge')):
        initial = 0 if edge == 'posedge' else 1
        body = f'''bit a={initial},b=0;int hits=0;
always_ff @({edge} (a^b))hits<=hits+1;
initial begin
 int errors=0;
 a={1-initial};#1;
 if(hits!=1)++errors;
 $display("errors=%0d",errors);
end
'''
        if home == 'interface':
            text = 'interface unit;\n' + body + 'endinterface\nmodule top;unit u();endmodule\n'
        elif home == 'generate':
            text = 'module top;if(1)begin:g\n' + body + 'end endmodule\n'
        else:
            text = 'module top;\n' + body + 'endmodule\n'
        yield emit(f'{home}_startup_{edge}', text)
    for count in (2, 9, 17):
        declarations = 'bit clock=0,enable=0;int errors=0;\n'
        checks = []
        for i in range(count):
            declarations += f'bit a{i}=0,b{i}=0;int hits{i}=0;\n'
            declarations += f'always_ff @(posedge (a{i}^b{i}) iff enable or negedge clock)hits{i}<=hits{i}+1;\n'
            checks.append(f'if(hits{i}!=2)++errors;')
        body = declarations + 'initial begin\nenable=1;clock=1;\n'
        body += '\n'.join(f'#1;a{i}=1;' for i in range(count))
        body += '\n#1;clock=0;#1;\n' + '\n'.join(checks) + '\n$display("errors=%0d",errors);\nend\n'
        yield emit(f'many_processes_{count}', 'module top;\n' + body + 'endmodule\n')

    for width, four, edge, gated in itertools.product((32, 257), (False, True), EDGES, (False, True)):
        yield emit(f'module_xor_{width}_{"logic" if four else "bit"}_{edge}_{"iff" if gated else "plain"}',
                   source('module', width, 'xor', four, edge, gated))
    for count in (2, 9, 17):
        declarations = 'bit reset=1,enable=1;int hits=0;\n'
        events, stimulus = ['negedge reset iff enable'], []
        for i in range(count):
            declarations += f'bit a{i}=0,b{i}=0;\n'
            events.append(f'posedge (a{i}^b{i}) iff enable')
            stimulus.append(f'#1;a{i}=1;')
        body = declarations + 'always_ff @(' + ' or '.join(events) + ')hits<=hits+1;\n'
        body += 'initial begin int errors=0;\n' + '\n'.join(stimulus)
        body += f'\n#1;reset=0;#1;if(hits!={count+1})++errors;\n$display("errors=%0d",errors);end\n'
        yield emit(f'many_items_{count}', 'module top;\n' + body + 'endmodule\n')
    for kind in ('dynamic', 'queue'):
        declaration = 'bit values[];' if kind == 'dynamic' else "bit values[$]='{0,0};"
        initialize = 'values=new[2];' if kind == 'dynamic' else ''
        replace = 'values=new[3](values);' if kind == 'dynamic' else 'values.push_back(0);'
        body = f'''module top;
{declaration}
int index=0,hits=0;
always_ff @(posedge values[index])hits<=hits+1;
initial begin
 int errors=0;
 {initialize}
 #1;values[1]=1;
 #1;index=1;
 #1;values[1]=0;
 #1;{replace}
 #1;values[1]=1;
 #1;if(hits!=2)++errors;
 $display("errors=%0d",errors);
end endmodule
'''
        yield emit('heap_' + kind, body)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--check', action='store_true')
    args = parser.parse_args()
    generated = dict(cases())
    failed = False
    for name, text in generated.items():
        path = OUT / (name + '.sv')
        if args.check:
            if not path.exists() or path.read_text() != text:
                print('out of date:', path.relative_to(OUT.parent.parent.parent))
                failed = True
        else:
            path.write_text(text)
    print(f'ff expression matrix: {len(generated)} sources')
    return int(failed)


if __name__ == '__main__':
    raise SystemExit(main())
