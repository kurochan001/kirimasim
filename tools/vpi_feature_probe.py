#!/usr/bin/env python3
# Copyright (c) 2026 The kirimasim Authors
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

"""Run original VPI black-box cases using an explicitly selected tool triple.

Reference availability is evidence for a proposed P1 subset, not a native pass.
No simulator source or generated simulation bytecode is read. Only the installed
public VPI header is included by the C compiler, never copied into the repository.
"""
import argparse
import datetime
import hashlib
import json
from pathlib import Path
import tempfile

import bounded_run

ROOT = Path(__file__).resolve().parents[1]
WIDE = "logic [128:0] expected; expected=129'h1000000000000000000000000a9abcdef; expected[29]=1'bx;expected[28]=1'bz;"


def module(body, declarations="", unit="1ns/1ns"):
    return f'`timescale {unit}\nmodule top;\n{declarations}\ninitial begin\n{body}\nend\nendmodule\n'


CASES = [
    ('registered_task', module('$k_task(7);'), 'task 7\n', '38 (API subclause unconfirmed)'),
    ('compiletf_call_context', module('$k_checked(7);'), 'compile 32\ntask 7\n', '38 (API subclause unconfirmed)'),
    ('module_iterator', module('$k_modules();'), 'modules 1\n', '38 (API subclause unconfirmed)'),
    ('vlog_info_plusarg', module('$k_info();'), 'argv 1\n', '38 (API subclause unconfirmed)'),
    ('finish_control', module('$k_finish();$display("BAD continuation");'), 'finish\n', '38 (API subclause unconfirmed)'),
    ('error_query_clean', module('$k_error();'), 'error 0\n', '38 (API subclause unconfirmed)'),
    ('integer_function', module('$display("function %0d",$k_add(20,22));'), 'function 42\n', '38 (API subclause unconfirmed)'),
    ('sized_four_state_function', module(WIDE+'\nif($k_wide() === expected) $display("sized ok"); else $display("sized BAD");'), 'sized ok\n', '38 (API subclause unconfirmed)'),
    ('hierarchical_lookup', '`timescale 1ns/1ns\nmodule child;logic [64:0] value;endmodule\nmodule top;child u();initial $k_hierarchy(u.value);endmodule\n', 'hierarchy top.u.value 65\nscope top.u\nmissing null\n', '38 (API subclause unconfirmed)'),
    ('wide_four_state_read_write', module(WIDE+'\n$k_vector(value);if(value===expected)$display("hdl ok");else $display("hdl BAD");','logic[128:0] value;'), 'vector ok\nhdl ok\n', '38 (API subclause unconfirmed)'),
    ('fixed_memory_word', module('$k_memory();$display("hdl %0d",values[1]);','int values[2:0];'), 'memory 13\nhdl 13\n', '38 (API subclause unconfirmed)'),
    ('real_read_write', module('$k_real(value);$display("hdl %.2f",value);','real value=1.5;'), 'real 1.50\nhdl 2.25\n', '38 (API subclause unconfirmed)'),
    ('packed_string_conversion', module('$k_string(value);$display("hdl %s",value);','reg[23:0] value="abc";'), 'string abc\nhdl  ok\n', '38 (API subclause unconfirmed)'),
    ('time_64', module('#4294967297;$k_time();',unit='1ps/1ps'), 'time 1 1\n', '38 (API subclause unconfirmed)'),
    ('simulation_lifecycle', module('$k_lifecycle(lifecycle_marker);$display("body");#1;$finish(0);','int lifecycle_marker;'), 'start\nbody\nend\n', '38.36'),
    ('callback_after_delay', module('$k_after();#3;$finish(0);'), 'after 2\n', '38.36'),
    ('callback_cancel', module('$k_cancel();#3;$finish(0);'), 'cancel 1\n', '38.36'),
    ('callback_next_time', module('#1;$k_next();#4;$finish(0);'), 'next 5\n', '38.36'),
    ('callback_read_write_read_only', module('#1;$k_regions(value);value<=1;$display("active %0d",value);#1;$display("hdl %0d",value);$finish(0);','int value=0;'), 'active 0\nrw 1\nro 2\nhdl 2\n', '4, 38.36'),
    ('callback_value_change_remove', module('#1;$k_watch(value);#1;value<=1;#1;value<=2;#1;$finish(0);','int value=0;'), 'change 1\n', '38.36'),
    ('callback_value_change_reentry', module('#1;$k_watch(value);$k_put_trace(value);#1;$finish(0);','int value=0;'), 'before put\nchange 1\nafter put\n', '38.36; reentry ordering is reference evidence, not a portable oracle'),
    ('delayed_variable_write', module('$k_delayed(value);#1;$display("before %0d",value);#2;$display("after %0d",value);','int value=0;'), 'before 0\nafter 9\n', '38 (API subclause unconfirmed)'),
    ('variable_force_release', module('#1;$k_force(value);value=0;$display("forced %0d",value);$k_release(value);$display("released %0d",value);value=0;$display("assigned %0d",value);','reg value=0;'), 'forced 1\nreleased 1\nassigned 0\n', '10.6; 38 (API subclause unconfirmed)'),
]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--iverilog', type=Path, required=True)
    parser.add_argument('--vvp', type=Path, required=True)
    parser.add_argument('--iverilog-vpi', type=Path, required=True)
    parser.add_argument('--kirimasim', type=Path)
    parser.add_argument('--plugin-source', type=Path, default=ROOT/'tests/probes/vpi/inventory.c',
                        help='original fixture or an explicit fault-injected copy')
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    programs = {name: str(getattr(args, name.replace('-', '_')).resolve())
                for name in ('iverilog', 'vvp', 'iverilog-vpi')}
    plugin = args.plugin_source.resolve()
    report = {'date': datetime.date.today().isoformat(), 'scope': 'Original finite VPI reference availability; not full VPI or native parity',
              'tool_sha256': {name: hashlib.sha256(Path(exe).read_bytes()).hexdigest()
                              for name, exe in programs.items()},
              'plugin_source': str(plugin.relative_to(ROOT)) if plugin.is_relative_to(ROOT) else plugin.name,
              'plugin_sha256': hashlib.sha256(plugin.read_bytes()).hexdigest(),
              'runner_sha256': hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),
              'versions': {}, 'cases': []}
    for name in ('iverilog', 'vvp'):
        p = bounded_run.run([programs[name], '-V'], timeout=15)
        lines = (p.stdout+p.stderr).splitlines()
        report['versions'][name] = next((line for line in lines if 'version' in line), 'unconfirmed')
    with tempfile.TemporaryDirectory(prefix='kirimasim-vpi-inventory-') as tmp:
        root = Path(tmp)
        local_plugin = root/'inventory.c';local_plugin.write_bytes(plugin.read_bytes())

        def clean(text):
            return text.replace(str(root), '<probe>')

        built = bounded_run.run([programs['iverilog-vpi'], 'inventory.c'], cwd=root, timeout=60)
        report['plugin_build'] = {'exit': built.returncode, 'stdout': clean(built.stdout), 'stderr': clean(built.stderr)}
        if built.returncode or not (root/'inventory.vpi').is_file():
            raise RuntimeError(f'VPI probe build failed: {clean(built.stdout+built.stderr)}')
        for name, source, expected, clauses in CASES:
            sv = root/(name+'.sv');sv.write_text(source)
            compiled = bounded_run.run([programs['iverilog'], '-g2012', '-L', str(root), '-minventory', '-o', 'design.vvp', sv.name], cwd=root, timeout=30)
            row = {'name': name, 'category': 'standard', 'clauses': clauses, 'source': source,
                   'source_sha256': hashlib.sha256(source.encode()).hexdigest(), 'expected_stdout': expected,
                   'compile_exit': compiled.returncode, 'compile_stderr': clean(compiled.stderr)}
            if compiled.returncode:
                row['status'] = 'reference_compile_error'
            else:
                ran = bounded_run.run([programs['vvp'], '-n', 'design.vvp', '+vpi_probe=42'], cwd=root, timeout=30)
                row.update({'run_exit': ran.returncode, 'stdout': clean(ran.stdout), 'stderr': clean(ran.stderr)})
                row['status'] = 'passed' if ran.returncode==0 and ran.stdout==expected and not ran.stderr else 'reference_mismatch'
            report['cases'].append(row)
            print(f'{name}: {row["status"]}', flush=True)
        if args.kirimasim:
            exe = args.kirimasim.resolve();sv=root/'registered_task.sv';sv.write_text(CASES[0][1])
            native = bounded_run.run([str(exe), '--vpi='+str(root/'inventory.vpi'), str(sv)], cwd=root, timeout=30)
            report['native_loader'] = {'version': bounded_run.run([str(exe),'--version'],timeout=15).stdout.strip(),
                                       'binary_sha256':hashlib.sha256(exe.read_bytes()).hexdigest(),
                                       'exit':native.returncode,'stdout':clean(native.stdout),'stderr':clean(native.stderr),
                                       'per_api_behavior':'not exercised by this reference-only tool'}
    report['summary'] = {'total':len(CASES),'reference_passed':sum(row['status']=='passed' for row in report['cases']),
                         'native_evaluated':0,'native_passed':None,'native_per_api_results':'not measured without a VPI host'}
    args.output.parent.mkdir(parents=True,exist_ok=True)
    args.output.write_text(json.dumps(report,indent=2)+'\n')
    print(report['summary'])
    return 0 if report['summary']['reference_passed']==len(CASES) else 1


if __name__=='__main__':raise SystemExit(main())
