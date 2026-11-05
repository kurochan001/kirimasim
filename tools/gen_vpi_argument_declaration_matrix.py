#!/usr/bin/env python3
# Copyright (c) 2026 The kirimasim Authors
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
"""Original whole-integral VPI declaration and activation matrix (IEEE 1800 ch. 36)."""
import argparse
import json
from pathlib import Path

CONTEXTS = ('static', 'task', 'function', 'nested', 'ref', 'block', 'fork',
            'interface', 'program', 'generate', 'package', 'class')


def shapes():
    yield 'scalar', 'logic', 0, 0, True, False, False, True
    yield 'packed_one', 'logic signed [7:7]', 7, 7, False, False, True, True
    yield 'int', 'int', 31, 0, False, True, True, False
    yield 'integer', 'integer', 31, 0, False, True, True, True
    for width in (33, 65, 129, 257):
        for ascending in (False, True):
            left, right = (-3, width - 4) if ascending else (width - 4, -3)
            for four in (False, True):
                signed = ascending
                kind = 'logic' if four else 'bit'
                declaration = f'{kind} {"signed " if signed else ""}[{left}:{right}]'
                yield (f'w{width}_{"asc" if ascending else "desc"}_{kind}', declaration,
                       left, right, False, False, signed, four)


def programs():
    for shape, declaration, left, right, scalar, integer, signed, four in shapes():
        width = abs(left - right) + 1
        local = f"{declaration} value='1;"
        # Positive constants are available to compiletf even when a negative
        # bound's unary expression would require runtime evaluation.
        call = (f'$decl(value,{left + 1024},{right + 1024},{int(scalar)},{int(integer)},'
                f'{int(signed)},{width},{int(four)});')
        # The plugin clears the declared left bit; all other bits stay one.
        check = ('if(value === (' + str(width) + "'b" + '0' + '1' * (width - 1) +
                 ')) $display("hdl ok"); else $display("hdl BAD");')
        for context in CONTEXTS:
            body = call + check
            if context == 'static':
                source = f'module top;{local} initial begin {body} end endmodule'
            elif context in ('block', 'fork'):
                part = f'begin : local_scope {local} {body} end'
                if context == 'fork':
                    part = 'fork ' + part + ' join'
                source = ('module top;task automatic run(); ' + part +
                          ' endtask initial run();endmodule')
            else:
                method = f'task automatic run();{local}{body} endtask'
                invoke = 'initial run();'
                if context == 'function':
                    method = f'function automatic void run();{local}{body} endfunction'
                elif context == 'nested':
                    method += ' task automatic outer();run();endtask'
                    invoke = 'initial outer();'
                elif context == 'ref':
                    method = (f'task automatic inspect(ref {declaration} value);{body} endtask '
                              f'task automatic run();{local}inspect(value);endtask')
                if context in ('interface', 'program'):
                    source = (f'{context} worker;{method} {invoke}end{context}'
                              ' module top;worker u();endmodule')
                elif context == 'package':
                    source = f'package worker;{method} endpackage module top;initial worker::run();endmodule'
                elif context == 'class':
                    source = f'class Worker;{method} endclass module top;Worker w;initial begin w=new;w.run();end endmodule'
                elif context == 'generate':
                    source = f'module top;if(1)begin:g {method} {invoke}end endmodule'
                else:
                    source = f'module top;{method} {invoke}endmodule'
            yield f'{context}_{shape}', '`timescale 1ns/1ns\n' + source + '\n', 'declaration errors=0\nhdl ok\n'
    yield 'activation_reuse', '''module top;
task automatic run();logic signed[-3:61] value='1;$decl_keep(value);endtask
initial begin run();run();end
endmodule
''', 'lifetime ok\nlifetime ok\n'
    yield 'readonly_ref', '''module top;
task automatic inspect(const ref logic[-3:61] value);$decl_readonly(value);endtask
task automatic run();logic[-3:61] value='1;inspect(value);
if(&value) $display("hdl ok"); else $display("hdl BAD");endtask
initial run();endmodule
''', 'readonly ok\nhdl ok\n'
    yield 'overlapping_activations', '''`timescale 1ns/1ns
module top;
task automatic run(input int id);logic signed[-3:61] value='1;
#(id+1);$decl_parallel(value,id);#3;
if(value[-3] === (id==0 ? 1'b0 : 1'b1)) $display("hdl %0d ok",id);
else $display("hdl BAD");endtask
initial fork run(0);run(1);join
endmodule
''', 'parallel saved\nparallel ok\nhdl 0 ok\nhdl 1 ok\n'


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=True)
    rows = []
    for name, source, expected in programs():
        (args.output / (name + '.sv')).write_text(source)
        rows.append(dict(name=name, expected_stdout=expected))
    (args.output / 'manifest.json').write_text(json.dumps(rows, indent=2) + '\n')
    print(f'VPI argument declarations: {len(rows)} original programs')


if __name__ == '__main__':
    main()
