#!/usr/bin/env python3
# Copyright (c) 2026 The kirimasim Authors
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
"""Original next-time callback cases under the approved P1 VPI contract 4."""
import argparse
import json
from pathlib import Path

CONTEXTS = ('module', 'child', 'generate', 'interface', 'program', 'static_task')
PHASES = ('call', 'readonly', 'timer')
PAYLOADS = ('zero_ticks', 'max_ticks', 'negative_real', 'infinite_real',
            'nan_real', 'huge_real', 'zero_real', 'finite_real')


def programs():
    for context in CONTEXTS:
        for phase, phase_name in enumerate(PHASES):
            for mode, payload in enumerate(PAYLOADS):
                body = (f'#1;$next_time_start({mode},{phase});\n'
                        '#2;$next_time_finish();\n')
                if context == 'static_task':
                    source = ('module top;task run();\n' + body +
                              'endtask initial run();endmodule\n')
                elif context in ('child', 'interface', 'program'):
                    kind = 'module' if context == 'child' else context
                    source = (f'{kind} worker;initial begin\n' + body +
                              f'end end{kind}\nmodule top;worker u();endmodule\n')
                else:
                    opening = 'if(1)begin:g\n' if context == 'generate' else ''
                    closing = 'end\n' if context == 'generate' else ''
                    source = ('module top;\n' + opening + 'initial begin\n' + body +
                              'end\n' + closing + 'endmodule\n')
                yield (f'{context}_{phase_name}_{payload}',
                       '`timescale 1ns/1ns\n' + source,
                       'next-time errors=0 callbacks=1 tick=3\n')


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
    print(f'VPI next-time: {len(manifest)} programs')


if __name__ == '__main__':
    main()
