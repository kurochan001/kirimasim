#!/usr/bin/env python3
# Copyright (c) 2026 The kirimasim Authors
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
"""Selected blocking writes of class properties: bits, parts and packed members.

Every expected value is computed here from the declared range of the property
(IEEE 1800-2023 7.2.1, 7.4.1, 8.4, 11.4.12, 11.5.1, 13.5), not from a run. A
position outside the property or with X/Z bits writes nothing, and a position
partly outside writes only the bits inside (11.5.1; VALUE_SEMANTICS.md 3.11).
"""
import argparse
from pathlib import Path
from typing import NamedTuple

from gen_class_static_matrix import wrap

OUT = Path(__file__).resolve().parent.parent / 'tests/suite/proc'
HOMES = ('unit', 'module', 'package', 'interface', 'program', 'generate')


class Declared(NamedTuple):
    text: str
    left: int
    right: int
    four: bool

    @property
    def width(self):
        return abs(self.left - self.right) + 1

    def index(self, position):
        """The selector that names a bit position, 0 being the least significant."""
        return self.right + position if self.left >= self.right else self.right - position

    def plus(self, low, width):
        """The start of `[start+:width]` naming the bits from position low up."""
        return self.index(low + width - 1) if self.left < self.right else self.index(low)

    def minus(self, low, width):
        """The start of `[start-:width]` naming the bits from position low up."""
        return self.index(low) if self.left < self.right else self.index(low + width - 1)

    def part(self, low, width):
        return f'[{self.index(low + width - 1)}:{self.index(low)}]'


TYPES = {
    'logic8': Declared('logic [7:0]', 7, 0, True),
    'bit8': Declared('bit [7:0]', 7, 0, False),
    'int': Declared('int', 31, 0, False),
    'integer': Declared('integer', 31, 0, True),
    'offset': Declared('logic [15:8]', 15, 8, True),
    'ascending': Declared('logic [0:7]', 0, 7, True),
    'logic65': Declared('logic [64:0]', 64, 0, True),
    'logic129': Declared('logic [128:0]', 128, 0, True),
    'bit129': Declared('bit [128:0]', 128, 0, False),
}


def pattern(width, four):
    """Bits by position: every bit a write must leave alone is visibly itself."""
    return [('x10z' if four else '10')[i % (4 if four else 2)] for i in range(width)]


def literal(bits):
    return f"{len(bits)}'b" + ''.join(reversed(bits))


def written(bits, low, value, four=True):
    """bits with value, most significant first, at position low and up."""
    result = list(bits)
    for k, bit in enumerate(reversed(value)):
        if 0 <= low + k < len(result):
            result[low + k] = bit if four or bit in '01' else '0'
    return result


def opposite(bits, low, width):
    """A value that differs from the bits there, most significant first."""
    return ''.join('0' if bits[i] == '1' else '1' for i in reversed(range(low, low + width)))


def added(bits, low, width, amount):
    part = bits[low:low + width]
    if set(part) - {'0', '1'}:
        return 'x' * width
    value = sum(int(b) << i for i, b in enumerate(part)) + amount
    return format(value & ((1 << width) - 1), f'0{width}b')


def xored(bits, low, value):
    width = len(value)
    part = reversed(bits[low:low + width])
    return ''.join('x' if a not in '01' else str(int(a) ^ int(b)) for a, b in zip(part, value))


