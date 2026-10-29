#!/usr/bin/env python3
# Copyright (c) 2026 The kirimasim Authors
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

"""Check mixed foreach traversal against independent explicit for loops."""

import argparse
from dataclasses import dataclass
from pathlib import Path
import re
import subprocess
import sys
import tempfile

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / "tools"))
import bounded_run  # noqa: E402


ENCODE = '''  function automatic longint encode(input int a, b, c, d);
    return (longint'(a) + 256) * 1000000000000 +
           (longint'(b) + 256) * 100000000 +
           (longint'(c) + 256) * 10000 + longint'(d) + 256;
  endfunction
'''


@dataclass
class Case:
    name: str
    source: str
    repeats: int = 1

    @property
    def stdout(self):
        return (self.name + " passed\n") * self.repeats


def module(name, declarations, body, prefix=""):
    return (prefix + "module top;\n  timeunit 1ns; timeprecision 1ps;\n" + ENCODE
            + declarations + "  initial begin\n" + body
            + f'    $display("{name} passed");\n  end\nendmodule\n')


def code(indices):
    return "encode(" + ", ".join([v or "0" for v in indices] + ["0"] * (4 - len(indices))) + ")"


def check_visits(message):
    return (f'    if (observed.size() != expected.size()) $fatal(1, "{message} size");\n'
            '    for (int check_index = 0; check_index < expected.size(); check_index++)\n'
            f'      if (observed[check_index] !== expected[check_index]) $fatal(1, "{message}");\n')


def explicit_loops(indices, dimensions, body):
    # These loops use declared bounds or the explicitly constructed row sizes.
    # Longint induction avoids reproducing a foreach int terminal-step wrap.
    result = ""
    for index, (left, right, dynamic) in zip(indices, dimensions):
        if index is None:
            continue
        if dynamic:
            result += f"    for (longint {index} = 0; {index} < {right}; {index}++)\n"
        else:
            step = "++" if left <= right else "--"
            compare = "<=" if left <= right else ">="
            start = f"64'sd{left}" if left >= 0 else f"-64'sd{-left}"
            end = f"64'sd{right}" if right >= 0 else f"-64'sd{-right}"
            result += f"    for (longint {index} = {start}; {index} {compare} {end}; {index}{step})\n"
    return result + "    begin\n" + body + "    end\n"


def traversal(name, queue=False, packed=((128, 0),), fixed=None, omitted=()):
    packed_type = "logic " + "".join(f"[{left}:{right}]" for left, right in packed)
    dynamic = "[$]" if queue else "[]"
    names = ["i", "j", "k", "l"][:len(packed) + (2 if fixed else 1)]
    names = [None if k in omitted else v for k, v in enumerate(names)]
    expected_names = [None if v is None else "e" + v for v in names]
    if fixed:
        first, second = fixed
        declarations = f"  typedef {packed_type} row_t{dynamic};\n  row_t values[{first}:{second}];\n"
        if queue:
            setup = f"    values[{first}] = '{{'1}}; values[{second}] = '{{'0, '1}};\n"
        else:
            setup = (f"    values[{first}] = new[1]; values[{second}] = new[2];\n"
                     f"    values[{first}][0] = '1; values[{second}][0] = '0;\n"
                     f"    values[{second}][1] = '1;\n")
        dimensions = [(first, second, False), (0, f"(ei == {first} ? 1 : 2)", True)]
    else:
        declarations = f"  {packed_type} values{dynamic};\n"
        setup = "    values = '{'1, '0};\n" if queue else (
            "    values = new[2]; values[0] = '1; values[1] = '0;\n")
        dimensions = [(0, "2", True)]
    dimensions += [(left, right, False) for left, right in packed]
    declarations += "  longint observed[$], expected[$];\n"
    loop = ",".join(v or "" for v in names)
    body = setup + f"    foreach (values[{loop}]) observed.push_back({code(names)});\n"
    body += explicit_loops(expected_names, dimensions,
                           f"      expected.push_back({code(expected_names)});\n")
    body += check_visits("foreach index sequence differs from for")
    return Case(name, module(name, declarations, body))


