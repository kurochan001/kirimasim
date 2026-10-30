#!/usr/bin/env python3
# Copyright (c) 2026 The kirimasim Authors
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

"""Generates the matrix tests of arrays of structures as arguments and as
automatic variables of TEST_SUITE.md ("Mechanism matrix") in
tests/suite/proc/.

  tools/gen_struct_array_arg_matrix.py           # writes the files
  tools/gen_struct_array_arg_matrix.py --check   # exit 1 if a file differs from what it would write

The rule: SIR_SPEC.md 4.2 and 8 (amended 2026-10-30, approved), with 13.5,
13.5.1, 13.5.2, 13.4.1, 7.6 and 6.21 of the standard. An array of
structures has one slot, or one frame variable when it is automatic, per
member, with the array's dimensions in front; every call of an automatic
subroutine has its own. An argument or a function value that is an array
of structures is passed per member: an input is copied at the call, so
the callee's changes stay its own; an output, and a function's value, is
copied back on return, so the caller sees nothing before; an inout is
copied in and back; a ref or const ref refers to the actual, so writes
are seen at once. Arrays of different bounds match their elements from
the left bound.

The axes: the actual (a static array with the formal's bounds, one with
descending bounds, a row of a two-dimensional array, an array of
structures that is a member of a static structure, and an automatic array
of the container), the direction (input, output, inout, ref, and const
ref read by a function, and the value of an automatic and of a static
function assigned to the actual), the bounds of the formal or of the
function's type (ascending and descending), and the container of the call
(an initial procedure, a static task, an always procedure and a fork
branch; one file each). The structure has an int, a 70-bit 4-state
vector, a string and a fixed array of bytes. Each task writes, waits, and
writes again, while another branch of a fork shows the actual between the
two writes; the caller shows it after the call. Two calls of a task with
an automatic array of structures overlap and keep their own values.

The expected outputs follow from the rule, worked out below without
reading kirimasim's code. A person checks them once for all the tests made
from them (TEST_SUITE.md, "Sampling by people").
"""

import argparse
import sys
from dataclasses import dataclass, field
from pathlib import Path

OUT = Path(__file__).resolve().parent.parent / "tests" / "suite" / "proc"
NAME = "struct_array_arg_matrix"
# The version of the rules below (TEST_SUITE.md, :generator:). Raise it
# whenever they change.
VERSION = 1

W_BASE = 0x2A_0000_0000_0000_0001  # bits 69..64 and bit 0 set
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

    def copy(self):
        return Item(self.n, self.w, self.s, list(self.arr))


def byte(x):
    """byte'(x): the low 8 bits, signed."""
    x &= 0xFF
    return x - 256 if x >= 128 else x


def item(base, tag):
    return Item(base, W_BASE + base, tag, [base + 1, base + 2, base + 3])


# The actual: (name, the expression passed, its two elements from the left
# bound, the places shown, whether it is static).
ACTUALS = [
    ("static", "sa", ["sa[0]", "sa[1]"], ["sa[0]", "sa[1]"], True),
    ("descending", "sd", ["sd[3]", "sd[2]"], ["sd[3]", "sd[2]"], True),
    ("row", "grid[1]", ["grid[1][0]", "grid[1][1]"],
     ["grid[0][0]", "grid[1][0]", "grid[1][1]"], True),
    ("member", "sb.list", ["sb.list[0]", "sb.list[1]"], ["sb.list[0]", "sb.list[1]"], True),
    ("automatic", "la", ["la[0]", "la[1]"], ["la[0]", "la[1]"], False),
]
# The direction, and the formal's bounds: the callee and its left index.
OPS = [("input", "t_in", 0), ("input_descending", "t_in_d", 1), ("output", "t_out", 0),
       ("inout", "t_inout", 0), ("inout_descending", "t_inout_d", 1), ("ref", "t_ref", 0),
       ("ref_descending", "t_ref_d", 1), ("const_ref", "f_sum", 0), ("return", "f_ret", 0),
       ("return_static", "f_ret_s", 1)]

SUBROUTINES = """\
  function automatic string show(input item_t x);
    return $sformatf("n=%0d w=%h s=%s arr=%0d,%0d,%0d", x.n, x.w, x.s, x.arr[0], x.arr[1],
                     x.arr[2]);
  endfunction
"""


def callee(name, direction, left, write1, write2):
    """A task that writes, waits two steps, and writes again; it shows its
    own formal after the first write. L and R stand for the formal's left
    and right index."""
    right = 1 - left
    body1 = write1.replace("L", str(left)).replace("R", str(right))
    body2 = write2.replace("L", str(left)).replace("R", str(right))
    return (f"  task automatic {name}({direction} item_t v [{left}:{right}], input int x);\n"
            f"    {body1}\n"
            f"    $display(\"{name} formal %s | %s\", show(v[{left}]), show(v[{right}]));\n"
            f"    #2;\n"
            f"    {body2}\n"
            f"  endtask\n")


