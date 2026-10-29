#!/usr/bin/env python3
# Copyright (c) 2026 The kirimasim Authors
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

"""Original declaration-order port matrix (7.6, 23.3.3.5, 23.3.3.7).

Expected bit strings are calculated here from ordinal correspondence and
strength dominance, independently of simulator output. No external sources.
"""
import argparse
import itertools
from pathlib import Path

OUT = Path(__file__).resolve().parent.parent / 'tests/suite/ports'
SHAPES = {
    'same': (((0, 1),), ((0, 1),)),
    'up': (((4, 5),), ((7, 6),)),
    'down': (((5, 4),), ((-2, -1),)),
    'rank2': (((1, 0), (4, 6)), ((3, 4), (-1, -3))),
}


def spelling(dims):
    return ''.join(f'[{left}:{right}]' for left, right in dims)


def elements(dims):
    return [''.join(f'[{n}]' for n in item) for item in itertools.product(*[
        range(left, right + (1 if left <= right else -1), 1 if left <= right else -1)
        for left, right in dims])]


def header(name):
    return f'''/*
:name: {name}
:description: Whole fixed-array ports preserve source bounds, ordinal identity and per-element sensitivity
:tags: ports
:type: simulation
:lrm: 1800-2023 7.6 23.3.3.5 23.3.3.7
:oracle: spec
:spec_check: ai
:fourstate: yes
:nondeterministic: no
:generator: gen_fixed_array_port_matrix.py@1
:top_module: top
:expected_stdout: @{name}.expected
*/
'''


def case(shape, width, context, mode, state):
    actual, formal = SHAPES[shape]
    ae, fe = elements(actual), elements(formal)
    name = f'fixed_array_port_{shape}_{width}_{context}_{mode}_{state}'
    scalar = 'logic' if state == 4 else 'bit'
    typ = f'{scalar} [{width-1}:0]'
    net = f'wire {typ}'
    inp = f'var {typ}' if mode == 'variable' else net
    out = net if mode in ('alias', 'strength') else f'var {typ}'
    strength = '(weak1,weak0) ' if mode == 'strength' else ''
    assigns = '\n'.join(f'assign {strength}q{f} = p{f};' for f in fe)
    construct = 'interface' if context == 'interface' else 'module'
    child = f'''{construct} leaf(input {inp} p{spelling(formal)}, output {out} q{spelling(formal)});
{assigns}
end{construct}
'''
    declarations = [f'{typ} stim{spelling(actual)};', f'{net if state == 4 else typ} q{spelling(actual)};']
    if mode in ('alias', 'strength'):
        declarations += [f'{net} a{spelling(actual)};']
        declarations += [f'assign a{a} = stim{a};' for a in ae]
        actual_name = 'a'
    else:
        actual_name = 'stim'
    if mode == 'strength':
        declarations += [f'assign (strong1,strong0) q{a} = ~stim{a};' for a in ae]
    if mode == 'resolvedcopy':
        declarations += [f"assign (weak1,weak0) q{a} = '1;" for a in ae]
    instance = f'leaf l({actual_name},q);'
    path = 'l'
    if context == 'generate':
        instance = f'if (1) begin: g\n{instance}\nend'
        path = 'g.l'
    mask = (1 << width) - 1
    values = [(0x19 + 37 * i) & mask for i in range(len(ae))]
    statements = [f"stim{a} = {width}'h{v:x};" for a, v in zip(ae, values)]
    expected = []
    for sample in range(3):
        if sample:
            k = (sample - 1) % len(ae)
            values[k] ^= 1 << ((width - 1) if sample == 1 else 0)
            statements.append(f"stim{ae[k]} = {width}'h{values[k]:x};")
        statements += ['#1;', '$display("' + ' '.join(['%b'] * (2 * len(ae))) + '", ' +
                       ', '.join(['q' + a for a in ae] + [path + '.p' + f for f in fe]) + ');']
        qvalues = [v ^ mask if mode == 'strength' else v for v in values]
        expected.append(' '.join(f'{v:0{width}b}' for v in qvalues + values) + '\n')
    statements += [f'$display("%0d %0d %0d %0d", $left(stim), $right(stim), $left({path}.p), $right({path}.p));']
    expected.append(f'{actual[0][0]} {actual[0][1]} {formal[0][0]} {formal[0][1]}\n')
    source = header(name) + child + '\nmodule top;\n' + '\n'.join(declarations) + '\n' + instance + '\ninitial begin\n' + '\n'.join(statements) + '\nend\nendmodule\n'
    return name, source, ''.join(expected)


