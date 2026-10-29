#!/usr/bin/env python3
# Copyright (c) 2026 The kirimasim Authors
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

"""Check $bits dynamic-return errors and legal unevaluated queries (20.6.2)."""

import argparse
from pathlib import Path
import re
import subprocess
import sys
import tempfile

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / "tools"))
import bounded_run  # noqa: E402


DIAGNOSTIC = ("$bits cannot use a function call with a dynamically sized return type "
              "(IEEE 1800-2023 20.6.2)")

ARRAY_FUNCTION = '''  typedef logic [8:0] samples_t[];
  function automatic samples_t samples();
    return '{9'h13, 9'h107, 9'h42};
  endfunction
'''

QUEUE_FUNCTION = '''  typedef byte entries_t[$];
  function automatic entries_t entries();
    return '{3, 9, 27};
  endfunction
'''

STRING_FUNCTION = '''  function automatic string label_text();
    return "query-value";
  endfunction
'''

PACKAGE_FUNCTION = "package shapes;\n" + ARRAY_FUNCTION + "endpackage\n"

CLASS_QUERY_FUNCTIONS = '''  int calls;
  function automatic holder make_holder(); calls++; return null; endfunction
  function automatic logic [18:0] consume(input holder value);
    calls++; return '1;
  endfunction
'''


def design(declarations, body):
    return "module top;\n" + declarations + body + "endmodule\n"


