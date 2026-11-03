#!/usr/bin/env python3
# Copyright (c) 2026 The kirimasim Authors
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

"""Original finite VPI net-write inventory; no native implementation or P1 gate.

Uses explicit installed tools and independently written C/SV. Only tool output
is read, never another simulator's implementation or generated simulation code.
"""
import argparse
import collections
import datetime
import hashlib
import json
from pathlib import Path
import re
import subprocess
import sys
import tempfile

import bounded_run

ROOT = Path(__file__).resolve().parents[1]


def op(flag, value):
    returned = ' returned=' + value if flag == 6 else ''
    return f'op {flag} error=0 event=0{returned} now={value}\n'


def cases():
    def record(name, source, expected, boundary='first-proposed-subset'):
        return dict(name=name, source='`timescale 1ns/1ns\n' + source,
                    expected_stdout=expected, category='standard',
                    boundary=boundary, oracle='drafted independently; not inferred from reference',
                    clauses='1800-2017 38.34/10.6.2 inspected; direct 2023 edition audit unconfirmed')

    def body(width, mode, literal=None):
        zero = '0' * width
        one = literal or '1' * width
        flag = 1 if mode == 'deposit' else 5
        text = f'#1;$display("before %b",n);$net_op(n,{width}\'b{one},{flag});\n'
        expected = f'before {zero}\n' + op(flag, one)
        if mode == 'deposit':
            text += '#1;$display("held %b",n);a=1;#1;a=0;#1;$display("after %b",n);\n'
            expected += f'held {one}\nafter {zero}\n'
        else:
            text += '#1;a=1;#1;a=0;#1;$display("held %b",n);$net_op(n,0,6);\n#1;$display("after %b",n);\n'
            expected += f'held {one}\n' + op(6, zero) + f'after {zero}\n'
        return text, expected

    for context in ('module', 'child', 'generate', 'interface', 'program'):
        for width in (1, 33, 129):
            for mode in ('deposit', 'force'):
                text, expected = body(width, mode)
                declaration = f'logic [{width-1}:0] a=0;wire [{width-1}:0] n;assign n=a;\n'
                if context in ('child', 'interface', 'program'):
                    kind = 'module' if context == 'child' else context
                    source = f'{kind} worker;\n{declaration}initial begin\n{text}end end{kind}\nmodule top;worker u();endmodule\n'
                else:
                    opening = 'if(1)begin:g\n' if context == 'generate' else ''
                    closing = 'end\n' if context == 'generate' else ''
                    source = f'module top;\n{opening}{declaration}initial begin\n{text}end\n{closing}endmodule\n'
                yield record(f'{context}_{width}_{mode}', source, expected)

    for width in (1, 33):
        for logic in ('x', 'z'):
            for mode in ('deposit', 'force'):
                text, expected = body(width, mode, logic * width)
                source = f'module top;logic [{width-1}:0] a=0;wire [{width-1}:0] n;assign n=a;initial begin\n{text}end endmodule\n'
                yield record(f'four_state_{width}_{logic}_{mode}', source, expected)

    for net in ('wire', 'tri', 'wand', 'wor', 'tri0', 'tri1'):
        text, expected = body(1, 'force')
        yield record('kind_' + net, f'module top;logic a=0;{net} n;assign n=a;initial begin\n{text}end endmodule\n', expected)
    for initial in (0, 1):
        forced = str(1-initial)
        text = f'#1;$net_op(n,1\'b{forced},5);#1;$display("held %b",n);$net_op(n,0,6);'
        yield record('kind_supply' + str(initial), f'module top;supply{initial} n;initial begin {text} end endmodule\n',
                     op(5, forced) + f'held {forced}\n' + op(6, str(initial)))

    yield record('deposit_same_resolved_driver_change',
                 'module top;logic a=0,b=1,c=0;wire n;assign n=a;assign n=b;assign n=c;\n'
                 'initial begin #1;$display("before %b",n);$net_op(n,1\'b1,1);#1;c=1;#1;$display("after %b",n);end endmodule\n',
                 'before x\n' + op(1, '1') + 'after x\n')
    yield record('deposit_unchanged_contribution',
                 'module top;logic a=0;wire n;assign n=a&1\'b0;\n'
                 'initial begin #1;$net_op(n,1\'b1,1);#1;a=1;#1;$display("after %b",n);end endmodule\n',
                 op(1, '1') + 'after 1\n')
    yield record('deposit_under_force',
                 'module top;logic a=0;wire n;assign n=a;initial begin #1;$net_op(n,1\'b1,5);'
                 '$net_op(n,1\'b0,1);#1;$display("held %b",n);$net_op(n,0,6);end endmodule\n',
                 op(5, '1') + op(1, '1') + 'held 1\n' + op(6, '0'))
    for mode in ('deposit', 'force'):
        flag = 1 if mode == 'deposit' else 5
        ending = '$net_op(n,0,6);' if mode == 'force' else ''
        yield record('undriven_' + mode,
                     f'module top;wire n;initial begin #1;$display("before %b",n);$net_op(n,1\'b1,{flag});#1;$display("held %b",n);{ending}end endmodule\n',
                     'before z\n' + op(flag, '1') + 'held 1\n' + (op(6, 'z') if ending else ''))
    yield record('vpi_force_hdl_release',
                 'module top;logic a=0;wire n;assign n=a;initial begin #1;$net_op(n,1\'b1,5);#1;release n;#0;$display("after %b",n);end endmodule\n',
                 op(5, '1') + 'after 0\n')
    yield record('hdl_force_vpi_release',
                 'module top;logic a=0;wire n;assign n=a;initial begin #1;force n=1;#1;$display("held %b",n);$net_op(n,0,6);end endmodule\n',
                 'held 1\n' + op(6, '0'))
    yield record('vpi_replaces_hdl_force_owner',
                 'module top;logic a=0;wire n;assign n=a;initial begin #1;force n=a;$net_op(n,1\'b1,5);#1;a=1;#1;a=0;#1;$display("held %b",n);release n;#0;$display("after %b",n);end endmodule\n',
                 op(5, '1') + 'held 1\nafter 0\n')
    yield record('port_alias_release',
                 'module leaf(input wire a,output wire n);assign n=a;endmodule\n'
                 'module top;logic a=0;wire n;leaf u(a,n);initial begin #1;$net_op(u.n,1\'b1,5);$display("aliases %b %b",n,u.n);#1;$net_op(n,0,6);$display("aliases %b %b",n,u.n);end endmodule\n',
                 op(5, '1') + 'aliases 1 1\n' + op(6, '0') + 'aliases 0 0\n')
    yield record('callback_publication_order',
                 'module top;logic a=0;wire n;assign n=a;initial begin #1;$net_watch(n);$display("before put");$net_op(n,1\'b1,5);$display("after put");$net_op(n,1\'b1,5);$net_op(n,0,6);end endmodule\n',
                 'before put\nchange 1\n' + op(5, '1') + 'after put\n' + op(5, '1') + 'change 0\n' + op(6, '0'))
    yield record('distributed_pending_release',
                 'module top;logic a=0;wire n;assign #4 n=a;initial begin #10;$net_op(n,1\'b0,5);a=1;#2;$net_op(n,0,6);$display("early %b",n);#3;$display("late %b",n);end endmodule\n',
                 op(5, '0') + op(6, '0') + 'early 0\nlate 1\n')
    yield record('module_path_pending_force',
                 'module leaf(input wire a,output wire n);assign n=a;specify(a=>n)=5;endspecify endmodule\n'
                 'module top;logic a=0;wire n;leaf u(a,n);initial begin #20;$net_op(n,1\'b0,5);a=1;#7;$display("held %b",n);$net_op(n,0,6);end endmodule\n',
                 op(5, '0') + 'held 0\n' + op(6, '1'))
    for primitive in ('tran', 'rtran'):
        source = f'module top;logic a=0;wire n,z;assign n=a;{primitive} link0(n,z);initial begin #1;$net_op(z,1\'b1,5);#1;$display("held %b %b",n,z);$net_op(z,0,6);#1;$display("after %b %b",n,z);end endmodule\n'
        # Keep physical-network observations separate until the contract review.
        yield record('passive_' + primitive + '_force', source, None, 'physical-force-observation')
    yield record('passive_deposit_boundary',
                 'module top;logic a=0;wire n,z;assign n=a;tran link0(n,z);initial begin #1;$net_op(z,1\'b1,1);#1;$display("deposited %b %b",n,z);a=1;#1;a=0;#1;$display("after %b %b",n,z);end endmodule\n',
                 None, 'later-passive-deposit-observation')