def extra_cases():
    for width, shape in itertools.product((1, 8, 65, 129), ('up', 'rank2')):
        actual, formal = SHAPES[shape]
        ae, fe = elements(actual), elements(formal)
        name = f'fixed_array_port_force_bounds_{shape}_{width}'
        typ = f'logic [{width-1}:0]'
        source = header(name) + f"module leaf(inout wire {typ} p{spelling(formal)}); endmodule\n"
        source += f"module top; wire {typ} a{spelling(actual)}; leaf l(a);\n"
        source += '\n'.join(f"assign a{a} = '0;" for a in ae)
        source += f"\nlogic [128:0] idx; initial begin #1; force l.p{fe[0]} = '1; #1;\n"
        source += '$display("%b %b %b", a' + ae[0] + ', l.p' + fe[0] + ', a' + ae[-1] + ');\n'
        source += f"release a{ae[0]}; #1; $display(\"%b %b\", a{ae[0]}, l.p{fe[0]});\n"
        expected = '1' * width + ' ' + '1' * width + ' ' + '0' * width + '\n'
        expected += '0' * width + ' ' + '0' * width + '\n'
        # Runtime index validation occurs in the view before any ordinal mapping.
        tail = fe[0][fe[0].find(']')+1:]
        for index in ("129'h1000000000000000000000000", "'x", "'z", str(formal[0][0] + 10)):
            source += f"idx={index}; $display(\"%b\", l.p[idx]{tail});\n"
            expected += 'x' * width + '\n'
        source += '$display("%0d %0d", $size(l.p), $dimensions(l.p)); end endmodule\n'
        expected += f'{abs(formal[0][1]-formal[0][0])+1} {len(formal)+1}\n'
        yield name, source, expected
    for kind, width in itertools.product(('wire', 'wand', 'wor', 'tri0', 'tri1', 'supply0', 'supply1'), (1, 8, 65, 129)):
        name = f'fixed_array_port_resolution_{kind}_{width}'
        typ = f'[{width-1}:0]'
        source = header(name) + f"module top; {kind} {typ} a[7:4];\n"
        source += "assign a[7]='0; assign a[7]='1; assign (weak1,weak0) a[6]='0; assign a[6]='z;\n"
        source += "assign a[5]='1; initial begin #1; $display(\"%b %b %b %b\", a[7], a[6], a[5], a[4]); end endmodule\n"
        values = ('0' if kind in ('wand', 'supply0') else '1' if kind in ('wor', 'supply1') else 'x',
                  '1' if kind in ('tri1', 'supply1') else '0',
                  '0' if kind == 'supply0' else '1',
                  '0' if kind in ('tri0', 'supply0') else '1' if kind in ('tri1', 'supply1') else 'z')
        yield name, source, ' '.join(v*width for v in values) + '\n'
    for typ, zero, first, second, fmt in (
            ('real', '0.000000', '1.250000', '2.500000', '%f'),
            ('shortreal', '0.000000', '1.250000', '2.500000', '%f'),
            ('string', '', 'alpha', 'beta', '%s'),
            ):
        for variable in (False, True):
            name = f'fixed_array_port_typed_{typ}_{"var" if variable else "net"}'
            source = header(name) + f'module leaf(input {"var " if variable else ""}{typ} p[7:6], output var {typ} q[-2:-1]); assign q[-2]=p[7]; assign q[-1]=p[6]; endmodule\n'
            source += f'module top; {typ} a[4:5], q[4:5]; leaf l(a,q); initial begin #1; $display("{fmt}|{fmt}",q[4],q[5]); '
            v1, v2 = ('"alpha"', '"beta"') if typ == 'string' else ('null','null') if typ == 'chandle' else ('1.25','2.5')
            source += f'a[4]={v1};a[5]={v2};#1;$display("{fmt}|{fmt}",q[4],q[5]);a[4]={v2};#1;$display("{fmt}|{fmt}",q[4],q[5]);end endmodule\n'
            source = source.replace('end endmodule\n', '$display("%0d %0d %0d",$left(l.p),$right(l.p),$dimensions(l.p));end endmodule\n')
            yield name, source, f'{zero}|{zero}\n{first}|{second}\n{second}|{second}\n7 6 {2 if typ == "string" else 1}\n' 
    for drive in ('none', 'pull0', 'pull1', 'default'):
        name = f'fixed_array_port_unconnected_{drive}'
        prefix = '' if drive in ('none','default') else f'`unconnected_drive {drive}\n'
        initializer = " = '{8'h12,8'h34}" if drive == 'default' else ''
        source = header(name) + prefix + f'module leaf(input wire [7:0] p[7:6]{initializer});endmodule\n'
        if prefix:
            source += '`nounconnected_drive\n'
        source += 'module top;leaf l(); initial begin #1;$display("%h %h",l.p[7],l.p[6]);end endmodule\n'
        expected = '12 34\n' if drive == 'default' else '00 00\n' if drive == 'pull0' else 'ff ff\n' if drive == 'pull1' else 'zz zz\n'
        yield name, source, expected
    for width in (8, 65):
        for effect in (False, True):
            name = f'fixed_array_port_conditional_{width}_{"effect" if effect else "value"}'
            source = header(name).replace('7.6 23.3.3.5', '7.6 11.4.11 23.3.3.5') + f'typedef logic [{width-1}:0] pair_t[2]; module leaf(input var pair_t p);endmodule\n'
            source += "module top; pair_t a='{1,2},b='{7,9};logic pick=0;int calls;function automatic pair_t next_value(input bit which);calls++;return which?a:b;endfunction\n"
            left, right = ('next_value(1)', 'next_value(0)') if effect else ('a','b')
            source += f'leaf l(pick?{left}:{right});initial begin #1;'
            expected = ''
            calls = 0
            for stage, pick in enumerate(('0','1','x')):
                if stage:
                    source += f"pick=1'b{pick};#1;"
                calls += (2 if pick == 'x' else 1) if effect else 0
                source += '$display("%b %b %0d",l.p[0],l.p[1],calls);'
                a, b = (1,2), (7,9)
                values = a if pick == '1' else b
                if pick == 'x':
                    # 11.4.11 compares complete array elements. Unequal logic
                    # elements take the all-X Table 7-1 default (7.4.5).
                    bits = [f'{x:0{width}b}' if x == y else 'x' * width
                            for x, y in zip(a, b)]
                else:
                    bits = [f'{n:0{width}b}' for n in values]
                expected += ' '.join(bits) + f' {calls}\n'
            source += 'end endmodule\n'
            yield name, source, expected
    for width in (1,8,65):
        name = f'fixed_array_port_delayed_{width}'
        source = header(name) + f"module top; logic [{width-1}:0] d='0;wire [{width-1}:0] q[4:5];\n"
        source += "assign #2 q[4]=d;assign (weak1,weak0) q[4]='1;assign #3 q[5]=~d;assign (weak1,weak0) q[5]='0;\n"
        source += 'initial begin #4;$display("%b %b",q[4],q[5]);d=\'1;#1;$display("%b %b",q[4],q[5]);#3;$display("%b %b",q[4],q[5]);end endmodule\n'
        yield name, source, '0'*width+' '+'1'*width+'\n'+'0'*width+' '+'1'*width+'\n'+'1'*width+' '+'0'*width+'\n'


