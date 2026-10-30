#!/usr/bin/env python3
# Copyright (c) 2026 The kirimasim Authors
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

"""Generates the matrix test of unsupported constructs of TEST_SUITE.md
("Mechanism matrix") in tests/cli/unsupported_matrix/.

  tools/gen_unsupported_matrix.py           # writes the files
  tools/gen_unsupported_matrix.py --check   # exit 1 if a file differs from what it would write

A construct that kirimasim does not implement yet is reported with its
position, and kirimasim ends with exit code 2 before anything runs; it is
never skipped or run in some other way (CLI.md 5). The diagnostics are
kirimasim's own, so they are checked here and not in tests/suite/
(TEST_SUITE.md 4).

One file per construct. The axis is the container (CLAUDE.md: the same
statement means the same wherever it runs): a statement or an expression
goes into an initial procedure, an always procedure, a final procedure, a
static task, an automatic task, a function, a branch of a fork and an
initial procedure in a generate block (the subroutines are called, each from
an initial procedure of its own); a construct with a timing control leaves
out the final procedure and the function. A module item goes into the top
module, a generate block and a module that the top instantiates. Ports,
arguments and the rest come once, as they are.

«» marks the position that an error line reports. tests/cli/run_cli_tests.py
runs each file and checks the exit code 2, an empty stdout and exactly
these lines (unsupported_matrix.stderr), in any order. Historical inputs in
unsupported_matrix.accepted instead require successful execution: silent,
or printing exactly an adjacent .expected file when there is one.
"""

from __future__ import annotations

import argparse
import re
import sys
from pathlib import Path

OUT = Path(__file__).resolve().parent.parent / "tests" / "cli"
NAME = "unsupported_matrix"
# The version of the source-generation rules below. Raise it when sources change.
VERSION = 19
# Historical inputs retained after their mechanisms become implemented.
# Acceptance, stdout and diagnostics are checked by the CLI runner.
ACCEPTED = {"nonblocking_delay_index", "member_run_time_bit", "event_named_iff", "intra_delay_array", "fork_declaration",
            "automatic_in_fork", "event_var_index", "switch_mos", "switch_bidirectional", "nonblocking_event",
            "argument_ref_struct", "argument_struct_array", "automatic_struct_array"}
# The standard output of accepted inputs that print (unsupported_matrix/<name>.expected).
EXPECTED = {"automatic_in_fork": "5\n"}

# The procedural containers: (name, text with {s}, takes a timing control).
PROC = [
    ("initial", "  initial begin\n{s}  end\n", True),
    ("always", "  always @(go) begin\n{s}  end\n", True),
    ("final", "  final begin\n{s}  end\n", False),
    ("task", "  task t_static;\n{s}  endtask\n  initial t_static();\n", True),
    ("auto", "  task automatic t_auto;\n{s}  endtask\n  initial t_auto();\n", True),
    ("function", "  function void f_call();\n{s}  endfunction\n  initial f_call();\n", False),
    ("fork", "  initial fork\n    begin\n  {s}    end\n  join\n", True),
    ("generate", "  if (1) begin : g\n    initial begin\n  {s}    end\n  end\n", True),
]

