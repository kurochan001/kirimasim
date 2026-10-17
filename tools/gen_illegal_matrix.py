#!/usr/bin/env python3
"""Generates the tests of illegal designs of TEST_SUITE.md ("失敗を期待する
テスト") in tests/suite/illegal/.

  tools/gen_illegal_matrix.py           # writes the files
  tools/gen_illegal_matrix.py --check   # exit 1 if a file differs from what it would write

For each feature kirimasim runs, at least one form that the standard makes
illegal (TEST_SUITE.md, P1 のあいだに足すもの 4): `kirimasim parse` must
exit with code 2 and report the error at the position of the illegal
construct. kirimasim leaves these checks to slang and never relaxes them
(CLAUDE.md, 違法な入力は既定で拒否する); the expected text is slang's.
One file per form, so that one error cannot hide another. The clause
numbers were checked against the standard; each description says why the
form is illegal, in our own words.
"""

import argparse
import sys
from pathlib import Path

OUT = Path(__file__).resolve().parent.parent / "tests" / "suite" / "illegal"
# The version of the rules below (TEST_SUITE.md, :generator:). Raise it
# whenever they change.
VERSION = 1

# (name, tags, clauses, why it is illegal, source, (line, column) of the
# error in the source, slang's message)
CASES = [
    ("function_delay", "function", "13.4",
     "a function contains a delay control",
     "module top;\n  function int f(int a);\n    #1 return a;\n  endfunction\nendmodule",
     (3, 5), "statements that pass time are not allowed in this context"),
    ("function_event_control", "function", "13.4",
     "a function waits for an event",
     "module top;\n  event e;\n  function void f();\n    @e;\n  endfunction\nendmodule",
     (4, 5), "statements that pass time are not allowed in this context"),
    ("function_fork_join", "function fork", "13.4 9.3.2",
     "a function contains a fork-join, which waits for its children",
     "module top;\n  function void f();\n    fork\n    join\n  endfunction\nendmodule",
     (4, 5), "statements that pass time are not allowed in this context"),
    ("function_wait_fork", "function fork", "13.4 9.6.1",
     "a function contains wait fork",
     "module top;\n  function void f();\n    wait fork;\n  endfunction\nendmodule",
     (3, 5), "statements that pass time are not allowed in this context"),
    ("function_calls_task", "function task", "13.4",
     "a function enables a task",
     "module top;\n  task t(); endtask\n  function void f();\n    t();\n  endfunction\nendmodule",
     (4, 5), "cannot invoke a task from within a function"),
    ("function_output_in_continuous_assign", "function", "13.4",
     "a function with an output argument is called in a continuous assignment",
     "module top;\n  function int f(output int o); o = 1; return 0; endfunction\n"
     "  int x, y;\n  assign x = f(y);\nendmodule",
     (4, 14), "cannot call a function with output, inout, or ref arguments outside of a "
     "procedural context"),
    ("task_return_value", "task", "12.8",
     "a return statement in a task has a value",
     "module top;\n  task t();\n    return 1;\n  endtask\nendmodule",
     (3, 5), "value of type 'int' cannot be assigned to type 'void'"),
    ("ref_in_static_task", "task ref", "13.5.2",
     "a static task has a ref argument",
     "module top;\n  task t(ref int x);\n  endtask\nendmodule",
     (2, 10), "'ref' arguments can only be used in 'automatic' subroutines"),
    ("return_in_initial", "procedure", "12.8",
     "a return statement is outside any subroutine",
     "module top;\n  initial return;\nendmodule",
     (2, 11), "return statement is only valid inside task and function blocks"),
    ("break_outside_loop", "procedure loop", "12.8",
     "a break statement is outside any loop",
     "module top;\n  initial break;\nendmodule",
     (2, 11), "statement can only be used inside a loop"),
    ("disable_function", "disable function", "9.6.2",
     "a disable statement names a function",
     "module top;\n  function void f(); endfunction\n  initial disable f;\nendmodule",
     (3, 19), "disable statement must specify a task or block name"),
    ("always_comb_delay", "procedure", "9.2.2.2.1",
     "an always_comb procedure contains a delay control",
     "module top;\n  logic a;\n  always_comb begin\n    #1 a = 1;\n  end\nendmodule",
     (4, 5), "statements that pass time are not allowed in this context"),
    ("always_ff_two_event_controls", "procedure", "9.2.2.4",
     "an always_ff procedure has a second event control",
     "module top;\n  logic clk, q;\n  always_ff @(posedge clk) begin\n    q <= 1;\n"
     "    @(posedge clk) q <= 0;\n  end\nendmodule",
     (5, 7), "always_ff procedure must have one and only one event control"),
    ("procedural_assign_to_net", "nets", "6.5",
     "a procedure assigns a net",
     "module top;\n  wire w;\n  initial w = 1;\nendmodule",
     (3, 11), "cannot assign to a net within a procedural context"),
    ("procedural_continuous_assign_to_net", "assign", "10.6.1",
     "assign (procedural continuous) names a net",
     "module top;\n  wire w;\n  initial assign w = 1;\nendmodule",
     (3, 18), "lvalue of procedural assign/deassign must be a variable or concatenation of "
     "variables"),
    ("force_variable_bit", "force", "10.6.2",
     "force names a bit-select of a variable",
     "module top;\n  logic [3:0] v;\n  initial force v[1] = 1'b1;\nendmodule",
     (3, 17), "lvalue of force/release must be a net, a variable, a constant select of a net, "
     "or a concatenation of these"),
    ("strength_both_highz", "nets strength", "10.3.4",
     "a continuous assignment gives both strengths highz",
     "module top;\n  wire w;\n  assign (highz0, highz1) w = 1;\nendmodule",
     (3, 19), "combination of drive strengths is invalid; cannot combine 'highz0' and "
     "'highz1'"),
    ("default_nettype_none", "nets directive", "22.8 6.10",
     "an undeclared name is assigned under `default_nettype none",
     "`default_nettype none\nmodule top;\n  assign w = 1;\nendmodule",
     (3, 10), "use of undeclared identifier 'w'"),
    ("redeclaration", "declaration", "6.5",
     "a name is declared twice in one scope",
     "module top;\n  int a;\n  int a;\nendmodule",
     (3, 7), "redefinition of 'a'"),
    ("const_assigned", "declaration", "6.20.6",
     "a const variable is assigned after its declaration",
     "module top;\n  const int c = 1;\n  initial c = 2;\nendmodule",
     (3, 13), "cannot assign to read-only variable 'c'"),
    ("parameter_not_constant", "parameter", "6.20.1",
     "a parameter's value reads a variable",
     "module top;\n  int v;\n  parameter int P = v;\nendmodule",
     (3, 21), "reference to non-constant variable 'v' is not allowed in a constant expression"),
    ("defparam_of_localparam", "parameter", "6.20.4",
     "defparam names a local parameter",
     "module child;\n  localparam int L = 1;\nendmodule\nmodule top;\n  child c();\n"
     "  defparam c.L = 2;\nendmodule",
     (6, 12), "'L' is a localparam and so cannot be the target of a defparam"),
    ("genvar_in_procedure", "generate", "27.4",
     "a procedure assigns a genvar, which exists only during elaboration",
     "module top;\n  genvar g;\n  initial g = 1;\nendmodule",
     (3, 11), "'g' cannot be used in an expression"),
    ("enum_from_integer", "enum", "6.19.3",
     "an integer is assigned to an enumeration variable without a cast",
     "module top;\n  typedef enum {A, B} e_t;\n  e_t v;\n  initial v = 1;\nendmodule",
     (4, 13), "no implicit conversion from 'int' to 'e_t'"),
    ("enum_duplicate_value", "enum", "6.19",
     "two names of an enumeration have the same value",
     "module top;\n  typedef enum {A = 1, B = 1} e_t;\nendmodule",
     (2, 28), "redefinition of enum value 1"),
    ("packed_struct_unpacked_member", "struct", "7.2.1",
     "a packed structure has an unpacked array member",
     "module top;\n  typedef struct packed { int a[2]; } s_t;\nendmodule",
     (2, 31), "packed members must be of integral type (not 'int$[2]')"),
    ("packed_struct_string_member", "struct string", "7.2.1",
     "a packed structure has a string member",
     "module top;\n  typedef struct packed { string s; } s_t;\nendmodule",
     (2, 27), "packed members must be of integral type (not 'string')"),
    ("pattern_too_few", "array", "10.9.1",
     "an assignment pattern has fewer items than the array has elements",
     "module top;\n  int a[3];\n  initial a = '{1, 2};\nendmodule",
     (3, 15), "assignment pattern for 'int$[3]' requires 3 elements but 2 were provided"),
    ("bit_select_of_real", "real", "6.12",
     "a bit of a real variable is selected",
     "module top;\n  real r;\n  initial r[0] = 1;\nendmodule",
     (3, 13), "value of type 'real' cannot be indexed"),
    ("stream_into_narrower", "stream", "11.4.14",
     "a stream is unpacked into a target with fewer bits",
     "module top;\n  logic [7:0] a;\n  logic [15:0] b;\n  initial a = {<<{b}};\nendmodule",
     (4, 15), "streaming operator target size 8 does not fit source size 16"),
    ("trigger_of_non_event", "event", "15.5.1",
     "-> names a variable that is not an event",
     "module top;\n  int v;\n  initial -> v;\nendmodule",
     (3, 14), "expression is not a triggerable event"),
    ("deferred_action_not_a_call", "assertion", "16.4",
     "the action of a deferred assertion is not a subroutine call",
     "module top;\n  int v;\n  initial assert #0 (v == 0) else v = 1;\nendmodule",
     (3, 35), "deferred assertion action must be a subroutine call"),
    ("modport_input_written", "interface", "25.5 23.3.3.2",
     "a module writes a modport input, which reads as an input port",
     "interface bus;\n  logic d;\n  modport m(input d);\nendinterface\nmodule user(bus.m b);\n"
     "  initial b.d = 1;\nendmodule\nmodule top;\n  bus i();\n  user u(i);\nendmodule",
     (6, 15), "cannot assign to input port 'd'"),
    ("package_missing_member", "package", "26.3",
     "a package reference names an item the package does not declare",
     "package p;\n  int a;\nendpackage\nmodule top;\n  initial p::b = 1;\nendmodule",
     (5, 12), "no member named 'b' in package 'p'"),
    ("udp_inout_port", "udp", "29.3.1",
     "a user-defined primitive has a bidirectional port",
     "primitive p(output o, inout i);\n  table\n    0 : 0;\n  endtable\nendprimitive\n"
     "module top;\nendmodule",
     (1, 22), "expected 'input'"),
]