IN_W1 = "v[L].n = v[L].n + x; v[R].arr[2] = byte'(x);"
IN_W2 = "v[R].s = {v[R].s, \"+\"}; v[L].w[3:0] = 4'h5;"
OUT_W1 = ("v[L] = '{n: x, w: 70'h1, s: \"o\", arr: '{1, 2, 3}}; v[R] = v[L]; "
          "v[R].s = \"p\";")
OUT_W2 = "v[R].n = x + 1; v[L].arr[0] = byte'(x);"

SUBROUTINES += callee("t_in", "input", 0, IN_W1, IN_W2)
SUBROUTINES += callee("t_in_d", "input", 1, IN_W1, IN_W2)
SUBROUTINES += callee("t_out", "output", 0, OUT_W1, OUT_W2)
SUBROUTINES += callee("t_inout", "inout", 0, IN_W1, IN_W2)
SUBROUTINES += callee("t_inout_d", "inout", 1, IN_W1, IN_W2)
SUBROUTINES += callee("t_ref", "ref", 0, IN_W1, IN_W2)
SUBROUTINES += callee("t_ref_d", "ref", 1, IN_W1, IN_W2)
SUBROUTINES += """\
  function automatic int f_sum(const ref item_t v [0:1], input int x);
    return v[0].n + v[1].n + v[1].arr[1] + x;
  endfunction
  function automatic pair_t f_ret(input int x);
    f_ret[0] = '{n: x, w: 70'h3, s: "r", arr: '{1, 2, 3}};
    f_ret[1] = f_ret[0];
    f_ret[1].n = x + 1;
    f_ret[1].arr[2] = byte'(x);
  endfunction
  function dpair_t f_ret_s(input int x);
    f_ret_s[1] = '{n: x, w: 70'h4, s: "q", arr: '{4, 5, 6}};
    f_ret_s[0] = f_ret_s[1];
    f_ret_s[0].s = "z";
  endfunction
  task automatic t_local(input int seed, input int ticks);
    item_t loc [0:1];
    loc[0].n = seed;
    loc[1] = '{n: seed + 1, w: 70'h7, s: $sformatf("l%0d", seed), arr: '{1, 2, 3}};
    #(ticks);
    $display("local %0d %0d %0d %s", seed, loc[0].n, loc[1].n, loc[1].s);
    loc[1].n = seed + 50;
    #1;
    $display("local %0d %0d %0d %s", seed, loc[0].n, loc[1].n, loc[1].s);
  endtask
"""


def in_first(left_item, right_item, x):
    """IN_W1 on (the formal's left element, its right element)."""
    left_item.n += x
    right_item.arr[2] = byte(x)
    return left_item.show(), right_item.show()


def in_second(left_item, right_item):
    """IN_W2."""
    right_item.s += "+"
    left_item.w = (left_item.w & ~0xF) | 0x5


