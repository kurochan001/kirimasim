#!/usr/bin/env python3
# Copyright (c) 2026 The kirimasim Authors
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

"""Generate original event-NBA capture/control matrices from the approved contract.

The per-notification OR rule is the project's approved choice, not an Icarus
oracle. No external simulator source, tests or reproducer was used.
"""
import argparse
import re
import sys
from pathlib import Path

OUT = Path(__file__).resolve().parent.parent / 'tests/suite/sched'
VERSION = 1
HOMES = ('initial', 'static_task', 'automatic_task', 'function', 'fork',
         'interface', 'package', 'program', 'generate', 'class')
CONTROLS = ('named', 'slot', 'expression', 'function', 'guard', 'named_guard',
            'duplicate', 'overlap')
SHAPES = ('scalar', 'array', 'record', 'concat', 'nested_bits', 'stream_lhs',
          'stream_rhs', 'queue', 'dynamic_array', 'string', 'real')


def qualify(text, prefix, names):
    return re.sub(r'\b(' + '|'.join(sorted(names, key=len, reverse=True)) + r')\b',
                  lambda m: prefix + m[0], text)


def wrap(home, types, variables, methods, issue, report, names):
    declarations = types + '\n' + '\n'.join(variables) + '\n'
    if home == 'class':
        declarations = types + '\n' + '\n'.join('static ' + v for v in variables) + '\n'
        methods = re.sub(r'^function ', 'static function ', methods, flags=re.M)
        return ('class holder;\n' + declarations + methods +
                'static function automatic void issue();\n' + issue +
                'endfunction\nendclass\nmodule top;\ninitial begin\nholder::issue();\n' +
                qualify(report, 'holder::', names) + 'end\nendmodule\n')
    if home in ('interface', 'package'):
        declaration = f'{home} worker;\n' + declarations + methods
        declaration += 'task automatic issue();\n' + issue + f'endtask\nend{home}\n'
        prefix = 'w.' if home == 'interface' else 'worker::'
        instances = 'worker w();\n' if home == 'interface' else ''
        return declaration + 'module top;\n' + instances + 'initial begin\n' + prefix + 'issue();\n' + qualify(report, prefix, names) + 'end\nendmodule\n'
    kind = 'program' if home == 'program' else 'module'
    text = f'{kind} worker;\n' if home == 'program' else 'module top;\n'
    if home == 'generate':
        text += 'if(1) begin:g\n'
    text += declarations + methods
    if home in ('static_task', 'automatic_task', 'function'):
        begin = ('function automatic void issue();\n' if home == 'function' else
                 'task automatic issue();\n' if home == 'automatic_task' else 'task issue();\n')
        text += begin + issue + ('endfunction\n' if home == 'function' else 'endtask\n')
        text += 'initial begin issue();\n' + report + 'end\n'
    elif home == 'fork':
        text += 'initial begin fork begin\n' + issue + 'end join\n' + report + 'end\n'
    else:
        text += 'initial begin\n' + issue + report + 'end\n'
    if home == 'generate':
        text += 'end\n'
    text += f'end{kind}\n'
    if home == 'program':
        text += 'module top; worker p(); endmodule\n'
    return text


