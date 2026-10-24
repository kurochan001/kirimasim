#!/usr/bin/env python3
# Copyright (c) 2026 The kirimasim Authors
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

"""Original array-method matrix, using a bit-string and stable-list oracle.

No expected value is obtained from another simulator. Numerical folds use
Python integers, bitwise folds use the four-state truth tables, and ordering
uses Python's stable sort within runs separated by unknown comparison keys.
The fixed-array cases also exercise descending and nonzero declared indices.
"""

import argparse
from pathlib import Path

OUT = Path(__file__).resolve().parent.parent / 'tests/suite/arrays'
VERSION = 1


def bits(value, width):
    return format(value % (1 << width), f'0{width}b')


def number(value, signed):
    if any(c in value for c in 'xz'):
        return None
    n = int(value, 2)
    return n - (1 << len(value)) if signed and value[0] == '1' else n


def fold(values, op, width):
    if not values:
        return bits(1 if op == 'product' else -1 if op == 'and' else 0, width)
    if op in ('sum', 'product'):
        if len(values) == 1:
            return values[0]
        ns = [number(v, False) for v in values]
        if None in ns:
            return 'x' * width
        if op == 'sum':
            return bits(sum(ns), width)
        result = 1
        for n in ns:
            result *= n
        return bits(result, width)
    out = values[0]
    for v in values[1:]:
        def combine(a, b):
            if op == 'and':
                return '0' if '0' in (a, b) else '1' if a == b == '1' else 'x'
            if op == 'or':
                return '1' if '1' in (a, b) else '0' if a == b == '0' else 'x'
            return str(int(a) ^ int(b)) if a in '01' and b in '01' else 'x'
        out = ''.join(combine(a, b) for a, b in zip(out, v))
    return out


def ordered(values, key, signed, reverse=False):
    # Unknown elements are barriers: a definite relational result is required.
    result, run = [], []
    for value in values:
        n = number(key(value), signed)
        if n is None:
            result.extend(v for _, v in sorted(run, key=lambda pair: pair[0], reverse=reverse))
            run = []
            result.append(value)
        else:
            run.append((n, value))
    result.extend(v for _, v in sorted(run, key=lambda pair: pair[0], reverse=reverse))
    return result


def extremum(values, signed, maximum=False, key=lambda x: x):
    if not values:
        return []
    selected = values[0]
    for value in values[1:]:
        a, b = number(key(value), signed), number(key(selected), signed)
        if a is not None and b is not None and (a > b if maximum else a < b):
            selected = value
    return [selected]