# (name, module declarations, statement, message, has a timing control);
# {c} in the statement is the container's name, for names that would clash.
STATEMENTS = [
    ("cond_matches", "  int x = 5, y;\n", "y = «»x matches 5 ? 1 : 0;",
     "conditional operator with a pattern", False),
    # Narrowed or cast, where slang v12.0 recorded a wrong constant (slang
    # issue #2029, fixed in the pinned commit).
    ("cond_matches_narrowed", "  int x = 5;\n  logic y1;\n", "y1 = «»x matches 5 ? 1 : 0;",
     "conditional operator with a pattern", False),
    ("cond_matches_cast", "  int x = 5, y;\n", "y = 1'(«»x matches 5 ? 1 : 0);",
     "conditional operator with a pattern", False),
    # A wildcard, which slang v12.0 folded to the true side, and the &&& form
    # (slang issue #2029).
    ("cond_matches_wildcard", "  int x = 5;\n  logic y1;\n", "y1 = «»x matches .* ? 0 : 2;",
     "conditional operator with a pattern", False),
    ("cond_matches_and", "  int x = 5;\n  logic y1;\n", "y1 = «»x matches 5 &&& x == 5 ? 0 : 2;",
     "conditional operator with a pattern", False),
    ("if_matches", "  int x = 5, y;\n", "«»if (x matches 5) y = 1;", "if with a pattern", False),
    ("display_p", "  logic [3:0] v = 4'h5;\n", "$display(«»\"%p\", v);", "format specifier %p",
     False),
    ("display_d_string", "  string s = \"ab\";\n", "$display(\"%d\", «»s);", "%d of a string",
     False),
    ("system_function", "  int y;\n", "y = «»$system(\"x\");",
     "system function $system", False),
    ("sdf_annotate", "", "«»$sdf_annotate(\"x.sdf\");", "system task $sdf_annotate", False),
    ("event_var_index", "  event ev[2];\n  int i;\n", "@(«»ev[i]);",
     "event selected by an index that is not constant", True),
    # The kernel evaluates a wait condition in the middle of another write.
    ("wait_writes",
     "  int k;\n  function bit wr();\n    k++;\n    return 1'b1;\n  endfunction\n",
     "«»wait (wr());", "wait condition that writes a variable or net", True),
    # Historical named-event iff input: now checked for acceptance.
    # Guards are evaluated only when their own event occurs.
    ("event_named_iff", "  event ev;\n  logic en = 1'b1;\n", "@(«»ev iff en);",
     "named event with iff", True),
    ("iff_writes",
     "  logic s;\n  int k;\n  function bit wr();\n    k++;\n    return 1'b1;\n  endfunction\n",
     "@(posedge s iff «»wr());", "iff condition that writes a variable or net", True),
    # An event expression, like an iff condition, is evaluated in the middle
    # of another write; a watch returns an integral value (SIR_SPEC.md 7.2).
    ("event_expression_writes",
     "  logic s;\n  int k;\n  function bit wr();\n    k++;\n    return s;\n  endfunction\n",
     "@(posedge «»wr());", "event expression that writes a variable or net", True),
    ("event_expression_real", "  real r;\n", "@(«»r + 1.0);",
     "event control on an expression of this type", True),
    ("stream_into_string", "  string s;\n  bit [15:0] a = 16'h4142;\n", "s = «»{>>{a}};",
     "streaming into this target", False),
    ("unpack_into_string", "  string s;\n", "«»{>>{s}} = 16'h4142;", "unpacking this stream",
     False),
    ("stream_with", "  byte arr[4];\n", "«»{>>{arr with [0:1]}} = 16'h0102;",
     "streaming with a with clause", False),
    ("array_slice", "  int a[4], b[2];\n", "«»b = a[1:2];",
     "assignment of this unpacked array value", False),
    ("intra_delay_array", "  int a[2], b[2];\n", "«»a = #1 b;",
     "intra-assignment timing control on this assignment", True),
    ("nonblocking_event", "  int a;\n  event e;\n", "«»a <= @(e) 1;",
     "nonblocking assignment with an event control", True),
    ("nonblocking_delay_index", "  int a[4];\n  int i = 1;\n", "«»a[i] <= #1 1;",
     "delayed nonblocking assignment to this target", True),
    ("value_plusargs_two", "  int a;\n", "void'(«»$value$plusargs(\"n=%d%d\", a));",
     "$value$plusargs with this format", False),
    ("sscanf_specifier", "  int a;\n", "void'(«»$sscanf(\"1\", \"%q\", a));",
     "format specifier %q in $sscanf", False),
    ("member_run_time_bit", "  struct packed { logic [3:0] f; } sp;\n  int i = 1;\n",
     "«»sp.f[i] = 1'b1;", "a select known at run time of a member or a part", False),
    ("trigger_event_control", "  event e, e2;\n", "«»->> @(e2) e;",
     "->> with an event control", True),
    ("randsequence", "  int y;\n", "«»randsequence(main) main : { y = 1; }; endsequence",
     "statement 'RandSequence'", False),
    ("wait_order", "  event e1, e2;\n", "«»wait_order(e1, e2);", "statement 'WaitOrder'", True),
    ("fork_declaration", "", "fork int «»k; join", "declarations in a fork block", True),
    ("disable_fork_block", "", "fork : fb_{c} #5; join_none «»disable fb_{c};",
     "disable of this block", True),
    ("force_real", "  real r;\n", "«»force r = 1.0;",
     "assign or force of a value that is not integral", False),
    ("force_unpacked_array", "  logic m[2];\n", "«»force m = '{1'b1, 1'b0};",
     "assignment to a whole unpacked array", False),
    ("parameter_struct_index",
     "  typedef struct { int a; } s_t;\n  localparam s_t P[2] = '{'{a: 1}, '{a: 2}};\n"
     "  int i = 1, y;\n", "y = «»P[i].a;",
     "parameter of type 'top.s_t$[0:1]' at a run-time index", False),
]

