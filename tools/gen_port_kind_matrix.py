#!/usr/bin/env python3
# Copyright (c) 2026 The kirimasim Authors
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
"""Partial-port net kinds per physical bit (23.3.3.7/Table 23-1).

All 100 ordered kind pairs, both directions, varied static maps, widths and
containers. The existing independent resolution oracle enumerates strength
points; the effective-kind oracle below is separate from the C++ table.
"""
import argparse
import itertools
from pathlib import Path
from gen_resolution_matrix import KINDS, OWN, resolved

OUT = Path(__file__).resolve().parent.parent / 'tests/suite/ports'


def effective(inner, outer):
    if inner in ('wire', 'tri'):
        return outer
    if outer in ('wire', 'tri'):
        return inner
    if inner in ('supply0', 'supply1') and outer not in ('supply0', 'supply1'):
        return inner
    return outer


def generated(name, body):
    return f'''/*
:name: {name}
:description: Effective net kinds and implicit origins belong to connected physical bits
:tags: ports
:type: simulation
:lrm: 1800-2023 23.3.3.7 28.12 28.15
:oracle: spec
:spec_check: ai
:fourstate: yes
:nondeterministic: no
:generator: gen_port_kind_matrix.py@1
:top_module: top
:expected_stdout: errors=0
*/
{body}
'''


def container(body, context):
    if context == 'generate':
        return 'module top; if(1) begin:g\n' + body + 'end endmodule'
    if context == 'instance':
        return 'module holder;\n' + body + 'endmodule\nmodule top; holder h(); endmodule'
    return 'module top;\n' + body + 'endmodule'


