#!/usr/bin/env python3
# Copyright (c) 2026 The kirimasim Authors
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
"""Original source %p/%0p matrix; compare tokens, not implementation-defined spaces."""
import argparse
import json
import re
import sys
import tempfile
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / 'tools'))
import bounded_run

from gen_pattern_format_matrix import CONTEXTS, APIS, POSTPONED, source
TOKEN = re.compile(r'"(?:\\.|[^"\\])*"|\\[^\s]+|[A-Za-z_$][A-Za-z0-9_$]*|[0-9]+|[^\s]')

def tokens(text):
    # Quoted string contents and escaped identifier spelling are retained.
    return tuple(TOKEN.findall(text))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--kirimasim', required=True)
    parser.add_argument('--context', choices=CONTEXTS, action='append')
    parser.add_argument('--api', choices=APIS, action='append')
    parser.add_argument('--report', type=Path)
    args = parser.parse_args()
    assert tokens("'{a:1, s:\"a b\"}") == tokens("' { a : 1 , s : \"a b\" }")
    assert tokens('"a b"') != tokens('"ab"')
    simulator = str(Path(args.kirimasim).resolve())
    results = []
    with tempfile.TemporaryDirectory(prefix='kirimasim-pattern-matrix-') as tmp:
        directory = Path(tmp)
        for context in args.context or CONTEXTS:
            for api in args.api or APIS:
                if api in POSTPONED and context in ('ref', 'final'):
                    continue
                text, expected, count = source(context, api)
                path = directory / f'{context}_{api}.sv'
                path.write_text(text)
                output = directory/'result.txt';output.unlink(missing_ok=True)
                ran = bounded_run.run([simulator,str(path)],cwd=tmp,text=False,timeout=30)
                diagnostics = b'\n'.join(x for x in ran.stderr.splitlines() if not x.startswith(
                    (b'kirimasim: info: kirimasim ',b'kirimasim: info: quiescent at ',
                     b'kirimasim: info: finish at ')))
                actual = output.read_bytes() if api in ('fwrite','fdisplay') and output.exists() else ran.stdout
                if ran.returncode or diagnostics or tokens(actual.decode()) != tokens(expected):
                    raise AssertionError(f'{path.name}: exit={ran.returncode}, stderr={ran.stderr!r}\n'
                                         f'expected tokens={tokens(expected)!r}\nactual tokens={tokens(actual.decode())!r}')
                if api in ('fwrite','fdisplay') and ran.stdout:
                    raise AssertionError(f'{path.name}: unexpected console output')
                results.append({'context':context,'api':api,'conversions':count,'passed':True})
    if args.report:
        args.report.parent.mkdir(parents=True,exist_ok=True)
        args.report.write_text(json.dumps(results,indent=2)+'\n')
    print(f'pattern formats: {len(results)} source programs, {sum(x["conversions"] for x in results)} conversions; whitespace-independent token checks')
    return 0


if __name__=='__main__':sys.exit(main())
