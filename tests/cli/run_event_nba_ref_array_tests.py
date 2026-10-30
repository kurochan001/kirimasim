#!/usr/bin/env python3
# Copyright (c) 2026 The kirimasim Authors
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

"""Original source checks for event-NBA destinations through fixed array refs."""

import argparse
from pathlib import Path
import re
import subprocess
import sys
import tempfile

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / 'tools'))
import bounded_run  # noqa: E402


def module(declarations, body):
    return ('`timescale 1ns/1ns\nmodule top;\n' + declarations + '\ninitial begin\n'
            + body + '\n$display("passed"); end\nendmodule\n')


def cases():
    result = []
    for width in (1, 32, 65, 129):
        for four in (False, True):
            dtype = f'{"logic" if four else "bit"} [{width-1}:0]'
            pattern = f"{{{width}{{1'bz}}}}" if four else "'1"
            for shape, indices, selected, other in (
                    ('[1:3]', '[i]', '[1]', '[3]'),
                    ('[3:1]', '[i]', '[1]', '[3]'),
                    ('[2:1][-1:0]', '[i][j]', '[1][0]', '[2][-1]')):
                declarations = f'''typedef {dtype} value_t;
value_t values{shape}; event go;
task automatic issue(ref static value_t r{shape});
  int i=1,j=0;
  value_t data={pattern};
  r{indices} <= @(go) data;
  i=3; j=-1; data='0;
endtask
'''
                body = ("values='{default:'0}; issue(values); ->go; #1;\n"
                        f'if (values{selected}!==value_t\'({pattern}) || values{other}!==value_t\'(0))\n'
                        '  $fatal(1,"frozen target and value");')
                result.append((f'w{width}_{four}_{len(result)}', module(declarations, body)))
    base = result[-1][1]
    result.append(('program', base.replace('module top;', 'program runner;')
                   .replace('endmodule', 'endprogram\nmodule top; runner p(); endmodule')))
    result.append(('interface', base.replace('module top;', 'interface runner;')
                   .replace('endmodule', 'endinterface\nmodule top; runner p(); endmodule')))
    result.append(('generate', base.replace('initial begin', 'if (1) begin : generated\ninitial begin')
                   .replace('endmodule', 'end\nendmodule')))
    result.append(('function', base.replace('task automatic issue', 'function automatic void issue')
                   .replace('endtask', 'endfunction')))
    result.append(('subarray_forwarding', module('''
int values[2][3:1]; event go;
task automatic inner(ref static int r[-1:1]); r[1]<=@(go) 7; endtask
task automatic outer(ref static int r[3:1]); inner(r); endtask
''', 'outer(values[1]); ->go; #1; if (values[1][1]!=7 || values[0][1]!=0 || values[1][3]!=0) $fatal(1,"base offset and formal bounds");')))
    result.append(('whole_array_snapshot', module('''
int values[3:1]; event go;
task automatic issue(ref static int r[3:1]);
  int source[3:1]='{7,11,13};
  r<=repeat(2) @(go or go) source;
  source='{99,101,103};
endtask
''', 'issue(values); ->go; #1; if (values[3]!=0) $fatal(1,"one countdown"); ->go; #1; if (values[3]!=7 || values[2]!=11 || values[1]!=13) $fatal(1,"aggregate snapshot");')))
    result.append(('packed_element_selectors', module('''
logic [128:0] values[2]; event go;
task automatic issue(ref static logic [128:0] r[2]);
  int i=1,pos=62;
  r[i][pos+:13] <= @(go) 13'h1a5b;
  i=0; pos=0;
endtask
''', "values='{default:'0}; issue(values); ->go; #1; if (values[1]!== (129'h1a5b<<62) || values[0]!==129'b0) $fatal(1,\"frozen packed selection\");")))
    result.append(('invalid_indices_still_register', module('''
int values[2]; event go; bit enabled=1;
task automatic issue(ref static int r[2]);
  logic [64:0] bad='x;
  int outside=3;
  r[bad]<=@(go iff enabled) 7;
  r[outside]<=repeat(2) @(go iff enabled) 11;
endtask
''', 'issue(values); ->go; ->go; #1; if (values[0]!=0 || values[1]!=0) $fatal(1,"invalid target control");')))
    result.append(('evaluation_order_and_zero_repeat', module('''
int values[2]; event go; int order;
function automatic int rhs(); order=order*10+1; return 7; endfunction
function automatic int count(); order=order*10+2; return 0; endfunction
function automatic int selector(); order=order*10+3; return 1; endfunction
task automatic issue(ref static int r[2]); r[selector()]<=repeat(count()) @(go) rhs(); endtask
''', 'issue(values); if (order!=123 || values[1]!=0) $fatal(1,"capture order"); #1; if (values[1]!=7) $fatal(1,"zero repeat NBA");')))
    result.append(('issuer_disable_and_pending_order', module('''
int values[2]; event go;
task automatic issue(ref static int r[2]); r[1]<=@(go) 7; disable issue; endtask
''', 'issue(values); values[1]<=11; ->go; #1; if (values[1]!=11) $fatal(1,"issue order survives return");')))
    result.append(('container_element_snapshots', module('''
typedef int queue_t[$]; queue_t values[2]; event go;
task automatic issue(ref static queue_t r[2]);
  queue_t data={7,11};
  r[1]<=@(go) data;
  data[0]=99;
endtask
''', 'issue(values); ->go; #1; if (values[1].size()!=2 || values[1][0]!=7 || values[1][1]!=11 || values[0].size()!=0) $fatal(1,"typed queue payload");')))
    result.append(('churn_and_slot_notifications', module('''
int values[2],seen; event go;
always @(values[1]) seen++;
task automatic issue(ref static int r[2],input int n); r[1]<=@(go) n; endtask
''', 'for (int n=1;n<=256;n++) begin issue(values,n); ->go; #1; if (values[1]!=n || values[0]!=0) $fatal(1,"reused registration"); end if (seen!=256) $fatal(1,"slot notices");')))
    result.append(('retained_ref_static_child', module('''
int values[2]; event go;
task automatic issue(ref static int r[2]);
  fork begin #1; r[1]<=@(go) 17; end join_none
endtask
''', 'issue(values); #2; ->go; #1; if (values[1]!=17 || values[0]!=0) $fatal(1,"retained static reference");')))
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--kirimasim', required=True, type=Path)
    binary = str(parser.parse_args().kirimasim.resolve())
    failures = []
    checks = 0
    with tempfile.TemporaryDirectory(prefix='kirimasim-nba-ref-array-') as directory:
        root = Path(directory)
        for name, source in cases():
            path = root / (name + '.sv')
            path.write_text(source)
            for flags in (('parse',), ('--dump-sir',), (), ('--init-values=zero',),
                          ('--init-values=one',), ('--init-values=random', '--seed=47')):
                result = bounded_run.run([binary, *flags, '--max-steps=1000000', str(path)],
                                         cwd=root, timeout=20)
                diagnostic = re.search(r': (?:error|fatal|warning):', result.stderr)
                if (result.returncode or diagnostic or
                        (flags[:1] not in (('parse',), ('--dump-sir',)) and result.stdout!='passed\n') or
                        (flags==('--dump-sir',) and ' shape=' not in result.stdout)):
                    failures.append(f'{name} {flags}: exit={result.returncode}\n{result.stdout}\n{result.stderr}')
                checks += 1
    print(f'event-nba-ref-array: {checks} checks; {len(failures)} failures')
    for failure in failures:
        print(failure)
    return bool(failures)


if __name__ == '__main__':
    try:
        raise SystemExit(main())
    except (bounded_run.OutputLimit, subprocess.TimeoutExpired) as error:
        print(f'FAIL: {error}', file=sys.stderr)
        raise SystemExit(1)