def case(shape, width, four, signed, context):
    type_text = f"{'logic' if four else 'bit'} {'signed' if signed else 'unsigned'} [{width-1}:0]"
    dims = '[4:1]' if shape == 'fixed' else '[]' if shape == 'dynamic' else '[$]'
    name = f"array_methods_{shape}_w{width}_{'four' if four else 'two'}_{'s' if signed else 'u'}_{context}"
    known = [-3, 2, -1, 2] if signed and width > 1 else [7, 2, 5, 2]
    sequences = [[bits(n, width) for n in known]]
    if four:
        sequences += [['x' * width, bits(2, width), bits(1, width), 'z' * width],
                      [bits(3, width), 'z' * width, bits(2, width), bits(1, width)]]
    if shape != 'fixed':
        sequences += [[], ['z' * width] if four else [bits(1, width)]]
    body, expected = [], []

    def reset(values):
        if shape == 'dynamic':
            body.append(f'    a = new[{len(values)}];')
        if shape == 'queue':
            body.append('    a.delete();')
        for i, v in enumerate(values):
            literal = f"{width}'b{v}"
            body.append(f'    a.push_back({literal});' if shape == 'queue'
                        else f'    a[{4-i if shape == "fixed" else i}] = {literal};')

    def output(label, values, target='a'):
        body.append(f'    $write("{label}");')
        if target == 'r':
            body.append('    $write(" %0d", r.size());')
        for i in range(len(values)):
            index = 4-i if target == 'a' and shape == 'fixed' else i
            body.append(f'    $write(" %b", {target}[{index}]);')
        body.append('    $write("\\n");')
        expected.append(label + (' ' + str(len(values)) if target == 'r' else '')
                        + ''.join(' ' + v for v in values) + '\n')

    for n, values in enumerate(sequences):
        reset(values)
        for op in ('sum', 'product', 'and', 'or', 'xor'):
            body.append(f'    $display("s{n}_{op} %b", a.{op}());')
            expected.append(f's{n}_{op} {fold(values, op, width)}\n')
        # Integral widening is determined by with-expression type, not source width.
        for cast in ('int', 'integer'):
            projected = []
            for v in values:
                extended = (v[0] if signed else '0') * max(0, 32 - len(v)) + v
                converted = extended[-32:]
                if cast == 'int':
                    converted = converted.translate(str.maketrans('xz', '00'))
                projected.append(converted)
            body.append(f'    $display("s{n}_wide_{cast} %b", a.sum() with ({cast}\'(item)));')
            expected.append(f's{n}_wide_{cast} {fold(projected, "sum", 32)}\n')
        for op in ('min', 'max'):
            body.append(f'    r = a.{op}();')
            output(f's{n}_{op}', extremum(values, signed, op == 'max'), 'r')
            body.append(f'    r = a.{op}() with (-item);')
            key = lambda v: 'x' * width if number(v, signed) is None else bits(-number(v, signed), width)
            output(f's{n}_{op}_key', extremum(values, signed, op == 'max', key), 'r')
        for op in ('sort', 'rsort', 'reverse', 'sort_key', 'rsort_key'):
            reset(values)
            method = op.split('_')[0]
            with_key = '_key' in op
            body.append(f'    a.{method}()' + (" with (item & E'(1));" if with_key else ';'))
            key = (lambda v: v[:-1].translate(str.maketrans('1xz', '000')) + v[-1]) if with_key else lambda v: v
            out = list(reversed(values)) if op == 'reverse' else ordered(values, key, signed, method == 'rsort')
            output(f's{n}_{op}', out)
    action = '\n'.join(body)
    declarations = f'    E a{dims};\n    E r[$];'
    if context == 'initial':
        run = declarations + '\n  initial begin\n' + action + '\n  end'
    elif context == 'automatic':
        run = '  task automatic run();\n' + declarations + '\n' + action + '\n  endtask\n  initial run();'
    else:
        run = f'  E source{dims};\n  task automatic run(ref E a{dims});\n    E r[$];\n' + action + '\n  endtask\n  initial run(source);'
    source = f'''/*
Generated by tools/gen_array_methods_matrix.py (v{VERSION}); do not edit.
:name: {name}
:description: Array methods share traversal and value rules across storage contexts
:tags: arrays
:type: simulation
:lrm: 1800-2023 7.12.1 7.12.2 7.12.3 13.5.2
:oracle: spec
:spec_check: ai
:fourstate: {"yes" if four else "no"}
:nondeterministic: no
:generator: gen_array_methods_matrix.py@{VERSION}
:top_module: top
:expected_stdout: @{name}.expected
*/
module top;
  typedef {type_text} E;
{run}
endmodule
'''
    return name, source, ''.join(expected)



def wrapped(name, declarations, body, context, dims, globals_=''):
    if context == 'initial':
        run = declarations + '\ninitial begin\n' + body + '\nend'
    elif context == 'automatic':
        run = 'task automatic run();\n' + declarations + '\n' + body + '\nendtask\ninitial run();'
    else:
        run = f'E source{dims};\ntask automatic run(ref E a{dims});\n' + declarations.replace(f'E a{dims};', '') + '\n' + body + '\nendtask\ninitial run(source);'
    return f'''/*
:name: {name}
:description: Array-method references, nonintegral keys and snapshot rules
:tags: arrays
:type: simulation
:lrm: 1800-2023 7.12.1 7.12.2 7.12.3 13.5.2
:oracle: spec
:spec_check: ai
:fourstate: yes
:nondeterministic: no
:generator: gen_array_methods_matrix.py@{VERSION}
:top_module: top
:expected_stdout: @{name}.expected
*/
module top;
{globals_}
{run}
endmodule
'''


