#!/usr/bin/env python3
# Copyright (c) 2026 The kirimasim Authors
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

"""Original source checks for event leaves in automatic unpacked structures."""

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
    constructors: int | None = None


TYPES = '''typedef struct {event ready; event done; int tag=7;} record_t;
typedef struct {int prefix=3; record_t inner; logic [128:0] payload='1;} nested_t;
'''


def module(declarations, body):
    return ('`timescale 1ns/1ns\nmodule top;\n' + declarations
            + '\ninitial begin\n' + body + '\n$display("passed");\nend\nendmodule\n')


def require(condition, label):
    return f'if (!({condition})) $fatal(1,"{label}");\n'


def cases():
    result = []
    for nested in (False, True):
        dtype = 'nested_t' if nested else 'record_t'
        field = 'value.inner' if nested else 'value'
        body = ('automatic ' + dtype + ' value;\n'
                + require(f'!{field}.ready.triggered && !{field}.done.triggered', 'fresh leaves')
                + f'->{field}.ready;\n'
                + require(f'{field}.ready.triggered && !{field}.done.triggered', 'distinct events')
                + require(f'{field}.tag==7', 'member default'))
        if nested:
            body += require("value.prefix==3 && value.payload===129'b1", 'nested defaults')
            body = body.replace("129'b1", "{129{1'b1}}")
        for home in ('initial', 'task', 'function', 'interface', 'package', 'unit', 'generate', 'program'):
            name = ('nested_' if nested else 'flat_') + home
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
                call = 'pool::check_value();' if home == 'package' else 'check_value();'
                routine = keyword + ' check_value();\n' + body + '\n' + ending + '\n'
                if home == 'package':
                    source = ('`timescale 1ns/1ns\npackage pool;\n' + TYPES + routine
                              + 'endpackage\nmodule top; initial begin ' + call*3 + printed + 'end endmodule\n')
                elif home == 'unit':
                    source = ('`timescale 1ns/1ns\n' + TYPES + routine
                              + 'module top; initial begin ' + call*3 + printed + 'end endmodule\n')
                else:
                    source = module(TYPES + routine, call*3)
            result.append(Case(name, source, 2))

    result.append(Case('null_and_default_members', module('''
typedef struct {event empty=null; event fresh; int tag=9;} T;
task automatic check_value();
  T value;
  if (value.empty.triggered || value.fresh.triggered || value.tag!=9) $fatal(1,"defaults");
  ->value.fresh;
  if (value.empty.triggered || !value.fresh.triggered) $fatal(1,"null identity");
endtask
''', 'check_value(); check_value();'), 1))
    for pattern in (False, True):
        initializer = "'{source.ready,source.done,19}" if pattern else 'source'
        body = f'''record_t source;
task automatic check_value();
  record_t copied={initializer};
  ->copied.ready;
  if (!source.ready.triggered || copied.done.triggered) $fatal(1,"initialization shares identity");
  copied.tag=31;
  if (source.tag!=7) $fatal(1,"ordinary member is copied");
endtask
'''
        result.append(Case('pattern_copy' if pattern else 'structure_copy', module(TYPES + body, 'check_value();'), 0))
    result.append(Case('nested_input_forward', module(TYPES + '''
nested_t source;
task automatic inner(input nested_t value);
  event shared=value.inner.ready;
  #1; ->shared;
  if (!value.inner.ready.triggered || value.inner.done.triggered) $fatal(1,"forwarded event");
  value.inner.tag=21;
endtask
task automatic outer(input nested_t value); inner(value); endtask
''', 'outer(source); if (!source.inner.ready.triggered || source.inner.tag!=7) $fatal(1,"input copy");'), 0))
    result.append(Case('default_input_pattern', module(TYPES + '''
task automatic check_value(input record_t value='{null,null,11});
  if (value.ready.triggered || value.done.triggered || value.tag!=11) $fatal(1,"default argument");
endtask
''', 'check_value();'), 0))
    result.append(Case('reentered_captured_blocks', module(TYPES + 'int completed;\n', '''
for (int n=0;n<6;n++) begin
  automatic int expected=n;
  automatic nested_t value;
  value.inner.tag=n;
  fork
    begin #2; ->value.inner.ready; end
    begin
      @(value.inner.ready);
      if (value.inner.tag!=expected || value.inner.done.triggered) $fatal(1,"retained activation");
      completed++;
    end
  join_none
end
wait fork;
if (completed!=6) $fatal(1,"capture completion");
''')))
    result.append(Case('returned_owner_and_pending_trigger', module(TYPES + '''
int completed;
task automatic launch(input int tag);
  nested_t value;
  value.inner.tag=tag;
  ->> #2 value.inner.ready;
  fork
    begin
      @(value.inner.ready);
      if (value.inner.tag!=tag || value.inner.done.triggered) $fatal(1,"owner returned");
      ->value.inner.done;
      completed++;
    end
  join_none
endtask
''', 'launch(11); launch(29); #3; if (completed!=2) $fatal(1,"completion");')))
    result.append(Case('deferred_input_capture', module(TYPES + '''
int seen;
function automatic void consume(input event value);
  if (!value.triggered) $fatal(1,"deferred event identity");
  seen++;
endfunction
task automatic issue();
  record_t value;
  ->value.ready;
  assert #0 (0) else consume(value.ready);
endtask
''', 'issue(); #1; if (seen!=1) $fatal(1,"deferred completion");')))
    result.append(Case('collection_and_reuse', module(TYPES + '''
task automatic churn();
  nested_t value;
  if (value.inner.ready.triggered || value.inner.done.triggered) $fatal(1,"reused trigger state");
  ->value.inner.ready;
endtask
''', 'for (int n=0;n<4000;n++) churn();')))
    return result


