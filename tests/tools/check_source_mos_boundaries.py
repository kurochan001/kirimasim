#!/usr/bin/env python3
# Copyright (c) 2026 The kirimasim Authors
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

"""Original source MOS policy boundaries; no fixture-specific acceptance."""
import argparse
from pathlib import Path
import re
import subprocess
import tempfile


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--kirimasim', type=Path, required=True)
    binary = str(parser.parse_args().kirimasim.resolve())
    cases = (
        ("wire a,q;assign a=1'b1;nmos m(q,a,1'b1);initial force q=0;",
         'force/release of a MOS strength net'),
        ("wire a,q,r;alias q=r;assign a=1'b1;nmos m(q,a,1'b1);initial force r=0;",
         'force/release of a MOS strength net'),
        ("wire a,b;tran m(a,b);", None),
        ('wire a,q;nmos m(q,a,1\'b1);initial $display("%v",q);', 'format specifier %v'),
        ("wire bit a,q;assign a=1'b1;nmos m(q,a,1'b1);", "'bit' is not a valid type for a net"),
        ("wire a;wire[1:0] q;logic i=0;assign a=1'b1;nmos m(q[i],a,1'b1);",
         "reference to non-constant variable 'i'"),
    )
    with tempfile.TemporaryDirectory(prefix='kirimasim-source-mos-') as directory:
        source = Path(directory, 'probe.sv')
        for body, diagnostic in cases:
            source.write_text('module top;\n'+body+'\nendmodule\n')
            result = subprocess.run([binary, source.name], cwd=directory, capture_output=True,
                                    text=True, timeout=15, check=False)
            if diagnostic is None:
                assert result.returncode == 0 and not result.stdout, result.stdout+result.stderr
                assert not [line for line in result.stderr.splitlines()
                            if not line.startswith('kirimasim: info:')], result.stderr
                continue
            assert result.returncode == 2 and not result.stdout, result.stdout+result.stderr
            assert diagnostic in result.stderr, result.stderr
            assert re.search(r'probe\.sv:2:\d+: error:', result.stderr), result.stderr
            assert 'internal error' not in result.stderr, result.stderr
    print(f'{len(cases)} positioned source MOS boundaries checked')
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