def invalid_cases():
    cases = {
        "array_parameter": design(ARRAY_FUNCTION,
            "  localparam int width = $bits(samples()); // ERROR\n"
            '  initial $display("must not run: %0d", width);\n'),
        "queue_parameter": design(QUEUE_FUNCTION,
            "  parameter int width = $bits(entries()); // ERROR\n"
            '  initial $display("must not run: %0d", width);\n'),
        "string_procedure": design(STRING_FUNCTION,
            "  initial begin\n"
            '    $display("%0d", $bits(label_text())); // ERROR\n'
            "  end\n"),
        "array_initializer": design(ARRAY_FUNCTION,
            "  int width = $bits(samples()); // ERROR\n"
            '  initial $display("must not run: %0d", width);\n'),
        "queue_automatic": design(QUEUE_FUNCTION,
            "  task automatic inspect();\n"
            "    int width = $bits(entries()); // ERROR\n"
            '    $display("must not run: %0d", width);\n'
            "  endtask\n"
            "  initial inspect();\n"),
        "string_unused_function": design(STRING_FUNCTION,
            "  function automatic int unused_query();\n"
            "    return $bits(label_text()); // ERROR\n"
            "  endfunction\n"
            '  initial $display("must not run");\n'),
        "array_parenthesized": design(ARRAY_FUNCTION,
            "  localparam int width = $bits((((samples())))); // ERROR\n"
            '  initial $display("must not run: %0d", width);\n'),
        "nested_array_function": design(ARRAY_FUNCTION +
            "  function automatic samples_t outer();\n"
            "    return samples();\n"
            "  endfunction\n",
            "  localparam int width = $bits(outer()); // ERROR\n"
            '  initial $display("must not run: %0d", width);\n'),
        "dynamic_function_argument": design(ARRAY_FUNCTION +
            "  function automatic int consume(input samples_t data);\n"
            "    return data.size();\n"
            "  endfunction\n",
            "  localparam int width = $bits(consume(samples())); // ERROR\n"
            '  initial $display("must not run: %0d", width);\n'),
        "dynamic_function_index": design(ARRAY_FUNCTION +
            "  function automatic samples_t empty_samples(); return '{}; endfunction\n"
            "  int values[2];\n",
            "  localparam int width = $bits(values[empty_samples().size()]); // ERROR\n"
            '  initial $display("must not run: %0d", width);\n'),
        "dynamic_system_function": design("",
            '  localparam int width = $bits($sformatf("value=%0d", 29)); // ERROR\n'
            '  initial $display("must not run: %0d", width);\n'),
        "two_dynamic_calls_one_query": design(ARRAY_FUNCTION +
            "  function automatic int combine(input samples_t a, input samples_t b);\n"
            "    return a.size() + b.size();\n"
            "  endfunction\n",
            "  localparam int width = $bits(combine(samples(), samples())); // ERROR\n"
            '  initial $display("must not run: %0d", width);\n'),
        "dynamic_event_queue": design(
            "  typedef event events_t[$];\n"
            "  function automatic events_t event_queue();\n"
            "    events_t events; return events;\n"
            "  endfunction\n"
            "  function automatic int consume(input events_t events);\n"
            "    return events.size();\n"
            "  endfunction\n",
            "  localparam int width = $bits(consume(event_queue())); // ERROR\n"
            '  initial $display("must not run: %0d", width);\n'),
        "class_inherited_queue": design(
            "  class queue_base; byte values[$]; endclass\n"
            "  class holder extends queue_base; bit ready; endclass\n" + CLASS_QUERY_FUNCTIONS,
            "  localparam int width = $bits(consume(make_holder())); // ERROR\n"
            '  initial $display("must not run: %0d", width);\n'),
        "class_recursive_queue": design(
            "  class holder; holder next; byte values[$]; endclass\n" + CLASS_QUERY_FUNCTIONS,
            "  localparam int width = $bits(consume(make_holder())); // ERROR\n"
            '  initial $display("must not run: %0d", width);\n'),
        "size_cast": design(ARRAY_FUNCTION,
            "  localparam int value = ($bits(samples()))'(1); // ERROR\n"
            '  initial $display("must not run: %0d", value);\n'),
        "packed_range": PACKAGE_FUNCTION + design("",
            "  logic [$bits(shapes::samples()) - 1:0] word; // ERROR\n"
            '  initial $display("must not run: %b", word);\n'),
        "unpacked_range": PACKAGE_FUNCTION + design("",
            "  logic words[$bits(shapes::samples())]; // ERROR\n"
            '  initial $display("must not run: %b", words[0]);\n'),
        "typedef_packed_range": PACKAGE_FUNCTION + design("",
            "  typedef logic [$bits(shapes::samples()) - 1:0] word_t; // ERROR\n"
            "  word_t word;\n"
            '  initial $display("must not run: %b", word);\n'),
        "input_port_width": PACKAGE_FUNCTION +
            "module leaf(input logic [$bits(shapes::samples()) - 1:0] value); // ERROR\n"
            "endmodule\n" + design("",
            "  logic [26:0] value;\n"
            "  leaf instance_name(value);\n"
            '  initial $display("must not run");\n'),
        "parameter_override": PACKAGE_FUNCTION +
            "module leaf #(parameter int Width = 1)();\n"
            '  initial $display("must not run: %0d", Width);\n'
            "endmodule\n" + design("",
            "  leaf #(.Width($bits(shapes::samples()))) instance_name(); // ERROR\n"),
        "type_override_caller_dynamic":
            "module leaf #(parameter type T = logic[3:0])();\n"
            "  function automatic logic [6:0] make(); return '1; endfunction\n"
            '  initial $display("width=%0d", $bits(T));\n'
            "endmodule\n" + design(
            "  typedef logic [8:0] samples_t[];\n"
            "  function automatic samples_t make();\n"
            "    return '{9'h13, 9'h107, 9'h42};\n"
            "  endfunction\n",
            "  leaf #(.T(logic[$bits(make()) - 1:0])) instance_name(); // ERROR\n"),
        "defparam_value": PACKAGE_FUNCTION +
            "module leaf #(parameter int Width = 1)();\n"
            '  initial $display("must not run: %0d", Width);\n'
            "endmodule\n" + design("",
            "  leaf instance_name();\n"
            "  defparam instance_name.Width = $bits(shapes::samples()); // ERROR\n"),
        "automatic_block_range": PACKAGE_FUNCTION + design("",
            "  initial begin : workspace\n"
            "    automatic logic [$bits(shapes::samples()) - 1:0] word; // ERROR\n"
            "    word = '0;\n"
            '    $display("must not run: %b", word);\n'
            "  end\n"),
        "for_initializer_range": PACKAGE_FUNCTION + design("",
            "  initial begin\n"
            "    for (logic [$bits(shapes::samples()) - 1:0] index = 0; index < 1; index++) // ERROR\n"
            '      $display("must not run: %0d", index);\n'
            "  end\n"),
        "generate_condition": PACKAGE_FUNCTION + design("",
            "  if ($bits(shapes::samples()) > 0) begin : selected // ERROR\n"
            '    initial $display("must not run");\n'
            "  end\n"),
        "generate_loop_size_cast": design(ARRAY_FUNCTION +
            "  function automatic samples_t indexed_samples(input int index);\n"
            "    return samples();\n"
            "  endfunction\n",
            "  for (genvar index = 0; index < ($bits(indexed_samples($bits(index))))'(1); index++) begin : items // ERROR\n"
            '    initial $display("must not run");\n'
            "  end\n"),
        "iterator_size_cast": design(ARRAY_FUNCTION +
            "  function automatic samples_t projected(input int width);\n"
            "    return samples();\n"
            "  endfunction\n"
            "  int values[2];\n",
            "  initial begin\n"
            "    values = '{1, 2};\n"
            '    $display("must not run: %0d", values.sum() with (($bits(projected($bits(item))))\'(item))); // ERROR\n'
            "  end\n"),
        "multiple_queries": design(ARRAY_FUNCTION + QUEUE_FUNCTION + STRING_FUNCTION,
            "  initial begin\n"
            '    $display("%0d", $bits(samples())); // ERROR\n'
            '    $display("%0d", $bits(entries())); // ERROR\n'
            '    $display("%0d", $bits(label_text())); // ERROR\n'
            "  end\n"),
    }
    return cases