def statements(t):
    """Name, statement with {T} as the property, its effect, and how many
    times one statement evaluates the property's receiver."""
    w, four = t.width, t.four
    init = pattern(w, four)
    low = w // 3
    const = t.part(low, 3)
    value = opposite(init, low, 3)
    top = w - 1
    yield 'bit_const', f"{{T}}[{t.index(low)}]=1'b{opposite(init, low, 1)};", \
        written(init, low, opposite(init, low, 1), four), 1
    yield 'bit_runtime', f"at={t.index(low + 1)};{{T}}[at]=1'b{opposite(init, low + 1, 1)};", \
        written(init, low + 1, opposite(init, low + 1, 1), four), 1
    yield 'bit_lowest', f"{{T}}[{t.index(0)}]=1'b{opposite(init, 0, 1)};", \
        written(init, 0, opposite(init, 0, 1), four), 1
    yield 'bit_highest', f"at={t.index(top)};{{T}}[at]=1'b{opposite(init, top, 1)};", \
        written(init, top, opposite(init, top, 1), four), 1
    yield 'bit_x', f"{{T}}[{t.index(low + 1)}]=1'bx;", written(init, low + 1, 'x', four), 1
    yield 'bit_z', f"at={t.index(low)};{{T}}[at]=1'bz;", written(init, low, 'z', four), 1
    yield 'part_const', f"{{T}}{const}=3'b{value};", written(init, low, value, four), 1
    yield 'part_narrow_value', f"{{T}}{const}=1'b1;", written(init, low, '001', four), 1
    yield 'part_wide_value', f"{{T}}{const}=8'hf6;", written(init, low, '110', four), 1
    yield 'part_signed_value', f"{{T}}{const}=-1;", written(init, low, '111', four), 1
    yield 'part_plus', f"at={t.plus(low, 3)};{{T}}[at+:3]=3'b{value};", written(init, low, value, four), 1
    yield 'part_minus', f"at={t.minus(low, 3)};{{T}}[at-:3]=3'b{value};", written(init, low, value, four), 1
    if w > 64:
        across = opposite(init, 61, 4)
        yield 'part_across_words', f"{{T}}{t.part(61, 4)}=4'b{across};", written(init, 61, across, four), 1
        yield 'part_plus_across_words', f"at={t.plus(61, 4)};{{T}}[at+:4]=4'b{across};", \
            written(init, 61, across, four), 1
    above = max(t.left, t.right) + 200
    below = min(t.left, t.right) - 1
    yield 'outside_above', f"at={above};{{T}}[at]=1'b1;", init, 1
    yield 'outside_below', f"at={below};{{T}}[at]=1'b1;", init, 1
    yield 'outside_part', f"at={above};{{T}}[at+:3]=3'b111;", init, 1
    yield 'unknown_bit', "{T}[xi]=1'b1;", init, 1
    yield 'unknown_part', "{T}[xi+:3]=3'b111;", init, 1
    yield 'partly_above', f"at={t.plus(w - 2, 4)};{{T}}[at+:4]=4'b1001;", written(init, w - 2, '1001', four), 1
    yield 'partly_below', f"at={t.plus(-2, 4)};{{T}}[at+:4]=4'b1001;", written(init, -2, '1001', four), 1
    yield 'compound_add', f"{{T}}{const}+=3;", written(init, low, added(init, low, 3, 3), four), 1
    yield 'compound_xor', f"{{T}}{const}^=3'b101;", written(init, low, xored(init, low, '101'), four), 1
    yield 'increment', f"{{T}}{const}++;", written(init, low, added(init, low, 3, 1), four), 1
    yield 'decrement', f"--{{T}}{const};", written(init, low, added(init, low, 3, -1), four), 1
    upper, lower = opposite(init, 4, 3), opposite(init, 0, 3)
    yield 'concatenation', f"{{{{T}}{t.part(4, 3)},{{T}}{t.part(0, 3)}}}=6'b{upper}{lower};", \
        written(written(init, 4, upper, four), 0, lower, four), 2
    yield 'output_argument', f"Box::put({{T}}{const},3'b{value});", written(init, low, value, four), 1


def definitions(declaration, init, prefix='', bodies=()):
    methods = ''.join(f'function void {name}();\nint at;logic [31:0] xi=\'x;\n{body}\nendfunction\n'
                      for name, body in bodies)
    return f'''{prefix}class Parent;
{declaration} p;
endclass
class Box extends Parent;
{declaration} v;
{declaration} a[-1:1];
static {declaration} s;
function new();
v={init};p={init};a[-1]={init};a[0]={init};a[1]={init};
endfunction
static function void put(output logic [2:0] o,input logic [2:0] n);o=n;endfunction
{methods}endclass
class Holder;
Box inner=new;
endclass
'''


