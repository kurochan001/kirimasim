#!/usr/bin/env python3
# Copyright (c) 2026 The kirimasim Authors
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

"""Check runtime array-query dimensions against explicit declared-range models."""

import argparse
from dataclasses import dataclass
from pathlib import Path
import re
import subprocess
import sys
import tempfile

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / "tools"))
import bounded_run  # noqa: E402


QUERIES = ("left", "right", "low", "high", "increment", "size")


@dataclass
class Case:
    name: str
    source: str
    repeats: int = 1

    @property
    def stdout(self):
        return (self.name + " passed\n") * self.repeats


def module(name, declarations, body, prefix=""):
    return (prefix + "module top;\n  timeunit 1ns; timeprecision 1ps;\n"
            + declarations + "  initial begin\n" + body
            + f'    $display("{name} passed");\n  end\nendmodule\n')


def check(expression, expected, label):
    return f'    if (({expression}) !== ({expected})) $fatal(1, "{label}");\n'


def range_values(left, right, dynamic=False):
    # Dynamic ranges have low 0, high size-1 and increment -1 even when empty.
    return (left, right, 0 if dynamic else min(left, right),
            right if dynamic else max(left, right),
            -1 if dynamic or left < right else 1, abs(right-left)+1 if not dynamic else right+1)


def range_checks(expression, dimension, left, right, dynamic=False):
    return "".join(check(f"${query}({expression}, {dimension})", expected, query)
                   for query, expected in zip(QUERIES, range_values(left, right, dynamic)))


def counts(expression, total, unpacked):
    return (check(f"$dimensions({expression})", total, "dimension count")
            + check(f"$unpacked_dimensions({expression})", unpacked, "unpacked count"))


def fixed_cases():
    cases = []
    shapes = [
        ("mixed_ranges", "logic [3:1][-2:0] values[7:5][-3:-1];",
         [(7, 5), (-3, -1), (3, 1), (-2, 0)], 2),
        ("negative_ranges", "bit [-4:-1][2:0] values[-5:-3][4:1];",
         [(-5, -3), (4, 1), (-4, -1), (2, 0)], 2),
        ("integer_leaf", "int values[-2:1][3:2];", [(-2, 1), (3, 2), (31, 0)], 2),
        ("packed_only", "logic [-7:-3][5:2] values;", [(-7, -3), (5, 2)], 0),
        ("plain_integer", "integer values;", [(31, 0)], 0),
        ("singleton_ranges", "logic [0:0] values[-1:-1];", [(-1, -1), (0, 0)], 1),
    ]
    for name, declaration, dimensions, unpacked in shapes:
        body = counts("values", len(dimensions), unpacked)
        # These literal expected tuples come from declarations, independent of
        # the implementation's runtime dimension selection and comparisons.
        for index, (left, right) in enumerate(dimensions, 1):
            body += f"    dim = {index};\n" + range_checks("values", "dim", left, right)
        body += "    dim = 0;\n"
        body += "".join(check(f"${q}(values, dim)", "32'bx", "zero dimension") for q in QUERIES)
        body += f"    dim = {len(dimensions)+1};\n"
        body += "".join(check(f"${q}(values, dim)", "32'bx", "excess dimension") for q in QUERIES)
        cases.append(Case(name, module(name, "  " + declaration + "\n  int dim;\n", body)))
    return cases


