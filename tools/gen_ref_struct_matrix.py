#!/usr/bin/env python3
# Copyright (c) 2026 The kirimasim Authors
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

"""Generates the matrix tests of ref structure arguments of TEST_SUITE.md
("Mechanism matrix") in tests/suite/proc/.

  tools/gen_ref_struct_matrix.py           # writes the files
  tools/gen_ref_struct_matrix.py --check   # exit 1 if a file differs from what it would write

The rule: SIR_SPEC.md 8 (amended 2026-10-30, approved), with 13.5.2 of the
standard. A ref or const ref argument of an unpacked structure type refers
to the actual itself, leaf by leaf: a read through it reads the actual's
member as it is then, and a write through it writes the actual's member at
once, seen by the caller and by other processes before the call returns.
The indices that choose the actual are evaluated once, at the call.

The axes: the actual (a static variable, an element of a static array of
structures at a constant and at a run-time index, a structure member of a
static structure, an element of an array of structures that is a member,
an automatic variable of the container, a ref argument passed on, and a
structure whose member is an array of structures, passed whole),
what the callee does through the reference (writes members, one by bits
and one an element of an array member; copies the whole structure out and
back; assigns a pattern; reads it as const ref; passes two members on as a
ref int and a ref array, or is given them so; writes across a wait while
another process writes the actual; issues NBAs, plain and delayed, through
ref static, which takes only a static variable or an element of a static
array here), and the container of the call (an initial procedure, a static
task, an always procedure and a fork branch; one file each). The structure
has an int, a 70-bit 4-state vector, a string and a fixed array of bytes,
so its leaves are narrower and wider than a word, a string and an array.
Every callee first sets the index of the run-time actual to another
element, which the reference does not follow. A const ref function in
always_comb shows the sensitivity to the actual's members.

The expected outputs follow from the rule, worked out below without
reading kirimasim's code. Checked 2026-10-30: Verilator 5.052 prints the
same lines for every case without an NBA, on a copy with ref static
written as ref and the index left alone (it supports neither ref static
nor a function that writes a module variable); Icarus 13.0 has no unpacked
structures. A person checks them once for all the tests made from them
(TEST_SUITE.md, "Sampling by people").
"""

import argparse
import sys
from dataclasses import dataclass, field
from pathlib import Path

OUT = Path(__file__).resolve().parent.parent / "tests" / "suite" / "proc"
NAME = "ref_struct_matrix"
# The version of the rules below (TEST_SUITE.md, :generator:). Raise it
# whenever they change.
VERSION = 1

W_BASE = 0x2A_0000_0000_0000_0001  # bits 69..64 and bit 0 set
W_PATTERN = 0x3_0000_0000_0000_0005
# The container of the calls, as the description names it.
CONTAINERS = {"initial": "an initial procedure", "task": "a static task",
              "always": "an always procedure", "fork": "a fork branch"}


@dataclass
class Item:
    n: int
    w: int
    s: str
    arr: list = field(default_factory=list)

    def literal(self):
        a = ", ".join(str(x) for x in self.arr)
        return f"'{{n: {self.n}, w: 70'h{self.w:x}, s: \"{self.s}\", arr: '{{{a}}}}}"

    def show(self):
        return f"n={self.n} w={self.w:018x} s={self.s} arr={','.join(str(x) for x in self.arr)}"


def byte(x):
    """byte'(x): the low 8 bits, signed."""
    x &= 0xFF
    return x - 256 if x >= 128 else x


def start():
    """The value of each actual before a case."""
    return {
        "sv": Item(10, W_BASE, "sv", [1, 2, 3]),
        "sa[0]": Item(20, W_BASE + 0x10, "a0", [21, 22, 23]),
        "sa[1]": Item(21, W_BASE + 0x11, "a1", [31, 32, 33]),
        "sa[2]": Item(22, W_BASE + 0x12, "a2", [41, 42, 43]),
        "so.inner": Item(30, W_BASE + 0x20, "so", [4, 5, 6]),
        "sb.list[0]": Item(60, W_BASE + 0x40, "b0", [51, 52, 53]),
        "sb.list[1]": Item(61, W_BASE + 0x41, "b1", [61, 62, 63]),
        "la": Item(40, W_BASE + 0x30, "la", [7, 8, 9]),
        "sp": Item(50, W_BASE + 0x50, "sp", [11, 12, 13]),
    }


