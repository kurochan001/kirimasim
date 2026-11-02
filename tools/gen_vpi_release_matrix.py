#!/usr/bin/env python3
# Copyright (c) 2026 The kirimasim Authors
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
"""Original static-variable VPI release-result matrix (1800-2023 38.34).

The C probe forces one, releases with empty output storage and checks every
returned word/format. Containers only change placement, not value semantics.
Like gen_vpi_matrix.py, programs are generated for the CLI checker on demand.
"""
import argparse
import json
from pathlib import Path

CONTEXTS = ('module', 'child', 'generate', 'interface', 'program', 'package',
            'static_task', 'function')
WIDTHS = (1, 8, 32, 33, 65, 129)


def programs():
    for context in CONTEXTS:
        for width in WIDTHS:
            for kind in ('bit', 'logic'):
                declaration = f'{kind} [{width-1}:0] value=0;\n'
                body = '$release_probe(value);$display("done");\n'
                if context in ('static_task', 'function'):
                    sub = 'task' if context == 'static_task' else 'function'
                    signature = 'task run();' if sub == 'task' else 'function void run();'
                    text = ('module top;\n' + signature + '\nstatic ' + declaration + body +
                            f'end{sub}\ninitial run();endmodule\n')
                elif context == 'package':
                    text = ('package values;\n' + declaration + 'endpackage\n'
                            'module top;initial begin\n' + body.replace('value)', 'values::value)') +
                            'end endmodule\n')
                elif context in ('child', 'interface', 'program'):
                    outer = 'module' if context == 'child' else context
                    text = (f'{outer} worker;\n' + declaration + 'initial begin\n' + body +
                            f'end end{outer}\nmodule top;worker u();endmodule\n')
                else:
                    opening = 'if(1)begin:g\n' if context == 'generate' else ''
                    closing = 'end\n' if context == 'generate' else ''
                    text = ('module top;\n' + opening + declaration + 'initial begin\n' + body +
                            'end\n' + closing + 'endmodule\n')
                name = f'{context}_{kind}_{width}'
                yield name, '`timescale 1ns/1ns\n' + text, 'release errors=0\ndone\n'


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', required=True, type=Path)
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=True)
    manifest = []
    for name, text, expected in programs():
        (args.output/(name+'.sv')).write_text(text)
        manifest.append(dict(name=name, expected_stdout=expected))
    (args.output/'manifest.json').write_text(json.dumps(manifest, indent=2)+'\n')
    print(f'VPI release matrix: {len(manifest)} programs')


if __name__ == '__main__':
    main()