def shape_data(shape):
    types = ''
    variables = []
    setup, issue, mutate = [], [], []
    pending, result, names = [], [], set()
    if shape == 'scalar':
        for width in (1, 32, 65, 129):
            for domain in ('bit', 'logic'):
                suffix = f'{domain}_{width}'
                q, src = 'q_' + suffix, 'src_' + suffix
                value = ((1 << (width - 1)) | 3) & ((1 << width) - 1)
                literal = f"{width}'h{value:x}"
                variables.append(f'{domain} [{width-1}:0] {q}, {src};')
                setup.append(f"{q}=0; {src}={literal};")
                issue.append(f'{q} <= CONTROL {src};')
                mutate.append(f'{src}=0;')
                pending.append(f'({q}===0)')
                result.append(f'({q}==={literal})')
                names.update((q, src))
        literal = "129'h1" + '0' * 30 + 'xz'
        variables.append('logic [128:0] q_unknown, src_unknown;')
        setup.append(f'q_unknown=0; src_unknown={literal};')
        issue.append('q_unknown <= CONTROL src_unknown;')
        mutate.append('src_unknown=0;')
        pending.append('(q_unknown===0)')
        result.append(f'(q_unknown==={literal})')
        names.update(('q_unknown', 'src_unknown'))
    elif shape == 'array':
        types = 'typedef logic [128:0] word_t;'
        variables = ['word_t q[3:2][-1:0], src[2][2];']
        setup = ["q='{default:'{default:0}}; src='{'{7,11},'{13,17}};"]
        issue = ['q <= CONTROL src;']
        mutate = ["src='{default:'{default:0}};"]
        for i, j, value in ((3, -1, 7), (3, 0, 11), (2, -1, 13), (2, 0, 17)):
            pending.append(f'(q[{i}][{j}]===0)')
            result.append(f'(q[{i}][{j}]==={value})')
    elif shape == 'record':
        types = 'typedef struct {logic [128:0] data; int tag; real number; string text; int items[$];} record_t;'
        variables = ['record_t q, src;']
        setup = ["q='{0,0,0.0,\"\",{}}; src='{7,11,2.5,\"saved\",'{13,17}};"]
        issue = ['q <= CONTROL src;']
        mutate = ['src.data=0; src.tag=0; src.number=0.0; src.text="changed"; src.items[0]=99;']
        pending = ['(q.data===0)', '(q.items.size()==0)']
        result = ['(q.data===129\'d7)', '(q.tag===11)', '(q.number==2.5)', '(q.text=="saved")',
                  '(q.items.size()==2)', '(q.items[0]===13)', '(q.items[1]===17)']
    elif shape == 'concat':
        variables = ['logic [64:0] hi;', 'logic [63:0] lo;', 'logic [128:0] src;']
        setup = ["hi=0; lo=0; src=129'h100000000000000000000000000000007;"]
        issue = ['{hi,lo} <= CONTROL src;']
        mutate = ['src=0;']
        pending = ['(hi===0)', '(lo===0)']
        result = ["({hi,lo}===129'h100000000000000000000000000000007)"]
        names.update(('hi', 'lo'))
    elif shape == 'nested_bits':
        types = 'typedef struct packed {logic [7:0] data; logic [3:0] keep;} packed_t;'
        variables = ['packed_t q;', 'logic [3:0] src;', 'int pos;']
        setup = ["q=12'h00a; src=4'hb; pos=-2;"]
        issue = ['q.data[pos+:4] <= CONTROL src;']
        mutate = ['src=0; pos=0;']
        pending = ["(q===12'h00a)"]
        result = ["(q===12'h02a)"]
        names.add('pos')
    elif shape in ('stream_lhs', 'stream_rhs'):
        variables = ['bit [7:0] q[3];', 'bit [23:0] src;']
        setup = ["q='{0,0,0}; src=24'h123456;"]
        issue = ['{<<8{q}} <= CONTROL src;' if shape == 'stream_lhs' else 'q <= CONTROL {>>{src}};']
        mutate = ['src=0;']
        values = (0x56, 0x34, 0x12) if shape == 'stream_lhs' else (0x12, 0x34, 0x56)
        for i, v in enumerate(values):
            pending.append(f'(q[{i}]===0)')
            result.append(f"(q[{i}]===8'h{v:02x})")
    elif shape in ('queue', 'dynamic_array'):
        types = 'typedef logic [128:0] word_t;'
        variables = ['word_t q[$:1], src[$];'] if shape == 'queue' else ['word_t q[], src[];']
        setup = ["q={}; src='{7,11};"] if shape == 'queue' else ['q=new[0]; src=new[2]; src[0]=7; src[1]=11;']
        issue = ['q <= CONTROL src;']
        mutate = ['src[0]=99; src.push_back(13);'] if shape == 'queue' else ['src[0]=99; src=new[0];']
        pending = ['(q.size()==0)']
        result = ['(q.size()==2)', "(q[0]===129'd7)", "(q[1]===129'd11)"]
    elif shape == 'string':
        variables = ['string q, src;']
        setup = ['q=""; src="saved";']
        issue = ['q <= CONTROL src;']
        mutate = ['src="changed";']
        pending = ['(q=="")']
        result = ['(q=="saved")']
    elif shape == 'real':
        variables = ['real q, src;']
        setup = ['q=0.0; src=2.5;']
        issue = ['q <= CONTROL src;']
        mutate = ['src=0.0;']
        pending = ['(q==0.0)']
        result = ['(q==2.5)']
    else:
        raise ValueError(shape)
    names.update(('q', 'src'))
    return types, variables, setup, issue, mutate, pending, result, names


