#!/usr/bin/env python3
# Copyright (c) 2026 The kirimasim Authors
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
"""The order in which operands, arguments and the two sides of an assignment run.

Short-circuiting is fixed by the standard (11.4.7, 11.4.11). The rest it leaves
open (11.3.5, 11.4.2, 10.4.1), so each test allows every order in <name>.expected
and <name>.choice holds the one of VALUE_SEMANTICS.md 3.1: left to right; a
blocking assignment or an NBA evaluates its right side first and then the left
side's indices (SCHEDULER_SPEC.md 13); an assignment operator reads its left side
first. Each call appends its digit to `order`. Every expected result is computed
here from these rules, not from a run.
"""
import argparse
import itertools
from pathlib import Path

OUT = Path(__file__).resolve().parent.parent / 'tests/suite/proc'
BINARY = {'add': '+', 'sub': '-', 'mul': '*', 'div': '/', 'mod': '%', 'pow': '**', 'and': '&', 'or': '|', 'xor': '^',
          'xnor': '^~', 'shl': '<<', 'shr': '>>', 'ashl': '<<<', 'ashr': '>>>', 'eq': '==', 'ne': '!=', 'ceq': '===',
          'cne': '!==', 'lt': '<', 'le': '<=', 'gt': '>', 'ge': '>='}


def forms():
    """Name, statement, whether it issues an NBA, the digits in the chosen order,
    and whether the standard fixes that order."""
    for name, op in BINARY.items():
        yield f'binary_{name}', f'r=f(1){op}f(2);', False, '12', False
    yield 'nested', 'r=f(1)+f(2)*f(3);', False, '123', False
    yield 'nested_groups', 'r=(f(1)+f(2))*(f(3)+f(4));', False, '1234', False
    yield 'and_true', 'r=f(1)&&f(2);', False, '12', True
    yield 'and_false', 'r=z(1)&&f(2);', False, '1', True
    yield 'or_true', 'r=f(1)||f(2);', False, '1', True
    yield 'or_false', 'r=z(1)||f(2);', False, '12', True
    yield 'implies_true', 'r=f(1)->f(2);', False, '12', True
    yield 'implies_false', 'r=z(1)->f(2);', False, '1', True
    yield 'conditional_true', 'r=f(1)?f(2):f(3);', False, '12', True
    yield 'conditional_false', 'r=z(1)?f(2):f(3);', False, '13', True
    yield 'concatenation', "v={4'(f(1)),4'(f(2)),4'(f(3))};", False, '123', False
    yield 'call', 'r=g(f(1),f(2));', False, '12', False
    yield 'task', 'tk(f(1),f(2));', False, '12', False
    yield 'sformatf', 's=$sformatf("%0d %0d",f(1),f(2));', False, '12', False
    yield 'swrite', '$swrite(s,"%0d %0d",f(1),f(2));', False, '12', False
    yield 'method', 's="abcdefgh"; s=s.substr(f(1),f(2));', False, '12', False
    yield 'blocking_element', 'b[f(1)]=f(2);', False, '21', False
    yield 'blocking_two_indices', 'a[f(1)][f(2)]=f(3);', False, '312', False
    yield 'blocking_bit', 'v[f(1)]=f(2)&1;', False, '21', False
    yield 'blocking_part', 'v[f(1)+:2]=f(2);', False, '21', False
    yield 'blocking_concatenation', "{b[f(1)],b[f(2)]}=64'(f(3));", False, '312', False
    yield 'nba_element', 'b[f(1)]<=f(2);', True, '21', False
    yield 'nba_two_indices', 'a[f(1)][f(2)]<=f(3);', True, '312', False
    yield 'nba_bit', 'v[f(1)]<=f(2)&1;', True, '21', False
    yield 'nba_delayed', 'b[f(1)]<=#1 f(2);', True, '21', False
    yield 'compound_element', 'b[f(1)]+=f(2);', False, '12', False
    yield 'compound_bit', 'v[f(1)]|=f(2)&1;', False, '12', False


