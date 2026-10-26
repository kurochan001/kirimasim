#!/usr/bin/env python3
# Copyright (c) 2026 The kirimasim Authors
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

"""Original live-runtime-process matrix: CLI 2.7 and SCHEDULER_SPEC 10.8/16.2."""

import argparse
import json
from pathlib import Path

OUT = Path(__file__).resolve().parents[1] / 'tests/cli/process_limit_matrix'
HOMES = ('module', 'static_task', 'automatic_task', 'function', 'interface', 'program', 'generate')


def wrap(home, body):
    declarations = 'int hits=0;'
    final = 'final $display("final=%0d",hits);'
    if home.endswith('_task'):
        lifetime = 'automatic' if home == 'automatic_task' else 'static'
        body = f'task {lifetime} act();\n{body}\nendtask initial act();'
    elif home == 'function':
        body = f'function automatic void act();\n{body}\nendfunction initial act();'
    elif home == 'generate':
        body = 'if(1)begin:g initial begin ' + body + ' end end'
    else:
        body = 'initial begin ' + body + ' end'
    if home in ('interface', 'program'):
        kind = 'program automatic' if home == 'program' else home
        return f'{kind} unit;\n{declarations}\n{body}\n{final}\nend{home}\nmodule top;unit u();endmodule\n'
    return f'module top;\n{declarations}\n{body}\n{final}\nendmodule\n'


def cases():
    # A function may create join_none descendants but may not itself wait.
    for home in HOMES:
        joins = ('join_none',) if home == 'function' else ('join', 'join_any', 'join_none')
        for join in joins:
            body = 'fork begin #1;++hits;end begin #2;++hits;end ' + join
            if home != 'function':
                body += ' wait fork;$display("done=%0d",hits);'
            for limit in (1, 2, 0):
                success = limit != 1
                output = ('done=2\n' if home != 'function' else '') + 'final=2\n' if success else 'final=0\n'
                reason = 'finish' if home == 'program' else 'quiescent'
                yield f'burst_{home}_{join}_{limit}', wrap(home, body), limit, output, 0 if success else 6, reason if success else 'resource_limit'
    for home in HOMES:
        if home == 'function':
            continue
        body = 'repeat(40)begin fork begin #1;++hits;end join end $display("done=%0d",hits);'
        yield f'churn_{home}', wrap(home, body), 1, 'done=40\nfinal=40\n', 0, 'finish' if home == 'program' else 'quiescent'
        body = '''fork
begin fork begin #3;++hits;end join_none end
join
fork begin #1;++hits;end join_none
#4;$display("done=%0d",hits);'''
        yield f'retained_ancestor_{home}', wrap(home, body), 2, 'done=2\nfinal=2\n', 0, 'finish' if home == 'program' else 'quiescent'
        body = '''fork begin #100;++hits;end begin #100;++hits;end join_none
#0;disable fork;
fork begin #1;++hits;end begin #2;++hits;end join
$display("done=%0d",hits);'''
        yield f'disable_{home}', wrap(home, body), 2, 'done=2\nfinal=2\n', 0, 'finish' if home == 'program' else 'quiescent'
    for wait in ('delay', 'event', 'guard'):
        statement = {'delay': '#100;', 'event': '@changed;', 'guard': 'wait(ready);'}[wait]
        text = f'''module top;int hits=0;event changed;bit ready=0;
initial begin fork begin {statement} ++hits;end join_none
#0;fork begin ++hits;end join_none
$display("unreached");end final $display("final=%0d",hits);endmodule\n'''
        yield f'waiting_{wait}', text, 1, 'final=0\n', 6, 'resource_limit'
    for home in ('module', 'program'):
        text = wrap(home, 'repeat(3)begin fork begin $strobe("late");end join end #1;$display("done=%0d",hits);')
        yield f'pending_reports_{home}', text, 1, 'late\nlate\nlate\ndone=0\nfinal=0\n', 0, 'finish' if home == 'program' else 'quiescent'
    for kind, declaration, touch in (
        ('integer', 'automatic int data=7;', 'hits+=data;'),
        ('queue', 'automatic int data[$]=\'{7,9};', 'hits+=data[0];'),
        ('string', 'automatic string data="kept";', 'if(data=="kept")++hits;'),
        ('event', 'automatic event changed;', '->changed;++hits;'),
    ):
        body = f'{declaration}fork begin {touch} end begin {touch} end join_none $display("unreached");'
        yield f'rejected_capture_{kind}', wrap('module', body), 1, 'final=0\n', 6, 'resource_limit'
    for home in ('module', 'program'):
        kind = 'program automatic' if home == 'program' else home
        name = 'unit' if home == 'program' else 'top'
        text = f'''{kind} {name};int hits=0;
function automatic int mark();++hits;return 7;endfunction
initial fork automatic int local_value=mark();
  begin #1;hits+=local_value;end begin #2;hits+=local_value;end
join
final $display("final=%0d",hits);end{home}\n'''
        if home == 'program':
            text += 'module top;unit u();endmodule\n'
        yield f'parent_initializer_{home}', text, 1, 'final=1\n', 6, 'resource_limit'
    static = '''module top;int hits=0;
for(genvar i=0;i<64;i++)begin:g initial #0; wire n;buf(n,1'b0);end
initial begin fork begin #1;++hits;end join $display("done=%0d",hits);end
final $display("final=%0d",hits);endmodule\n'''
    for limit in (1, 0, None, 18446744073709551615):
        yield f'static_exclusion_{limit}', static, limit, 'done=1\nfinal=1\n', 0, 'quiescent'


def artifacts():
    manifest = []
    for name, source, limit, stdout, exit_code, reason in cases():
        header = f'''// Copyright (c) 2026 The kirimasim Authors
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// Generated by tools/gen_process_limit_matrix.py; edit the generator.
// Grounds: CLI 2.7; SCHEDULER_SPEC 10.8/16.2; IEEE 1800-2023 9.3/9.6/24.3.
// Original source and AI-drafted oracle; spec_check: ai.
`timescale 1ns/1ns
'''
        yield name + '.sv', header + source
        manifest.append({'source': name + '.sv', 'limit': limit, 'stdout': stdout,
                         'exit': exit_code, 'reason': reason})
    yield 'cases.json', json.dumps(manifest, indent=2) + '\n'


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--check', action='store_true')
    args = parser.parse_args()
    if not args.check:
        OUT.mkdir(parents=True, exist_ok=True)
    bad = []
    for name, content in artifacts():
        path = OUT / name
        if args.check:
            if not path.exists() or path.read_text() != content:
                bad.append(name)
        else:
            path.write_text(content)
    if bad:
        print('Out of date: ' + ', '.join(bad))
    return bool(bad)


if __name__ == '__main__':
    raise SystemExit(main())
