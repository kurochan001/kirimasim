#!/usr/bin/env python3
# Copyright (c) 2026 The kirimasim Authors
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
"""Original typed implicit-container source matrix, approved contract 1-8.

The oracle models normalized source sequences and one resume per writer yield.
It never derives expectations from simulator output. The earlier investigation's
unapproved int/X/Z traces are retained separately, unchanged.
"""
import argparse
from collections import Counter
import fnmatch
import hashlib
import json
from pathlib import Path
import re
import subprocess
import sys
import tempfile

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / 'tools'))
import bounded_run  # noqa: E402


def normalized(value, width, four):
    if value in ('x', 'z'):
        return value * width if four else '0' * width
    return format(value & ((1 << width) - 1), f'0{width}b')


def oracle(mode, style, width, four, selected=False):
    operations = (((1,), (1, 2)), ((1, 7),), ((1, 7),), ((9, 7), (1, 7)),
                  (('x', 7),), (('z', 7),), ((),), ((),), ((4,),), ((4,),))
    active = mode != 'star' or style in ('direct', 'argument', 'side_effect')
    hits = int(mode != 'star')
    calls = hits if style != 'direct' else 0
    arguments = hits if style == 'side_effect' else 0
    observed = 0 if hits else -9
    state = ()
    output = []
    for phase, changes in enumerate(operations):
        event = False
        for values in changes:
            after = tuple(normalized(v, width, four) for v in values)
            event |= state != after
            state = after
        if active and event:
            hits += 1
            calls += style != 'direct'
            arguments += style == 'side_effect'
            observed = len(state)
        output.append(f'{phase} {hits}\n' if selected else
                      f'{phase} {hits} {calls} {arguments} {observed}\n')
    return ''.join(output)


def wrap(home, content):
    if home == 'interface':
        return 'interface worker;\n'+content+'endinterface\nmodule top;worker u();endmodule\n'
    if home == 'generate':
        return 'module top;if(1)begin:g\n'+content+'end endmodule\n'
    return 'module top;\n'+content+'endmodule\n'


