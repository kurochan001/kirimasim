#!/usr/bin/env python3
# Copyright (c) 2026 The kirimasim Authors
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

"""Original physical-bit mappings for fixed-array element ports (7.4, 23.3.3.7).

An independent per-bit strength oracle computes settled values. It also checks
every untouched bit and element, rather than just the connected formal.
"""
import argparse
import itertools
from pathlib import Path

OUT = Path(__file__).resolve().parent.parent / 'tests/suite/ports'
SHAPES = {
    'up': (((4, 5),), ('[4]', '[5]')),
    'down': (((-1, -2),), ('[-1]', '[-2]')),
    'rank2': (((2, 1), (-1, 1)), ('[2][-1]', '[1][0]')),
}


def dimensions(dims):
    return ''.join(f'[{a}:{b}]' for a, b in dims)


def elements(dims):
    return [''.join(f'[{x}]' for x in indices) for indices in itertools.product(*[
        range(a, b + (1 if a <= b else -1), 1 if a <= b else -1) for a, b in dims])]


def resolve(a, b, arank, brank):
    if a == 'z':
        return b
    if b == 'z':
        return a
    if a == b:
        return a
    if arank == brank:
        return 'x'
    return a if arank > brank else b


def generated(name, body):
    return f'''/*
:name: {name}
:description: Static array-element ports preserve physical origins, bounds and untouched bits
:tags: ports
:type: simulation
:lrm: 1800-2023 7.4 23.3.3.7 28.12
:oracle: spec
:spec_check: ai
:fourstate: yes
:nondeterministic: no
:generator: gen_array_element_port_matrix.py@1
:top_module: top
:expected_stdout: errors=0
*/
{body}
'''


def case(width, shape, mapping, direction, context, profile, ascending=False):
    dims, (other, target) = SHAPES[shape]
    es = elements(dims)
    size = width if mapping == 'whole' else width + 2
    bounds = f'[0:{size-1}]' if ascending else f'[{size-1}:0]'
    coordinate = lambda bit: size - 1 - bit if ascending else bit
    # List source physical positions from the formal's least significant bit.
    if mapping == 'whole':
        bits = [(target, bit) for bit in range(width)]
        connection = 'w' + target
    elif mapping == 'bit':
        bits = [(target, width)]
        width = 1
        connection = f'w{target}[{coordinate(bits[0][1])}]'
    elif mapping == 'part':
        bits = [(target, bit + 1) for bit in range(width)]
        connection = f'w{target}[{coordinate(width)}:{coordinate(1)}]'
    else:
        low = width // 2
        bits = [(target, bit) for bit in range(low)] + [
            (other, bit) for bit in range(low, width)]
        connection = ('{' + f'w{other}[{coordinate(width-1)}:{coordinate(low)}],'
                      + f'w{target}[{coordinate(low-1)}:{coordinate(0)}]' + '}')
    ranks = [(3, 3), (3, 6), (6, 3)][profile]
    strengths = {3: '(weak1,weak0)', 6: '(strong1,strong0)'}
    name = f'array_element_port_{shape}_{size}_{mapping}_{direction}_{context}_{profile}'
    if ascending:
        name += '_ascending'
    declarations = f'''logic [{width-1}:0] a,b;
wire {bounds} w{dimensions(dims)};
leaf u({connection},a);
assign {strengths[ranks[1]]} {connection}=b;
'''
    steps = [('0', '1'), ('0', 'z'), ('z', 'z'), ('1', '0'), ('x', 'z'), ('z', 'z')]
    statements = ['int errors=0;']
    for a, b in steps:
        statements += [f"a='{a}; b='{b}; #1;"]
        value = resolve(a, b, *ranks)
        statements += [f"if(u.p !== {width}'b{value*width}) ++errors;"]
        expected = {e: ['z'] * size for e in es}
        for e, bit in bits:
            expected[e][bit] = value
        for e in es:
            literal = ''.join(reversed(expected[e]))
            statements += [f"if(w{e} !== {size}'b{literal}) ++errors;"]
    statements += ['$display("errors=%0d",errors);']
    body = declarations + '\ninitial begin\n' + '\n'.join(statements) + '\nend\n'
    if context == 'top':
        body = 'module top;\n' + body + 'endmodule'
    elif context == 'generate':
        body = 'module top; if(1) begin:g\n' + body + 'end endmodule'
    else:
        body = ('module holder;\n' + body + 'endmodule\n'
                'module top; holder h(); endmodule')
    leaf = f'''module leaf({direction} wire [{width-1}:0] p,input logic [{width-1}:0] d);
assign {strengths[ranks[0]]} p=d;
endmodule
'''
    return name, generated(name, leaf + body)