def semantic_cases():
    for shape in ('fixed', 'dynamic', 'queue', 'bounded'):
        dims = '[4:1]' if shape == 'fixed' else '[]' if shape == 'dynamic' else '[$]'
        # A bounded actual receiver passes through an unbounded formal ref.
        for context in ('initial', 'automatic', 'ref'):
            for element in ('string', 'real', 'shortreal'):
                name = f'array_methods_keys_{shape}_{element}_{context}'
                values = ['bb', 'a', 'ba', 'a'] if element == 'string' else [2.5, -1.0, 2.5, 0.0]
                declarations = f'E a{dims};\nE r[$];'
                body, expected = [], []

                def reset():
                    if shape == 'dynamic':
                        body.append('a = new[4];')
                    if shape in ('queue', 'bounded'):
                        body.append('a.delete();')
                    for i, v in enumerate(values):
                        lit = '"' + v + '"' if element == 'string' else str(v)
                        body.append(f'a.push_back({lit});' if shape in ('queue', 'bounded') else f'a[{4-i if shape == "fixed" else i}] = {lit};')

                def output(label, data, target='a'):
                    fmt = '%s' if element == 'string' else '%.1f'
                    body.append(f'$write("{label}");')
                    if target == 'r':
                        body.append('$write(" %0d", r.size());')
                    for i in range(len(data)):
                        index = 4-i if shape == 'fixed' and target == 'a' else i
                        body.append(f'$write(" {fmt}", {target}[{index}]);')
                    body.append('$write("\\n");')
                    expected.append(label + (' ' + str(len(data)) if target == 'r' else '') + ''.join(' ' + (v if element == 'string' else f'{v:.1f}') for v in data) + '\n')

                reset()
                for method in ('min', 'max'):
                    body.append(f'r = a.{method}();')
                    output(method, [min(values) if method == 'min' else max(values)], 'r')
                for method in ('sort', 'rsort', 'reverse'):
                    reset()
                    body.append(f'a.{method}();')
                    data = list(reversed(values)) if method == 'reverse' else sorted(values, reverse=method == 'rsort')
                    output(method, data)
                source = wrapped(name, declarations, '\n'.join(body), context, dims, f'typedef {element} E;')
                if shape == 'bounded':
                    # Do not change the formal signature.
                    source = source.replace('E a[$];', 'E a[$:5];').replace('E source[$];', 'E source[$:5];')
                yield name, source, ''.join(expected)

            for nested in (False, True):
                name = f'array_methods_refs_{shape}_{"nested" if nested else "scalar"}_{context}'
                first, last = (4, 1) if shape == 'fixed' else (0, 3)
                globals_ = 'typedef int Row[$];\ntypedef Row E;' if nested else 'typedef int E;'
                suffix = '[0]' if nested else ''
                key = ' with (item[0])' if nested else ''
                dims_actual = '[$:5]' if shape == 'bounded' else dims
                holder = f'''
task automatic hold(ref int element, ref E receiver{dims});
  receiver.sort(){key};
  $display("sort %0d", element);
  element = 99;
  $display("write %0d", receiver[{first}]{suffix});
  receiver.reverse();
  $display("reverse %0d", element);
endtask
'''
                declarations = f'E a{dims};\nE r[$];'
                body = 'a = new[4];\n' if shape == 'dynamic' else ''
                for i, value in enumerate((8, 3, 6, 3)):
                    literal = "'{" + str(value) + '}' if nested else str(value)
                    body += f'a.push_back({literal});\n' if shape in ('queue', 'bounded') else f'a[{4-i if shape == "fixed" else i}] = {literal};\n'
                body += f'hold(a[{first}]{suffix}, a);\n'
                body += f'$display("values %0d %0d", a[{first}]{suffix}, a[{last}]{suffix});\n'
                body += f'r = a.min(){key};\n'
                body += f'r[0]{suffix} = 77;\n'
                body += f'$display("copy %0d %0d", a[{first}]{suffix}, r[0]{suffix});\n'
                source = wrapped(name, declarations, body, context, dims, globals_ + holder)
                if shape == 'bounded':
                    source = source.replace('E a[$];', 'E a[$:5];').replace('E source[$];', 'E source[$:5];')
                expected = 'sort 8\nwrite 3\nreverse 99\nvalues 99 3\ncopy 99 77\n' if nested else 'sort 3\nwrite 99\nreverse 8\nvalues 8 99\ncopy 8 77\n'
                yield name, source, expected

    for shape in ('fixed', 'dynamic', 'queue'):
        for method in ('min', 'max'):
            name = f'array_methods_snapshot_{shape}_{method}'
            dims = '[4:1]' if shape == 'fixed' else '[]' if shape == 'dynamic' else '[$]'
            declarations = f'typedef int E;\nE original{dims};\nint calls, trace, receivers;'
            mutation = 'foreach (original[i]) original[i] = 99;' if shape == 'fixed' else 'original.delete();'
            helpers = f'''
function int key(input int value);
  calls++;
  trace = trace * 10 + value;
  {mutation}
  return -value;
endfunction
'''
            body = 'original = new[4];\n' if shape == 'dynamic' else ''
            for i, value in enumerate((8, 3, 6, 3)):
                body += f'original.push_back({value});\n' if shape == 'queue' else f'original[{4-i if shape == "fixed" else i}] = {value};\n'
            body += f'r = original.{method}() with (key(item));\n'
            body += '$display("snapshot %0d %0d %0d %0d", calls, trace, r.size(), r[0]);\n'
            if shape != 'fixed':
                body += f'calls = 0; r = original.{method}() with (key(item));\n$display("empty %0d %0d", calls, r.size());\n'
                helpers += f'typedef int Array{dims};\nfunction Array provide(); receivers++; return original; endfunction\n'
                body += "original = '{5, 2};\n" + f'r = provide().{method}() with (key(item));\n' + '$display("receiver %0d %0d %0d", receivers, calls, r[0]);\n'
            source = wrapped(name, 'int r[$];', body, 'initial', dims, declarations + helpers)
            expected = f'snapshot 4 8363 1 {8 if method == "min" else 3}\n'
            if shape != 'fixed':
                expected += f'empty 0 0\nreceiver 1 2 {5 if method == "min" else 2}\n'
            yield name, source, expected