def case(home, shape, control):
    name = f'event_nba_front_{home}_{shape}_{control}'
    types, variables, setup, assignments, mutate, pending, result, names = shape_data(shape)
    variables += ["bit [128:0] clock=0;", 'bit enable=1;', 'event tick;']
    names.update(('clock', 'enable', 'tick'))
    methods = '''function automatic bit gate();
  int private_work=0;
  private_work++;
  return (private_work==1) && enable;
endfunction
function automatic bit [128:0] sample();
  bit [128:0] private_work=0;
  private_work ^= clock;
  return private_work;
endfunction
'''
    subjects = {'named': 'tick', 'slot': 'clock', 'expression': "clock ^ 129'h55",
                'function': 'sample()', 'guard': "clock ^ 129'h55 iff gate()",
                'named_guard': 'tick iff gate()', 'duplicate': 'tick or tick',
                'overlap': "clock or (clock ^ 129'h55)"}
    statement = f"repeat(129'sd2) @({subjects[control]})"
    issue = '\n'.join(setup + [line.replace('CONTROL', statement) for line in assignments] + mutate) + '\n'
    notification = '->tick;' if control in ('named', 'named_guard', 'duplicate') else "clock = clock ^ (129'd1<<128);"
    report = '#1;\n'
    if control in ('guard', 'named_guard'):
        report += 'enable=0; ' + notification + '\nenable=1;\n#1;\n'
    report += notification + '\n#1;\n'
    report += '$display("pending %0d",' + ' && '.join(pending) + ');\n'
    report += notification + '\n#1;\n'
    report += '$display("value %0d",' + ' && '.join(result) + ');\n'
    source = wrap(home, types, variables, methods, issue, report, names)
    header = f'''/*
:name: {name}
:description: Event NBA retains early values, one independent count, fresh callback work and approved per-notification OR coalescing
:tags: sched nba event arrays
:type: simulation
:lrm: 1800-2023 9.4.2 9.4.5 10.4.2 13.4.4 15.5
:oracle: spec
:spec_check: ai
:fourstate: yes
:nondeterministic: no
:generator: gen_event_nba_matrix.py@{VERSION}
:top_module: top
:expected_stdout: @{name}.expected
:expected_stderr: @{name}.stderr
*/
// Generated by tools/gen_event_nba_matrix.py; edit the generator.
// OR coalescing follows the approved event-NBA project contract.
`timescale 1ns/1ns
'''
    return {name+'.sv': header+source, name+'.expected': 'pending 1\nvalue 1\n', name+'.stderr': ''}


def output(name, source, expected):
    sample = case('initial', 'scalar', 'named')
    header = sample['event_nba_front_initial_scalar_named.sv'].split('module top;')[0]
    header = header.replace('event_nba_front_initial_scalar_named', name)
    return {name+'.sv': header+source, name+'.expected': expected, name+'.stderr': ''}


def counts(home, label, value, positive):
    name = f'event_nba_front_count_{home}_{label}'
    variables = ['logic signed [128:0] remaining;', 'int trace, idx, event_reads, payload;',
                 'int q[2];', 'event ticks[2];']
    methods = '''function automatic int rhs(); trace=trace*10+1; return payload; endfunction
function automatic logic signed [128:0] count_value();
  trace=trace*10+2; payload=0; return remaining;
endfunction
function automatic int pick(); trace=trace*10+3; remaining=9; return idx++; endfunction
function automatic int event_pick(); trace=trace*10+4; event_reads++; return 0; endfunction
'''
    issue = f'''trace=0; idx=1; event_reads=0; payload=42; remaining={value}; q='{{0,0}};
q[pick()] <= repeat(count_value()) @(ticks[event_pick()]) rhs();
idx=0; payload=99; remaining=4;
'''
    report = '$display("capture %0d %0d",trace,event_reads);\n#1;\n'
    if positive:
        report += '->ticks[0]; ->ticks[0];\n#1;\n'
    report += '$display("value %0d %0d",q[0],q[1]);\n'
    names = ('remaining','trace','idx','event_reads','payload','q','ticks')
    source = wrap(home, '', variables, methods, issue, report, names)
    expected = ('capture 1234 1\n' if positive else 'capture 123 0\n')+'value 0 42\n'
    return output(name, source, expected)