# The actual: (name, the expression passed, the place it names, whether a
# ref static argument takes it, the places shown after the case). slang
# (the pinned version) rejects a member of a static structure as a ref
# static actual, so the members are not passed to t_nba; an NBA to an
# automatic variable is illegal (10.4.2).
ACTUALS = [
    ("static", "sv", "sv", True, ["sv"]),
    ("element", "sa[1]", "sa[1]", True, ["sa[0]", "sa[1]", "sa[2]"]),
    ("runtime_element", "sa[k]", "sa[2]", True, ["sa[0]", "sa[1]", "sa[2]"]),
    ("member", "so.inner", "so.inner", False, ["so.inner"]),
    ("member_element", "sb.list[1]", "sb.list[1]", False, ["sb.list[0]", "sb.list[1]"]),
    ("automatic", "la", "la", False, ["la"]),
    ("passed_on", "sp", "sp", True, ["sp"]),
]
OPS = ["write", "whole", "pattern", "read", "member", "wait", "nba"]
# The callee of each operation, and the one that passes its ref on.
CALLEE = {"write": "f_write", "whole": "t_whole", "pattern": "f_pattern", "read": "f_read",
          "member": "f_member", "wait": "t_wait", "nba": "t_nba"}

SUBROUTINES = """\
  function automatic void bump();
    k = 0;
  endfunction
  function automatic string show(input item_t x);
    return $sformatf("n=%0d w=%h s=%s arr=%0d,%0d,%0d", x.n, x.w, x.s, x.arr[0], x.arr[1],
                     x.arr[2]);
  endfunction
  function automatic void f_write(ref item_t r, input int v);
    bump();
    r.n = r.n + v;
    r.w[69:66] = 4'ha;
    r.s = {r.s, "+"};
    r.arr[2] = byte'(r.arr[1] + v);
  endfunction
  task automatic t_whole(ref item_t r, input int v);
    item_t t;
    bump();
    t = r;
    t.n = t.n * 2;
    t.arr[0] = byte'(v);
    t.s = "whole";
    r = t;
  endtask
  function automatic void f_pattern(ref item_t r, input int v);
    bump();
    r = '{n: v, w: 70'h%(pattern)x, s: "pat", arr: '{byte'(v), byte'(v + 1), byte'(v + 2)}};
  endfunction
  function automatic string f_read(const ref item_t r, input int v);
    bump();
    return $sformatf("%%0d %%h %%s %%0d", r.n + v, r.w, r.s, r.arr[1]);
  endfunction
  function automatic void f_member(ref int x, ref byte a [0:2], input int v);
    bump();
    x = x + v;
    a[0] = byte'(a[2] + v);
  endfunction
  task automatic t_wait(ref item_t r, input int v);
    bump();
    r.n = r.n + v;
    #2;
    r.s = $sformatf("%%0d", r.n);
  endtask
  task automatic t_nba(ref static item_t r, input int v);
    bump();
    r.n <= r.n + v;
    r.arr[0] <= #1 byte'(v);
    r.w[3:0] <= #1 4'h5;
  endtask
  function automatic void p_write(ref item_t p, input int v);
    f_write(p, v);
  endfunction
  task automatic p_whole(ref item_t p, input int v);
    t_whole(p, v);
  endtask
  function automatic void p_pattern(ref item_t p, input int v);
    f_pattern(p, v);
  endfunction
  function automatic string p_read(const ref item_t p, input int v);
    return f_read(p, v);
  endfunction
  function automatic void p_member(ref item_t p, input int v);
    f_member(p.n, p.arr, v);
  endfunction
  task automatic p_wait(ref item_t p, input int v);
    t_wait(p, v);
  endtask
  task automatic p_nba(ref static item_t p, input int v);
    t_nba(p, v);
  endtask
  function automatic void f_bag(ref bag_t b, input int v);
    bump();
    b.list[1].n = b.list[1].n + v;
    b.list[1].w[69:68] = 2'b01;
    b.list[1].arr[2] = byte'(v);
    b.list[0].s = {b.list[0].s, "+"};
    b.m = b.list[1].arr[2] + b.list[0].n;
  endfunction
  task automatic t_bag_nba(ref static bag_t b, input int v);
    b.list[0].n <= v;
    b.list[1].w[3:0] <= #1 4'h5;
    b.list[0].arr[1] <= #1 byte'(v);
  endtask
  function automatic int f_peek(const ref item_t r);
    return r.n + r.arr[1];
  endfunction
""".replace("%(pattern)x", f"{W_PATTERN:x}").replace("%%", "%")


