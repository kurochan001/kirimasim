#!/usr/bin/env python3
# Copyright (c) 2026 The kirimasim Authors
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

"""Generate original delayed-NBA target capture tests (10.4.2, 9.4.5).

The right value, target indices and delay are captured at issue time.
Axes: two/four-state 1/32/65/129-bit values and eight execution containers.
Each case covers multidimensional arrays, selected bits, concatenations,
partial/invalid/X/Z/wide indices, zero delay, overlap and single evaluation.
Only newly generated expectations are written; no external tests are used.
"""

import argparse
import sys
from pathlib import Path

OUT = Path(__file__).resolve().parent.parent / 'tests' / 'suite' / 'sched'
VERSION = 1
HOMES = ('initial', 'task', 'function', 'fork', 'interface', 'package', 'program', 'generate')


def case(home, width, domain):
    name = f'delayed_nba_target_{home}_{domain}_{width}'
    whole = width + 8
    ones = (1 << width) - 1
    declarations = f'''  {domain} [{width-1}:0] a[2:0][-1:1], d;
  {domain} [{whole-1}:0] v;
  struct packed {{ bit [3:0] upper; {domain} [{width-1}:0] payload; bit [3:0] lower; }} s;
  int i,j,pos;
  logic [128:0] wide_index;
'''
    lines, expected = [], []
    def line(text):
        lines.append('    ' + text)
    def show(tag, expr, value):
        line(f'$display("{tag} %b",{expr});')
        expected.append(f'{tag} {value}')
    def bits(value, size):
        return f'{value:0{size}b}'
    def reset():
        line("foreach(a[x,y]) a[x][y]=0; v=0; d='1; i=1; j=0; pos=4;")
    # A function cannot wait; its caller waits after each invocation instead.
    # It issues a single bundle, while every other container runs all shapes.
    if home == 'function':
        reset()
        line('a[i++][j++] <= #2 d; v[pos+:$bits(d)] <= #2 d;')
        line("s='1; s.payload <= #2 '0; d=0; pos=0;")
        body = '\n'.join(lines)
        call = '''    issue();
    $display("indices %0d %0d",i,j);
    #1; $display("before %b",a[1][0]);
    #2; $display("element %b",a[1][0]);
    $display("bits %b",v); $display("member %b",s);
'''
        expected = ['indices 2 1', 'before ' + bits(0,width), 'element ' + bits(ones,width),
                    'bits ' + bits(ones << 4,whole), 'member ' + bits((15 << (width+4)) | 15,whole)]
    else:
        reset()
        line('a[i++][j++] <= #2 d; d=0;')
        line('$display("indices %0d %0d",i,j);')
        expected.append('indices 2 1')
        line('#1;')
        show('before','a[1][0]',bits(0,width))
        line('#2;')
        show('element','a[1][0]',bits(ones,width))
        show('neighbor','a[0][1]',bits(0,width))
        reset()
        line('v[pos+:$bits(d)] <= #2 d; pos=0; d=0; v[1:0]=2; #3;')
        show('up','v',bits((ones << 4) | 2,whole))
        reset()
        line(f'pos={width+3}; v[pos-:$bits(d)] <= #2 d; pos=0; d=0; #3;')
        show('down','v',bits(ones << 4,whole))
        reset()
        line('pos=-2; v[pos+:$bits(d)] <= #2 d; pos=4; #3;')
        show('partial_low','v',bits(ones >> 2,whole))
        reset()
        line(f'pos={whole-2}; v[pos+:$bits(d)] <= #2 d; pos=4; #3;')
        show('partial_high','v',bits((ones & 3) << (whole-2),whole))
        reset()
        line('{a[i][j],v[pos+:$bits(d)]} <= #2 {d,d}; i=0; j=1; pos=0; d=0; #3;')
        show('concat_element','a[1][0]',bits(ones,width))
        show('concat_bits','v',bits(ones << 4,whole))
        reset()
        line("s='1; s.payload <= #2 '0; #3;")
        show('member','s',bits((15 << (width+4)) | 15,whole))
        reset()
        line('a[i][j] <= #0 d; d=0;')
        show('zero_before','a[1][0]',bits(0,width))
        line('#0;')
        show('zero_inactive','a[1][0]',bits(0,width))
        line('#1;')
        show('zero_after','a[1][0]',bits(ones,width))
        reset()
        line("i=9; a[i][j] <= #2 d; wide_index='x; a[wide_index][j] <= #2 d;")
        line("wide_index='z; a[wide_index][j] <= #2 d;")
        line("wide_index=129'h10000000000000001; a[wide_index][j] <= #2 d;")
        line("wide_index='x; v[wide_index+:$bits(d)] <= #2 d;")
        line("wide_index='z; v[wide_index+:$bits(d)] <= #2 d;")
        line("wide_index=129'h10000000000000001; v[wide_index+:$bits(d)] <= #2 d; #3;")
        show('invalid_element','a[1][0]',bits(0,width))
        show('invalid_bits','v',bits(0,whole))
        reset()
        line(f'pos={width//2}; a[i][j][pos+:$bits(d)] <= #2 d; i=0; j=1; pos=0; d=0; #3;')
        show('array_bits','a[1][0]',bits((ones << (width//2)) & ones,width))
        reset()
        line("d='x; a[i][j] <= #2 d; d=0; #3;")
        show('x_data','a[1][0]',('x'*width if domain == 'logic' else bits(0,width)))
        reset()
        line("d='z; a[i][j] <= #2 d; d=0; #3;")
        show('z_data','a[1][0]',('z'*width if domain == 'logic' else bits(0,width)))
        reset()
        line('a[i][j] <= #2 d; a[i][j] <= #2 0; #3;')
        show('same_time_order','a[1][0]',bits(0,width))
        body = '\n'.join(lines)
        call = '    run();\n'
    if home == 'interface':
        source = 'interface worker;\n' + declarations + '  task automatic run();\n' + body
        source += '\n  endtask\nendinterface\nmodule top; worker w(); initial w.run(); endmodule\n'
    elif home == 'package':
        source = 'package worker;\n' + declarations + '  task automatic run();\n' + body
        source += '\n  endtask\nendpackage\nmodule top; initial worker::run(); endmodule\n'
    elif home == 'program':
        source = 'program worker;\n' + declarations + '  initial begin\n' + body
        source += '\n  end\nendprogram\nmodule top; worker w(); endmodule\n'
    elif home == 'generate':
        source = 'module top; if (1) begin:g\n' + declarations + '  initial begin\n' + body
        source += '\n  end\nend endmodule\n'
    elif home == 'task':
        # Static local storage in an automatic task remains a legal NBA target.
        declarations = ''.join('  static ' + s.strip() + '\n' for s in declarations.splitlines())
        source = 'module top; task automatic run();\n' + declarations + body
        source += '\n  endtask\n  initial run();\nendmodule\n'
    elif home == 'function':
        source = 'module top;\n' + declarations + '  function automatic void issue();\n' + body
        source += '\n  endfunction\n  initial begin\n' + call + '  end\nendmodule\n'
    else:
        source = 'module top;\n' + declarations + '  initial begin\n'
        source += ('    fork begin\n' + body + '\n    end join\n' if home == 'fork' else body + '\n')
        source += '  end\nendmodule\n'
    header = f'''/*
:name: {name}
:description: Delayed NBA captures runtime target and value across execution containers
:tags: sched nba delay
:type: simulation
:lrm: 1800-2023 7.4.5 9.4.5 10.4.2
:oracle: spec
:spec_check: ai
:fourstate: yes
:nondeterministic: no
:generator: gen_delayed_nba_target_matrix.py@{VERSION}
:top_module: top
:expected_stdout: @{name}.expected
:expected_stderr: @{name}.stderr
*/
// Generated by tools/gen_delayed_nba_target_matrix.py; edit the generator.
`timescale 1ns/1ns
'''
    return {name+'.sv': header+source, name+'.expected': '\n'.join(expected)+'\n', name+'.stderr': ''}


def files():
    result = {}
    for home in HOMES:
        for width in (1,32,65,129):
            for domain in ('bit','logic'):
                result.update(case(home,width,domain))
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument('--check',action='store_true')
    args = parser.parse_args()
    bad = []
    for name,text in files().items():
        path = OUT/name
        if args.check:
            if not path.exists() or path.read_text() != text:
                bad.append(name)
        else:
            path.write_text(text)
    if bad:
        print('gen_delayed_nba_target_matrix: out of date: '+', '.join(bad),file=sys.stderr)
    return int(bool(bad))


if __name__ == '__main__':
    sys.exit(main())
