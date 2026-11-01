#!/usr/bin/env python3
# Copyright (c) 2026 The kirimasim Authors
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
"""Original assignment-pattern source matrix (SYSTEM_TASKS 3.9, IEEE 21.2.1.6).

The CLI regression imports this generator. --output exports the same original
sources and expected pattern text for whitespace-independent judging for independent review.
"""
import argparse
import json
from pathlib import Path

CONTEXTS = ('initial', 'always', 'final', 'static_task', 'automatic_task',
            'function', 'ref', 'fork', 'generate', 'interface', 'program', 'package')
APIS = ('write', 'display', 'writeb', 'displayh', 'fwrite', 'fdisplay', 'swrite',
        'swriteh', 'sformat', 'sformatf', 'runtime_string', 'runtime_packed',
        'packed_target', 'strobe', 'monitor', 'fstrobe', 'fmonitor')
POSTPONED = ('strobe', 'monitor', 'fstrobe', 'fmonitor')


def source(context, api):
    cases = [('v', '3', '3'), ('unknown', 'X', 'X'), ('real_value', '1.5', '1.5'),
             ('short_value', '1.25', '1.25'), ('"a b"', '"a b"', '"a b"'),
             ('text_value', '"a b"', '"a b"'), ('tag', 'FIRST', 'FIRST'),
             ('record_value', "'{tag:FIRST, count:-3, state:z}", "'{FIRST, -3, z}"),
             ('fixed_values', "'{'{1, 2}, '{3, 4}}", "'{'{1, 2}, '{3, 4}}"),
             ('dynamic_values', "'{7, 8}", "'{7, 8}"),
             ('queue_values', "'{9, 10}", "'{9, 10}")]
    format_text = '|'.join('%' + mode for _ in cases for mode in ('p', '0p'))
    expected = '|'.join(text for _, named, positional in cases for text in (named, positional))
    arguments = ', '.join(expr for expr, _, _ in cases for _ in range(2))
    declarations = [
        'logic[64:0] v=3; logic[8:0] unknown=9\'b000x00001;',
        'real real_value=1.5; shortreal short_value=1.25; string text_value="a b";',
        "typedef enum logic[64:0] { FIRST=65'h10000000000000001, LAST=2 } tag_t;",
        'tag_t tag=FIRST;',
        'struct packed { tag_t tag; byte count; logic[2:0] state; } record_value;',
        "int fixed_values[2:1][-1:0]='{'{1,2},'{3,4}};",
        'int dynamic_values[]; int queue_values[$];',
        'string text,format_value;', f'bit[{len(format_text)*8-1}:0] packed_format;',
        'bit[16383:0] packed_result;int fd;']
    body = ['record_value.tag=FIRST;record_value.count=-3;record_value.state=3\'bzzz;',
            "dynamic_values=new[2];dynamic_values[0]=7;dynamic_values[1]=8;queue_values='{9,10};"]
    literal = json.dumps(format_text)
    if api in ('fwrite', 'fdisplay'):
        body += ['fd=$fopen("result.txt","w");', f'${api}(fd,{literal},{arguments});', '$fclose(fd);']
    elif api in ('fstrobe', 'fmonitor'):
        body += [f'${api}(32\'h80000001,{literal},{arguments});']
    elif api in ('write', 'display', 'writeb', 'displayh', 'strobe', 'monitor'):
        body += [f'${api}({literal},{arguments});']
    elif api == 'packed_target':
        body += [f'$sformat(packed_result,{literal},{arguments});', '$write("%s",packed_result);']
    else:
        if api in ('swrite', 'swriteh', 'sformat'):
            body += [f'${api}(text,{literal},{arguments});']
        elif api == 'sformatf':
            body += [f'text=$sformatf({literal},{arguments});']
        else:
            body += [f'format_value={literal};packed_format={literal};',
                     f'text=$sformatf({"format_value" if api == "runtime_string" else "packed_format"},{arguments});']
        body += ['$write("%s",text);']
    if api in ('display', 'displayh', 'fdisplay', *POSTPONED):
        expected += '\n'
    if context == 'program' and api in POSTPONED:
        # Keep the program alive until the queued Postponed output runs;
        # otherwise its implicit exit ends the simulation in this slot.
        body += ['#1;']
    opening = {'interface':'interface worker;', 'program':'program worker;',
               'package':'package worker;'}
    lines = [opening.get(context, 'module top;'), *declarations]
    if context in ('static_task', 'automatic_task', 'function', 'ref', 'package'):
        signature = {'function':'function automatic void run();',
                     'automatic_task':'task automatic run();',
                     'ref':'task automatic run(ref logic[64:0] v);'}.get(context, 'task run();')
        lines += [signature, *body, 'endfunction' if context == 'function' else 'endtask']
        if context != 'package':lines += ['initial run(v);' if context == 'ref' else 'initial run();']
    else:
        if context == 'generate':lines += ['if(1)begin:g']
        if context == 'always':lines += ['event go;initial #1 ->go;', 'always @(go)begin']
        else:lines += ['final begin' if context == 'final' else 'initial begin']
        lines += ['fork begin', *body, 'end join'] if context == 'fork' else body
        lines += ['end']
        if context == 'generate':lines += ['end']
    if context == 'interface':lines += ['endinterface', 'module top;worker u();endmodule']
    elif context == 'program':lines += ['endprogram', 'module top;worker u();endmodule']
    elif context == 'package':lines += ['endpackage', 'module top;initial worker::run();endmodule']
    else:lines += ['endmodule']
    return '\n'.join(lines)+'\n', expected, len(cases)*2


def programs(contexts=CONTEXTS, apis=APIS):
    for context in contexts:
        for api in apis:
            if api in POSTPONED and context in ('ref', 'final'):
                continue
            text, expected, count = source(context, api)
            yield context, api, text, expected, count


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=True)
    records=[]
    for context,api,text,expected,count in programs():
        name=f'{context}_{api}.sv'
        (args.output/name).write_text(text)
        records.append({'source':name,'expected_pattern_text':expected,'conversions':count})
    (args.output/'manifest.json').write_text(json.dumps(records,indent=2)+'\n')
    print(f'pattern matrix: exported {len(records)} original sources')
    return 0


if __name__=='__main__':raise SystemExit(main())