def dynamic_cases():
    cases = []
    for kind, suffix in (("dynamic", "[]"), ("queue", "[$]")):
        name = kind + "_resize"
        body = counts("values", 3, 1)
        for length in (0, 1, 4, 0, 2):
            body += (f"    values = new[{length}];\n" if kind == "dynamic"
                     else "    values = '{" + ", ".join("'0" for _ in range(length)) + "};\n")
            body += "    dim = 1;\n" + range_checks("values", "dim", 0, length-1, True)
            body += "    dim = 2;\n" + range_checks("values", "dim", 5, 2)
            body += "    dim = 3;\n" + range_checks("values", "dim", -2, 0)
        body += "    values.delete();\n    dim = 1;\n" + range_checks("values", "dim", 0, -1, True)
        cases.append(Case(name, module(name, f"  logic [5:2][-2:0] values{suffix};\n  int dim;\n", body)))

    name = "selected_dynamic_rows"
    declarations = "  typedef logic [6:3] row_t[];\n  row_t values[-2:0];\n  int dim, selected;\n"
    body = "    values[-2] = new[2]; values[-1] = new[0]; values[0] = new[5];\n"
    for index, length in ((-2, 2), (-1, 0), (0, 5)):
        body += f"    selected = {index}; dim = 1;\n"
        body += range_checks("values[selected]", "dim", 0, length-1, True)
        body += "    dim = 2;\n" + range_checks("values[selected]", "dim", 6, 3)
    # Later dynamic dimensions require an explicit subarray selection. Fixed
    # dimensions beyond them are still pure type queries with a constant dim.
    body += range_checks("values", "1", -2, 0) + range_checks("values", "3", 6, 3)
    body += counts("values", 3, 2)
    cases.append(Case(name, module(name, declarations, body)))

    name = "selected_nested_queue"
    declarations = "  typedef logic [8:4] row_t[$];\n  row_t values[];\n  int dim, selected;\n"
    body = "    values = new[2]; values[0] = '{'0}; values[1] = '{'0,'1,'0};\n"
    for index, length in ((0, 1), (1, 3)):
        body += f"    selected = {index}; dim = 1;\n"
        body += range_checks("values[selected]", "dim", 0, length-1, True)
        body += "    dim = 2;\n" + range_checks("values[selected]", "dim", 8, 4)
    body += counts("values", 3, 2)
    cases.append(Case(name, module(name, declarations, body)))
    return cases


def string_cases():
    name = "string_resize"
    body = counts("value", 1, 0)
    for text in ("", "a", "query", "", "xy"):
        body += f'    value = "{text}"; dim = 1;\n'
        body += range_checks("value", "dim", 0, len(text)-1, True)
    body += "    dim = 2;\n"
    body += "".join(check(f"${q}(value, dim)", "32'bx", "string invalid dimension") for q in QUERIES)
    return [Case(name, module(name, "  string value;\n  int dim;\n", body))]


def invalid_runtime_cases():
    cases = []
    dimensions = [("zero", "int", "0"), ("negative", "int", "-1"),
                  ("large", "int", "2147483647"),
                  ("all_x", "logic [31:0]", "'x"),
                  ("all_z", "logic [31:0]", "'z"),
                  ("partial_x", "logic [31:0]", "32'b000000000000000000000000000000x1"),
                  ("wide", "logic [128:0]", "129'h10000000000000001"),
                  ("high_x", "logic [128:0]", "129'bx" + "0" * 127 + "1"),
                  ("unsigned32", "logic [31:0]", "32'h80000001")]
    for suffix, dtype, value in dimensions:
        name = "invalid_" + suffix
        declarations = ("  logic [7:3] fixed_value[-2:0];\n  logic [7:3] dynamic_value[];\n"
                        "  string text_value;\n" + f"  {dtype} dim;\n")
        body = f'    dynamic_value = new[3]; text_value = "abc"; dim = {value};\n'
        for expression in ("fixed_value", "dynamic_value", "text_value"):
            body += "".join(check(f"${q}({expression}, dim)", "32'bx", "runtime invalid dimension")
                            for q in QUERIES)
        cases.append(Case(name, module(name, declarations, body)))
    # A valid wide unsigned value must retain its complete width without
    # sign-extending bit 31 or truncating a wider invalid value to one.
    name = "valid_wide_dimension"
    declarations = "  logic [7:3] values[-2:0];\n  logic [128:0] dim;\n"
    body = "    dim = 1;\n" + range_checks("values", "dim", -2, 0)
    body += "    dim = 2;\n" + range_checks("values", "dim", 7, 3)
    cases.append(Case(name, module(name, declarations, body)))
    name = "narrow_unsigned_dimension"
    declarations = "  logic [7:3] values[2:-1][6:4];\n  bit dim;\n"
    body = "    dim = 1;\n" + range_checks("values", "dim", 2, -1)
    body += "    dim = 0;\n"
    body += "".join(check(f"${q}(values, dim)", "32'bx", "narrow zero dimension")
                    for q in QUERIES)
    cases.append(Case(name, module(name, declarations, body)))
    name = "narrow_signed_dimension"
    declarations = "  logic [7:3] values[2:-1][6:4];\n  logic signed [0:0] dim;\n"
    body = "    dim = 1'b1;\n"
    body += "".join(check(f"${q}(values, dim)", "32'bx", "signed one-bit dimension is negative")
                    for q in QUERIES)
    body += "    dim = 1'b0;\n"
    body += "".join(check(f"${q}(values, dim)", "32'bx", "signed one-bit zero dimension")
                    for q in QUERIES)
    cases.append(Case(name, module(name, declarations, body)))
    return cases


