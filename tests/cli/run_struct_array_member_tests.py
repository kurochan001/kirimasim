#!/usr/bin/env python3
# Copyright (c) 2026 The kirimasim Authors
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

"""Original source checks for fixed array members in automatic records."""

import argparse
from dataclasses import dataclass
from pathlib import Path
import re
import subprocess
import sys
import tempfile

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / "tools"))
import bounded_run  # noqa: E402


@dataclass
class Case:
    name: str
    source: str


TYPES = '''typedef struct {
  int words[3:1];
  logic [128:0] wide[-1:0][2:1];
  string text[2];
  real fraction[2];
  int preset[2]='{11,13};
} record_t;
typedef struct {int prefix=17; record_t inner;} nested_t;
'''


def module(declarations, body):
    return ('`timescale 1ns/1ns\nmodule top;\n' + declarations
            + '\ninitial begin\n' + body + '\n$display("passed");\nend\nendmodule\n')


def cases():
    result = []
    for nested in (False, True):
        dtype = 'nested_t' if nested else 'record_t'
        field = 'value.inner' if nested else 'value'
        body = (f'automatic {dtype} value;\n'
                f'if ({field}.words[3]!=0 || {field}.text[1]!="" || {field}.fraction[0]!=0.0 ||\n'
                f'    {field}.preset[0]!=11 || {field}.preset[1]!=13) $fatal(1,"defaults");\n'
                f'for (int i=3;i>=1;i--) {field}.words[i]=i*7;\n'
                f'{field}.wide[-1][2]=\'1; {field}.wide[0][1]=129\'h123;\n'
                f'{field}.wide[0][1][8:4]=5\'h1f;\n'
                f'{field}.text[1]="kept"; {field}.fraction[0]=1.5;\n'
                f'if ({field}.words[1]!=7 || {field}.words[3]!=21 ||\n'
                f'    {field}.wide[-1][2]!=={{129{{1\'b1}}}} || {field}.wide[0][1][8:4]!=31 ||\n'
                f'    {field}.text[1]!="kept" || {field}.fraction[0]!=1.5) $fatal(1,"elements");\n'
                f'if ($size({field}.words)!=3 || $left({field}.words)!=3 ||\n'
                f'    $dimensions({field}.wide)!=3 || $size({field}.wide,2)!=2) $fatal(1,"shape");\n')
        if nested:
            body += 'if (value.prefix!=17) $fatal(1,"nested default");\n'
        for home in ('initial', 'task', 'function', 'interface', 'package', 'unit', 'generate', 'program'):
            printed = '$display("passed");\n'
            if home in ('initial', 'generate', 'interface', 'program'):
                scope = ('interface holder;\n' if home == 'interface' else
                         'program holder;\n' if home == 'program' else 'module top;\n')
                start = 'if (1) begin : generated\n' if home == 'generate' else ''
                end = 'end\n' if home == 'generate' else ''
                source = ('`timescale 1ns/1ns\n' + scope + TYPES + start
                          + 'initial begin\n' + body + printed + 'end\n' + end)
                source += ('endinterface\nmodule top; holder h(); endmodule\n' if home == 'interface'
                           else 'endprogram\nmodule top; holder h(); endmodule\n' if home == 'program'
                           else 'endmodule\n')
            else:
                keyword = 'function automatic void' if home == 'function' else 'task automatic'
                ending = 'endfunction' if home == 'function' else 'endtask'
                routine = keyword + ' check_value();\n' + body + ending + '\n'
                call = 'pool::check_value();' if home == 'package' else 'check_value();'
                if home == 'package':
                    source = ('`timescale 1ns/1ns\npackage pool;\n' + TYPES + routine
                              + 'endpackage\nmodule top; initial begin ' + call*3 + printed + 'end endmodule\n')
                elif home == 'unit':
                    source = ('`timescale 1ns/1ns\n' + TYPES + routine
                              + 'module top; initial begin ' + call*3 + printed + 'end endmodule\n')
                else:
                    source = module(TYPES + routine, call*3)
            result.append(Case(('nested_' if nested else 'flat_') + home, source))

    result.append(Case('copies_patterns_and_returns', module('''
typedef struct {int words[3:1]; string text[2];} T;
function automatic T make(input int n);
  T value='{words:'{n,n+1,n+2}, text:'{"left","right"}};
  return value;
endfunction
task automatic check_value();
  T a=make(7);
  T b=a;
  if (b.words[3]!=7 || b.words[1]!=9 || b.text[1]!="right") $fatal(1,"copy");
  b.words[3]=31; b.text[1]="changed";
  if (a.words[3]!=7 || a.text[1]!="right") $fatal(1,"independent copy");
  a=b;
  if (a.words[3]!=31 || a.text[1]!="changed") $fatal(1,"assignment");
endtask
''', 'check_value();')))
    result.append(Case('input_output_inout_and_defaults', module('''
typedef struct {int words[2]='{3,5}; int tag=7;} T;
T actual;
task automatic put(output T value);
  if (value.words[0]!=3 || value.words[1]!=5 || value.tag!=7) $fatal(1,"output default");
  value.words[0]=11;
endtask
task automatic change(inout T value); #1; value.words[1]+=2; endtask
task automatic take(input T value='{words:'{17,19},tag:23});
  if (value.words[0]!=17 || value.words[1]!=19 || value.tag!=23) $fatal(1,"input default");
  value.words[0]=99;
endtask
''', 'put(actual); change(actual); take(); if (actual.words[0]!=11 || actual.words[1]!=7) $fatal(1,"copyback");')))
    result.append(Case('array_arguments_and_refs', module('''
typedef struct {int words[3:1];} T;
task automatic change(ref int words[3:1]); words[2]=41; endtask
task automatic copy(output int words[3:1]); words='{5,7,11}; endtask
task automatic check_value();
  T value;
  copy(value.words); change(value.words);
  if (value.words[3]!=5 || value.words[2]!=41 || value.words[1]!=11) $fatal(1,"array formal");
endtask
''', 'check_value();')))
    result.append(Case('captured_reentry_waits_and_owner_return', module('''
typedef struct {int words[2]; logic [128:0] wide[2];} T;
int completed,total;
task automatic launch(input int n);
  T value;
  value.words[0]=n; value.wide[1]='1;
  fork
    begin #1; value.words[1]=value.words[0]+1; end
    begin
      wait(value.words[1]!=0);
      if (value.words[1]!=n+1 || value.wide[1]!=={129{1'b1}}) $fatal(1,"capture");
      total+=value.words[0]; completed++;
    end
  join_none
endtask
''', 'for (int n=0;n<12;n++) launch(n); #2; if (completed!=12 || total!=66) $fatal(1,"completion");')))
    result.append(Case('fixed_arrays_of_containers', module('''
typedef int queue_t[$];
typedef struct {queue_t queues[2]; int dynamic_words[2][];} T;
task automatic check_value();
  T value;
  T copied;
  if (value.queues[0].size()!=0 || value.dynamic_words[1].size()!=0) $fatal(1,"empty");
  value.queues[0].push_back(7); value.dynamic_words[1]=new[2]; value.dynamic_words[1][0]=13;
  copied=value;
  copied.queues[0].push_back(11); copied.dynamic_words[1][0]=17;
  if (value.queues[0].size()!=1 || value.dynamic_words[1][0]!=13 || copied.queues[0][1]!=11)
    $fatal(1,"container deep copy");
endtask
''', 'for (int n=0;n<100;n++) check_value();')))
    result.append(Case('invalid_indices', module('''
typedef struct {int words[2]; logic [128:0] wide[2]; string text[2];} T;
task automatic check_value();
  T value;
  logic [1:0] bad='x;
  int below=-1, above=3;
  value.words[0]=7; value.words[1]=11;
  value.words[bad]=99; value.words[below]=101;
  if (value.words[0]!=7 || value.words[1]!=11 || value.words[above]!=0 ||
      value.wide[bad]!=={129{1'bx}} || value.text[below]!="") $fatal(1,"invalid index");
endtask
''', 'check_value();')))
    result.append(Case('four_state_defaults', module('''
typedef struct {logic [128:0] wide[2]; logic [4:0] narrow[2]; bit [7:0] bits[2];
                logic [4:0] preset[2]='{5'd7,5'd11};} T;
task automatic inspect(input T value);
  if (value.bits[0]!=0 || value.bits[1]!=0 || value.preset[0]!=7 || value.preset[1]!=11)
    $fatal(1,"explicit and two-state defaults");
  if ($test$plusargs("one")) begin
    if (value.wide[0]!=={129{1'b1}} || value.wide[1]!=={129{1'b1}} ||
        value.narrow[0]!==5'b11111 || value.narrow[1]!==5'b11111) $fatal(1,"one defaults");
  end else if ($test$plusargs("zero")) begin
    if (value.wide[0]!==129'b0 || value.wide[1]!==129'b0 ||
        value.narrow[0]!==5'b0 || value.narrow[1]!==5'b0) $fatal(1,"zero defaults");
  end else if ($test$plusargs("random")) begin
    if ($isunknown(value.wide[0]) || $isunknown(value.wide[1]) ||
        value.wide[0]===value.wide[1]) $fatal(1,"independent random defaults");
  end else begin
    if (value.wide[0]!=={129{1'bx}} || value.wide[1]!=={129{1'bx}} ||
        value.narrow[0]!==5'bx || value.narrow[1]!==5'bx) $fatal(1,"four-state defaults");
  end
endtask
task automatic produce(output T value); endtask
task automatic check_value();
  T value;
  T output_value;
  inspect(value); produce(output_value); inspect(output_value);
endtask
''', 'check_value();')))
    result.append(Case('blocking_snapshot_across_wait', module('''
typedef int queue_t[$];
typedef struct {int words[2]; queue_t queues[2];} T;
task automatic check_value();
  T source, copied;
  source.words='{7,11}; source.queues[1].push_back(13);
  fork begin #1; source.words[0]=41; source.queues[1][0]=43; end join_none
  copied=#2 source;
  if (copied.words[0]!=7 || copied.words[1]!=11 || copied.queues[1][0]!=13 ||
      source.words[0]!=41 || source.queues[1][0]!=43) $fatal(1,"blocking snapshot");
endtask
''', 'check_value();')))
    result.append(Case('pattern_retains_earlier_container_arrays', module('''
typedef int queue_t[$];
typedef struct {queue_t queues[2]; int mark;} T;
T source;
function automatic int mutate(); source.queues[0].push_back(99); return 1; endfunction
task automatic check_value();
  T value='{queues:source.queues,mark:mutate()};
  if (value.queues[0].size()!=1 || value.queues[0][0]!=7 || value.queues[1][0]!=11 ||
      value.mark!=1 || source.queues[0].size()!=2) $fatal(1,"pattern snapshot");
endtask
''', 'source.queues[0].push_back(7); source.queues[1].push_back(11); check_value();')))
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--kirimasim', required=True, type=Path)
    binary = str(parser.parse_args().kirimasim.resolve())
    failures = []
    checks = 0
    with tempfile.TemporaryDirectory(prefix='kirimasim-struct-arrays-') as directory:
        root = Path(directory)
        for case in cases():
            path = root / (case.name + '.sv')
            path.write_text(case.source)
            for flags in (('parse',), ('--dump-sir',), (), ('--init-values=zero',),
                          ('--init-values=one',), ('--init-values=random', '--seed=43')):
                mode = next((flag.split('=', 1)[1] for flag in flags
                             if flag.startswith('--init-values=')), None)
                plusargs = ['+' + mode] if mode else []
                result = bounded_run.run([binary, *flags, '--max-steps=1000000', str(path), *plusargs],
                                         cwd=root, timeout=20)
                if (result.returncode or ': error:' in result.stderr or
                        (flags[:1] not in (('parse',), ('--dump-sir',)) and result.stdout!='passed\n') or
                        re.search(r': (?:fatal|warning):', result.stderr)):
                    failures.append(f'{case.name} {flags}: exit={result.returncode}\n'
                                    f'{result.stdout}\n{result.stderr}')
                checks += 1
    print(f'struct-array-members: {checks} checks; {len(failures)} failures')
    for failure in failures:
        print(failure)
    return bool(failures)


if __name__ == '__main__':
    try:
        raise SystemExit(main())
    except (bounded_run.OutputLimit, subprocess.TimeoutExpired) as error:
        print(f'FAIL: {error}', file=sys.stderr)
        raise SystemExit(1)