def main():
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument('--iverilog', type=Path, required=True)
    parser.add_argument('--vvp', type=Path, required=True)
    parser.add_argument('--iverilog-vpi', type=Path, required=True)
    parser.add_argument('--kirimasim', type=Path, required=True)
    parser.add_argument('--plugin-source', type=Path, default=ROOT/'tests/probes/vpi/net_writes.c')
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--case', action='append')
    parser.add_argument('--require-reference-oracles', action='store_true')
    args = parser.parse_args()
    tools = {name: getattr(args, name.replace('-', '_')).resolve()
             for name in ('iverilog', 'vvp', 'iverilog-vpi', 'kirimasim')}
    selected = [c for c in cases() if not args.case or c['name'] in args.case]
    assert selected and len({c['name'] for c in selected}) == len(selected)
    assert not args.case or set(args.case) == {c['name'] for c in selected}
    plugin = args.plugin_source.resolve()
    sha = lambda p: hashlib.sha256(Path(p).read_bytes()).hexdigest()
    report = dict(date=datetime.date.today().isoformat(),
                  scope='Original finite VPI net-write inventory; proposed contract, not native support or a P1 gate',
                  baseline_commit=subprocess.check_output(['git', 'rev-parse', 'HEAD'], cwd=ROOT, text=True).strip(),
                  runner_sha256=sha(__file__), plugin_sha256=sha(plugin),
                  tool_sha256={k: sha(v) for k, v in tools.items()}, versions={}, cases=[])
    for name in ('iverilog', 'vvp', 'kirimasim'):
        version = bounded_run.run([str(tools[name]), '--version' if name == 'kirimasim' else '-V'], timeout=15)
        report['versions'][name] = next((x for x in (version.stdout+version.stderr).splitlines() if 'version' in x or 'kirimasim ' in x), 'unconfirmed')
    with tempfile.TemporaryDirectory(prefix='vpi-net-inventory-') as temp:
        directory = Path(temp)
        def clean(text):
            text = text.replace(str(directory), '<probe>').replace(str(ROOT), '<repo>')
            return re.sub(r'/Users/[^\s:]+', '<external-path>', text)
        local = directory/'net_writes.c';local.write_bytes(plugin.read_bytes())
        built = bounded_run.run([str(tools['iverilog-vpi']), local.name], cwd=directory, timeout=60)
        report['plugin_build'] = dict(exit=built.returncode, stdout=clean(built.stdout), stderr=clean(built.stderr))
        assert built.returncode == 0 and (directory/'net_writes.vpi').is_file(), report['plugin_build']
        for case in selected:
            source = directory/'case.sv';source.write_text(case['source'])
            row = dict(case, source_sha256=hashlib.sha256(case['source'].encode()).hexdigest())
            compiled = bounded_run.run([str(tools['iverilog']), '-g2012', '-gspecify', '-s', 'top', '-o', 'sim', source.name], cwd=directory, timeout=15)
            row['reference'] = dict(compile_exit=compiled.returncode, diagnostics=clean(compiled.stderr))
            if compiled.returncode == 0:
                ran = bounded_run.run([str(tools['vvp']), '-n', '-M'+str(directory), '-mnet_writes', 'sim'], cwd=directory, timeout=15)
                row['reference'].update(run_exit=ran.returncode, stdout=clean(ran.stdout), stderr=clean(ran.stderr),
                                        status='observed_without_oracle' if case['expected_stdout'] is None and ran.returncode == 0 else
                                        'passed' if ran.returncode == 0 and ran.stdout == case['expected_stdout'] and not ran.stderr else 'reference_difference')
            else:
                row['reference']['status'] = 'compile_error'
            ran = bounded_run.run([str(tools['kirimasim']), '--top=top', '--vpi='+str(directory/'net_writes.vpi'), source.name], cwd=directory, timeout=15)
            diagnostics = '\n'.join(s for s in ran.stderr.splitlines() if not s.startswith('kirimasim: info:'))
            status = 'passed' if case['expected_stdout'] is not None and ran.returncode == 0 and ran.stdout == case['expected_stdout'] and not diagnostics else (
                'unsupported_vpi' if ran.returncode == 0 and ' error=3 ' in ran.stdout else 'native_failure_or_unjudged')
            row['native'] = dict(exit=ran.returncode, stdout=clean(ran.stdout), diagnostics=clean(diagnostics), status=status,
                                 passed=status == 'passed', process_exit_alone_is_not_a_pass=True)
            report['cases'].append(row)
            print(f"{case['name']}: reference={row['reference']['status']} native={status}", flush=True)
    report['summary'] = dict(total=len(selected),
                             reference_statuses=dict(collections.Counter(c['reference']['status'] for c in report['cases'])),
                             native_statuses=dict(collections.Counter(c['native']['status'] for c in report['cases'])),
                             first_proposed_subset=sum(c['boundary'] == 'first-proposed-subset' for c in selected),
                             p1_denominators_changed=False)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(report, indent=2)+'\n')
    print(report['summary'])
    independent = any(c['native']['exit'] < 0 or 'internal error' in c['native']['diagnostics'] or
                      c['reference'].get('run_exit', 0) < 0 for c in report['cases'])
    differences = args.require_reference_oracles and any(c['expected_stdout'] is not None and c['reference']['status'] != 'passed' for c in report['cases'])
    return 1 if independent or differences else 0


if __name__ == '__main__':
    sys.exit(main())