def case(container, actual, op, v, pad):
    """The statements of one case and the lines they print."""
    label, expr, named, static, shown = actual
    tag = f"{container} {label} {op}"
    vals = start()
    sv, exp = [f"{pad}// {tag}"], []
    for place in shown:
        sv.append(f"{pad}{place} = {vals[place].literal()};")
    sv.append(f"{pad}k = 2;")
    callee = ("p_" + CALLEE[op][2:]) if label == "passed_on" else CALLEE[op]
    it = vals[named]
    if op == "write":
        sv.append(f"{pad}{callee}({expr}, {v});")
        it.n += v
        it.w = (it.w & ~(0xF << 66)) | (0xA << 66)
        it.s += "+"
        it.arr[2] = byte(it.arr[1] + v)
    elif op == "whole":
        sv.append(f"{pad}{callee}({expr}, {v});")
        it.n *= 2
        it.arr[0] = byte(v)
        it.s = "whole"
    elif op == "pattern":
        sv.append(f"{pad}{callee}({expr}, {v});")
        vals[named] = Item(v, W_PATTERN, "pat", [byte(v), byte(v + 1), byte(v + 2)])
    elif op == "read":
        sv.append(f'{pad}$display("{tag} read %s", {callee}({expr}, {v}));')
        exp.append(f"{tag} read {it.n + v} {it.w:018x} {it.s} {it.arr[1]}")
    elif op == "member":
        # Members of the actual passed on as a ref int and a ref array.
        if label == "passed_on":
            sv.append(f"{pad}{callee}({expr}, {v});")
        else:
            sv.append(f"{pad}{callee}({expr}.n, {expr}.arr, {v});")
        it.n += v
        it.arr[0] = byte(it.arr[2] + v)
    elif op == "wait":
        # The callee writes, waits, and reads what another process wrote
        # meanwhile; that process sees the callee's write before it returns.
        sv += [f"{pad}fork", f"{pad}  {callee}({expr}, {v});",
               f"{pad}  begin",
               f"{pad}    #1 $display(\"{tag} inside %s\", show({named}));",
               f"{pad}    {named}.n = {100 + v};",
               f"{pad}  end", f"{pad}join"]
        it.n += v
        exp.append(f"{tag} inside {it.show()}")
        it.n = 100 + v
        it.s = str(it.n)
    else:
        # The plain NBA applies in this time slot; the delayed ones in the
        # NBA region one step later, after a process woken then has read.
        sv.append(f"{pad}{callee}({expr}, {v});")
        sv.append(f'{pad}$display("{tag} issued %s", show({named}));')
        exp.append(f"{tag} issued {it.show()}")
        it.n += v
        sv.append(f'{pad}#1 $display("{tag} step %s", show({named}));')
        exp.append(f"{tag} step {it.show()}")
        sv.append(f"{pad}#1;")
        it.arr[0] = byte(v)
        it.w = (it.w & ~0xF) | 0x5
    fmt = " | ".join("%s" for _ in shown)
    args = ", ".join(f"show({place})" for place in shown)
    sv.append(f'{pad}$display("{tag} after {fmt}", {args});')
    exp.append(f"{tag} after " + " | ".join(vals[place].show() for place in shown))
    return sv, exp


def bag_cases(container, v, pad):
    """A structure whose member is an array of structures, passed whole: its
    leaves have the array's dimension before their own."""
    sv, exp = [], []
    for op in ["write", "nba"]:
        tag = f"{container} bag {op}"
        vals = start()
        l0, l1 = vals["sb.list[0]"], vals["sb.list[1]"]
        m = 70
        sv += [f"{pad}// {tag}", f"{pad}sb.list[0] = {l0.literal()};",
               f"{pad}sb.list[1] = {l1.literal()};", f"{pad}sb.m = {m};"]
        if op == "write":
            sv.append(f"{pad}f_bag(sb, {v});")
            l1.n += v
            l1.w = (l1.w & ~(0x3 << 68)) | (0x1 << 68)
            l1.arr[2] = byte(v)
            l0.s += "+"
            m = l1.arr[2] + l0.n
        else:
            sv.append(f"{pad}t_bag_nba(sb, {v});")
            sv.append(f'{pad}$display("{tag} issued %s | %s", show(sb.list[0]), show(sb.list[1]));')
            exp.append(f"{tag} issued {l0.show()} | {l1.show()}")
            l0.n = v
            sv.append(f'{pad}#1 $display("{tag} step %s | %s", show(sb.list[0]), '
                      'show(sb.list[1]));')
            exp.append(f"{tag} step {l0.show()} | {l1.show()}")
            sv.append(f"{pad}#1;")
            l1.w = (l1.w & ~0xF) | 0x5
            l0.arr[1] = byte(v)
        sv.append(f'{pad}$display("{tag} after %s | %s | %0d", show(sb.list[0]), '
                  'show(sb.list[1]), sb.m);')
        exp.append(f"{tag} after {l0.show()} | {l1.show()} | {m}")
        v += 1
    return sv, exp, v