# (name, item, message): a module item.
ITEMS = [
    ("net_delay", "wire #5 «»nd;\n  assign «»nd = 1'b1;", "net with a delay"),
    ("net_unpacked_init", "wire «»wa[2] = '{1'b0, 1'b1};", "assignment to a whole unpacked array"),
    ("alias_part", "wire [3:0] a, b;\n  «»alias a[1:0] = b[1:0];",
     "an alias of a part or a concatenation of nets"),
    ("alias_array", "wire w1[2], w2[2];\n  «»alias w1 = w2;", "an alias of an array of nets"),
    ("alias_types", "wire [3:0] a;\n  wire signed [3:0] b;\n  «»alias a = b;",
     "an alias of nets of different types or kinds"),
    ("alias_apart", "wire a, b, c, d;\n  alias a = b;\n  alias c = d;\n  «»alias a = c;",
     "an alias of nets that other aliases or ports joined apart"),
    ("cond_matches_items",
     "int x = 5;\n  wire z, o;\n  assign z = «»x matches 5 ? 1 : 0;\n"
     "  wire n = «»x matches 5 ? 1 : 0;\n  logic v = «»x matches 5 ? 1 : 0;\n"
     "  and a(o, «»x matches 5 ? 1 : 0, 1'b1);", "conditional operator with a pattern"),
    # The iff of an always and an always_ff whose event control becomes the
    # process's triggers.
    ("iff_writes_trigger",
     "logic c, q, r;\n  int k;\n  function bit wr();\n    k++;\n    return 1'b1;\n  endfunction\n"
     "  always @(posedge c iff «»wr()) q = 1'b1;\n  always_ff @(posedge c iff «»wr()) r <= 1'b1;",
     "iff condition that writes a variable or net"),
    # always_ff keeps its event control as the process's triggers, which are
    # slot ranges.
    ("always_ff_expression", "logic c, d, q;\n  «»always_ff @(posedge (c & d)) q <= 1'b1;",
     "always_ff with an event control on an expression"),
    ("switch_mos", "wire a, b, c;\n  nmos «»n1(a, b, c);", "switch 'nmos'"),
    ("switch_bidirectional", "wire a, b;\n  tran «»t1(a, b);", "switch 'tran'"),
]