def neighboring_cases():
    for width, kind in itertools.product((8, 65, 129), ('wire', 'wand', 'wor')):
        name = f'fixed_array_port_partial_{kind}_{width}'
        half = width // 2
        source = header(name) + f"module top; {kind} [{width-1}:0] a[4:5];\n"
        source += f"assign a[4]='0;assign a[4][{width-1}:{half}]='1;assign a[5][{width-1}:{half}]='1;assign a[5][{half-1}:0]='0;\n"
        source += 'initial begin #1;$display("%b %b",a[4],a[5]);end endmodule\n'
        top = 'x' if kind == 'wire' else '0' if kind == 'wand' else '1'
        yield name, source, top*(width-half)+'0'*half+' '+'1'*(width-half)+'0'*half+'\n'
    for width in (8, 65, 129):
        name = f'fixed_array_port_transitive_{width}'
        source = header(name) + f"module leaf(inout wire [{width-1}:0] p[-3:-4]);assign (weak1,weak0)p[-3]='1;assign p[-4]='1;endmodule\n"
        source += f"module middle(inout wire [{width-1}:0] p[7:6]);leaf l(p);endmodule\n"
        source += f"module top;wire [{width-1}:0] a[2:3];middle m(a);assign a[2]='0;initial begin #1;$display(\"%b %b %b\",a[2],m.p[7],m.l.p[-3]);force m.l.p[-4]='0;#1;$display(\"%b %b\",a[3],m.p[6]);release a[3];#1;$display(\"%b\",m.l.p[-4]);end endmodule\n"
        yield name, source, ('0'*width+' ')*2+'0'*width+'\n'+'0'*width+' '+'0'*width+'\n'+'1'*width+'\n'
    for drive in ('pull0', 'pull1'):
        name = f'fixed_array_port_pull_strength_{drive}'
        weak = '1' if drive == 'pull0' else '0'
        value = '0' if drive == 'pull0' else '1'
        source = header(name) + f"`unconnected_drive {drive}\nmodule leaf(input wire [64:0] p[7:6]);assign (weak1,weak0)p[7]='{weak};endmodule\n`nounconnected_drive\n"
        source += 'module top;leaf l();initial begin #1;$display("%b %b",l.p[7],l.p[6]);end endmodule\n'
        yield name, source, value*65+' '+value*65+'\n'
    name = 'fixed_array_port_default_scope'
    source = header(name) + "module leaf #(parameter int V=7)(input wire [7:0] p[7:6]='{V, V+1});endmodule\nmodule top;parameter V=99;leaf #(.V(12))l();initial begin #1;$display(\"%0d %0d\",l.p[7],l.p[6]);end endmodule\n"
    yield name, source, '12 13\n'
    for width in (8,65):
        name = f'fixed_array_port_program_{width}'
        source = header(name) + f"program worker(input logic clk,input var logic [{width-1}:0] p[7:6],output var logic [{width-1}:0] q[-2:-1]);\n"
        source += 'initial begin @(posedge clk);$display("%0d %0d",p[7],p[6]);q=p;#0;$display("%0d %0d",top.q[4],top.q[5]);#1;end endprogram\n'
        source += f"module top;logic clk=0;logic [{width-1}:0] a[4:5];wire [{width-1}:0] q[4:5];worker p(clk,a,q);initial begin a='{{0,0}};#2;a='{{5,9}};a[4]<=7;clk=1;#10;end endmodule\n"
        yield name, source, '7 9\n7 9\n'


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--check', action='store_true')
    args = parser.parse_args()
    failures = []
    settings = itertools.product(SHAPES, (1, 8, 65, 129), ('module', 'generate', 'interface'),
                                 ('copy', 'variable', 'alias', 'strength', 'resolvedcopy'), (2, 4))
    cases = [case(*setting) for setting in settings
             if setting[-1] == 4 or setting[-2] == 'variable']
    cases.extend(extra_cases())
    cases.extend(neighboring_cases())
    for name, source, expected in cases:
        for suffix, data in (('.sv', source), ('.expected', expected)):
            path = OUT / (name + suffix)
            if args.check:
                if not path.exists() or path.read_text() != data:
                    failures.append(str(path.relative_to(OUT.parent.parent.parent)))
            else:
                path.parent.mkdir(parents=True, exist_ok=True)
                path.write_text(data)
    count = len(cases)
    if failures:
        raise SystemExit('stale matrix files: ' + ', '.join(failures))
    print(f'{count} fixed-array port cases {"checked" if args.check else "generated"}')


if __name__ == '__main__':
    main()