def spellings(statement, expect, init, calls):
    """Methods, and the checks of a body, for every way of naming the property."""
    use = lambda name: statement.replace('{T}', name)
    bodies = (('by_this', use('this.v')), ('by_name', use('v')), ('by_inherited', use('p')),
              ('by_super', use('super.p')), ('by_element', use('a[0]')), ('by_this_element', use('this.a[0]')))
    checks = f'''object=new;{use('object.v')}
if(object.v!=={expect}) ++errors;
object=new;calls=0;{use('receiver().v')}
if(object.v!=={expect}||calls!={calls}) ++errors;
holder=new;{use('holder.inner.v')}
if(holder.inner.v!=={expect}) ++errors;
object=new;{use('object.a[index]')}
if(object.a[0]!=={expect}||object.a[-1]!=={init}||object.a[1]!=={init}) ++errors;
object=new;object.by_this();
if(object.v!=={expect}||object.p!=={init}) ++errors;
object=new;object.by_name();
if(object.v!=={expect}||object.p!=={init}) ++errors;
object=new;object.by_inherited();
if(object.p!=={expect}||object.v!=={init}) ++errors;
object=new;object.by_super();
if(object.p!=={expect}||object.v!=={init}) ++errors;
object=new;object.by_element();
if(object.a[0]!=={expect}||object.a[1]!=={init}||object.v!=={init}) ++errors;
object=new;object.by_this_element();
if(object.a[0]!=={expect}||object.a[1]!=={init}||object.v!=={init}) ++errors;
Box::s={init};{use('object.s')}
if(Box::s!=={expect}) ++errors;
Box::s={init};{use('Box::s')}
if(Box::s!=={expect}) ++errors;
plain={init};{use('plain')}
if(plain!=={expect}) ++errors;
$display("errors=%0d",errors);'''
    return bodies, checks


ACTOR = '''Box object;Holder holder;int calls=0;int at;int index=0;logic [31:0] xi='x;
function Box receiver();++calls;return object;endfunction
'''


def source(name, description, text, expected='errors=0', extra='', lrm='7.2.1 7.4.1 8.4 11.4.12 11.5.1'):
    name = 'class_select_' + name
    return name, f'''/*
:name: {name}
:description: {description}
:tags: proc class selects
:type: simulation
:lrm: 1800-2023 {lrm}
:oracle: spec
:spec_check: ai
:fourstate: yes
:nondeterministic: no
:generator: gen_class_select_matrix.py@1
:top_module: top
:expected_stdout: {expected}
{extra}:timeout: 15
*/
// Generated by tools/gen_class_select_matrix.py; edit the generator.
`timescale 1ns/1ns
{text}'''


def forms():
    """Every statement of every type, each through every way of naming the property."""
    for kind, t in TYPES.items():
        init = literal(pattern(t.width, t.four))
        for shape, statement, bits, calls in statements(t):
            bodies, checks = spellings(statement, literal(bits), init, calls)
            text = wrap('unit', definitions(t.text, init, bodies=bodies), 'int errors=0;\n' + checks,
                        actor=ACTOR + f'{t.text} plain;\n')
            yield source(f'{kind}_{shape}', 'A selected write of a class property changes only the selected bits',
                         text)


def homes():
    """The same statements give the same bits in every container and lifetime."""
    for home in HOMES:
        for kind in ('logic129', 'int', 'ascending'):
            t = TYPES[kind]
            init = literal(pattern(t.width, t.four))
            bits = pattern(t.width, t.four)
            low = t.width // 3
            lines = []
            for statement, position, value in ((f"{{T}}[{t.index(low)}]=1'b{opposite(bits, low, 1)};", low,
                                                opposite(bits, low, 1)),
                                               (f"at={t.index(low + 1)};{{T}}[at]=1'b{opposite(bits, low + 1, 1)};",
                                                low + 1, opposite(bits, low + 1, 1)),
                                               (f"{{T}}{t.part(0, 2)}=2'b{opposite(bits, 0, 2)};", 0,
                                                opposite(bits, 0, 2)),
                                               (f"at={t.plus(low + 3, 3)};{{T}}[at+:3]=3'b{opposite(bits, low + 3, 3)};",
                                                low + 3, opposite(bits, low + 3, 3))):
                lines.append(statement)
                bits = written(bits, position, value, t.four)
            expect = literal(bits)
            use = lambda name: '\n'.join(s.replace('{T}', name) for s in lines)
            body = f'''object=new;
{use('object.v')}
if(object.v!=={expect}) ++errors;
object=new;
{use('object.a[index]')}
if(object.a[0]!=={expect}||object.a[1]!=={init}) ++errors;
object=new;object.by_name();
if(object.v!=={expect}) ++errors;
$display("errors=%0d",errors);'''
            classes = definitions(t.text, init, bodies=(('by_name', use('v')),))
            yield source(f'{home}_{kind}_static', 'Selected writes of class properties do not depend on the container',
                         wrap(home, classes, 'int errors=0;\n' + body, actor=ACTOR))
            actor = ("task automatic exercise();\nBox object;int at;int index=0;int errors=0;\n" + body +
                     "\nendtask\n")
            yield source(f'{home}_{kind}_automatic', 'Selected writes of class properties do not depend on the lifetime',
                         wrap(home, classes, 'exercise();', actor=actor))