# (name, text, message): the whole file, as it is.
OTHERS = [
    ("specify_path",
     "module top(input a, output b);\n  assign b = a;\n  «»specify\n    (a => b) = 1;\n"
     "  endspecify\nendmodule\n", "specify block with path delays or timing checks"),
    ("port_inout_part",
     "module child(inout wire «»a);\nendmodule\n\nmodule top;\n  wire [1:0] w;\n"
     "  child c(.a(w[0]));\nendmodule\n", "inout port"),
    ("port_ref",
     "module child(ref logic «»a);\nendmodule\n\nmodule top;\n  logic x;\n  child c(.a(x));\n"
     "endmodule\n", "ref port"),
    ("force_port_net",
     "module child(input wire a);\nendmodule\n\nmodule top;\n  wire w;\n  child c(w);\n"
     "  initial «»force w = 1'b1;\nendmodule\n", "assign or force of a net connected through a port"),
    ("argument_ref_struct",
     "module top;\n  typedef struct { int a; } s_t;\n  s_t s;\n"
     "  task automatic t(ref s_t «»r);\n    r.a = 1;\n  endtask\n  initial t(s);\nendmodule\n",
     "argument 'r' of an array of structures or a ref structure"),
    ("argument_struct_array",
     "module top;\n  typedef struct { int a; } s_t;\n  s_t s[2];\n"
     "  task automatic t(input s_t «»r[2]);\n  endtask\n  initial t(s);\nendmodule\n",
     "argument 'r' of an array of structures or a ref structure"),
    ("automatic_struct_array",
     "module top;\n  typedef struct { int a; } s_t;\n  task automatic t;\n    s_t «»arr[2];\n"
     "    arr[0].a = 1;\n  endtask\n  initial t();\nendmodule\n",
     "automatic unpacked array in a structure or of structures"),
    ("automatic_in_fork",
     "module top;\n  task automatic t;\n    int x = 5;\n    fork\n      $display(\"%0d\", «»x);\n"
     "    join_none\n  endtask\n  initial t();\nendmodule\n", "automatic variable 'x' here"),
    # An empty connection of a UDP: the last input, an input in the middle and
    # the output; in the top module, an instance array (one line per
    # element), a generate block and an instantiated module.
    ("udp_unconnected",
     "primitive p(output y, input a, b);\n  table\n    1 ? : 1;\n    0 0 : 0;\n  endtable\n"
     "endprimitive\n\nmodule sub;\n  wire y;\n  p «»u(y, 1'b1, );\nendmodule\n\nmodule top;\n"
     "  wire y;\n  wire [1:0] ys;\n  reg [1:0] a = 2'b11;\n  p «»u(y, , 1'b0);\n"
     "  p «»«»row[1:0](ys, a, );\n  if (1) begin : g\n    p «»v(, 1'b0, 1'b1);\n  end\n"
     "  sub s();\n  initial #1 $display(\"%b %b\", y, ys);\nendmodule\n",
     "a UDP instance with an unconnected port"),
    ("output_array_slice",
     "module top;\n  logic [7:0] m [0:3];\n  task automatic t(output logic [7:0] o [0:1]);\n"
     "    o[0] = 1;\n    o[1] = 2;\n  endtask\n  initial «»t(m[1:2]);\nendmodule\n",
     "an output or inout array argument of this actual"),
    ("deferred_waiting_task",
     "module top;\n  task t;\n    #1;\n  endtask\n  initial «»assert #0 (0) else t();\nendmodule\n",
     "deferred assertion whose action is a task that may wait"),
]
# Drivers of a net that meet through a port between nets resolve on one net
# (VALUE_SEMANTICS.md 5.3). A static partial port between plain four-state
# nets of one kind and width keeps its strengths; the other shapes are
# reported at the connection until the nets are merged: a wired net, a
# tri0 net, a connection of another width and an element of an array of
# nets. Each in the top module, a generate block and an instantiated module.
NET_PORT_SHAPES = [
    ("wired", "module child(output wand o);\n  assign o = 1'b0;\nendmodule\n",
     "wand [1:0] w;\n  assign w[1] = 1'b1;", "w[1]"),
    ("tri0", "module child(output tri0 o);\n  assign o = 1'bz;\nendmodule\n",
     "wire [1:0] w;\n  assign w[1] = 1'bz;", "w[1]"),
    ("width", "module child(output wire [1:0] o);\n  assign o = 2'b00;\nendmodule\n",
     "wire [2:0] w;\n  assign w = 3'b111;", "w[2:0]"),
    ("array", "module child(output wire o);\n  assign o = 1'b0;\nendmodule\n",
     "wire w[2];\n  assign w[1] = 1'b1;", "w[1]"),
]