def programs():
    for home in ('module', 'interface', 'generate'):
        for mode, procedure in (('comb', 'always_comb'), ('latch', 'always_latch'),
                                ('star', 'always @*')):
            for kind, dims in (('array', '[]'), ('queue', '[$]'), ('bounded', '[$:2]')):
                for width in (1, 65):
                    for four in (False, True):
                        leaf = f'{"logic" if four else "bit"} [{width-1}:0]'
                        for style in ('direct', 'global', 'argument', 'nested', 'side_effect'):
                            declarations = (f'{leaf} q{dims};int hits=0,calls=0,arg_calls=0,observed=-9;\n'
                                            'function automatic int global_size();++calls;return q.size();endfunction\n'
                                            f'function automatic int argument_size(input {leaf} values[]);'
                                            '++calls;return values.size();endfunction\n'
                                            'function automatic int nested_size();return global_size();endfunction\n'
                                            f'function automatic int effect_size(input {leaf} values[], input int ignored);'
                                            '++calls;return values.size();endfunction\n')
                            # Queue and dynamic input types are intentionally matched.
                            declarations = declarations.replace('values[]', 'values'+('[]' if kind == 'array' else '[$]'))
                            expression = {'direct': 'q.size()', 'global': 'global_size()',
                                          'argument': 'argument_size(q)', 'nested': 'nested_size()',
                                          'side_effect': 'effect_size(q,++arg_calls)'}[style]
                            grow = 'q=new[2];q[0]=1;q[1]=2;' if kind == 'array' else 'q.push_back(1);q.push_back(2);'
                            regrow = 'q=new[1];q[0]=4;' if kind == 'array' else 'q.push_back(4);'
                            changes = (grow, 'q[1]=7;', 'q=q;', 'q[0]=9;q[0]=1;',
                                       "q[0]='x;", "q[0]='z;", 'q.delete();', 'q.delete();',
                                       regrow, 'q=q;')
                            writer = 'initial begin #1;\n'
                            for phase, change in enumerate(changes):
                                writer += change+f' #1;$display("{phase} %0d %0d %0d %0d",hits,calls,arg_calls,observed);\n'
                            content = declarations + f'{procedure} begin ++hits;observed={expression};end\n'+writer+'end\n'
                            name = f'{home}_{mode}_{kind}_{width}_{int(four)}_{style}'
                            yield name, wrap(home, content), oracle(mode, style, width, four), None
    # Whole-identifier wildcard dependency for a selected read, with no size query.
    for kind, dims in (('array', '[]'), ('queue', '[$]')):
        for width in (1, 65, 130):
            for four in (False, True):
                leaf = f'{"logic" if four else "bit"} [{width-1}:0]'
                grow = 'q=new[2];q[0]=1;q[1]=2;' if kind == 'array' else 'q.push_back(1);q.push_back(2);'
                regrow = 'q=new[1];q[0]=4;' if kind == 'array' else 'q.push_back(4);'
                changes = (grow, 'q[1]=7;', 'q=q;', 'q[0]=9;q[0]=1;',
                           "q[0]='x;", "q[0]='z;", 'q.delete();', 'q.delete();', regrow, 'q=q;')
                content = f'{leaf} q{dims},sampled;int hits=0;always @* begin ++hits;sampled=q[0];end\n'
                # sampled is a scalar, not another container declaration.
                content = content.replace(f'q{dims},sampled;', f'q{dims};{leaf} sampled;')
                content += 'initial begin #1;'
                for phase, change in enumerate(changes):
                    content += change+f'#1;$display("{phase} %0d",hits);'
                content += 'end\n'
                yield f'selected_{kind}_{width}_{int(four)}', wrap('module', content), oracle('star', 'direct', width, four, True), None
    for home in ('module', 'interface'):
        for kind, dims in (('array', '[]'), ('queue', '[$]')):
            for width in (1, 130):
                grow = 'q=new[1];' if kind == 'array' else 'q.push_back(1);'
                content = (f'logic [{width-1}:0] q{dims};wire[31:0] observed;assign observed=q.size();\n'
                           'initial begin #1;'+grow+'#1;$display("%0d",observed);q=q;'
                           '#1;$display("%0d",observed);q.delete();#1;$display("%0d",observed);end\n')
                yield f'{home}_continuous_{kind}_{width}', wrap(home, content), '1\n1\n0\n', None
    for home in ('unit', 'package', 'class_static', 'static_subroutine'):
        for mode, procedure in (('comb', 'always_comb'), ('latch', 'always_latch'), ('star', 'always @*')):
            for indirect in (False, True):
                outer = ''
                inner = ''
                if home == 'unit':
                    outer, access = 'logic[64:0] q[$];\n', '$unit::q'
                elif home == 'package':
                    outer, access = 'package data;logic[64:0] q[$];endpackage\n', 'data::q'
                elif home == 'class_static':
                    outer, access = 'class Data;static logic[64:0] q[$];endclass\n', 'Data::q'
                else:
                    inner, access = 'function int holder();logic[64:0] q[$];return 0;endfunction\n', 'top.holder.q'
                expression = 'read_size()' if indirect else access+'.size()'
                content = inner+f'int hits=0,observed=-9;function int read_size();return {access}.size();endfunction\n'
                content += f'{procedure} begin ++hits;observed={expression};end\n'
                content += f'initial begin #1;{access}.push_back(1);#1;$display("%0d %0d",hits,observed);end\n'
                expected = '0 -9\n' if mode == 'star' and indirect else f'{1 if mode == "star" else 2} 1\n'
                yield f'{home}_{mode}_{int(indirect)}', outer+wrap('module', content), expected, None
    # Still unsupported: contributing selects, nonintegral/nested layouts,
    # const-ref bindings and called container bodies in continuous assignments.
    unsupported = 'implicit sensitivity to dynamic container values'
    for procedure in ('always_comb', 'always_latch', 'assign'):
        declaration = 'wire[31:0] observed;' if procedure == 'assign' else 'logic[31:0] observed;'
        for expression in ('q[0]', 'q[0]+q.size()'):
            content = f'logic[31:0] q[$];{declaration}{procedure} observed={expression};'
            error = 'cannot refer to an element or member of a dynamic type outside of a procedural context' if procedure == 'assign' else unsupported
            yield f'boundary_select_{procedure}_{len(expression)}', wrap('module', content), '', error
    for leaf in ('real', 'string', 'int'):
        dims = '[][]' if leaf == 'int' else '[]'
        content = f'{leaf} q{dims};int observed;always_comb observed=q.size();'
        yield f'boundary_layout_{leaf}', wrap('module', content), '', unsupported
    for direction in ('ref', 'const ref'):
        content = ('logic[31:0] q[$];int observed;function automatic int read_size('+direction+
                   ' logic[31:0] values[$]);return values.size();endfunction\n'
                   'always @* observed=read_size(q);')
        yield 'boundary_'+direction.replace(' ', '_'), wrap('module', content), '', unsupported
    content = ('int q[$];wire[31:0] observed;function int read_size();return q.size();endfunction\n'
               'assign observed=read_size();')
    yield 'boundary_continuous_call', wrap('module', content), '', unsupported


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--kirimasim', type=Path)
    parser.add_argument('--case', default='*')
    parser.add_argument('--json', type=Path)
    parser.add_argument('--self-check', action='store_true')
    args = parser.parse_args()
    cases = list(programs())
    assert len(cases) == len({name for name, *_ in cases})
    assert len(cases) == len({hashlib.sha256(source.encode()).hexdigest() for _, source, *_ in cases})
    assert oracle('comb', 'direct', 65, True).splitlines()[4:6] == ['4 5 0 0 2', '5 6 0 0 2']
    assert oracle('comb', 'direct', 65, False).splitlines()[4:6] == ['4 5 0 0 2', '5 5 0 0 2']
    if args.self_check:
        print(f'{len(cases)} distinct sources; independent normalization and event model checked')
        return 0
    if not args.kirimasim:
        parser.error('--kirimasim is required')
    binary = str(args.kirimasim.resolve())
    rows = []
    with tempfile.TemporaryDirectory(prefix='implicit container acceptance ') as temporary:
        work = Path(temporary)
        for name, source, expected, error in cases:
            if not fnmatch.fnmatchcase(name, args.case):
                continue
            source = '`timescale 1ns/1ns\n'+source
            path = work / 'probe.sv'
            path.write_text(source)
            row = dict(name=name, source=source, source_sha256=hashlib.sha256(source.encode()).hexdigest(),
                       expected_stdout=expected, expected_error=error)
            try:
                result = bounded_run.run([binary, '--top=top', str(path)], cwd=work, timeout=15)
                row.update(exit=result.returncode, stdout=result.stdout,
                           stderr=result.stderr.replace(temporary, '<probe>'))
                row['passed'] = (result.returncode == 2 and re.search(r':\d+:\d+: error:', result.stderr) is not None and
                                 error in result.stderr) if error else result.returncode == 0 and result.stdout == expected
            except (subprocess.TimeoutExpired, bounded_run.OutputLimit) as failure:
                row.update(passed=False, failure=type(failure).__name__)
            rows.append(row)
            if not row['passed']:
                print(name, 'FAILED', row.get('exit'), repr(row.get('stdout')), row.get('stderr', '')[:600])
    if not rows:
        parser.error('no cases selected')
    summary = dict(Counter('passed' if row['passed'] else 'failed' for row in rows))
    if args.json:
        args.json.parent.mkdir(parents=True, exist_ok=True)
        args.json.write_text(json.dumps(dict(summary=summary, cases=rows), ensure_ascii=False, indent=2)+'\n')
    print(f'implicit container matrix: {len(rows)} cases, {summary}')
    return int(any(not row['passed'] for row in rows))


if __name__ == '__main__':
    raise SystemExit(main())