def rejected():
    for automatic in (False, True):
        for assignment in ('a=b;', 'a<=b;', 'a=#1 b;', 'a<=#1 b;', 'a<=@(go) b;'):
            if automatic and '<=' in assignment:
                continue
            declaration = 'automatic record_t a,b;' if automatic else 'record_t a,b;'
            yield ('aggregate_' + str(automatic) + '_' + str(len(assignment)),
                   module(TYPES + 'event go;', declaration + assignment), 'event used as a value')
    for direction in ('output', 'inout'):
        yield (direction, module(TYPES + f'task automatic check_value({direction} record_t value); endtask\nrecord_t actual;',
                                 'check_value(actual);'), 'output or inout structure containing events')
    yield ('return', module(TYPES + 'function automatic record_t make(); record_t value; return value; endfunction\nrecord_t actual;',
                            'actual=make();'), 'event used as a value')
    for declaration in ('record_t rows[2];', 'struct {event value[2];} item;'):
        yield ('array_' + str(len(declaration)), module(TYPES + 'task automatic check_value();' + declaration + 'endtask',
                                                       'check_value();'), 'automatic unpacked array in a structure or of structures')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--kirimasim', required=True, type=Path)
    binary = str(parser.parse_args().kirimasim.resolve())
    failures = []
    checks = 0
    with tempfile.TemporaryDirectory(prefix='kirimasim-struct-events-') as directory:
        root = Path(directory)

        def run(path, *flags):
            return bounded_run.run([binary, *flags, str(path)], cwd=root, timeout=20)

        def fail(name, result):
            failures.append(f'{name}: exit={result.returncode}\n{result.stdout}\n{result.stderr}')

        for case in cases():
            path = root / (case.name + '.sv')
            path.write_text(case.source)
            for command in (('parse',), ('--dump-sir',)):
                result = run(path, *command)
                if result.returncode or ': error:' in result.stderr:
                    fail(case.name + ' ' + repr(command), result)
                if command == ('--dump-sir',) and case.constructors is not None:
                    if result.stdout.count('event.new') != case.constructors:
                        fail(case.name + ' constructor count', result)
                checks += 1
            for flags in ((), ('--init-values=zero',), ('--init-values=one',),
                          ('--init-values=random', '--seed=37')):
                result = run(path, '--max-steps=1000000', *flags)
                if (result.returncode or result.stdout != 'passed\n' or
                        re.search(r': (?:error|fatal|warning):', result.stderr)):
                    fail(case.name + ' run ' + repr(flags), result)
                checks += 1
        for index, (name, source, diagnostic) in enumerate(rejected()):
            path = root / f'rejected_{index}_{name}.sv'
            path.write_text(source)
            for flags in ((), ('--dump-sir',)):
                result = run(path, *flags)
                if (result.returncode != 2 or result.stdout or diagnostic not in result.stderr
                        or 'internal error' in result.stderr):
                    fail(name + ' rejection', result)
                checks += 1
        path = root / 'null_field_wait.sv'
        path.write_text(module('typedef struct {event ready=null;} T;',
                               'automatic T value; @(value.ready);'))
        for flags in (('parse',), ('--dump-sir',)):
            result = run(path, *flags)
            if result.returncode or ': error:' in result.stderr:
                fail('null field compile', result)
            checks += 1
        for flags in ((), ('--init-values=zero',), ('--init-values=one',),
                      ('--init-values=random', '--seed=37')):
            result = run(path, *flags)
            if (result.returncode != 2 or result.stdout or 'wait on a null event' not in result.stderr
                    or 'internal error' in result.stderr):
                fail('null field wait', result)
            checks += 1
    print(f'struct-events: {checks} checks; {len(failures)} failures')
    for failure in failures:
        print(failure)
    return bool(failures)


if __name__ == '__main__':
    try:
        raise SystemExit(main())
    except (bounded_run.OutputLimit, subprocess.TimeoutExpired) as error:
        print(f'FAIL: {error}', file=sys.stderr)
        raise SystemExit(1)