def valid_cases():
    cases = {
        "fixed_calls": ('''module top;
  typedef logic [8:0] pair_t[2];
  int calls;
  byte entries[$];
  function automatic logic [72:0] wide_value();
    calls++; return '1;
  endfunction
  function automatic pair_t pair_value();
    calls++; return '{9'h13, 9'h107};
  endfunction
  function automatic int argument();
    calls++; return 17;
  endfunction
  function automatic logic [14:0] consume(input int value);
    calls += value; return 15'h1234;
  endfunction
  initial begin
    $display("%0d %0d %0d %0d", $bits(wide_value()), $bits(pair_value()),
             $bits(consume(argument())), $bits(entries.size()));
    $display("calls=%0d", calls);
  end
endmodule
''', "73 18 15 32\ncalls=0\n"),
        "dynamic_variable_argument": ('''module top;
  typedef logic [8:0] samples_t[];
  samples_t data;
  int calls;
  function automatic logic [22:0] consume(input samples_t value);
    calls++; return value.size();
  endfunction
  initial begin
    data = new[3];
    $display("%0d %0d", $bits(consume(data)), $bits(data.size()));
    $display("calls=%0d", calls);
  end
endmodule
''', "23 32\ncalls=0\n"),
        "fixed_handle_and_event_calls": ('''module top;
  event pulse;
  int calls;
  function automatic event event_value(); calls++; return pulse; endfunction
  function automatic chandle handle_value(); calls++; return null; endfunction
  function automatic int event_wrapper(input event value); calls++; return 17; endfunction
  function automatic logic [10:0] handle_wrapper(input chandle value);
    calls++; return 11'h37;
  endfunction
  localparam int event_width = $bits(event_wrapper(event_value()));
  localparam int handle_width = $bits(handle_wrapper(handle_value()));
  initial $display("%0d %0d calls=%0d", event_width, handle_width, calls);
endmodule
''', "32 11 calls=0\n"),
        "type_override_caller_fixed": ('''module leaf #(parameter type T = logic[3:0])();
  typedef byte entries_t[$];
  function automatic entries_t make(); return '{4, 8}; endfunction
  initial $display("width=%0d", $bits(T));
endmodule
module top;
  function automatic logic [12:0] make(); return '1; endfunction
  leaf #(.T(logic[$bits(make()) - 1:0])) instance_name();
endmodule
''', "width=13\n"),
        "type_and_literal_queries": ('''module top;
  typedef struct packed { logic [4:0] code; bit [6:0] flags; } record_t;
  int calls;
  logic [3:0] choices[2];
  function automatic int index_value();
    calls++; return 1;
  endfunction
  initial begin
    $display("%0d %0d %0d", $bits(record_t), $bits("abc"),
             $bits(choices[index_value()]));
    $display("calls=%0d", calls);
  end
endmodule
''', "12 24 4\ncalls=0\n"),
    }
    for name, declaration in (
            ("class_fixed_members", "  class holder; bit [6:0] tag; int count; endclass\n"),
            ("class_empty", "  class holder; endclass\n"),
            ("class_recursive_only", "  class holder; holder next; endclass\n")):
        source = design(declaration + CLASS_QUERY_FUNCTIONS,
            "  localparam int width = $bits(consume(make_holder()));\n"
            '  initial $display("width=%0d calls=%0d", width, calls);\n')
        cases[name] = (source, "width=19 calls=0\n")
    return cases