def evaluation_cases():
    cases = []
    declarations = '''  int value_calls, dimension_calls, chosen_dimension;
  typedef logic [8:4] vector_t;
  typedef vector_t array_t[];
  function automatic vector_t fixed_value(); value_calls++; return '0; endfunction
  function automatic array_t dynamic_value(); value_calls++; return '{'0,'0,'0}; endfunction
  function automatic string string_value(); value_calls++; return "abcd"; endfunction
  function automatic int dimension(); dimension_calls++; return chosen_dimension; endfunction
'''
    for name, expression, dimensions in (
            ("fixed_receiver_unevaluated", "fixed_value()", [(8, 4, False)]),
            ("dynamic_receiver_evaluated", "dynamic_value()", [(0, 2, True), (8, 4, False)]),
            ("string_receiver_evaluated", "string_value()", [(0, 3, True)])):
        body = "    value_calls = 0; dimension_calls = 0;\n"
        for index, (left, right, dynamic) in enumerate(dimensions, 1):
            body += f"    chosen_dimension = {index};\n"
            body += range_checks(expression, "dimension()", left, right, dynamic)
            body += check("dimension_calls", 6*index, "dimension evaluated once per query")
            body += check("value_calls", 6 if dimensions[0][2] else 0, "receiver evaluation count")
        body += f"    chosen_dimension = {len(dimensions)+1};\n"
        body += "".join(check(f"${q}({expression}, dimension())", "32'bx", "invalid receiver query")
                        for q in QUERIES)
        body += check("dimension_calls", 6*(len(dimensions)+1), "invalid dimension evaluated once")
        body += check("value_calls", 6 if dimensions[0][2] else 0, "invalid receiver unevaluated")
        body += counts(expression, len(dimensions), 1 if len(dimensions) == 2 else 0)
        body += check("value_calls", 6 if dimensions[0][2] else 0, "count query receiver unevaluated")
        cases.append(Case(name, module(name, declarations, body)))

    name = "fixed_subarray_receiver_unevaluated"
    declarations = "  logic [6:3] values[4:2][-3:-1];\n  int dim, calls;\n" + '''  function automatic int selected(); calls++; return 4; endfunction
'''
    body = "    dim = 1; calls = 0;\n" + range_checks("values[selected()]", "dim", -3, -1)
    body += "    dim = 2;\n" + range_checks("values[selected()]", "dim", 6, 3)
    body += counts("values[selected()]", 2, 1) + check("calls", 0, "fixed selector unevaluated")
    cases.append(Case(name, module(name, declarations, body)))

    name = "dynamic_subarray_receiver_once"
    declarations = "  typedef logic [6:3] row_t[];\n  row_t values[4:2];\n  int dim, calls;\n" + '''  function automatic int selected(); calls++; return 3; endfunction
'''
    body = "    values[3] = new[4]; dim = 1; calls = 0;\n"
    body += range_checks("values[selected()]", "dim", 0, 3, True)
    body += check("calls", 6, "dynamic selector evaluated once")
    body += "    dim = 2;\n" + range_checks("values[selected()]", "dim", 6, 3)
    body += check("calls", 6, "packed selector unevaluated")
    cases.append(Case(name, module(name, declarations, body)))

    name = "dimension_sideeffect_on_receiver"
    declarations = "  logic [6:3] values[];\n  int calls;\n" + '''  function automatic int resize_and_choose();
    calls++; values = new[calls + 1]; return 1;
  endfunction
'''
    body = "    calls = 0; values = new[0];\n"
    body += check("$size(values, resize_and_choose())", 2, "size sees dimension side effect")
    body += check("calls", 1, "resize dimension once")
    body += check("$right(values, resize_and_choose())", 2, "right sees dimension side effect")
    body += check("calls", 2, "second resize dimension once")
    cases.append(Case(name, module(name, declarations, body)))
    return cases