PAIR = Declared('value_t', 7, 0, True)
LOW = 'logic [64:0]'
PACKED = {
    'struct': ("typedef struct packed {logic [3:0] hi;struct packed {logic [1:0] a;logic [1:0] b;} lo;} value_t;\n", 8, (
        ('member', "{T}.hi=4'b1010;", 4, '1010'),
        ('nested_member', "{T}.lo.a=2'b10;", 2, '10'),
        ('member_bit', "{T}.hi[2]=1'b1;", 6, '1'),
        ('nested_member_bit', "{T}.lo.b[1]=1'b1;", 1, '1'),
        ('member_part', "{T}.hi[2:1]=2'b01;", 5, '01'),
        ('member_runtime_bit', "at=3;{T}.hi[at]=1'b1;", 7, '1'),
        ('member_runtime_part', "at=1;{T}.hi[at+:2]=2'b01;", 5, '01'),
        ('member_outside', "at=9;{T}.lo.a[at]=1'b1;", 0, ''),
        ('member_partly_outside', "at=2;{T}.hi[at+:4]=4'b1001;", 6, '01'),
        ('member_runtime_compound', "at=0;{T}.lo.a[at]+=1;", 2, '1'),
    )),
    'wide_struct': (f"typedef struct packed {{{LOW} hi;logic [63:0] lo;}} value_t;\n", 129, (
        ('member_top', "{T}.hi[64]=1'b1;", 128, '1'),
        ('member_across', "{T}.lo[63]=1'b1;", 63, '1'),
        ('member_runtime', "at=2;{T}.hi[at+:4]=4'b0110;", 66, '0110'),
        ('member_outside', "at=70;{T}.lo[at]=1'b1;", 0, ''),
        ('member_part_outside_neighbour', "at=62;{T}.lo[at+:4]=4'b1001;", 62, '01'),
    )),
    'union': ("typedef union packed {logic [7:0] whole;struct packed {logic [3:0] hi;logic [3:0] lo;} parts;} value_t;\n", 8, (
        ('member', "{T}.whole[3]=1'b1;", 3, '1'),
        ('nested_member', "{T}.parts.hi=4'b0110;", 4, '0110'),
        ('nested_member_runtime', "at=2;{T}.parts.lo[at]=1'b1;", 2, '1'),
    )),
    'array': ('typedef logic [3:0][7:0] value_t;\n', 32, (
        ('element', "{T}[2]=8'h12;", 16, '00010010'),
        ('element_runtime', "at=3;{T}[at]=8'h34;", 24, '00110100'),
        ('element_bits', "{T}[1][3:0]=4'b0111;", 8, '0111'),
        ('element_runtime_bit', "at=2;{T}[at][3]=1'b1;", 19, '1'),
        ('element_runtime_bits', "at=2;{T}[at][5:4]=2'b01;", 20, '01'),
        ('bit_runtime', "at=5;{T}[1][at]=1'b1;", 13, '1'),
        ('both_runtime', "at=2;other=6;{T}[at][other]=1'b1;", 22, '1'),
        ('part', "{T}[3:2]=16'hbeef;", 16, '1011111011101111'),
        ('part_runtime', "at=1;{T}[at+:2]=16'h1234;", 8, '0001001000110100'),
        ('element_outside', "at=9;{T}[at]=8'hff;", 0, ''),
        ('element_outside_bit', "at=9;{T}[at][1]=1'b1;", 0, ''),
        ('child_outside', "at=9;{T}[1][at]=1'b1;", 0, ''),
        ('child_of_runtime_outside', "at=1;other=9;{T}[at][other]=1'b1;", 0, ''),
        ('child_partly_outside', "at=6;{T}[1][at+:4]=4'b1001;", 14, '01'),
        ('element_unknown', "{T}[xi]=8'hff;", 0, ''),
        ('child_unknown', "{T}[1][xi]=1'b1;", 0, ''),
    )),
}


