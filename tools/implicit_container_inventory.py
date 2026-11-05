#!/usr/bin/env python3
# Copyright (c) 2026 The kirimasim Authors
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
"""Record original implicit-container probes against proposed, unapproved oracles.

This is an investigation tool, not an acceptance test. Exit zero means the
observations were recorded, not that any simulator passed every proposed rule.
"""
import argparse
from collections import Counter
import hashlib
import json
from pathlib import Path
import subprocess
import tempfile

import bounded_run

ROOT = Path(__file__).resolve().parent.parent
PHASES = (((1,), (1, 2)), ((1, 7),), ((1, 7),), ((9, 7), (1, 7)),
          (('X', 7),), (('Z', 7),), ((),), ((),), ((4,),), ((4,),))


def trace(mode, style):
    # A waiter detaches after its first event, then resumes once after the
    # writer yields. Compare each committed value, including A -> B -> A.
    sensitive = mode != 'star' or style in ('direct', 'argument')
    hits = int(mode != 'star')
    calls = hits if style != 'direct' else 0
    observed = 0 if hits else -9
    previous = ()
    rows = []
    for phase, changes in enumerate(PHASES):
        changed = False
        for current in changes:
            changed |= current != previous
            previous = current
        if sensitive and changed:
            hits += 1
            calls += int(style != 'direct')
            observed = len(previous)
        rows.append(f'{phase} {hits} {calls} {observed}\n')
    return ''.join(rows)


def programs():
    historical = json.loads((ROOT / 'docs/measurements/2026-11-05/container-implicit-followup.json').read_text())
    for row in historical['cases']:
        assert hashlib.sha256(row['source'].encode()).hexdigest() == row['source_sha256']
        yield dict(name=row['name'], source=row['source'], expected_stdout=row['expected_stdout'],
                   oracle_status='existing recorded source and oracle retained byte for byte',
                   category='historical_required_gap')
    for home in ('module', 'interface', 'generate'):
        for mode, procedure in [('comb', 'always_comb'), ('latch', 'always_latch'),
                                ('star', 'always @*')]:
            for kind, dims in [('queue', '[$]'), ('array', '[]')]:
                for style in ('direct', 'global', 'argument', 'nested'):
                    definitions = (f'int q{dims}; int hits=0,calls=0,observed=-9;\n'
                                   'function automatic int global_size();\n'
                                   '++calls; return q.size(); endfunction\n'
                                   f'function automatic int argument_size(input int values{dims});\n'
                                   '++calls; return values.size(); endfunction\n'
                                   'function automatic int nested_size();\n'
                                   'return global_size(); endfunction\n')
                    expression = dict(direct='q.size()', global_='global_size()',
                                      argument='argument_size(q)', nested='nested_size()')
                    expression = expression['global_' if style == 'global' else style]
                    grow = ('q.push_back(1);q.push_back(2);' if kind == 'queue'
                            else 'q=new[2];q[0]=1;q[1]=2;')
                    regrow = 'q.push_back(4);' if kind == 'queue' else 'q=new[1];q[0]=4;'
                    operations = (grow, 'q[1]=7;', 'q=q;', 'q[0]=9;q[0]=1;',
                                  "q[0]='x;", "q[0]='z;", 'q.delete();', 'q.delete();',
                                  regrow, 'q=q;')
                    body = '#1;\n'
                    for phase, operation in enumerate(operations):
                        body += operation + f' #1;$display("{phase} %0d %0d %0d",hits,calls,observed);\n'
                    observer = f'{procedure} begin ++hits;observed={expression};end\n'
                    content = definitions + observer + 'initial begin\n' + body + 'end\n'
                    if home == 'interface':
                        source = 'interface worker;\n' + content + 'endinterface\nmodule top;worker u();endmodule\n'
                    else:
                        opening = 'if(1)begin:g\n' if home == 'generate' else ''
                        closing = 'end\n' if home == 'generate' else ''
                        source = 'module top;\n' + opening + content + closing + 'endmodule\n'
                    yield dict(name=f'{home}_{mode}_{kind}_{style}',
                               source='`timescale 1ns/1ns\n' + source,
                               expected_stdout=trace(mode, style),
                               oracle_status='proposed for human review; not an accepted suite expectation',
                               category='proposed_value_change_trace')
    for home in ('module', 'interface'):
        for kind, dims in [('queue', '[$]'), ('array', '[]')]:
            grow = 'q.push_back(7);' if kind == 'queue' else 'q=new[1];'
            content = (f'int q{dims};wire [31:0] observed;assign observed=q.size();\n'
                       'initial begin #1;' + grow +
                       '#1;$display("%0d",observed);q=q;#1;$display("%0d",observed);'
                       'q.delete();#1;$display("%0d",observed);end\n')
            source = ('module top;\n'+content+'endmodule\n' if home == 'module' else
                      'interface worker;\n'+content+'endinterface\nmodule top;worker u();endmodule\n')
            yield dict(name=f'{home}_continuous_{kind}', source='`timescale 1ns/1ns\n'+source,
                       expected_stdout='1\n1\n0\n',
                       oracle_status='proposed for human review; not an accepted suite expectation',
                       category='proposed_direct_continuous')
    # Separate minimal admission/scheduling evidence from operations that
    # expose independent reference failures in the larger traces.
    for home in ('module', 'interface', 'generate'):
        for mode, procedure in [('comb', 'always_comb'), ('latch', 'always_latch'),
                                ('star', 'always @*')]:
            for kind, dims in [('queue', '[$]'), ('array', '[]')]:
                grow = 'q.push_back(7);' if kind == 'queue' else 'q=new[1];'
                content = (f'int q{dims};int observed=-9;{procedure} observed=q.size();\n'
                           'initial begin #1;' + grow + '#1;$display("%0d",observed);end\n')
                if home == 'interface':
                    source = 'interface worker;\n'+content+'endinterface\nmodule top;worker u();endmodule\n'
                else:
                    opening = 'if(1)begin:g\n' if home == 'generate' else ''
                    closing = 'end\n' if home == 'generate' else ''
                    source = 'module top;\n'+opening+content+closing+'endmodule\n'
                yield dict(name=f'{home}_{mode}_{kind}_growth',
                           source='`timescale 1ns/1ns\n'+source, expected_stdout='1\n',
                           oracle_status='new proposed source for the existing recorded size-growth behavior',
                           category='minimal_size_growth')