def mechanisms():
    result = {}
    variants = [('zero', "129'sd0"), ('negative', "-129'sd7"),
                ('minimum', "129'sh1"+'0'*32), ('x', "'x"), ('z', "'z"),
                ('mixed_x', "129'b1x"), ('mixed_z', "129'b1z"), ('positive', "129'sd2")]
    for home in HOMES:
        for label, value in variants:
            result.update(counts(home, label, value, label=='positive'))
    for location in ('return', 'ended'):
        for spelling in ('named', 'guarded'):
            guard = ' iff gate()' if spelling=='guarded' else ''
            source = f'''module top;
event events[2];
int q=0, choice=0;
function automatic bit gate(); int work=0; work++; return work==1; endfunction
task automatic issue();
  choice=0;
  q <= repeat(2) @(events[choice]{guard}) 42;
  choice=1;
endtask
'''
            if location=='ended':
                source += 'initial issue();\ninitial begin\n#1;\n'
            else:
                source += 'initial begin\nissue();\n#1;\n'
            source += '''->events[choice];
#1; $display("wrong %0d",q);
->events[0];
#1; $display("pending %0d",q);
->events[0];
#1; $display("value %0d",q);
end
initial begin #0; repeat(100) churn(); end
task automatic churn(); event temporary; ->temporary; endtask
endmodule
'''
            result.update(output(f'event_nba_front_lifetime_{location}_{spelling}', source,
                                 'wrong 0\npending 0\nvalue 42\n'))
    for order in ('earlier', 'later', 'old_ready', 'drained', 'nba_induced'):
        statements = {
            'earlier': 'q <= @(tick) 1; ->tick; q<=2;',
            'later': 'q<=2; q<=@(tick) 1; ->tick;',
            'old_ready': 'q<=@(tick) 1; #1; q<=2; ->tick;',
            'drained': 'q<=2; q<=@(tick) 1; #1; ->tick;',
            'nba_induced': 'q<=@(posedge clock) 1; clock<=1; q<=2;',
        }
        source = '''module top; event tick; bit clock=0; int q=0;
initial begin
'''+statements[order]+'''\n#1; $display("value %0d",q);
end endmodule
'''
        expected = 1 if order in ('later','drained') else 2
        result.update(output('event_nba_front_issue_order_'+order, source, f'value {expected}\n'))
    source = '''program worker;
initial begin top.q <= @(top.tick) 1; #3; end
endprogram
module top; event tick; int q=0; worker p();
initial begin #1; q<=2; ->tick; #1; $display("reactive %0d",q); end
endmodule
'''
    result.update(output('event_nba_front_origin_reactive', source, 'reactive 1\n'))
    source = '''module top; event tick; int q=0;
initial begin
q <= @(tick) 42;
wait fork;
$display("fork %0t",$time);
#1; ->tick; #1; $display("value %0d",q);
end endmodule
'''
    result.update(output('event_nba_front_independent_wait_fork',source,'fork 0\nvalue 42\n'))
    source = '''program worker;
initial top.q <= @(top.tick) 42;
endprogram
module top; event tick; int q=0; worker p();
initial begin #1; ->tick; end
final $display("final %0d",q);
endmodule
'''
    result.update(output('event_nba_front_program_completion',source,'final 0\n'))
    return result

def wide_counts():
    result = {}
    for width in (65, 129):
        for domain in ('bit', 'logic'):
            for signed in (False, True):
                label = 'signed' if signed else 'unsigned'
                name = f'event_nba_front_wide_count_{domain}_{width}_{label}'
                value = (1 << (width-2)) | 2
                sign = 'signed ' if signed else ''
                source = f'''module top;
event tick;
{domain} {sign}[{width-1}:0] remaining={width}'h{value:x};
int q=0;
initial begin
q <= repeat(remaining) @(tick) 42;
remaining=0;
->tick; ->tick;
#1; $display("pending %0d",q);
end endmodule
'''
                result.update(output(name, source, 'pending 0\n'))
    return result