def home_cases():
    cases = []
    declarations = "  logic [5:1] values[2:-1];\n  int dim;\n"
    body = "    dim = 1;\n" + range_checks("values", "dim", 2, -1)
    body += "    dim = 2;\n" + range_checks("values", "dim", 5, 1) + counts("values", 2, 1)
    for home in ("generate", "interface", "package", "unit", "automatic", "ref", "program"):
        name = "home_" + home
        printed = f'    $display("{name} passed");\n'
        if home == "generate":
            source = "module top; if (1) begin : nested\n" + declarations + "  initial begin\n" + body + printed + "  end end endmodule\n"
        elif home == "interface":
            source = "interface storage;\n" + declarations + "  initial begin\n" + body + printed + "  end endinterface\nmodule top; storage bus(); endmodule\n"
        elif home == "package":
            source = module(name, "  int dim;\n", re.sub(r"\bvalues\b", "pool::values", body),
                            "package pool; timeunit 1ns; timeprecision 1ps; logic [5:1] values[2:-1]; endpackage\n")
        elif home == "unit":
            source = module(name, "  int dim;\n", body, "logic [5:1] values[2:-1];\n")
        elif home in ("automatic", "ref"):
            args = "ref logic [5:1] values[2:-1]" if home == "ref" else ""
            local = "  int dim;\n" if home == "ref" else declarations
            top = "  logic [5:1] actual[2:-1];\n" if home == "ref" else ""
            source = ("module top; timeunit 1ns; timeprecision 1ps;\n" + top
                      + f"  task automatic inspect({args});\n" + local + body + "    #1;\n"
                      + body + printed + "  endtask\n  initial begin inspect(" + ("actual" if home == "ref" else "")
                      + "); inspect(" + ("actual" if home == "ref" else "") + "); end\nendmodule\n")
        else:
            source = ("program worker;\n" + declarations + "  initial begin\n" + body
                      + printed + "  end endprogram\nmodule top; worker process_instance(); endmodule\n")
        cases.append(Case(name, source, 2 if home in ("automatic", "ref") else 1))

    name = "dynamic_ref_wait"
    declarations = '''  logic [8:4] values[];
  task automatic inspect(ref logic [8:4] receiver[]);
    int dim;
    dim = 1;
''' + range_checks("receiver", "dim", 0, 2, True) + "    #2;\n" + range_checks("receiver", "dim", 0, 4, True) + '''    dim = 2;
''' + range_checks("receiver", "dim", 8, 4) + "  endtask\n"
    body = '''    values = new[3];
    fork
      inspect(values);
      begin #1; values = new[5]; end
    join
'''
    cases.append(Case(name, module(name, declarations, body)))
    name = "hierarchical_nested_dimension_counts"
    prefix = '''module storage;
  timeunit 1ns; timeprecision 1ps;
  logic [7:3] values[][];
endmodule
'''
    body = counts("holder.values", 3, 2)
    body += check("$size(holder.values, 3)", 5, "hierarchical nested packed dimension")
    cases.append(Case(name, module(name, "  storage holder();\n", body, prefix)))
    return cases


def cases():
    scalar_counts = Case("scalar_dimension_counts", module("scalar_dimension_counts",
        "  logic scalar;\n  logic [0:0] vector;\n",
        counts("scalar", 0, 0) + counts("vector", 1, 0)))
    return (fixed_cases() + dynamic_cases() + string_cases() + invalid_runtime_cases()
            + evaluation_cases() + home_cases() + [scalar_counts]
            + [case for case, _ in conditional_cases()])


def conditional_cases():
    cases = []
    # Casts make these true strings. Bare literal branches instead produce a
    # packed integral vector whose type query must not evaluate its condition.
    for label, dimension, expected, checks in (("one", "1", "0", 1),
                                              ("zero", "0", "32'bx", 0),
                                              ("two", "2", "32'bx", 0),
                                              ("unknown", "32'bx", "32'bx", 0)):
        name = "conditional_string_" + label
        body = f"    dim = {dimension};\n" + check(
            '$size(1\'bx ? string\'("abcd") : string\'("wxyz"), dim)', expected,
            "conditional string query")
        cases.append((Case(name, module(name, "  integer dim;\n", body)), checks))
    name = "conditional_equal_strings"
    body = "    dim = 1;\n" + check(
        '$size(1\'bx ? string\'("abcd") : string\'("abcd"), dim)', 4,
        "equal conditional string query")
    cases.append((Case(name, module(name, "  int dim;\n", body)), 1))
    name = "conditional_packed_string_literals"
    body = "    dim = 1;\n" + check('$size(1\'bx ? "abcd" : "wxyz", dim)', 32,
                                        "literal branches have packed integral type")
    cases.append((Case(name, module(name, "  int dim;\n", body)), 0))
    name = "conditional_fixed_receiver"
    body = "    dim = 1;\n" + check("$size(1'bx ? 8'h00 : 8'hff, dim)", 8,
                                        "fixed conditional receiver unevaluated")
    cases.append((Case(name, module(name, "  int dim;\n", body)), 0))
    for kind, declaration, setup in (("fixed", "logic [7:3] values;", ""),
                                      ("dynamic", "logic [7:3] values[];", "    values = new[5];\n")):
        name = "conditional_dimension_" + kind
        body = setup + check("$size(values, (1'bx ? 1 : 1))", 5,
                             "conditional dimension evaluated once")
        cases.append((Case(name, module(name, "  " + declaration + "\n", body)), 1))
    return cases