def observe(command, directory, expected, timeout):
    try:
        result = bounded_run.run(command, cwd=directory, timeout=timeout)
    except subprocess.TimeoutExpired:
        return dict(outcome='timeout', exit=None, matches=False)
    except bounded_run.OutputLimit:
        return dict(outcome='output_limit', exit=None, matches=False)
    status = 'match' if result.returncode == 0 and result.stdout == expected else 'mismatch'
    if result.returncode != 0:
        status = 'unsupported' if 'unsupported: implicit sensitivity to dynamic container values' in result.stderr else 'process_failure'
    return dict(outcome=status, exit=result.returncode, stdout=result.stdout,
                stderr=result.stderr.replace(str(directory), '<probe>'), matches=status == 'match')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--kirimasim', type=Path)
    parser.add_argument('--iverilog', type=Path)
    parser.add_argument('--vvp', type=Path)
    parser.add_argument('--output', type=Path)
    parser.add_argument('--sources', type=Path)
    parser.add_argument('--self-check', action='store_true')
    args = parser.parse_args()
    if bool(args.iverilog) != bool(args.vvp):
        parser.error('select both reference compiler and runtime')
    cases = list(programs())
    assert len(cases) == 96 and len({x['name'] for x in cases}) == 96
    assert len({hashlib.sha256(x['source'].encode()).hexdigest() for x in cases}) == 96
    assert trace('comb', 'direct').splitlines()[1:4] == ['1 3 0 2', '2 3 0 2', '3 4 0 2']
    assert trace('star', 'global') == ''.join(f'{i} 0 0 -9\n' for i in range(10))
    if args.self_check:
        print('96 distinct original sources; proposed state-change trace checks passed')
        return 0
    if not args.kirimasim or not args.output:
        parser.error('comparison needs --kirimasim and --output')
    def version(tool, flag):
        r = bounded_run.run([str(tool.resolve()), flag], timeout=10)
        return (r.stdout+'\n'+r.stderr).strip().splitlines()[0]
    report = dict(oracle_status='Proposed traces require review; observed matches are not P1 acceptance',
                  scope='Separate investigation, not appended to the 188-probe inventory or fixed P1 populations',
                  native_version=version(args.kirimasim, '--version'), cases=[])
    if args.iverilog:
        report.update(reference_compiler=version(args.iverilog, '-V'),
                      reference_runtime=version(args.vvp, '-V'))
    with tempfile.TemporaryDirectory(prefix='implicit container review ') as tmp:
        directory = Path(tmp)
        for case in cases:
            source = directory / 'probe.sv'
            source.write_text(case['source'])
            row = dict(case, source_sha256=hashlib.sha256(case['source'].encode()).hexdigest())
            row['native'] = observe([str(args.kirimasim.resolve()), '--top=top', str(source)],
                                    directory, case['expected_stdout'], 10)
            if args.iverilog:
                compiled = observe([str(args.iverilog.resolve()), '-g2012', '-s', 'top',
                                    '-o', 'sim', str(source)], directory, '', 10)
                row['reference_compile'] = compiled
                if compiled.get('exit') == 0:
                    row['reference'] = observe([str(args.vvp.resolve()), 'sim'], directory,
                                               case['expected_stdout'], 10)
            if args.sources:
                args.sources.mkdir(parents=True, exist_ok=True)
                (args.sources / (case['name'] + '.sv')).write_text(case['source'])
            report['cases'].append(row)
            print(case['name'], row['native']['outcome'],
                  row.get('reference', row.get('reference_compile', {})).get('outcome', ''), flush=True)
    report['summary'] = dict(total=len(cases), native=dict(Counter(x['native']['outcome'] for x in report['cases'])))
    if args.iverilog:
        report['summary']['reference'] = dict(Counter(
            x.get('reference', x['reference_compile'])['outcome'] for x in report['cases']))
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(report, ensure_ascii=False, indent=2)+'\n')
    print('Observations recorded; proposed oracles remain unapproved:', report['summary'])
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