DECLARATIONS = """int order=0;
int r;
int a[0:9][0:9];
int b[0:9];
logic [31:0] v;
string s;
function automatic int f(int x); order=order*10+x; return x; endfunction
function automatic int z(int x); order=order*10+x; return 0; endfunction
function automatic int g(int p,int q); return p+q; endfunction
task automatic tk(input int p,input int q); endtask
"""
SHOW = '#2 $display("%0d",order);'


def scoped(place, statement):
    if place == 'initial':
        return f'initial begin\n{statement}\n{SHOW}\nend\n'
    if place == 'task':
        return f'task automatic run();\n{statement}\nendtask\ninitial begin\nrun();\n{SHOW}\nend\n'
    if place == 'function':
        return f'function automatic void run();\n{statement}\nendfunction\ninitial begin\nrun();\n{SHOW}\nend\n'
    if place == 'fork':
        return f'initial begin\nfork\nbegin\n{statement}\nend\njoin\n{SHOW}\nend\n'
    if place == 'method':
        return (f'class box;\ntask run();\n{statement}\nendtask\nendclass\n'
                f'initial begin\nbox k=new;\nk.run();\n{SHOW}\nend\n')
    return f'if(1) begin:g\ninitial begin\n{statement}\n{SHOW}\nend\nend\n'


def source(name, statement, nba, chosen, fixed, place):
    if place == 'generate':
        body = 'if(1) begin:g\n' + DECLARATIONS + scoped('initial', statement) + 'end\n'
    else:
        body = DECLARATIONS + scoped(place, statement)
    name = f'eval_order_{place}_{name}'
    head = f'''/*
:name: {name}
:description: Operands, arguments and the sides of an assignment run in the chosen order
:tags: proc order
:type: simulation
:lrm: 1800-2023 10.4.1 10.4.2 11.3.5 11.4.1 11.4.2 11.4.7 11.4.11
:oracle: spec{'' if fixed else ' choice'}
:spec_check: ai
:fourstate: yes
:nondeterministic: {'no' if fixed else 'yes'}
:generator: gen_eval_order_matrix.py@1
:top_module: top
:expected_stdout: {chosen if fixed else '@' + name + '.expected'}
:timeout: 15
*/
// Generated by tools/gen_eval_order_matrix.py; edit the generator.
`timescale 1ns/1ns
module top;
{body}endmodule
'''
    candidates = sorted(''.join(p) for p in itertools.permutations(chosen))
    return name, head, None if fixed else '---\n'.join(c + '\n' for c in candidates), None if fixed else chosen + '\n'


# The forms that take in the most of a container: an operator, a call, each side.
ACROSS = ('binary_add', 'nested', 'and_false', 'conditional_false', 'concatenation', 'call', 'task', 'sformatf',
          'blocking_element', 'blocking_two_indices', 'nba_element', 'compound_element')


def cases():
    every = list(forms())
    for name, statement, nba, chosen, fixed in every:
        yield source(name, statement, nba, chosen, fixed, 'initial')
    for place in ('task', 'function', 'fork', 'method', 'generate'):
        for name, statement, nba, chosen, fixed in every:
            if name not in ACROSS:
                continue
            # A function has no task call and no NBA (13.4.4).
            if place == 'function' and (nba or name == 'task'):
                continue
            yield source(name, statement, nba, chosen, fixed, place)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--check', action='store_true')
    args = parser.parse_args()
    generated = list(cases())
    assert len({c[0] for c in generated}) == len(generated)
    failed = False
    for name, text, expected, choice in generated:
        for suffix, content in (('.sv', text), ('.expected', expected), ('.choice', choice)):
            if content is None:
                continue
            path = OUT / (name + suffix)
            if args.check:
                if not path.exists() or path.read_text() != content:
                    print(f'out of date: {path.relative_to(OUT.parent.parent.parent)}')
                    failed = True
            else:
                path.write_text(content)
    print(f'evaluation order matrix: {len(generated)} sources')
    return int(failed)


if __name__ == '__main__':
    raise SystemExit(main())