def net_port_file(child: str, net: str, outer: str, container: str) -> str:
    use = f"{net}\n  child c(.o(«»{outer}));"
    if container == "top":
        return f"{child}\nmodule top;\n  {use}\nendmodule\n"
    if container == "generate":
        return (f"{child}\nmodule top;\n  if (1) begin : g\n    {use.replace(chr(10), chr(10) + '  ')}"
                f"\n  end\nendmodule\n")
    return f"{child}\nmodule mid;\n  {use}\nendmodule\n\nmodule top;\n  mid m();\nendmodule\n"


OTHERS += [
    (f"net_port_meet_{shape}_{container}", net_port_file(child, net, outer, container),
     f"drivers of the net '{scope}.w' that meet through a port")
    for shape, child, net, outer in NET_PORT_SHAPES
    for container, scope in [("top", "top"), ("generate", "top.g"), ("instance", "top.m")]
]
OTHERS += [
    # A DPI-C import (35.5): reported once at its declaration, at its name,
    # whether it is called or not, wherever it is declared; never run.
    (f"dpi_{name}", text, f"DPI-C import '{sub}'") for name, text, sub in [
        ("void_function",
         "module top;\n  import \"DPI-C\" function void «»get(output int x);\n  int v = 7;\n"
         "  initial begin\n    get(v);\n    $display(\"%0d\", v);\n  end\nendmodule\n", "get"),
        ("int_function",
         "module top;\n  import \"DPI-C\" function int «»add_one(input int x);\n"
         "  initial $display(\"%0d\", add_one(1));\nendmodule\n", "add_one"),
        ("task",
         "module top;\n  import \"DPI-C\" task «»wait_cycles(input int n);\n"
         "  initial wait_cycles(3);\nendmodule\n", "wait_cycles"),
        ("pure",
         "module top;\n  import \"DPI-C\" pure function int «»twice(input int x);\n"
         "  int v;\n  initial v = twice(2);\nendmodule\n", "twice"),
        ("context",
         "module top;\n  import \"DPI-C\" context task «»ctx();\n  initial ctx();\nendmodule\n",
         "ctx"),
        ("c_name",
         "module top;\n  import \"DPI-C\" c_get = function int «»get();\n"
         "  initial $display(\"%0d\", get());\nendmodule\n", "get"),
        ("inout_open_array",
         "module top;\n  import \"DPI-C\" function void «»fill(inout int a[]);\n"
         "  int m [0:3];\n  initial fill(m);\nendmodule\n", "fill"),
        ("uncalled",
         "module top;\n  import \"DPI-C\" function void «»never();\n"
         "  initial $display(\"ran\");\nendmodule\n", "never"),
        ("called_first",
         "module top;\n  initial early();\n  import \"DPI-C\" function void «»early();\n"
         "endmodule\n", "early"),
        ("package",
         "package p;\n  import \"DPI-C\" function int «»from_c();\nendpackage\n\n"
         "module top;\n  import p::*;\n  initial $display(\"%0d %0d\", p::from_c(), from_c());\n"
         "endmodule\n", "from_c"),
        ("unit",
         "import \"DPI-C\" function void «»unit_level();\n\nmodule top;\n"
         "  initial unit_level();\nendmodule\n", "unit_level"),
        ("interface",
         "interface bus;\n  import \"DPI-C\" function void «»poke();\nendinterface\n\n"
         "module top;\n  bus b();\n  initial b.poke();\nendmodule\n", "poke"),
        ("generate",
         "module top;\n  if (1) begin : g\n    import \"DPI-C\" function void «»inner();\n"
         "    initial inner();\n  end\nendmodule\n", "inner"),
        ("two_instances",
         "module sub;\n  import \"DPI-C\" function void «»each();\n  initial each();\n"
         "endmodule\n\nmodule top;\n  sub a();\n  sub b();\nendmodule\n", "each"),
    ]
]

