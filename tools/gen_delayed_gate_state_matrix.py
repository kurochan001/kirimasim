#!/usr/bin/env python3
# Copyright (c) 2026 The kirimasim Authors
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

"""Original complete-state inertial gate matrix (28.6, 28.16).

A small discrete-event oracle uses complete truth-table states and independent
committed/pending records. Expectations do not come from simulator output.
Samples are one picosecond after boundaries, avoiding Active-region races.
"""

import argparse
from pathlib import Path

OUT = Path(__file__).resolve().parent.parent / 'tests/suite/nets'
VERSION = 1
DELAYS = {'single': (3, 3, 3), 'pair': (4, 2, 2), 'triple': (4, 3, 5), 'zero': (0, 0, 0)}
# Desired noninverted output data, control: enabled, disabled or literal X/Z.
EVENTS = [(0, '1', 'x'), (10000, '0', 'x'), (11000, '0', 'z'),
          (20000, '1', 'x'), (20500, '1', 'z'), (21000, '0', 'x'),
          (30000, '1', 'enabled'), (40000, '0', 'enabled'),
          (50000, '0', 'disabled'), (60000, 'x', 'x'),
          (70000, 'z', 'z'), (80000, '1', 'x'), (90000, '0', 'z')]


def state(data, control):
    if control == 'enabled':
        return (data if data in '01' else 'x', True, True)
    if control == 'disabled':
        return ('z', True, True)
    return ('x', data != '0', data != '1')


def resolve(value):
    logic, one, zero = value
    if logic == 'z':
        return '1'  # weak-1 background
    if logic == '0':
        return '0'
    if logic == '1':
        return '1'
    return 'x' if zero else '1'


def oracle(delays, samples):
    current, pending = ('z', False, False), None
    observations = {}
    actions = [(t, 0, (d, c)) for t, d, c in EVENTS]
    actions += [(t, 1, None) for t in samples]
    rise, fall, off = [n * 1000 for n in delays]
    for time, is_sample, inputs in sorted(actions):
        if pending is not None and pending[0] <= time:
            current, pending = pending[1], None
        if is_sample:
            observations[time] = resolve(current)
            continue
        candidate = state(*inputs)
        if pending is not None and pending[1] == candidate:
            continue
        pending = None
        if candidate != current:
            delay = rise if candidate[0] == '1' else fall if candidate[0] == '0' else off if candidate[0] == 'z' else min(rise, fall, off)
            if delay == 0:
                current = candidate
            else:
                pending = (time + delay, candidate)
    return observations


def case(gate, mode, width, context):
    delays = DELAYS[mode]
    name = f'delayed_gate_state_{gate}_{mode}_{context}_{width}'
    samples = sorted({time + offset for time, _, _ in EVENTS for offset in (1, 1001, 2001, 3001, 4001, 5001)})
    observations = oracle(delays, samples)
    delay = '#3' if mode == 'single' else '#(4,2)' if mode == 'pair' else '#(4,3,5)' if mode == 'triple' else '#0'
    control = 'control' if context != 'shortcircuit' else 'permit && read_control(control)'
    primitive = f'{gate} {delay} g[{width-1}:0](q,data,{control});'
    if context == 'generate':
        primitive = 'if (1) begin: nested\n' + primitive + '\nend'
    inverted = gate.startswith('not')
    enabled = gate.endswith('1')
    body, expected = [], []
    actions = [(t, 0, (d, c)) for t, d, c in EVENTS]
    actions += [(t, 1, None) for t in samples]
    previous, sample_number = 0, 0
    for time, is_sample, inputs in sorted(actions):
        if time != previous:
            body.append(f'  #{time-previous}ps;')
        previous = time
        if is_sample:
            body.append(f'  $display("s{sample_number} %b", q);')
            expected.append(f's{sample_number} {observations[time] * width}\n')
            sample_number += 1
        else:
            data, ctrl = inputs
            actual = '0' if data == '1' else '1' if data == '0' else data
            if not inverted:
                actual = data
            c = str(int(enabled if ctrl == 'enabled' else not enabled)) if ctrl in ('enabled', 'disabled') else ctrl
            body.append(f"  data = '{actual}; control = 1'b{c};")
    source = f'''/*
:name: {name}
:description: Complete inertial gate states including side-only pulses and identical reevaluation
:tags: nets
:type: simulation
:lrm: 1800-2023 28.6 28.16
:oracle: spec
:spec_check: ai
:fourstate: yes
:nondeterministic: no
:generator: gen_delayed_gate_state_matrix.py@{VERSION}
:top_module: top
:expected_stdout: @{name}.expected
*/
module top;
timeunit 1ns; timeprecision 1ps;
logic [{width-1}:0] data = '{'0' if inverted else '1'};
logic control = 1'bx, permit = 1;
wire [{width-1}:0] q;
function automatic logic read_control(input logic v); return v; endfunction
{primitive}
assign (weak1,weak0) q = '1;
initial begin
{chr(10).join(body)}
end
endmodule
'''
    return name, source, ''.join(expected)


def files():
    for gate in ('bufif0', 'bufif1', 'notif0', 'notif1'):
        for mode in DELAYS:
            for width in (1, 33, 65, 129, 257):
                for context in ('module', 'generate', 'shortcircuit'):
                    name, source, expected = case(gate, mode, width, context)
                    yield name + '.sv', source
                    yield name + '.expected', expected


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--check', action='store_true')
    args = parser.parse_args()
    failures = 0
    for name, text in files():
        path = OUT / name
        if args.check:
            if not path.exists() or path.read_text() != text:
                print('gen_delayed_gate_state_matrix: out of date: ' + name)
                failures += 1
        else:
            path.write_text(text)
    return bool(failures)


if __name__ == '__main__':
    raise SystemExit(main())
