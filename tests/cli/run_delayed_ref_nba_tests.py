#!/usr/bin/env python3
# Copyright (c) 2026 The kirimasim Authors
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

"""Original checks for delayed NBAs to static variables through scalar refs."""

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
            for shape, selected, untouched in (('[3:1]', '[1]', '[3]'),
                                                ('[2][3:1]', '[1][1]', '[0][1]')):
                body = ("values='{" + ("default:'{default:'0}" if shape.startswith('[2]') else "default:'0")
                        + '}; issue(values' + selected + '); #1;\n'
                        f"if (values{selected}!==T'(0)) $fatal(1,\"early update\");\n"
                        '#2;\n'
                        f"if (values{selected}!==T'({pattern}) || values{untouched}!==T'(0))\n"
                        '  $fatal(1,"captured ref and value");')
                source = module(f'''typedef {dtype} T;
T values{shape};
task automatic issue(ref static T r);
  T data={pattern};
  r<=#2 data;
  data='0;
endtask
''', body)
                result.append((f'w{width}_{four}_{len(result)}', source))
    base = result[-1][1]
    result.append(('program', base.replace('module top;', 'program runner;')
                   .replace('endmodule', 'endprogram\nmodule top; runner p(); endmodule')))
    result.append(('interface', base.replace('module top;', 'interface runner;')
                   .replace('endmodule', 'endinterface\nmodule top; runner p(); endmodule')))
    result.append(('generate', base.replace('initial begin', 'if (1) begin : generated\ninitial begin')
                   .replace('endmodule', 'end\nendmodule')))
    result.append(('function', base.replace('task automatic issue', 'function automatic void issue')
                   .replace('endtask', 'endfunction')))
    result.append(('actual_selector_and_delay_once', module('''
int values[2],index,calls,order;
function automatic int rhs(); order=order*10+1; return 17; endfunction
function automatic int delay_value(); calls++; order=order*10+2; return 2; endfunction
task automatic issue(ref static int r); r<=#(delay_value()) rhs(); endtask
''', 'issue(values[index++]); index=0; if (calls!=1 || order!=12) $fatal(1,"evaluation order"); #3; if (values[0]!=17 || values[1]!=0 || calls!=1) $fatal(1,"captured scalar actual");')))
    result.append(('forwarded_ref_and_issuer_disable', module('''
int value;
task automatic inner(ref static int r); r<=#2 7; disable inner; endtask
task automatic outer(ref static int r); inner(r); endtask
''', 'outer(value); wait fork; if ($time!=0) $fatal(1,"not a child"); #3; if (value!=7) $fatal(1,"survives disable");')))
    result.append(('retained_static_ref_child', module('''
int value;
task automatic issue(ref static int r);
  fork begin #1; r<=#2 11; end join_none
endtask
''', 'issue(value); #2; if(value!=0) $fatal(1,"early child update"); #2; if(value!=11) $fatal(1,"retained reference");')))
    result.append(('same_deadline_order', module('''
int value;
task automatic issue(ref static int r,input int data); r<=#2 data; endtask
''', 'issue(value,7); value<=#2 11; issue(value,13); #3; if (value!=13) $fatal(1,"same deadline order");')))
    result.append(('zero_and_unknown_delay', module('''
int value; logic[128:0] ticks;
task automatic issue(ref static int r,input int data); r<=#(ticks) data; endtask
''', "ticks='x; issue(value,7); ticks=0; issue(value,11); if(value!=0) $fatal(1,\"not immediate\"); #1; if(value!=11) $fatal(1,\"zero delay NBA order\");")))
    result.append(('reals_strings_and_null_handles', module('''
real number; shortreal small_number; string text; chandle handle;
task automatic put_real(ref static real r); r<=#2 1.5; endtask
task automatic put_small(ref static shortreal r); r<=#2 2.25; endtask
task automatic put_text(ref static string r); string source="snapshot"; r<=#2 source; source="changed"; endtask
task automatic put_handle(ref static chandle r); r<=#2 null; endtask
''', 'put_real(number); put_small(small_number); put_text(text); put_handle(handle); #3; if(number!=1.5 || small_number!=2.25 || text!="snapshot" || handle!=null) $fatal(1,"nonintegral values");')))
    result.append(('container_values_and_collection', module('''
typedef int Q[$]; typedef int D[];
Q queue_value; D array_value;
task automatic put_queue(ref static Q r); Q data={7,11}; r<=#2 data; data[0]=99; endtask
task automatic put_array(ref static D r); D data=new[2]; data[0]=13;data[1]=17;r<=#2 data;data[0]=101;endtask
task automatic churn(); Q work={31,37}; work.push_back(41); endtask
''', 'put_queue(queue_value); put_array(array_value); for(int n=0;n<2000;n++) churn(); #3; if(queue_value.size()!=2 || queue_value[0]!=7 || queue_value[1]!=11 || array_value.size()!=2 || array_value[0]!=13 || array_value[1]!=17) $fatal(1,"typed roots and snapshots");')))
    result.append(('packed_structure_actual', module('''
typedef struct packed {logic[7:0] tag; logic[128:0] payload;} T;
T value;
task automatic issue(ref static T r); T data; data.tag=17;data.payload='1;r<=#2 data;data='0;endtask
''', "value='0;issue(value);#3;if(value.tag!=17 || value.payload!=={129{1'b1}}) $fatal(1,\"packed structure actual\");")))
    result.append(('program_origin_re_nba', '''`timescale 1ns/1ns
module top;
  int value;
  runner p();
  initial begin #1; value<=11; #1; if(value!=7) $fatal(1,"Re-NBA origin"); $display("passed"); end
endmodule
program runner;
  task automatic issue(ref static int r); r<=#1 7; endtask
  initial begin issue(top.value); #2; end
endprogram
'''))
    result.append(('snapshot_before_delay_side_effect', module('''
typedef int Q[$]; Q source={7,11}; Q value;
function automatic int delay_value(); source[0]=99; return 2; endfunction
task automatic issue(ref static Q r); r<=#(delay_value()) source; endtask
''', 'issue(value); #3; if(value.size()!=2 || value[0]!=7 || value[1]!=11 || source[0]!=99) $fatal(1,"snapshot before delay");')))
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--kirimasim', required=True, type=Path)
    binary = str(parser.parse_args().kirimasim.resolve())
    failures = []
    checks = 0
    with tempfile.TemporaryDirectory(prefix='kirimasim-delayed-ref-nba-') as directory:
        root = Path(directory)
        for name, source in cases():
            path = root / (name + '.sv')
            path.write_text(source)
            for flags in (('parse',), ('--dump-sir',), (), ('--init-values=zero',),
                          ('--init-values=one',), ('--init-values=random', '--seed=59')):
                result = bounded_run.run([binary, *flags, '--max-steps=2000000', str(path)],
                                         cwd=root, timeout=20)
                if (result.returncode or re.search(r': (?:error|fatal|warning):', result.stderr) or
                        (flags[:1] not in (('parse',), ('--dump-sir',)) and result.stdout!='passed\n') or
                        (flags==('--dump-sir',) and 'nba.ref.delay ' not in result.stdout)):
                    failures.append(f'{name} {flags}: exit={result.returncode}\n{result.stdout}\n{result.stderr}')
                checks += 1
    print(f'delayed-ref-nba: {checks} checks; {len(failures)} failures')
    for failure in failures:
        print(failure)
    return bool(failures)


if __name__ == '__main__':
    try:
        raise SystemExit(main())
    except (bounded_run.OutputLimit, subprocess.TimeoutExpired) as error:
        print(f'FAIL: {error}', file=sys.stderr)
        raise SystemExit(1)