def pair(inner, outer, direction, index):
    width = (1, 65, 129)[index % 3]
    size = width + 2
    context = ('top', 'generate', 'instance')[(index // 3) % 3]
    mapping = ('part', 'concat', 'array')[(index // 9) % 3]
    if width == 1:
        mapping = 'array' if mapping == 'array' else 'bit'
    asc = index % 2 == 1
    bounds = f'[0:{size-1}]' if asc else f'[{size-1}:0]'
    coord = lambda bit: size - 1 - bit if asc else bit
    suffix = '[0]' if mapping == 'array' else ''
    target = 'w' + suffix
    bits = list(range(1, width + 1))
    if mapping == 'bit':
        connection = f'{target}[{coord(1)}]'
    elif mapping == 'concat':
        # Reverse the physical order of two pieces; do not rely on uniform data.
        low = width // 2
        bits = list(range(low+1, width+1)) + list(range(1, low+1))
        connection = ('{' + f'{target}[{coord(low)}:{coord(1)}],'
                      + f'{target}[{coord(width)}:{coord(low+1)}]' + '}')
    else:
        connection = f'{target}[{coord(width)}:{coord(1)}]'
    arank, brank = ((3, 3), (3, 6), (6, 3))[index % 3]
    strength = {3: '(weak1,weak0)', 6: '(strong1,strong0)'}
    declarations = f'''logic [{width-1}:0] a,b;
{outer} {bounds} w{'[-1:0]' if suffix else ''};
leaf u({connection},a);
assign {strength[brank]} {connection}=b;
'''
    steps = [('0', '1'), ('1', '0'), ('x', 'z'), ('z', 'x'),
             ('z', 'z'), ('1', 'z'), ('z', '0'), ('z', 'z')]
    statements = ['int errors=0;']
    kind = effective(inner, outer)
    for a, b in steps:
        statements += [f"a='{a}; b='{b}; #1;"]
        value = resolved(kind, [(a, arank, arank), (b, brank, brank)])
        statements += [f"if(u.p !== {width}'b{value*width}) ++errors;"]
        expected = [resolved(outer, [])] * size
        for bit in bits:
            expected[bit] = value
        literal = ''.join(reversed(expected))
        statements += [f"if({target} !== {size}'b{literal}) ++errors;"]
        if suffix:
            statements += [f"if(w[-1] !== {size}'b{resolved(outer, [])*size}) ++errors;"]
    # Walk a single zero against ones to expose a permutation of selected bits.
    a, b = ['1'] * width, ['1'] * width
    a[0], b[0] = '0', '0'
    statements += [f"a={width}'b{''.join(reversed(a))}; b={width}'b{''.join(reversed(b))}; #1;"]
    expected = [resolved(outer, [])] * size
    formal = []
    for i, bit in enumerate(bits):
        value = resolved(kind, [(a[i], arank, arank), (b[i], brank, brank)])
        expected[bit] = value
        formal.append(value)
    statements += [f"if(u.p !== {width}'b{''.join(reversed(formal))}) ++errors;",
                   f"if({target} !== {size}'b{''.join(reversed(expected))}) ++errors;",
                   '$display("errors=%0d",errors);']
    body = declarations + '\ninitial begin\n' + '\n'.join(statements) + '\nend\n'
    leaf = f'''module leaf({direction} {inner} [{width-1}:0] p,input logic [{width-1}:0] d);
assign {strength[arank]} p=d;
endmodule
'''
    name = f'port_kind_{inner}_{outer}_{direction}_{context}_{mapping}_{width}'
    return name, generated(name, leaf + container(body, context))


def unwritten(inner, outer, direction, context):
    kind = effective(inner, outer)
    value = resolved(kind, [])
    neighbour = resolved(outer, [])
    name = f'port_kind_unwritten_{inner}_{outer}_{direction}_{context}'
    body = f'''{outer} [2:0] w;
leaf u(w[1]);
initial begin
int errors=0;
if(w !== 3'b{neighbour}{value}{neighbour} || u.p !== 1'b{value}) ++errors;
#1; if(w !== 3'b{neighbour}{value}{neighbour} || u.p !== 1'b{value}) ++errors;
$display("errors=%0d",errors);
end
'''
    leaf = f'module leaf({direction} {inner} p); endmodule\n'
    return name, generated(name, leaf + container(body, context))


def mixed(context, direction):
    name = f'port_kind_mixed_{context}_{direction}'
    body = '''logic a=0,b=1;
wire [3:0] w[2];
leaf u({w[1][2],w[0][1]},a);
assign (weak1,weak0) w[0][1]=b;
assign (weak1,weak0) w[1][2]=b;
initial begin
int errors=0;
#1; if(w[0] !== 4'bzz0z || w[1] !== 4'bz1zz || u.p !== 2'b10) ++errors;
a=1'bz; b=1'bz; #1;
if(w[0] !== 4'bzz0z || w[1] !== 4'bz1zz || u.p !== 2'b10) ++errors;
$display("errors=%0d",errors);
end
'''
    leaf = f'''module leaf({direction} tri0 lo,{direction} tri1 hi,input logic d);
assign (weak1,weak0) lo=d;
assign (weak1,weak0) hi=d;
endmodule
'''
    # Each formal has a distinct kind, while both actuals share the same storage slot.
    body = body.replace('leaf u({w[1][2],w[0][1]},a);', 'leaf u(w[0][1],w[1][2],a);')
    body = body.replace("u.p !== 2'b10", "{u.hi,u.lo} !== 2'b10")
    return name, generated(name, leaf + container(body, context))


def cases():
    for i, (inner, outer) in enumerate(itertools.product(KINDS, repeat=2)):
        for j, direction in enumerate(('input', 'output')):
            yield pair(inner, outer, direction, 2*i+j)
    for i, (inner, outer) in enumerate(itertools.product(OWN, repeat=2)):
        for j, direction in enumerate(('input', 'output')):
            yield unwritten(inner, outer, direction, ('top','generate','instance')[(i+j)%3])
    for context, direction in itertools.product(('top','generate','instance'), ('input','output')):
        yield mixed(context, direction)


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
                print(f'generated file differs: {path.name}')
                failed = True
        else:
            path.write_text(source)
    for path in OUT.glob('port_kind_*.sv'):
        if path.stem not in rows:
            print(f'unexpected generated file: {path.name}')
            failed = True
    print(f'{len(rows)} port-kind sources {"checked" if args.check else "generated"}')
    return int(failed)


if __name__ == '__main__':
    raise SystemExit(main())
