#!/usr/bin/env python3
# Copyright (c) 2026 The kirimasim Authors
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
"""Original packed-bit VPI cases under VALUE_SEMANTICS 2 / IEEE 11.8.1."""
import argparse
import json
from pathlib import Path

WIDTHS = (1, 32, 33, 65, 129, 257)
CONTEXTS = ('module', 'child', 'generate', 'interface', 'program', 'static_task')


def programs():
    for context in CONTEXTS:
        for kind in ('logic', 'bit', 'wire'):
            for signed in (False, True):
                for ascending in (False, True):
                    for width in WIDTHS:
                        low, high = -17, width - 18
                        left, right = (low, high) if ascending else (high, low)
                        middle = low + width // 2
                        sign = 'signed' if signed else 'unsigned'
                        prefix = 'static ' if context == 'static_task' else ''
                        target = 'driver' if kind == 'wire' else 'value'
                        decl = f'{prefix}{kind} {sign} [{left}:{right}] value=\'1;\n'
                        if kind == 'wire':
                            decl = (f'logic [{left}:{right}] driver=\'1;\n'
                                    f'wire {sign} [{left}:{right}] value=driver;\n')
                        # All values and callback counts follow the scripted writes,
                        # not a simulator's output. Unselected neighbours stay one.
                        body = (f'#1;$packed_bit_start(value,{middle},{low},{high},'
                                f'{int(signed)},{int(kind != "bit")},{int(kind == "wire")});\n'
                                f'#1;{target}[{middle}]=0;\n'
                                '#1;$packed_bit_check(0);\n')
                        if kind == 'bit':
                            body += f'{target}[{middle}]=1;\n#1;$packed_bit_check(1);\n'
                        else:
                            body += (f'{target}[{middle}]=1\'bx;\n#1;$packed_bit_check(3);\n'
                                     f'{target}[{middle}]=1\'bz;\n#1;$packed_bit_check(2);\n')
                        body += '$packed_bit_finish();\n'
                        if context == 'static_task':
                            # A net cannot be declared in a task; use an ordinary
                            # module declaration with a task-issued inspection.
                            if kind == 'wire':
                                source = ('module top;\n' + decl + 'task run();\n' + body +
                                          'endtask\ninitial run();\nendmodule\n')
                            else:
                                source = ('module top;task run();\n' + decl + body +
                                          'endtask\ninitial run();\nendmodule\n')
                        elif context in ('child', 'interface', 'program'):
                            outer = 'module' if context == 'child' else context
                            source = (f'{outer} worker;\n' + decl + 'initial begin\n' + body +
                                      f'end end{outer}\nmodule top;worker u();endmodule\n')
                        else:
                            opening = 'if(1)begin:g\n' if context == 'generate' else ''
                            closing = 'end\n' if context == 'generate' else ''
                            source = ('module top;\n' + opening + decl + 'initial begin\n' +
                                      body + 'end\n' + closing + 'endmodule\n')
                        name = f'{context}_{kind}_{sign}_{"up" if ascending else "down"}_{width}'
                        callbacks = 2 if kind == 'bit' else 3
                        yield name, '`timescale 1ns/1ns\n' + source, (
                            f'packed-bit errors=0 callbacks={callbacks}\n')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=True)
    manifest = []
    for name, source, expected in programs():
        (args.output / (name + '.sv')).write_text(source)
        manifest.append(dict(name=name, expected_stdout=expected))
    (args.output / 'manifest.json').write_text(json.dumps(manifest, indent=2) + '\n')
    print(f'VPI packed bits: {len(manifest)} programs')


if __name__ == '__main__':
    main()
