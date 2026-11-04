#!/usr/bin/env python3
# Copyright (c) 2026 The kirimasim Authors
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
"""Check packed-bit values/metadata/callbacks independently of parent signedness."""
import argparse
import hashlib
import json
from pathlib import Path
import sys
import tempfile

from run_vpi_tests import ROOT, build_plugin
import bounded_run
from gen_vpi_packed_bit_matrix import programs


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--kirimasim', type=Path, required=True)
    parser.add_argument('--cc', default='cc')
    parser.add_argument('--iverilog', type=Path)
    parser.add_argument('--vvp', type=Path)
    parser.add_argument('--report', type=Path)
    parser.add_argument('--case', action='append')
    args = parser.parse_args()
    assert bool(args.iverilog) == bool(args.vvp)
    results = []
    with tempfile.TemporaryDirectory(prefix='vpi packed bits ') as tmp:
        root = Path(tmp)
        plugin = root / 'packed_bit.vpi'
        build_plugin(args.cc, ROOT / 'tests/probes/vpi/packed_bit.c', plugin)
        if args.iverilog:
            # Keep stronger native checks. This companion tests common numeric
            # reads without claiming reference support for bit callbacks or the
            # host's separately approved nearest-value-format policy.
            build_plugin(args.cc, ROOT / 'tests/probes/vpi/packed_bit.c',
                         root / 'packed_bit_reads.vpi', extra=('-DPACKED_BIT_REFERENCE_READS',))
        for name, source, expected in programs():
            if args.case and name not in args.case:
                continue
            path = root / 'case.sv'
            path.write_text(source)
            ran = bounded_run.run([str(args.kirimasim.resolve()), '--top=top',
                                   '--vpi=' + str(plugin), str(path)], cwd=root, timeout=15)
            diagnostic = '\n'.join(line for line in ran.stderr.splitlines()
                                   if not line.startswith('kirimasim: info:'))
            row = dict(name=name, source_sha256=hashlib.sha256(source.encode()).hexdigest(),
                       expected_stdout=expected, native_exit=ran.returncode,
                       native_stdout=ran.stdout, diagnostics=diagnostic.replace(tmp, '<probe>'),
                       passed=ran.returncode == 0 and ran.stdout == expected and not diagnostic)
            if args.iverilog:
                compiled = bounded_run.run([str(args.iverilog.resolve()), '-g2012', '-s', 'top',
                                           '-o', 'sim', str(path)], cwd=root, timeout=15)
                ref = bounded_run.run([str(args.vvp.resolve()), '-M' + str(root), '-mpacked_bit_reads',
                                       'sim'], cwd=root, timeout=15) if compiled.returncode == 0 else None
                row.update(reference_compile_exit=compiled.returncode,
                           reference_compile_diagnostics=compiled.stderr.replace(tmp, '<probe>'),
                           reference_exit=ref.returncode if ref else None,
                           reference_stdout=ref.stdout if ref else None,
                           reference_diagnostics=ref.stderr.replace(tmp, '<probe>') if ref else None,
                           reference_expected_stdout='packed-bit-read errors=0 missing=0\n',
                           reference_profile='numeric reads; callbacks and ObjTypeVal excluded',
                           reference_matches=bool(ref and ref.returncode == 0 and
                                                  ref.stdout == 'packed-bit-read errors=0 missing=0\n' and
                                                  not ref.stderr))
            results.append(row)
            if not row['passed']:
                print(name, 'FAILED', repr(ran.stdout), row['diagnostics'], flush=True)
    assert results, 'no selected cases'
    if args.report:
        args.report.parent.mkdir(parents=True, exist_ok=True)
        args.report.write_text(json.dumps(results, indent=2) + '\n')
    count = sum(row['passed'] for row in results)
    print(f'VPI packed bits: {count}/{len(results)} passed')
    return 0 if count == len(results) else 1


if __name__ == '__main__':
    sys.exit(main())
