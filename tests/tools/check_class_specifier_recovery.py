#!/usr/bin/env python3
# Copyright (c) 2026 The kirimasim Authors
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

"""Original matrix: malformed class specifiers must remain input errors (CLI 5)."""

import argparse
from pathlib import Path
import re
import subprocess
import tempfile


def cases():
    for home in ('unit', 'module', 'package'):
        for kind in ('function', 'task', 'constraint'):
            for marker in ('initial', '87', 'mystery_marker', 'byte'):
                if kind == 'function':
                    member = f'function : {marker} int read_tag(); return 29; endfunction'
                elif kind == 'task':
                    member = f'task : {marker} exchange(); endtask'
                else:
                    member = f'constraint : {marker} bounded {{ sample inside {{[0:17]}}; }}'
                body = 'class entry_cell;\nrand int sample;\n' + member + '\nendclass\n'
                if home == 'unit':
                    source = body + 'module top; endmodule\n'
                elif home == 'module':
                    source = 'module top;\n' + body + 'endmodule\n'
                else:
                    source = 'package scope_lab;\n' + body + 'endpackage\nmodule top; endmodule\n'
                yield f'{home}_{kind}_{marker}', source, 0 if marker == 'initial' else 2


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--kirimasim', type=Path, required=True)
    binary = str(parser.parse_args().kirimasim.resolve())
    checked = 0
    with tempfile.TemporaryDirectory(prefix='kirimasim-class-specifier-') as directory:
        source = Path(directory, 'probe.sv')
        for name, text, expected in cases():
            source.write_text(text)
            for mode in ('parse', 'run'):
                args = [binary, *(['parse'] if mode == 'parse' else []), '--top=top', source.name]
                result = subprocess.run(args, cwd=directory, capture_output=True,
                                        text=True, timeout=10, check=False)
                assert result.returncode == expected, (name, mode, result.returncode, result.stderr)
                assert not result.stdout, (name, mode, result.stdout)
                assert 'internal compiler error' not in result.stderr, result.stderr
                assert 'Supposedly unreachable' not in result.stderr, result.stderr
                if expected == 2:
                    assert re.search(r'probe\.sv:\d+:\d+: error:.*class specifier', result.stderr), result.stderr
                else:
                    assert 'error:' not in result.stderr, result.stderr
                checked += 1
    print(f'{checked} class-specifier parse/run checks passed, including valid controls')
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
