#!/usr/bin/env python3
# Copyright (c) 2026 The kirimasim Authors
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
"""Check whole automatic VPI argument declarations and selected numeric bindings."""
import argparse
import hashlib
import json
from pathlib import Path
import sys
import tempfile

from run_vpi_tests import ROOT, build_plugin
import bounded_run
from gen_vpi_argument_declaration_matrix import programs


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--kirimasim', type=Path, required=True)
    parser.add_argument('--cc', default='cc')
    parser.add_argument('--iverilog', type=Path)
    parser.add_argument('--vvp', type=Path)
    parser.add_argument('--case', action='append')
    parser.add_argument('--report', type=Path)
    args = parser.parse_args()
    assert bool(args.iverilog) == bool(args.vvp)
    results = []
    with tempfile.TemporaryDirectory(prefix='vpi argument declarations ') as tmp:
        root = Path(tmp)
        plugin = root / 'argument_declaration.vpi'
        source_c = ROOT / 'tests/probes/vpi/argument_declaration.c'
        build_plugin(args.cc, source_c, plugin)
        if args.iverilog:
            build_plugin(args.cc, source_c, root / 'argument_reference.vpi',
                         extra=('-DDECLARATION_REFERENCE',))
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
            if args.iverilog and name not in ('activation_reuse', 'readonly_ref', 'overlapping_activations'):
                compiled = bounded_run.run([str(args.iverilog.resolve()), '-g2012', '-s', 'top',
                                           '-o', 'sim', str(path)], cwd=root, timeout=15)
                ref = bounded_run.run([str(args.vvp.resolve()), '-M' + str(root),
                                       '-margument_reference', 'sim'], cwd=root, timeout=15
                                      ) if compiled.returncode == 0 else None
                row.update(reference_compile_exit=compiled.returncode,
                           reference_compile_diagnostics=compiled.stderr.replace(tmp, '<probe>'),
                           reference_exit=ref.returncode if ref else None,
                           reference_stdout=ref.stdout if ref else None,
                           reference_diagnostics=ref.stderr.replace(tmp, '<probe>') if ref else None,
                           reference_profile='declaration metadata and numeric 0/1 bit reads/writes; '
                                             'no parent, ObjTypeVal, X/Z or expired-capability checks',
                           reference_matches=bool(ref and ref.returncode == 0 and
                                                  ref.stdout == expected and not ref.stderr))
            results.append(row)
            if not row['passed']:
                print(name, 'FAILED', repr(ran.stdout), row['diagnostics'], flush=True)
    assert results, 'no selected cases'
    if args.report:
        args.report.parent.mkdir(parents=True, exist_ok=True)
        args.report.write_text(json.dumps(results, indent=2) + '\n')
    count = sum(row['passed'] for row in results)
    print(f'VPI argument declarations: {count}/{len(results)} passed')
    return 0 if count == len(results) else 1


if __name__ == '__main__':
    sys.exit(main())