def files():
    out = {}
    for name, tags, clauses, why, src, (line, col), message in CASES:
        head = [
            "/*",
            f":name: illegal_{name}",
            f":description: illegal: {why}",
            f":tags: illegal {tags}",
            ":type: elaboration",
            f":lrm: 1800-2023 {clauses}",
            ":oracle: spec",
            ":spec_check: ai",
            ":fourstate: no",
            ":nondeterministic: no",
            f":generator: gen_illegal_matrix.py@{VERSION}",
            f":should_fail_because: {why}",
            None,  # :expected_error:, filled in below
            "*/",
            "// Generated by tools/gen_illegal_matrix.py; change the generator, not this file.",
        ]
        shift = len(head)
        head[12] = f":expected_error: {line + shift}:{col}: error: {message}"
        out[f"{name}.sv"] = "\n".join(head) + "\n" + src + "\n"
    return out


def main():
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("--check", action="store_true")
    args = ap.parse_args()
    bad = 0
    want = files()
    if args.check:
        have = {p.name for p in OUT.glob("*.sv")} if OUT.exists() else set()
        for name in sorted(have - set(want)):
            print(f"gen_illegal_matrix: tests/suite/illegal/{name} is not made by the generator",
                  file=sys.stderr)
            bad += 1
    else:
        OUT.mkdir(exist_ok=True)
    for name, text in want.items():
        path = OUT / name
        if args.check:
            if not path.exists() or path.read_text(encoding="utf-8") != text:
                print(f"gen_illegal_matrix: tests/suite/illegal/{name} is out of date; run "
                      "tools/gen_illegal_matrix.py", file=sys.stderr)
                bad += 1
        else:
            path.write_text(text, encoding="utf-8")
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main())
