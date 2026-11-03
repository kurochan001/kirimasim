#!/usr/bin/env python3
# Copyright (c) 2026 The kirimasim Authors
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
"""Original nearest-value-format cases for IEEE 1800-2023 38.22 and 38.34."""
import argparse
import json
from pathlib import Path

CONTEXTS = ('module', 'child', 'generate', 'interface', 'program', 'package',
            'static_task', 'function')
# Expected ABI formats follow the declared object, independently of the host.
TYPES = (
    ('scalar_bit', 'bit', 5), ('scalar_logic', 'logic', 5),
    ('one_bit_vector', 'bit [0:0]', 9), ('one_logic_vector', 'logic [-3:-3]', 9),
    ('wide_bit', 'bit [32:0]', 9), ('wide_logic', 'logic [128:0]', 9),
    ('int', 'int', 6), ('integer', 'integer', 6), ('real', 'real', 7),
)


def programs():
    for context in CONTEXTS:
        for kind, spelling, expected_format in TYPES:
            declaration = f'{spelling} value=0;\n'
            call = f'$object_format_start(value,{expected_format});\n'
            finish = '#1;$object_format_finish();\n'
            if context in ('static_task', 'function'):
                sub = 'task' if context == 'static_task' else 'function'
                signature = 'task run();' if sub == 'task' else 'function void run();'
                source = ('module top;\n' + signature + '\nstatic ' + declaration + call +
                          f'end{sub}\ninitial begin run();\n' + finish + 'end endmodule\n')
            elif context == 'package':
                source = ('package values;\n' + declaration + 'endpackage\nmodule top;\n'
                          'initial begin\n' + call.replace('value,', 'values::value,') +
                          finish + 'end endmodule\n')
            elif context in ('child', 'interface', 'program'):
                outer = 'module' if context == 'child' else context
                source = (f'{outer} worker;\n' + declaration + 'initial begin\n' + call +
                          finish + f'end end{outer}\nmodule top;worker u();endmodule\n')
            else:
                opening = 'if(1)begin:g\n' if context == 'generate' else ''
                closing = 'end\n' if context == 'generate' else ''
                source = ('module top;\n' + opening + declaration + 'initial begin\n' +
                          call + finish + 'end\n' + closing + 'endmodule\n')
            yield f'{context}_{kind}', '`timescale 1ns/1ns\n' + source, 'object-format errors=0 callbacks=1\n'
    for context in ('module', 'child', 'generate', 'interface', 'program'):
        for kind, spelling, expected_format in (
                ('scalar_net', 'wire', 5), ('one_net_vector', 'wire [0:0]', 9),
                ('wide_net', 'wire [64:0]', 9)):
            declaration = f'{spelling} value=1;\n'
            body = f'initial #1 $object_format_read(value,{expected_format});\n'
            if context in ('child', 'interface', 'program'):
                outer = 'module' if context == 'child' else context
                source = (f'{outer} worker;\n' + declaration + body +
                          f'end{outer}\nmodule top;worker u();endmodule\n')
            else:
                opening = 'if(1)begin:g\n' if context == 'generate' else ''
                closing = 'end\n' if context == 'generate' else ''
                source = 'module top;\n' + opening + declaration + body + closing + 'endmodule\n'
            yield f'{context}_{kind}', '`timescale 1ns/1ns\n' + source, 'object-format read-errors=0\n'


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=True)
    manifest = []
    for name, text, expected in programs():
        (args.output/(name+'.sv')).write_text(text)
        manifest.append(dict(name=name, expected_stdout=expected))
    (args.output/'manifest.json').write_text(json.dumps(manifest, indent=2)+'\n')
    print(f'VPI object format: {len(manifest)} programs')


if __name__ == '__main__':
    main()
