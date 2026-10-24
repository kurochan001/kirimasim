#!/usr/bin/env python3
# Copyright (c) 2026 The kirimasim Authors
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

"""Original sparse-storage and explicit fixed-array network boundaries."""
import argparse
from pathlib import Path
import re
import subprocess
import tempfile


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--kirimasim', required=True)
    args = parser.parse_args()
    binary = str(Path(args.kirimasim).resolve())
    checks = 0
    with tempfile.TemporaryDirectory() as folder:
        path = Path(folder, 'probe.sv')
        # 64 drivers, each covering just one of 262144 physical elements.
        # The SIR must allocate 64 private cells, rather than 64 full arrays.
        source = 'module top;wire [64:0] q[0:262143];\n'
        for cell in range(32):
            for value in (0, 1):
                source += f"assign q[{cell*4096}]='{value};\n"
        source += 'endmodule\n'
        path.write_text(source)
        result = subprocess.run([binary, '--dump-sir', path.name], cwd=folder,
                                capture_output=True, text=True, timeout=30, check=False)
        rows = [line for line in result.stdout.splitlines() if line.startswith('slot ') and 'driver-of=' in line]
        if result.returncode or len(rows) != 64 or any(' [0:0] ' not in row for row in rows):
            raise SystemExit('FAIL: compact sparse net array driver allocation\n' + result.stdout + result.stderr)
        ordinals = [int(re.search(r'driver-elements=\[(\d+)\]', row)[1]) for row in rows]
        if sorted(ordinals) != sorted([cell*4096 for cell in range(32)]*2):
            raise SystemExit('FAIL: sparse driver physical ordinal mapping')
        checks += 1
        for width in (1, 8, 65, 129):
            for leaf in ('enum', 'record'):
                typedef = (f"typedef enum logic [{width-1}:0] {{Zero='0,One='1}} value_t;"
                           if leaf == 'enum' else
                           f"typedef struct packed {{logic [{width-1}:0] bits;}} value_t;")
                source = typedef + "module leaf(inout wire value_t p[7:6]);endmodule "
                source += "module top;wire value_t a[4:5];leaf l(a);assign a[4]=value_t'('0);assign a[5]=value_t'('1);initial begin #1;"
                source += "$display(\"%0d %0d %0d %0d\", l.p[7] === value_t'('0),l.p[6] === value_t'('1),$left(l.p),$dimensions(l.p));"
                field = '.bits' if leaf == 'record' else ''
                source += f"force l.p[7]{field}[{width-1}]=1'b1;#1;$display(\"%0d\",a[4]{field}[{width-1}]===1'b1);release a[4]{field}[{width-1}];#1;$display(\"%0d\",l.p[7]{field}[{width-1}]===1'b0);end endmodule\n"
                path.write_text(source)
                result = subprocess.run([binary, path.name], cwd=folder, capture_output=True,
                                        text=True, timeout=15, check=False)
                if result.returncode or result.stdout != '1 1 7 2\n1\n1\n':
                    raise SystemExit(f'FAIL: typed {leaf} width {width}\n' + result.stdout + result.stderr)
                checks += 1
        cases = [
            ('moving element', "module top;wire [7:0] a[2];logic [7:0] d;int i;assign a[i]=d;assign a[0]='1;endmodule",
             "reference to non-constant variable 'i'"),
            ('moving packed bit', "module top;wire [7:0] a[2];logic d;int i;assign a[0][i]=d;assign a[0]='1;endmodule",
             "reference to non-constant variable 'i'"),
            ('unpacked tristate sides', "module top;wire a[2];logic d,c;bufif1 g(a[0],d,c);assign a[0]=1'b1;endmodule",
             'three-state side resolution of a net array'),
            ('chandle ports remain strict', 'module leaf(input var chandle p[2]);endmodule module top;chandle a[2];leaf l(a);endmodule',
             'not a valid type for a port'),
            ('two-state nets remain strict', 'module leaf(input wire bit p[2]);endmodule module top;wire bit a[2];leaf l(a);endmodule',
             'not a valid type for a net'),
        ]
        for name, source, diagnostic in cases:
            path.write_text(source + '\n')
            result = subprocess.run([binary, path.name], cwd=folder, capture_output=True,
                                    text=True, timeout=15, check=False)
            if result.returncode != 2 or result.stdout or diagnostic not in result.stderr:
                raise SystemExit(f'FAIL: {name}: {result.returncode}\n' + result.stdout + result.stderr)
            checks += 1
    print(f'checked {checks} sparse allocation and explicit array network boundaries')
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