def random_sequence(seed, count):
    # SYSTEM_TASKS.md 13.1/13.3: project SplitMix64 sequence, upper 32 bits.
    mask = (1 << 64)-1
    result = []
    for _ in range(count):
        seed = (seed+0x9e3779b97f4a7c15) & mask
        z = ((seed ^ (seed >> 30))*0xbf58476d1ce4e5b9) & mask
        z = ((z ^ (z >> 27))*0x94d049bb133111eb) & mask
        result.append((z ^ (z >> 31)) >> 32)
    return result


def origin_callbacks():
    result = {}
    origin = random_sequence(123, 7)
    trigger = random_sequence(456, 3)
    for live in (False, True):
        for task in (False, True):
            for watch in (False, True):
                name = ('event_nba_front_callback_'+('live' if live else 'ended')+
                        ('_task' if task else '_direct')+('_watch' if watch else '_guard'))
                return_type = 'bit [128:0]' if watch else 'bit'
                returned = 'clock' if watch else 'work==1'
                notification = "clock ^= (129'd1<<128);" if watch else '->tick;'
                fire = 'fire();' if task else notification*2
                callback = 'callback()' if watch else 'tick iff callback()'
                source = f'''module top;
event tick; bit [128:0] clock=0; int q=0;
function automatic {return_type} callback();
  int work=0;
  work++;
  $display("callback %m %0d %0d",work,$urandom());
  return {returned};
endfunction
task automatic fire(); {notification} {notification} endtask
initial begin
  int unused;
  unused=$urandom(123);
  $display("origin before %0d",$urandom());
  q <= repeat(2) @({callback}) 42;
  $display("origin after %0d",$urandom());
'''
                if live:
                    source += '  #3; $display("origin resume %0d",$urandom());\n'
                source += f'''end
initial begin
  int unused;
  #1;
  unused=$urandom(456);
  $display("trigger before %0d",$urandom());
  {fire}
  $display("trigger after %0d",$urandom());
  #1; $display("value %0d",q);
end endmodule
'''
                expected = f'origin before {origin[1]}\n'
                if watch:
                    expected += f'callback top.callback 1 {origin[2]}\n'
                after = 3 if watch else 2
                expected += (f'origin after {origin[after]}\ntrigger before {trigger[1]}\n'+
                             f'callback top.callback 1 {origin[after+1]}\n'+
                             f'callback top.callback 1 {origin[after+2]}\n'+
                             f'trigger after {trigger[2]}\nvalue 42\n')
                if live:
                    expected += f'origin resume {origin[after+3]}\n'
                row = output(name, source, expected)
                row[name+'.sv'] = row[name+'.sv'].replace(':top_module: top\n',
                                                       ':top_module: top\n:args: --max-call-depth=1\n')
                result.update(row)
    return result


def aggregate_order():
    result = {}
    for home in HOMES:
        for shape in ('array', 'record'):
            name = f'event_nba_front_aggregate_order_{home}_{shape}'
            if shape == 'array':
                types = 'typedef logic [128:0] row_t[2];'
                variables = ['row_t q[2], src;', 'int trace, idx;', 'event ticks[2];']
                setup = "q='{default:'{default:0}}; src='{7,11};"
                mutate = "src='{99,101};"
                pending = '(q[1][0]===0) && (q[1][1]===0)'
                final = '(q[1][0]===7) && (q[1][1]===11) && (q[0][0]===0)'
            else:
                types = 'typedef struct {logic [128:0] data; int items[$];} row_t;'
                variables = ['row_t q[2], src;', 'int trace, idx;', 'event ticks[2];']
                setup = "q='{'{0,{}},'{0,{}}}; src='{7,'{11,13}};"
                mutate = 'src.data=99; src.items[0]=101; src.items.push_back(17);'
                pending = '(q[1].data===0) && (q[1].items.size()==0)'
                final = '(q[1].data===7) && (q[1].items.size()==2) && (q[1].items[0]===11) && (q[1].items[1]===13) && (q[0].data===0)'
            methods = f'''function automatic int count_value();
trace=trace*10+1; {mutate} return 2;
endfunction
function automatic int pick(); trace=trace*10+2; return idx++; endfunction
function automatic int event_pick(); trace=trace*10+3; return 0; endfunction
'''
            issue = f'''{setup} trace=0; idx=1;
q[pick()] <= repeat(count_value()) @(ticks[event_pick()]) src;
idx=0;
'''
            report = f'''$display("capture %0d",trace);
->ticks[0]; #1; $display("pending %0d",{pending});
->ticks[0]; #1; $display("value %0d",{final});
'''
            source = wrap(home, types, variables, methods, issue, report,
                          ('q','src','trace','idx','ticks'))
            result.update(output(name, source, 'capture 123\npending 1\nvalue 1\n'))
    return result