def boundary_cases():
    for shape in ('fixed', 'dynamic', 'queue'):
        dims = '[4:1]' if shape == 'fixed' else '[]' if shape == 'dynamic' else '[$]'
        for context in ('initial', 'automatic', 'ref'):
            name = f'array_methods_nan_{shape}_{context}'
            declarations = f'E a{dims};\nE r[$];'
            body, expected = [], []
            nan = 0x7ff8000000000001
            import struct
            known = [struct.unpack('>Q', struct.pack('>d', v))[0] for v in (3.0, -2.0, 1.0)]
            for pattern, values in enumerate(([nan] + known, [known[0], nan] + known[1:], known + [nan])):
                def reset():
                    if shape == 'dynamic': body.append('a = new[4];')
                    if shape == 'queue': body.append('a.delete();')
                    for i, value in enumerate(values):
                        expression = f"$bitstoreal(64'h{value:016x})"
                        body.append(f'a.push_back({expression});' if shape == 'queue' else f'a[{4-i if shape == "fixed" else i}] = {expression};')
                def key(value):
                    return None if value == nan else struct.unpack('>d', struct.pack('>Q', value))[0]
                reset()
                for method in ('min', 'max'):
                    chosen = values[0]
                    for value in values[1:]:
                        x, y = key(value), key(chosen)
                        if x is not None and y is not None and (x < y if method == 'min' else x > y):
                            chosen = value
                    body.append(f'r = a.{method}(); $display("n{pattern}_{method} %0d %016h", r.size(), $realtobits(r[0]));')
                    expected.append(f'n{pattern}_{method} 1 {chosen:016x}\n')
                for method in ('sort', 'rsort'):
                    reset()
                    body.append(f'a.{method}(); $write("n{pattern}_{method}");')
                    out, run = [], []
                    for value in values:
                        if value == nan:
                            out += sorted(run, key=key, reverse=method == 'rsort') + [value]
                            run = []
                        else: run.append(value)
                    out += sorted(run, key=key, reverse=method == 'rsort')
                    for i in range(4):
                        body.append(f'$write(" %016h", $realtobits(a[{4-i if shape == "fixed" else i}]));')
                    body.append('$write("\\n");')
                    expected.append(f'n{pattern}_{method}' + ''.join(f' {value:016x}' for value in out) + '\n')
            yield name, wrapped(name, declarations, '\n'.join(body), context, dims, 'typedef real E;'), ''.join(expected)

            name = f'array_methods_named_nested_{shape}_{context}'
            declarations = f'E a{dims};\nE r[$];\nint total;'
            body = 'a = new[4];\n' if shape == 'dynamic' else ''
            for i, row in enumerate(((8, 1), (2, 5), (3, 0), (3, 0))):
                value = "'{" + ','.join(map(str,row)) + '}'
                body += f'a.push_back({value});\n' if shape == 'queue' else f'a[{4-i if shape == "fixed" else i}] = {value};\n'
            body += '''
r = a.min(x) with (x.sum(y) with (y));
total = r[0].sum(z) with (z);
$display("nested %0d %0d %0d", r.size(), r[0][0], total);
r[0][0] = 99;
r = a.min(x) with (x.sum(y) with (y));
$display("copy %0d", r[0][0]);
'''
            yield name, wrapped(name, declarations, body, context, dims, 'typedef int Row[$];\ntypedef Row E;'), 'nested 1 3 3\ncopy 3\n'

def files():
    for shape in ('fixed', 'dynamic', 'queue'):
        for width in (1, 9, 32, 65, 129):
            for four in (False, True):
                for signed in (False, True):
                    for context in ('initial', 'automatic', 'ref'):
                        name, source, expected = case(shape, width, four, signed, context)
                        yield name + '.sv', source
                        yield name + '.expected', expected
    for name, source, expected in semantic_cases():
        yield name + '.sv', source
        yield name + '.expected', expected
    for name, source, expected in boundary_cases():
        yield name + '.sv', source
        yield name + '.expected', expected


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--check', action='store_true')
    args = parser.parse_args()
    errors = 0
    if not args.check:
        OUT.mkdir(parents=True, exist_ok=True)
    for name, text in files():
        path = OUT / name
        if args.check:
            if not path.exists() or path.read_text() != text:
                print(f'gen_array_methods_matrix: out of date: {name}')
                errors += 1
        else:
            path.write_text(text)
    return bool(errors)


if __name__ == '__main__':
    raise SystemExit(main())