def home_cases():
    declaration = "  logic [128:0] values[];\n  longint observed[$], expected[$];\n"
    body = '''    values = new[2]; values[0] = '1; values[1] = '0;
    foreach (values[i,j]) begin
      observed.push_back(encode(i,j,0,0));
      if (values[i][j] !== (i == 0)) $fatal(1, "mixed element read");
    end
    for (int i = 0; i < 2; i++)
      for (int j = 128; j >= 0; j--) expected.push_back(encode(i,j,0,0));
''' + check_visits("home index sequence")
    cases = []
    for home in ("generate", "interface", "package", "unit", "static_task", "automatic"):
        name = "home_" + home
        printed = f'    $display("{name} passed");\n'
        if home == "generate":
            text = ("module top;\n" + ENCODE + "  if (1) begin : nested\n" + declaration
                    + "  initial begin\n" + body + printed + "  end\n  end\nendmodule\n")
        elif home == "interface":
            text = ("interface storage;\n" + ENCODE + declaration + "  initial begin\n"
                    + body + printed + "  end\nendinterface\nmodule top; storage bus(); endmodule\n")
        elif home in ("package", "unit"):
            prefix = ("package pool;\n  timeunit 1ns; timeprecision 1ps;\n" + declaration
                      + "endpackage\n" if home == "package"
                      else declaration)
            qualified = body
            if home == "package":
                for symbol in ("values", "observed", "expected"):
                    qualified = re.sub(r"\b" + symbol + r"\b", "pool::" + symbol, qualified)
            text = module(name, "", qualified, prefix)
        else:
            lifetime = "automatic" if home == "automatic" else "static"
            # Explicit reset keeps the static helper's second call independent.
            reset = "    observed.delete(); expected.delete();\n"
            text = ("module top;\n" + ENCODE + f"  task {lifetime} scan();\n" + declaration
                    + reset + body + printed + "  endtask\n  initial begin scan(); scan(); end\nendmodule\n")
        cases.append(Case(name, text, 2 if home in ("static_task", "automatic") else 1))

    cases.append(Case("home_program", '''program worker;
  timeunit 1ns; timeprecision 1ps;
  logic [128:0] values[];
  int count;
  initial begin
    values = new[2];
    foreach (values[i,j]) begin
      if (i != count / 129 || j != 128 - count % 129) $fatal(1, "program order");
      count++;
    end
    if (count != 258) $fatal(1, "program count");
    $display("home_program passed");
  end
endprogram
module top;
  timeunit 1ns; timeprecision 1ps;
  worker test();
endmodule
'''))
    for kind, suffix in (("dynamic", "[]"), ("queue", "[$]")):
        name = "home_ref_" + kind
        declarations = (f"  logic [128:0] values{suffix};\n"
                        f"  task automatic scan(ref logic [128:0] value{suffix});\n"
                        "    int count;\n"
                        "    foreach (value[i,j]) begin\n"
                        '      if (i != count / 129 || j != 128 - count % 129) $fatal(1, "ref order");\n'
                        "      count++; value[i][j] = 1'b1;\n"
                        "    end\n"
                        '    if (count != 258) $fatal(1, "ref count");\n'
                        "  endtask\n")
        setup = "    values = new[2];\n" if kind == "dynamic" else "    values = '{'0, '0};\n"
        cases.append(Case(name, module(name, declarations, setup + '''    scan(values);
    if (values[0] !== '1 || values[1] !== '1) $fatal(1, "ref mixed write");
    scan(values);
''')))
    cases.append(Case("home_struct", module("home_struct", '''  typedef struct {
    logic [128:0] values[];
  } record_t;
  record_t record_value;
  int count;
''', '''    record_value.values = new[2];
    foreach (record_value.values[i,j]) begin
      if (i != count / 129 || j != 128 - count % 129) $fatal(1, "member order");
      count++;
    end
    if (count != 258) $fatal(1, "member count");
''')))
    return cases