def case(container, actual, op, x, pad):
    label, expr, elems, shown, _ = actual
    opname, sub, left = op
    tag = f"{container} {label} {opname}"
    vals = {place: item(10 * (i + 1), f"e{i}") for i, place in enumerate(shown)}
    sv, exp = [f"{pad}// {tag}"], []
    for place in shown:
        sv.append(f"{pad}{place} = {vals[place].literal()};")
    actual_items = [vals[e] for e in elems]  # from the actual's left bound

    def show_line(when):
        fmt = " | ".join("%s" for _ in shown)
        args = ", ".join(f"show({place})" for place in shown)
        sv.append(f'{pad}$display("{tag} {when} {fmt}", {args});')

    def expect(when):
        exp.append(f"{tag} {when} " + " | ".join(vals[place].show() for place in shown))

    if opname.startswith("return"):
        # The value's elements from its left bound go to the actual's (7.6).
        sv.append(f"{pad}{expr} = {sub}({x});")
        if opname == "return":
            got = [Item(x, 3, "r", [1, 2, 3]), Item(x + 1, 3, "r", [1, 2, byte(x)])]
        else:
            got = [Item(x, 4, "q", [4, 5, 6]), Item(x, 4, "z", [4, 5, 6])]
        for place, value in zip(elems, got):
            vals[place] = value
        show_line("after")
        expect("after")
        return sv, exp
    if opname == "const_ref":
        sv.append(f'{pad}$display("{tag} sum %0d", {sub}({expr}, {x}));')
        total = actual_items[0].n + actual_items[1].n + actual_items[1].arr[1] + x
        exp.append(f"{tag} sum {total}")
        show_line("after")
        expect("after")
        return sv, exp
    sv += [f"{pad}fork", f"{pad}  {sub}({expr}, {x});",
           f"{pad}  begin"]
    # The formal's left element is the actual's left one (7.6).
    if opname.startswith("output"):
        formal = [Item(x, 1, "o", [1, 2, 3]), Item(x, 1, "p", [1, 2, 3])]
        exp.append(f"{sub} formal {formal[0].show()} | {formal[1].show()}")
        sv.append(f'{pad}    #1 $display("{tag} during {" | ".join("%s" for _ in shown)}", '
                  f'{", ".join(f"show({place})" for place in shown)});')
        expect("during")
        formal[1].n = x + 1
        formal[0].arr[0] = byte(x)
        for place, value in zip(elems, formal):
            vals[place] = value
    elif opname.startswith("ref"):
        # The actual itself: the display between the writes sees the first.
        first = in_first(actual_items[0], actual_items[1], x)
        exp.append(f"{sub} formal {first[0]} | {first[1]}")
        sv.append(f'{pad}    #1 $display("{tag} during {" | ".join("%s" for _ in shown)}", '
                  f'{", ".join(f"show({place})" for place in shown)});')
        expect("during")
        in_second(actual_items[0], actual_items[1])
    else:
        copies = [it.copy() for it in actual_items]
        first = in_first(copies[0], copies[1], x)
        in_second(copies[0], copies[1])
        exp.append(f"{sub} formal {first[0]} | {first[1]}")
        sv.append(f'{pad}    #1 $display("{tag} during {" | ".join("%s" for _ in shown)}", '
                  f'{", ".join(f"show({place})" for place in shown)});')
        expect("during")
        if opname.startswith("inout"):
            for place, value in zip(elems, copies):
                vals[place] = value
    sv += [f"{pad}  end", f"{pad}join"]
    show_line("after")
    expect("after")
    return sv, exp


def body(container, pad):
    sv, exp = [f"{pad}automatic item_t la [0:1];"], []
    x = 3
    for actual in ACTUALS:
        for op in OPS:
            s, e = case(container, actual, op, x, pad)
            sv += s
            exp += e
            x += 1
    # Two calls with their own automatic array of structures overlap.
    # The first prints one and two steps after it starts, the second, which
    # starts a step later, three and four steps after that.
    sv += [f"{pad}// {container} local", f"{pad}fork", f"{pad}  t_local(100, 2);",
           f"{pad}  #1 t_local(200, 3);", f"{pad}join"]
    exp += ["local 100 100 101 l100", "local 100 100 150 l100",
            "local 200 200 201 l200", "local 200 200 250 l200"]
    return sv, exp


def files():
    out = {}
    for container, where in CONTAINERS.items():
        name = f"{NAME}_{container}"
        head = [
            "/*",
            f":name: {name}",
            ":description: arrays of unpacked structures as input, output, inout, ref and const "
            f"ref arguments and as automatic variables, called from {where}",
            ":tags: proc",
            ":type: simulation",
            ":lrm: 1800-2023 13.5 13.5.1 13.5.2 7.6 6.21",
            ":oracle: spec",
            ":spec_check: ai",
            ":fourstate: no",
            ":nondeterministic: no",
            f":generator: gen_struct_array_arg_matrix.py@{VERSION}",
            f":expected_stdout: @{name}.expected",
            "*/",
            "// Generated by tools/gen_struct_array_arg_matrix.py; change the generator, not this "
            "file.",
            "`timescale 1ns/1ns",
            "module top;",
            "  typedef struct { int n; logic [69:0] w; string s; byte arr [0:2]; } item_t;",
            "  typedef struct { item_t list [0:1]; int m; } bag_t;",
            "  typedef item_t pair_t [0:1];",
            "  typedef item_t dpair_t [1:0];",
            "  item_t sa [0:1];",
            "  item_t sd [3:2];",
            "  item_t grid [0:1][0:1];",
            "  bag_t sb;",
        ]
        sv = head + SUBROUTINES.rstrip("\n").split("\n")
        stmts, exp = body(container, "      " if container == "fork" else "    ")
        if container == "initial":
            sv += ["  initial begin"] + stmts + ["  end"]
        elif container == "task":
            sv += ["  initial run();", "  task static run();"] + stmts + ["  endtask"]
        elif container == "always":
            sv += ["  logic go = 1'b0;", "  initial #1 go = 1'b1;",
                   "  always @(posedge go) begin"] + stmts + ["  end"]
        else:
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
                print(f"gen_struct_array_arg_matrix: {path.relative_to(OUT.parent.parent.parent)} "
                      "is out of date; run tools/gen_struct_array_arg_matrix.py", file=sys.stderr)
                bad += 1
        else:
            path.write_text(text, encoding="utf-8")
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main())