def rejected_cases():
    # Strict slang acceptance stays in place for statically invalid dimensions.
    for label, dimension in (("zero", "0"), ("negative", "-1"), ("excess", "3"),
                             ("unknown", "32'bx"), ("wide", "129'h10000000000000001")):
        yield ("constant_" + label,
               "module top; logic [7:3] values[2:-1]; initial $display(\"%0d\", $size(values, "
               + dimension + ")); endmodule\n", "is not a valid dimension", True)
    yield ("nested_dynamic_constant", "module top; int values[][]; initial $display(\"%0d\", $size(values, 2)); endmodule\n",
           "dynamically sized dimension", True)
    yield ("nested_dynamic_runtime", "module top; int values[][]; int dim; initial begin dim=1; $display(\"%0d\", $size(values, dim)); end endmodule\n",
           "unsupported:", False)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--kirimasim", type=Path, required=True)
    binary = str(parser.parse_args().kirimasim.resolve())
    failures = []
    executions = parses = dumps = rejections = diagnostics = 0
    with tempfile.TemporaryDirectory(prefix="kirimasim-runtime-query-") as directory:
        root = Path(directory)

        def run(path, *options):
            return bounded_run.run([binary, *options, str(path)], cwd=root, timeout=15)

        def fail(label, result):
            failures.append(f"{label}: exit={result.returncode}\n{result.stdout}\n{result.stderr}")

        for case in cases():
            path = root / (case.name + ".sv")
            path.write_text(case.source)
            result = run(path, "parse")
            if result.returncode or result.stdout or ": error:" in result.stderr:
                fail(case.name + " parse", result)
            parses += 1
            result = run(path, "--dump-sir")
            if result.returncode or not result.stdout or ": error:" in result.stderr:
                fail(case.name + " SIR", result)
            dumps += 1
            for flags in ((), ("--init-values=random", "--seed=23")):
                result = run(path, "--max-steps=200000", *flags)
                if (result.returncode or result.stdout != case.stdout or
                        re.search(r": (?:error|fatal|warning):", result.stderr)):
                    fail(case.name + " run " + repr(flags), result)
                executions += 1

        for name, source, diagnostic, parse_error in rejected_cases():
            path = root / (name + ".sv")
            path.write_text(source)
            commands = (("parse",), (), ("--dump-sir",)) if parse_error else ((), ("--dump-sir",))
            for command in commands:
                result = run(path, *command)
                if (result.returncode != 2 or result.stdout or diagnostic not in result.stderr or
                        not re.search(re.escape(path.name) + r":\d+:\d+: error:", result.stderr) or
                        "internal error" in result.stderr):
                    fail(name + " " + repr(command), result)
                rejections += 1

        for case, checks in conditional_cases():
            path = root / (case.name + ".sv")
            result = run(path, "--cond-x=warn")
            summary = re.findall(r"condition X check: (\d+) evaluations at (\d+) source positions",
                                 result.stderr)
            warnings = re.findall(r": warning: .*contains X/Z bits \(--cond-x=warn\)", result.stderr)
            if (result.returncode or result.stdout != case.stdout or len(warnings) != checks or
                    summary != ([(str(checks), "1")] if checks else []) or
                    re.search(r": (?:error|fatal):", result.stderr)):
                fail(case.name + " condition evaluation", result)
            diagnostics += 1
    print(f"runtime-array-query: {executions} execution, {parses} parse, {dumps} SIR, "
          f"{rejections} rejection, {diagnostics} condition-diagnostic checks; {len(failures)} failures")
    for failure in failures:
        print(failure)
    return bool(failures)


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (bounded_run.OutputLimit, subprocess.TimeoutExpired) as error:
        print(f"FAIL: {error}", file=sys.stderr)
        raise SystemExit(1)