def control_cases():
    return [
        Case("empty_omitted", module("empty_omitted", '''  logic [1:0][2:0] values[];
  logic [128:0] queue[$];
  typedef logic [128:0] row_t[];
  row_t rows[2];
  int count;
''', '''    foreach (values[i,j,k]) count++;
    foreach (queue[i,j]) count++;
    foreach (rows[i,j,k]) count++;
    if (count != 0) $fatal(1, "empty mixed traversal");
    values = new[2];
    foreach (values[,,]) begin #1; $fatal(1, "all omitted dynamic loop ran"); end
    foreach (queue[,]) begin #1; $fatal(1, "all omitted queue loop ran"); end
    foreach (rows[,,]) begin #1; $fatal(1, "all omitted fixed dynamic loop ran"); end
    if ($time != 0) $fatal(1, "omitted loop consumed time");
''')),
        Case("empty_omitted_outer", module("empty_omitted_outer", '''  logic [128:0] values[];
  logic [-128:0] queue[$];
  longint observed[$], expected[$];
''', '''    values = new[0];
    foreach (values[,j]) observed.push_back(longint'(j));
    for (longint j = 128; j >= 0; j--) expected.push_back(j);
    if (observed.size() != 129) $fatal(1, "empty dynamic outer suppressed fixed dimension");
''' + check_visits("empty dynamic outer omitted") + '''    observed.delete(); expected.delete();
    foreach (queue[,j]) observed.push_back(longint'(j));
    for (longint j = -128; j <= 0; j++) expected.push_back(j);
    if (observed.size() != 129) $fatal(1, "empty queue outer suppressed fixed dimension");
''' + check_visits("empty queue outer omitted"))),
        Case("break_continue", module("break_continue", '''  logic [2:0] values[];
  int grid[2][3];
  int count, continued, inner_count, outer_count;
''', '''    values = new[2];
    foreach (values[i,j]) begin count++; break; end
    if (count != 1) $fatal(1, "break did not leave the whole foreach");
    foreach (values[i,j]) begin continued++; continue; $fatal(1, "continue fell through"); end
    if (continued != 6) $fatal(1, "continue skipped a dimension");
    for (int outer = 0; outer < 2; outer++)
      foreach (values[i,j]) begin
        for (int inner = 0; inner < 3; inner++) begin inner_count++; break; end
        outer_count++;
        break;
      end
    if (inner_count != 2 || outer_count != 2) $fatal(1, "nested for break owner");
    count = 0;
    foreach (values[i,j]) foreach (grid[a,b]) begin count++; break; end
    if (count != 6) $fatal(1, "nested fixed foreach break owner");
    count = 0;
    foreach (grid[a,b]) foreach (values[i,j]) begin count++; break; end
    if (count != 6) $fatal(1, "nested mixed foreach break owner");
''')),
        Case("omitted_break", module("omitted_break", '''  logic [1:0][2:0] values[];
  int count, continued;
''', '''    values = new[2];
    for (int outer = 0; outer < 2; outer++)
      foreach (values[,j,k]) begin count++; break; end
    if (count != 2) $fatal(1, "omitted outer break owner");
    foreach (values[i,,k]) begin continued++; continue; $fatal(1, "omitted continue"); end
    if (continued != 6) $fatal(1, "omitted middle continue");
''')),
        Case("wait_lifetime", module("wait_lifetime", '''  task automatic scan();
    logic [2:0] values[];
    longint observed[$], expected[$];
    values = new[2];
    foreach (values[i,j]) begin
      automatic int row = i, bit_index = j;
      #1;
      if (i != row || j != bit_index) $fatal(1, "loop indices lost across wait");
      observed.push_back(encode(i,j,0,0));
    end
    for (int i = 0; i < 2; i++)
      for (int j = 2; j >= 0; j--) expected.push_back(encode(i,j,0,0));
''' + check_visits("waiting loop order") + '''  endtask
''', '''    scan(); scan();
    if ($time != 12) $fatal(1, "waiting loop lifetime");
''')),
        Case("disable_return", module("disable_return", '''  logic [2:0] values[];
  int count;
  function automatic int first_index(ref logic [2:0] value[]);
    foreach (value[i,j]) return i * 10 + j;
    return -1;
  endfunction
''', '''    values = new[2];
    begin : whole_loop
      foreach (values[i,j]) begin
        #1; count++;
        if (count == 2) disable whole_loop;
      end
    end
    if (count != 2 || $time != 2) $fatal(1, "disable mixed loop");
    if (first_index(values) != 2 || first_index(values) != 2)
      $fatal(1, "function return from mixed loop");
    values.delete();
    if (first_index(values) != -1) $fatal(1, "empty function mixed loop");
''')),
        Case("shadow_capture", module("shadow_capture", '''  logic [2:0] values[];
  int i = 99, j = 77;
  int children, child_rows, child_bits;
  task automatic scan();
    foreach (values[i,j]) begin
      automatic int row = i, bit_index = j;
      fork
        begin
          #1;
          if (row < 0 || row > 1 || bit_index < 0 || bit_index > 2)
            $fatal(1, "captured loop-body lifetime");
          children++;
          child_rows += row; child_bits += bit_index;
        end
      join_none
    end
  endtask
''', '''    values = new[2];
    scan();
    #2;
    if (i != 99 || j != 77 || children != 6 || child_rows != 3 || child_bits != 6)
      $fatal(1, "loop scope or child lifetime");
''')),
    ]