def packed():
    """Members of packed structures and unions, and elements of packed arrays.
    A child outside its parent never reaches a neighbour (7.4.5, 11.5.1)."""
    for kind, (typedef, width, cases) in PACKED.items():
        init = literal(pattern(width, True))
        for shape, statement, low, value in cases:
            expect = literal(written(pattern(width, True), low, value) if value else pattern(width, True))
            use = lambda name: statement.replace('{T}', name)
            method = "function void by_name();\nint at;int other;logic [31:0] xi='x;\n" + use('v') + '\nendfunction\n'
            classes = f'''{typedef}class Box;
value_t v;
value_t a[-1:1];
function new();
v={init};a[-1]={init};a[0]={init};a[1]={init};
endfunction
{method}endclass
'''
            body = f'''int errors=0;
object=new;{use('object.v')}
if(object.v!=={expect}) ++errors;
object=new;{use('object.a[index]')}
if(object.a[0]!=={expect}||object.a[-1]!=={init}||object.a[1]!=={init}) ++errors;
object=new;object.by_name();
if(object.v!=={expect}) ++errors;
$display("errors=%0d",errors);'''
            actor = "Box object;int at;int other;int index=0;logic [31:0] xi='x;\n"
            yield source(f'{kind}_{shape}', 'A selected write inside a packed property changes only its own bits',
                         wrap('unit', classes, body, actor=actor), lrm='7.2.1 7.3.1 7.4.1 7.4.5 8.4 11.5.1')


def nulls():
    """Through a null handle a selected write is the same error as a whole one (8.4)."""
    for shape, statement in (('bit', "object.v[3]=1'b1;"), ('runtime_bit', "object.v[at]=1'b1;"),
                             ('part', "object.v[5:4]=2'b01;"), ('compound', "object.v[5:4]+=1;")):
        text = f'''class Box;
logic [7:0] v;
endclass
module top;
Box object;int at=2;
initial begin
$display("before");
{statement}
$display("after");
end
endmodule
'''
        yield source(f'null_{shape}', 'A selected write through a null handle stops with the error of a whole write',
                     text, expected='before', extra=':expected_exit: 2\n:expected_stderr_regex: error: null class handle access\n',
                     lrm='8.4 11.5.1')


def timed():
    """A blocking write with a delay or an event takes its value before the wait
    and its target after it, whole or selected (9.4.5; SCHEDULER_SPEC.md 8)."""
    for shape, statement, release in (
            ('bit_delay', 'object.v[3]=#2 value;', ''),
            ('bit_event', 'object.v[3]=@(go) value;', '#1 ->go;'),
            ('part_delay', 'object.v[4:3]=#2 {1\'b0,value};', ''),
            ('runtime_bit_delay', 'object.v[at]=#2 value;', ''),
            ('whole_delay', 'object.v=#2 {4\'h0,value,3\'h0};', '')):
        text = f"""class Box;
logic [7:0] v=8'h00;
endclass
module top;
Box object,first;int errors=0;int at=3;logic value=1'b1;event go;
initial begin
first=new;object=first;
{statement}
if(object==first) ++errors;
if(first.v!==8'h00) ++errors;
if(object.v!==8'h08) ++errors;
$display("errors=%0d",errors);
end
initial begin
#1 object=new;value=1'b0;
{release}
end
endmodule
"""
        yield source(f'timed_{shape}', 'A blocking selected write with a wait takes its value before and its target after',
                     text, lrm='9.4.5 8.4 11.5.1')


def cases():
    yield from forms()
    yield from homes()
    yield from packed()
    yield from nulls()
    yield from timed()


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
                print(f'out of date: {path.relative_to(OUT.parent.parent.parent)}')
                failed = True
        else:
            path.write_text(text)
    print(f'class select matrix: {len(generated)} sources')
    return int(failed)


if __name__ == '__main__':
    raise SystemExit(main())
