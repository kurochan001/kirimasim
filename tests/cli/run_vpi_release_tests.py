#!/usr/bin/env python3
# Copyright (c) 2026 The kirimasim Authors
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
"""Verify owned release results across source containers, widths and formats."""
import argparse
import hashlib
import json
from pathlib import Path
import sys
import tempfile

from run_vpi_tests import build_plugin, ROOT
import bounded_run
from gen_vpi_release_matrix import programs


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--kirimasim', type=Path, required=True)
    parser.add_argument('--cc', default='cc')
    parser.add_argument('--iverilog', type=Path)
    parser.add_argument('--vvp', type=Path)
    parser.add_argument('--report', type=Path)
    args = parser.parse_args()
    assert bool(args.iverilog) == bool(args.vvp)
    results = []
    with tempfile.TemporaryDirectory(prefix='vpi release ') as tmp:
        root = Path(tmp)
        plugin = root/'release_probe.vpi'
        build_plugin(args.cc, ROOT/'tests/probes/vpi/release_result.c', plugin)
        for name, source, expected in programs():
            file = root/'case.sv'
            file.write_text(source)
            ran = bounded_run.run([str(args.kirimasim.resolve()), '--top=top',
                                   '--vpi='+str(plugin), str(file)], cwd=root, timeout=15)
            diagnostics = '\n'.join(s for s in ran.stderr.splitlines()
                                    if not s.startswith('kirimasim: info:'))
            row = dict(name=name, source_sha256=hashlib.sha256(source.encode()).hexdigest(),
                       native_exit=ran.returncode, native_stdout=ran.stdout,
                       passed=ran.returncode == 0 and ran.stdout == expected and not diagnostics)
            if args.iverilog:
                compiled = bounded_run.run([str(args.iverilog.resolve()), '-g2012', '-s', 'top',
                                           '-o', 'sim', str(file)], cwd=root, timeout=15)
                ref = bounded_run.run([str(args.vvp.resolve()), '-M'+str(root), '-mrelease_probe',
                                       'sim'], cwd=root, timeout=15) if compiled.returncode == 0 else None
                row.update(reference_compile_exit=compiled.returncode,
                           reference_run_exit=ref.returncode if ref else None,
                           reference_stdout=ref.stdout if ref else None,
                           reference_matches=bool(ref and ref.returncode == 0 and ref.stdout == expected))
            results.append(row)
            if not row['passed']:
                print(name, 'FAILED', repr(ran.stdout), diagnostics.replace(tmp, '<probe>'), flush=True)
    if args.report:
        args.report.write_text(json.dumps(results, indent=2)+'\n')
    count = sum(row['passed'] for row in results)
    print(f'VPI release: {count}/{len(results)} passed')
    return 0 if count == len(results) else 1


if __name__ == '__main__':
    sys.exit(main())