def aliases(context, direction):
    name = f'array_element_port_alias_{context}_{direction}'
    body = '''logic a=0,b=1;
wire [1:0] w[-1:0];
middle m(w,a);
assign (weak1,weak0) w[0][1]=b;
initial begin
int errors=0;
#1; if(w[0] !== 2'bxz || m.u.p !== 1'bx) ++errors;
b=1'bz; #1; if(w[0] !== 2'b0z || m.u.p !== 1'b0) ++errors;
a=1'bz; #1; if(w[0] !== 2'bzz || m.u.p !== 1'bz) ++errors;
if(w[-1] !== 2'bzz) ++errors;
if($left(m.bus)!=5 || $right(m.bus)!=4) ++errors;
$display("errors=%0d",errors);
end
'''
    if context == 'generate':
        body = 'if(1) begin:g\n' + body + 'end\n'
    elif context == 'instance':
        body = 'holder h(); endmodule\nmodule holder;\n' + body
    source = f'''module leaf({direction} wire p,input logic d);
assign (weak1,weak0) p=d;
endmodule
module middle(inout wire [1:0] bus[5:4], input logic d);
leaf u(bus[4][1],d);
endmodule
module top;
{body}
endmodule'''
    return name, generated(name, source)


def cases():
    for wi, width in enumerate((1, 65, 129)):
        for si, shape in enumerate(SHAPES):
            for mi, mapping in enumerate(('whole', 'bit', 'part', 'concat')):
                if mapping == 'concat' and width == 1:
                    continue
                for di, direction in enumerate(('input', 'output')):
                    for ci, context in enumerate(('top', 'generate', 'instance')):
                        yield case(width, shape, mapping, direction, context,
                                   (wi + si + mi + di + ci) % 3,
                                   ascending=(wi + si + mi + di + ci) % 2 == 1)
    for context, direction in itertools.product(('top', 'generate', 'instance'),
                                               ('input', 'output')):
        yield aliases(context, direction)
    for spelling in ('parameter', 'genvar', 'hierarchical', 'packed'):
        name = 'array_element_port_spelling_' + spelling
        leaf = '''module leaf(output wire [64:0] p);
assign (weak1,weak0) p='0;
endmodule
'''
        if spelling == 'genvar':
            body = '''wire [64:0] w[-1:0];
for(genvar i=-1;i<=0;i++) begin:g
leaf u(w[i]);
assign (weak1,weak0) w[i]='1;
end
initial begin #1;
if(w[-1] === {65{1'bx}} && w[0] === {65{1'bx}})
  $display("errors=0"); else $display("errors=1");
end'''
        elif spelling == 'packed':
            body = '''wire [1:0][64:0] w[-1:0];
leaf u(w[0][1]);
assign (weak1,weak0) w[0][1]='1;
initial begin #1;
if(w[0][1] === {65{1'bx}} && w[0][0] === {65{1'bz}} &&
   w[-1] === {130{1'bz}} && u.p === {65{1'bx}})
  $display("errors=0"); else $display("errors=1");
end'''
        else:
            at = 'top.w[0]' if spelling == 'hierarchical' else 'w[INDEX]'
            body = f'''localparam int INDEX=0;
wire [64:0] w[-1:0];
leaf u({at});
assign (weak1,weak0) w[0]='1;
initial begin #1;
if(w[0] === {{65{{1'bx}}}} && w[-1] === {{65{{1'bz}}}} && u.p === {{65{{1'bx}}}})
  $display("errors=0"); else $display("errors=1");
end'''
        yield name, generated(name, leaf + 'module top;\n' + body + '\nendmodule')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--check', action='store_true')
    args = parser.parse_args()
    rows = dict(cases())
    failed = False
    for name, source in rows.items():
        path = OUT / (name + '.sv')
        if args.check:
            if not path.exists() or path.read_text() != source:
                print('out of date:', path.name)
                failed = True
        else:
            path.write_text(source)
    print(f'array element port matrix: {len(rows)} sources')
    return int(failed)


if __name__ == '__main__':
    raise SystemExit(main())