# (name, declarations, statement, whole error text): an error, not an
# unsupported construct (SYSTEM_TASKS.md 6.3), in every container.
ERRORS = [
    ("sscanf_fewer_targets", "  int a;\n", "void'(«»$sscanf(\"1 2\", \"%d %d\", a));",
     "$sscanf has fewer arguments than its format has conversions"),
    # A % with no conversion after it (SYSTEM_TASKS.md 3.1).
    ("format_percent_end", "", "$display(«»\"100%\");", "format has a % without a conversion"),
    ("format_percent_width", "  logic [3:0] v = 4'h5;\n", "$display(«»\"%5\", v);",
     "format has a % without a conversion"),
    # A constant format that is not a literal, whose arguments slang does not
    # count (SYSTEM_TASKS.md 3.13).
    ("sformatf_const_too_few", "  localparam string F = \"%0d-%0h\";\n  string s;\n  int a;\n",
     "s = $sformatf(«»F, a);", "no argument for the format's %h"),
    ("sformat_const_too_many", "  localparam string F = \"%0d\";\n  string s;\n  int a;\n",
     "$sformat(s, F, a, «»a);", "too many arguments for the format"),
]


def header() -> str:
    return f"// Generated by tools/gen_unsupported_matrix.py (version {VERSION}); do not edit.\n"


def statement_file(decl: str, stmt: str, timing: bool) -> str:
    body = "module top;\n  event go;\n" + decl
    for container, text, ok in PROC:
        if timing and not ok:
            continue
        body += text.format(s=f"    {stmt.replace('{c}', container)}\n")
    return body + "endmodule\n"


def item_file(item: str) -> str:
    return (f"module sub;\n  {item}\nendmodule\n\n"
            f"module top;\n  {item}\n  if (1) begin : g\n    {item.replace(chr(10), chr(10) + '  ')}\n"
            f"  end\n  sub s();\nendmodule\n")


def mark(path: str, text: str, message: str):
    """The file without its marks, and the error lines they stand for."""
    lines, errors = [], []
    for number, line in enumerate(text.splitlines(), 1):
        while m := re.search("«»", line):
            errors.append(f"{NAME}/{path}:{number}:{m.start() + 1}: error: {message}")
            line = line[:m.start()] + line[m.end():]
        lines.append(line)
    return "\n".join(lines) + "\n", errors


def unsupported(what: str) -> str:
    return f"unsupported: {what} (not implemented yet)"


def files():
    out, errors = {}, []

    def add(name, text, message):
        path = f"{name}.sv"
        sv, errs = mark(path, header() + text, message)
        out[f"{NAME}/{path}"] = sv
        if name not in ACCEPTED:
            errors.extend(errs)

    for name, decl, stmt, what, timing in STATEMENTS:
        add(name, statement_file(decl, stmt, timing), unsupported(what))
    for name, decl, stmt, what in ERRORS:
        add(name, statement_file(decl, stmt, False), what)
    for name, item, what in ITEMS:
        add(name, item_file(item), unsupported(what))
    for name, text, what in OTHERS:
        add(name, text, unsupported(what))
    for name, text in EXPECTED.items():
        out[f"{NAME}/{name}.expected"] = text
    out[f"{NAME}.accepted"] = "\n".join(sorted(ACCEPTED)) + "\n"
    out[f"{NAME}.stderr"] = "\n".join(sorted(errors)) + "\n"
    return out


def main():
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("--check", action="store_true")
    args = ap.parse_args()
    want = files()
    bad = 0
    if args.check:
        have = {f"{NAME}/{p.name}" for p in (OUT / NAME).glob("*.sv")}
        for extra in sorted(have - set(want)):
            print(f"gen_unsupported_matrix: tests/cli/{extra} is not generated; remove it",
                  file=sys.stderr)
            bad += 1
    else:
        (OUT / NAME).mkdir(exist_ok=True)
    for name, text in want.items():
        path = OUT / name
        if args.check:
            if not path.exists() or path.read_text(encoding="utf-8") != text:
                print(f"gen_unsupported_matrix: tests/cli/{name} is out of date; run "
                      "tools/gen_unsupported_matrix.py", file=sys.stderr)
                bad += 1
        else:
            path.write_text(text, encoding="utf-8")
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main())