def body(container, pad):
    sv, exp = [f"{pad}automatic item_t la;"], []
    v = 3
    for actual in ACTUALS:
        for op in OPS:
            if op == "nba" and not actual[3]:
                continue
            s, e = case(container, actual, op, v, pad)
            sv += s
            exp += e
            v += 1
    s, e, v = bag_cases(container, v, pad)
    sv += s
    exp += e
    # A const ref function in always_comb reads the members it is given.
    sv += [f"{pad}// {container} comb", f"{pad}sw.n = {10 * v};",
           f'{pad}#1 $display("{container} comb n %0d", seen);',
           f"{pad}sw.arr[1] = byte'({v});",
           f'{pad}#1 $display("{container} comb arr %0d", seen);']
    exp += [f"{container} comb n {10 * v}", f"{container} comb arr {11 * v}"]
    return sv, exp


def files():
    out = {}
    for container in CONTAINERS:
        name = f"{NAME}_{container}"
        head = [
            "/*",
            f":name: {name}",
            ":description: a ref or const ref argument of an unpacked structure type reads and "
            f"writes the actual's members at once, called from {CONTAINERS[container]}",
            ":tags: proc",
            ":type: simulation",
            ":lrm: 1800-2023 13.5.2 7.2 10.4.2 9.4.1",
            ":oracle: spec",
            ":spec_check: ai",
            ":fourstate: no",
            ":nondeterministic: no",
            f":generator: gen_ref_struct_matrix.py@{VERSION}",
            f":expected_stdout: @{name}.expected",
            "*/",
            "// Generated by tools/gen_ref_struct_matrix.py; change the generator, not this file.",
            "`timescale 1ns/1ns",
            "module top;",
            "  typedef struct { int n; logic [69:0] w; string s; byte arr [0:2]; } item_t;",
            "  typedef struct { int tag; item_t inner; } outer_t;",
            "  typedef struct { item_t list [0:1]; int m; } bag_t;",
            "  item_t sv, sp, sw;",
            "  item_t sa [0:2];",
            "  outer_t so;",
            "  bag_t sb;",
            "  int k, seen;",
            "  always_comb seen = f_peek(sw);",
        ]
        sv = head + SUBROUTINES.rstrip("\n").split("\n")
        if container == "initial":
            stmts, exp = body(container, "    ")
            sv += ["  initial begin"] + stmts + ["  end"]
        elif container == "task":
            stmts, exp = body(container, "    ")
            sv += ["  initial run();", "  task static run();"] + stmts + ["  endtask"]
        elif container == "always":
            stmts, exp = body(container, "    ")
            sv += ["  logic go = 1'b0;", "  initial #1 go = 1'b1;",
                   "  always @(posedge go) begin"] + stmts + ["  end"]
        else:
            stmts, exp = body(container, "      ")
            sv += ["  initial", "    fork", "      begin"] + stmts + ["      end", "    join"]
        sv += ["endmodule", ""]
        out[f"{name}.sv"] = "\n".join(sv)
        out[f"{name}.expected"] = "\n".join(exp) + "\n"
    return out


def main():
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("--check", action="store_true")
    args = ap.parse_args()
    bad = 0
    for name, text in files().items():
        path = OUT / name
        if args.check:
            if not path.exists() or path.read_text(encoding="utf-8") != text:
                print(f"gen_ref_struct_matrix: {path.relative_to(OUT.parent.parent.parent)} is "
                      "out of date; run tools/gen_ref_struct_matrix.py", file=sys.stderr)
                bad += 1
        else:
            path.write_text(text, encoding="utf-8")
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main())