def valid_parse_cases():
    # These dynamic variable queries are legal, but runtime lowering of them
    # is an existing unsupported boundary, outside this diagnostic change.
    return {
        "dynamic_variables": '''module top;
  logic [8:0] samples[];
  byte entries[$];
  string label_text;
  initial begin
    samples = new[3]; entries = '{3, 9}; label_text = "abcd";
    $display("%0d %0d %0d", $bits(samples), $bits(entries), $bits(label_text));
  end
endmodule
''',
    }


def expected_errors(source):
    return [(number, line.index("$bits") + 1, DIAGNOSTIC)
            for number, line in enumerate(source.splitlines(), 1) if "// ERROR" in line]


def diagnostics(stderr, source):
    found = re.findall(r"(?m)^(.+?):(\d+):(\d+): error: ([^\n]+)$", stderr)
    positions = []
    for filename, line, column, message in found:
        path = Path(filename)
        if not path.is_absolute():
            path = source.parent / path
        if path.resolve() != source:
            return None
        positions.append((int(line), int(column), message))
    return positions


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--kirimasim", type=Path, required=True)
    exe = str(parser.parse_args().kirimasim.resolve())
    failures = []
    checked = 0
    with tempfile.TemporaryDirectory(prefix="kirimasim-bits-query-") as directory:
        root = Path(directory).resolve()
        for name, source in invalid_cases().items():
            path = root / f"{name}.sv"
            path.write_text(source, encoding="utf-8")
            expected = sorted(expected_errors(source))
            for mode in ((), ("parse",), ("--dump-sir",)):
                checked += 1
                try:
                    result = bounded_run.run([exe, *mode, str(path)], cwd=root, timeout=20)
                except (subprocess.TimeoutExpired, bounded_run.OutputLimit) as error:
                    failures.append(f"{name} {mode}: {error}")
                    continue
                actual = diagnostics(result.stderr, path)
                if (result.returncode != 2 or result.stdout or actual is None or
                        sorted(actual) != expected or
                        result.stderr.count("error:") != len(expected)):
                    failures.append(f"{name} {mode}: code={result.returncode}, "
                                    f"stdout={result.stdout!r}, stderr={result.stderr!r}; "
                                    f"expected errors={expected!r}")
        controls = [(name, source, expected, ((), ("parse",), ("--dump-sir",)))
                    for name, (source, expected) in valid_cases().items()]
        controls += [(name, source, "", (("parse",),))
                     for name, source in valid_parse_cases().items()]
        for name, source, expected, modes in controls:
            path = root / f"{name}.sv"
            path.write_text(source, encoding="utf-8")
            for mode in modes:
                checked += 1
                try:
                    result = bounded_run.run([exe, *mode, str(path)], cwd=root, timeout=20)
                except (subprocess.TimeoutExpired, bounded_run.OutputLimit) as error:
                    failures.append(f"{name} {mode}: {error}")
                    continue
                wanted = expected if not mode else ""
                output_ok = (result.stdout.startswith("; sir format ") if mode == ("--dump-sir",)
                             else result.stdout == wanted)
                if result.returncode != 0 or not output_ok or "error:" in result.stderr:
                    failures.append(f"{name} {mode}: code={result.returncode}, "
                                    f"stdout={result.stdout!r}, stderr={result.stderr!r}")
    print(f"bits queries: {checked} checks, {len(failures)} failures")
    for failure in failures:
        print(failure)
    return bool(failures)


if __name__ == "__main__":
    raise SystemExit(main())