def cancellation_and_empty():
    result = {}
    for end in ('block', 'fork', 'task'):
        for watch in (False, True):
            notification = 'clock^=1;' if watch else '->tick;'
            event = 'sample()' if watch else 'tick iff gate()'
            statement = f'q <= repeat(2) @({event}) 42; #9;'
            source = '''module top;
event tick; bit clock=0; int q=0;
function automatic bit gate(); int work=0; work++; return work==1; endfunction
function automatic bit sample(); bit work=0; work^=clock; return work; endfunction
'''
            if end=='block':
                source += f'initial begin:issuer {statement} end\ninitial begin #1; disable issuer; end\n'
            elif end=='fork':
                source += f'initial begin fork begin {statement} end join_none #1; disable fork; end\n'
            else:
                source += f'task issue(); {statement} endtask\ninitial issue();\ninitial begin #1; disable issue; end\n'
            source += f'''initial begin #2; {notification} #1; $display("pending %0d",q);
{notification} #1; $display("value %0d",q); end
endmodule
'''
            result.update(output('event_nba_front_cancel_'+end+('_watch' if watch else '_guard'),
                                 source, 'pending 0\nvalue 42\n'))
    for target in ('valid', 'element', 'bits'):
        lhs = {'valid':'q[0]', 'element':'q[idx]', 'bits':'bits[pos+:3]'}[target]
        source = f'''module top;
event tick; int q[2]='{{7,9}}; int idx=-1, pos=20; logic [7:0] bits=8'h55;
function automatic bit gate(int item);
  int work=0; work++;
  $display("guard %0d %0d",item,work);
  return work==1;
endfunction
initial begin
  {lhs} <= repeat(2) @(tick iff gate(1) or tick iff gate(2)) 3;
  ->tick; #1; $display("pending %0d",q[0]);
  ->tick; #1; $display("value %0d %0d %0d",q[0],q[1],bits);
end endmodule
'''
        value = 3 if target=='valid' else 7
        expected = f'guard 1 1\nguard 2 1\npending 7\nguard 1 1\nguard 2 1\nvalue {value} 9 85\n'
        result.update(output('event_nba_front_empty_'+target,source,expected))
    source = '''module top; event tick; int q=0;
initial begin
q <= repeat(2) @(tick) 10;
q <= @(tick) 20;
->tick; #1; $display("first %0d",q);
->tick; #1; $display("second %0d",q);
end endmodule
'''
    result.update(output('event_nba_front_independent_counts',source,'first 20\nsecond 10\n'))
    return result


def files():
    result = mechanisms()
    result.update(cancellation_and_empty())
    result.update(wide_counts())
    result.update(origin_callbacks())
    result.update(aggregate_order())
    for home in HOMES:
        for shape in SHAPES:
            for control in CONTROLS:
                result.update(case(home, shape, control))
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument('--check', action='store_true')
    args = parser.parse_args()
    bad = []
    for name, text in files().items():
        path = OUT/name
        if args.check:
            if not path.exists() or path.read_text()!=text:
                bad.append(name)
        else:
            path.write_text(text)
    if bad:
        print('gen_event_nba_matrix: out of date: '+', '.join(bad), file=sys.stderr)
    return int(bool(bad))


if __name__=='__main__':
    sys.exit(main())
