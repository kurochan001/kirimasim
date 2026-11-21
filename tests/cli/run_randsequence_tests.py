#!/usr/bin/env python3
# Copyright (c) 2026 The kirimasim Authors
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

"""Check the ordered calls, selection and bounds of SYSTEM_TASKS.md 13.3."""

import argparse
from pathlib import Path
import sys
import tempfile

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / "tools"))
import bounded_run  # noqa: E402


def module(body):
    return "module top; int trace; initial begin\n" + body + '\n$display("ok"); end endmodule\n'


def cases():
    # Both spellings of the starting production and every cumulative boundary.
    for seed_start in ("", "pick"):
        for weights in [(1, 1, 1), (3, 1, 2), (1, 0xfffffffd, 1)]:
            a, b, c = weights
            choices = [f"p{i}" + (f" := (32'd{w})" if w != 1 else "")
                       for i, w in enumerate(weights)]
            yield "weighted", module(f"""
int unsigned bucket, after_draw;
for (int seed=1; seed<=31; seed++) begin
  void'($urandom(seed));
  bucket = $urandom_range(32'd{a+b+c-1});
  after_draw = $urandom();
  void'($urandom(seed));
  randsequence({seed_start})
    pick: {" | ".join(choices)};
    p0: {{ trace = 3; }}; p1: {{ trace = 5; }}; p2: {{ trace = 8; }};
  endsequence
  if (trace != (bucket < 32'd{a} ? 3 : bucket < 32'd{a+b} ? 5 : 8) ||
      $urandom() != after_draw) $fatal(1,"weighted selection");
end"""), None

    calls = """randsequence(start)
  unused: unused;
  start: twice leaf { trace = 10 * trace + 9; };
  twice: leaf leaf;
  leaf: { trace = 10 * trace + 2; };
endsequence"""
    check = 'if (trace != 2229) $fatal(1,"call order");'
    yield "no_draw", module(f"""int unsigned next_draw;
void'($urandom(7)); next_draw=$urandom(); void'($urandom(7));
{calls}
{check}
if ($urandom() != next_draw) $fatal(1,"single production consumed a draw");"""), None
    for decl, run in [
        (f'task automatic go(ref int trace); {calls} endtask', 'go(trace);'),
        (f'function automatic int go(); int trace; {calls} return trace; endfunction',
         'trace = go();'),
        (f'task automatic go(); automatic int captured=0; fork begin\n'
         + calls.replace("trace", "captured") + '\nend join trace=captured; endtask', 'go();'),
    ]:
        yield "activation", ('module top; int trace; ' + decl + ' initial begin '
                             + run + check + ' $display("ok"); end endmodule'), None

    yield "package", (f'package grammar; task automatic go(ref int trace); {calls} endtask endpackage '
                      'module top; int trace; initial begin grammar::go(trace); ' + check
                      + ' $display("ok"); end endmodule'), None
    yield "interface", (f'interface bus; int trace; task go(); {calls} endtask endinterface '
                        'module top; bus link(); initial begin link.go(); '
                        + check.replace('trace', 'link.trace') + ' $display("ok"); end endmodule'), None
    yield "class", (f'module top; class worker; int trace; function void go(); {calls} endfunction '
                    'endclass initial begin worker obj=new; obj.go(); '
                    + check.replace('trace', 'obj.trace') + ' $display("ok"); end endmodule'), None
    yield "nonwaiting_task", ('module top; int trace; task choose(); randcase 1: trace=7; '
                              'endcase endtask initial begin randsequence() start: {choose();}; '
                              'endsequence if (trace!=7) $fatal; $display("ok"); end endmodule'), None
    yield "waiting_virtual_callee", """module top;
class base; virtual task delay_task(); endtask endclass
class derived extends base; virtual task delay_task(); #1; endtask endclass
task wrapper(base obj); obj.delay_task(); endtask
initial begin derived d=new; randsequence() start: {wrapper(d);}; endsequence end endmodule""", "waiting randsequence task call"
    yield "waiting_callee", ('module top; int trace; task leaf_task(); #1 trace++; endtask '
                             'task wrapper(); leaf_task(); endtask initial begin '
                             'randsequence() action: {wrapper();}; endsequence end endmodule'), "waiting randsequence task call"
    yield "weighted_trailer", module("""
randsequence(start)
  start: leaf := 3 { trace = 10 * trace + 6; };
  leaf: { trace=7; };
endsequence
if (trace != 76) $fatal(1,"weight trailer order");"""), None
    yield "nested_outer_reference", module("""
randsequence(start)
  start: { randsequence() inner: leaf leaf; endsequence } leaf;
  leaf: {trace++;};
endsequence
if (trace != 3) $fatal(1,"enclosing production");
"""), None
    yield "inner_loop", module("""
randsequence(start)
  start: { repeat (5) begin trace++; if (trace==1) continue; break; end };
endsequence
if (trace != 2) $fatal(1,"loop target");"""), None
    # Shared productions have one body per grammar node. The inactive path
    # represents over a billion leaves yet remains cheap to compile.
    dag = "".join(f"a{i}: a{i+1} a{i+1};\n" for i in range(30)) + "a30: { trace++; };"
    yield "shared_graph", module('if ($test$plusargs("expand")) begin randsequence()\n'
                                 + dag + '\nendsequence end'), None
    dag = "".join(f"a{i}: a{i+1} a{i+1};\n" for i in range(7)) + "a7: { trace++; };"
    yield "shared_execution", module("randsequence()\n" + dag +
                                     '\nendsequence if (trace != 128) $fatal(1,"shared calls");'), None

    for name, rules, reason in [
        ("cycle", "pick: leaf | pick;", "recursive randsequence productions"),
        ("nested_cycle", "pick: {randsequence() inner: pick; endsequence};",
         "recursive randsequence productions"),
        ("cycle_indirect", "pick: leaf | inner; inner: pick;", "recursive randsequence productions"),
        ("dynamic", "pick: leaf := trace;", "positive integral constant"),
        ("zero", "pick: leaf := 0;", "positive integral constant"),
        ("negative", "pick: leaf := (-1);", "positive integral constant"),
        ("unknown", "pick: leaf := (3'bx);", "positive integral constant"),
        ("overflow", "pick: leaf := (32'hffffffff) | leaf;", "weight sum wider than 32 bits"),
        ("single_overflow", "pick: leaf := (65'd4294967296);", "weight sum wider than 32 bits"),
        ("args", "pick: inner(2); inner(int x): leaf;", "production arguments"),
        ("result", "pick: inner; int inner: {return 1;};", "arguments or return values"),
        ("join", "pick: rand join leaf leaf;", "randsequence rand join"),
        ("control", "pick: repeat (2) leaf;", "production control item"),
        ("local", "pick: {int x; x=3;} ;", "code block declaration"),
        ("type_decl", "pick: {typedef int local_t; trace++;};", "code block declaration"),
        ("delay", "pick: {#1 trace++;};", "waiting randsequence code block"),
        ("exit", "pick: {break;};", "transfer out of a randsequence code block"),
        ("return", "pick: {return;};", "transfer out of a randsequence code block"),
    ]:
        yield name, module(f"randsequence(pick) {rules} leaf: {{ trace++; }}; endsequence"), reason


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--kirimasim", required=True)
    ap.add_argument("--roundtrip", required=True)
    args = ap.parse_args()
    with tempfile.TemporaryDirectory() as tmp:
        for i, (name, source, reason) in enumerate(cases()):
            path = Path(tmp) / f"{name}_{i}.sv"
            path.write_text(source, encoding="utf-8")
            run = bounded_run.run([args.kirimasim, str(path)], timeout=15)
            if reason:
                ok = (run.returncode == 2 and "error: unsupported: " in run.stderr
                      and reason in run.stderr and not run.stdout)
            else:
                ok = run.returncode == 0 and run.stdout == "ok\n"
                if ok:
                    run = bounded_run.run([args.roundtrip, str(path)], timeout=15)
                    ok = run.returncode == 0
            if not ok:
                sys.exit(f"{path.name} (exit {run.returncode}, expected {reason or 'ok'})\n"
                         f"{source}\n{run.stdout}{run.stderr}")


if __name__ == "__main__":
    main()