def cases():
    result = []
    shapes = (((128, 0),), ((-128, 0),), ((0, -128),), ((1, 0), (128, 0)))
    for queue in (False, True):
        kind = "queue" if queue else "dynamic"
        for index, shape in enumerate(shapes):
            result.append(traversal(f"{kind}_packed_{index}", queue, shape))
        for bound in ((3, 2), (-2, -1)):
            for index, shape in enumerate((shapes[0], shapes[-1])):
                tag = "down" if bound[0] > bound[1] else "up"
                result.append(traversal(f"fixed_{kind}_{tag}_{index}", queue, shape, bound))
        for index, bound in enumerate(((2147483646, 2147483647),
                                       (-2147483647, -2147483648))):
            result.append(traversal(f"{kind}_terminal_{index}", queue, (bound,)))
    result += [traversal("omit_packed_middle", packed=((1, 0), (128, 0)), omitted=(1,)),
               traversal("omit_packed_trailing", packed=((1, 0), (128, 0)), omitted=(2,)),
               traversal("omit_dynamic_outer", omitted=(0,)),
               traversal("omit_fixed_packed_middle", packed=((1, 0), (128, 0)),
                         fixed=(3, 2), omitted=(2,)),
               traversal("omit_fixed_packed_trailing", packed=((1, 0), (128, 0)),
                         fixed=(-2, -1), omitted=(3,))]
    return result + home_cases() + control_cases()


def invalid_cases():
    return [
        ("omitted_dynamic_parent", '''module top;
  int values[][];
  initial foreach (values[,j]) $display("%0d", j);
endmodule
''', "previous dimension was omitted"),
        ("omitted_fixed_parent", '''module top;
  typedef int row_t[];
  row_t values[2];
  initial foreach (values[,j]) $display("%0d", j);
endmodule
''', "previous dimension was omitted"),
        ("loop_variable_scope", '''module top;
  logic [2:0] values[];
  initial begin
    values = new[2];
    foreach (values[i,j]) begin end
    $display("%0d", i);
  end
endmodule
''', "undeclared identifier 'i'"),
        ("too_many_dimensions", '''module top;
  logic [2:0] values[];
  initial foreach (values[i,j,k]) begin end
endmodule
''', "too many loop variables"),
    ]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--kirimasim", type=Path, required=True)
    binary = str(parser.parse_args().kirimasim.resolve())
    failures = []
    executions = parses = dumps = invalids = 0
    with tempfile.TemporaryDirectory(prefix="kirimasim-mixed-foreach-") as directory:
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
            for flags in ((), ("--init-values=random", "--seed=19")):
                result = run(path, "--max-steps=200000", *flags)
                if (result.returncode or result.stdout != case.stdout or
                        re.search(r": (?:error|fatal|warning):", result.stderr)):
                    fail(case.name + " run " + repr(flags), result)
                executions += 1

        for name, text, diagnostic in invalid_cases():
            path = root / (name + ".sv")
            path.write_text(text)
            for command in (("parse",), (), ("--dump-sir",)):
                result = run(path, *command)
                if (result.returncode != 2 or result.stdout or diagnostic not in result.stderr or
                        not re.search(re.escape(path.name) + r":\d+:\d+: error:", result.stderr) or
                        "internal error" in result.stderr):
                    fail(name + " " + repr(command), result)
                invalids += 1
    print(f"mixed-foreach: {executions} execution, {parses} parse, {dumps} SIR, "
          f"{invalids} invalid-source checks; {len(failures)} failures")
    for failure in failures:
        print(failure)
    return bool(failures)


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (bounded_run.OutputLimit, subprocess.TimeoutExpired) as error:
        print(f"FAIL: {error}", file=sys.stderr)
        raise SystemExit(1)
