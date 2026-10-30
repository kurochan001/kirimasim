#!/usr/bin/env python3
# Copyright (c) 2026 The kirimasim Authors
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

"""Generates the matrix tests of the members of an inside set of
TEST_SUITE.md ("Mechanism matrix") in tests/suite/values/.

  tools/gen_inside_member_matrix.py           # writes the files
  tools/gen_inside_member_matrix.py --check   # exit 1 if a file differs from what it would write

The rule: VALUE_SEMANTICS.md 2 and 3.10, with 11.4.13, 11.4.6, 11.8.1,
11.8.2 and 12.5.4 of the standard. The left expression of inside and every
member of its set, the elements of an unpacked array in it included, take
one common type: the widest width, signed only when all are signed,
4-state when any is. A member is extended to that type by the type's sign,
not its own (11.8.2), so a signed element compared with a wider unsigned
left expression is zero-extended, and its x or z sign bit fills nothing.
It is then compared with the left expression by ==?, its own x and z bits
matching anything; the set matches when a member gives 1, and gives x
when none does and one gives x. A case inside item matches as such a set
does, the common type taken over the case expression and every item, and
only when it gives 1.

The axes: the types of the left expression and of the elements (bit and
logic, signed and unsigned, the element narrower, as wide and wider, in
one word and across words); their values (an x or z element at its sign
bit and below it, an x left expression); the shape of the array (one
element, three in ascending and in descending order, two dimensions, and
one row of them as the member); another member of the set, or another
case item, that sets the common type; the way of reading (inside with the
array, inside with the same elements listed, which slang converts, and a
case inside item); the place and spelling of the array (a module
variable, an automatic variable of the container, a ref argument, a
package variable, an interface member, a variable of a generate scope, a
static class property, and an inside in a continuous assignment); and the
container of the statements (an initial procedure, an automatic task, an
always procedure and a fork branch; one file each). Every type case is
read from a module variable, and four of them from every other place.

The expected outputs follow from the rule, worked out below without
reading kirimasim's code; on every line the listed form gives what the
array form gives. For reference (2026-10-31): Icarus 13.0 does not support
inside, and Verilator 5.052 rejects these files, as it does not take ==?
with a 4-state right operand that is not a constant. A person checks them
once for all the tests made from them (TEST_SUITE.md, "Sampling by
people").
"""

import argparse
import itertools
import sys
from dataclasses import dataclass
from pathlib import Path

OUT = Path(__file__).resolve().parent.parent / "tests" / "suite" / "values"
NAME = "inside_member_matrix"
# The version of the rules below (TEST_SUITE.md, :generator:). Raise it
# whenever they change.
VERSION = 1

CONTAINERS = {"initial": "an initial procedure", "task": "an automatic task",
              "always": "an always procedure", "fork": "a fork branch"}


@dataclass(frozen=True)
class T:
    """An integral type: 4-state or not, signed or not, its width."""
    four: bool
    signed: bool
    width: int

    @property
    def name(self):
        return f"{'l' if self.four else 'b'}{'s' if self.signed else 'u'}{self.width}_t"

    def typedef(self):
        kind = "logic" if self.four else "bit"
        sign = " signed" if self.signed else ""
        return f"typedef {kind}{sign} [{self.width - 1}:0] {self.name};"

    def literal(self, bits):
        assert len(bits) == self.width and (self.four or set(bits) <= set("01")), bits
        return f"{self.width}'{'s' if self.signed else ''}b{bits}"


BU2, BU3, BS2, BS3 = T(False, False, 2), T(False, False, 3), T(False, True, 2), T(False, True, 3)
LU3, LS2, LS3 = T(True, False, 3), T(True, True, 2), T(True, True, 3)
BU65, BS64, BU72, BS65 = T(False, False, 65), T(False, True, 64), T(False, False, 72), T(
    False, True, 65)
LU72, LS65 = T(True, False, 72), T(True, True, 65)


@dataclass(frozen=True)
class Case:
    """A set with an array: the left expression's type and values, the
    elements' type, the array's dimensions and its elements in order (the
    left bound first, the last dimension fastest), another member of the
    set (its type and value), and the row of the first dimension that is
    the member instead of the whole array."""
    label: str
    left: T
    elem: T
    dims: tuple
    elems: tuple
    lefts: tuple
    extra: tuple = None
    row: int = None


def wide(top):
    """top bits, then 63 zeros and a 1."""
    return top + "0" * 63 + "1"


CASES = [
    Case("narrow_signed_bit", BU3, BS2, ((0, 0),), ("11",), ("111", "011")),
    Case("both_signed_bit", BS3, BS2, ((0, 0),), ("11",), ("111", "011")),
    Case("wider_unsigned_element", BS2, BU3, ((0, 1),), ("111", "001"), ("11", "01")),
    Case("wider_signed_element", BS2, BS3, ((0, 0),), ("111",), ("11", "01")),
    Case("equal_width", BU2, BS2, ((0, 0),), ("11",), ("11", "01")),
    Case("narrow_unsigned_element", BU3, BU2, ((0, 0),), ("11",), ("111", "011")),
    Case("signed_left_unsigned_element", BS3, BU2, ((0, 0),), ("11",), ("111", "011")),
    Case("logic_x_sign", LU3, LS2, ((0, 0),), ("x1",), ("111", "011", "001")),
    Case("logic_z_sign", LU3, LS2, ((0, 0),), ("z0",), ("110", "010", "000")),
    Case("logic_x_sign_signed_left", LS3, LS2, ((0, 0),), ("x1",), ("111", "101", "110")),
    Case("logic_x_below_sign", LU3, LS2, ((0, 0),), ("1x",), ("111", "010", "0x0")),
    Case("logic_left_x", LU3, BS2, ((0, 0),), ("11",), ("x11", "1x1")),
    Case("logic_element_bit_left", BU3, LS2, ((0, 0),), ("x1",), ("111", "001")),
    Case("ascending", BU3, BS2, ((0, 2),), ("01", "10", "11"), ("111", "110", "010", "011")),
    Case("descending", BU3, BS2, ((2, 0),), ("01", "10", "11"), ("111", "110", "010", "011")),
    Case("two_dimensions", BU3, BS2, ((0, 1), (0, 1)), ("00", "01", "10", "11"),
         ("110", "111", "010", "000")),
    Case("row", BU3, BS2, ((0, 1), (0, 1)), ("00", "01", "10", "11"),
         ("010", "011", "001", "111"), row=1),
    Case("common_from_member", BS2, BS2, ((0, 0),), ("11",), ("11", "01"), extra=(BU3, "100")),
    Case("common_from_signed_member", BS2, BS2, ((0, 0),), ("11",), ("11", "01"),
         extra=(BS3, "100")),
    Case("word_boundary", BU65, BS64, ((0, 0),), ("1" + "0" * 63,),
         ("01" + "0" * 63, "11" + "0" * 63)),
    Case("wide_word", BU72, BS65, ((0, 0),), (wide("1"),),
         (wide("00000001"), wide("11111111"))),
    Case("wide_word_x_sign", LU72, LS65, ((0, 0),), (wide("x"),),
         (wide("00000001"), wide("00000000"), wide("11111110"))),
]
# The cases read from every place, not only from a module variable.
PLACE_CASES = ["narrow_signed_bit", "logic_x_sign", "common_from_member", "wide_word"]
PLACES = ["automatic", "ref", "package", "interface", "generate", "class", "assign"]


def place_cases():
    """The cases of PLACE_CASES with their numbers, which name their variables."""
    return [(k, c) for k, c in enumerate(CASES) if c.label in PLACE_CASES]


# --- The rule ----------------------------------------------------------------


def common(c):
    types = [c.left, c.elem] + ([c.extra[0]] if c.extra else [])
    return T(any(t.four for t in types), all(t.signed for t in types),
             max(t.width for t in types))


def extend(bits, to):
    """A value extended to type to by to's sign (11.8.2)."""
    return (bits[0] if to.signed else "0") * (to.width - len(bits)) + bits


def wildcard_equal(a, b):
    """a ==? b (11.4.6): x and z of b match anything; otherwise a differing
    known bit gives 0, and an x or z of a gives x."""
    result = "1"
    for p, q in zip(a, b):
        if q in "xz":
            continue
        if p in "xz":
            result = "x"
        elif p != q:
            return "0"
    return result


def any_of(results):
    return "1" if "1" in results else "x" if "x" in results else "0"


def indices(c):
    """The element indices in order, the left bound first, the last
    dimension fastest."""
    ranges = [range(l, r + 1) if l <= r else range(l, r - 1, -1) for l, r in c.dims]
    return list(itertools.product(*ranges))


def members(c):
    """The indices and values of the elements of the set's array member."""
    pairs = list(zip(indices(c), c.elems))
    return [(i, v) for i, v in pairs if c.row is None or i[0] == c.row]


def expect(c, v):
    """array (also listed) and case for left value v."""
    ct = common(c)
    lv = extend(v, ct)
    arr = any_of([wildcard_equal(lv, extend(e, ct)) for _, e in members(c)])
    extra = wildcard_equal(lv, extend(c.extra[1], ct)) if c.extra else "0"
    whole = any_of([arr, extra])
    hit = 1 if arr == "1" else 2 if extra == "1" else 0
    return whole, hit


# --- The text ----------------------------------------------------------------


def dims(c):
    return "".join(f"[{l}:{r}]" for l, r in c.dims)


def sub(name, index):
    return name + "".join(f"[{i}]" for i in index)


def check(c, label, v, arr, show, out, pad):
    """Lines comparing v with the member arr; show is what array= prints."""
    member = arr if c.row is None else f"{arr}[{c.row}]"
    listed = ", ".join(sub(arr, i) for i, _ in members(c))
    extra = f", {c.extra[0].literal(c.extra[1])}" if c.extra else ""
    item = f" {c.extra[0].literal(c.extra[1])}: hit = 2;" if c.extra else ""
    out += [f"{pad}case ({v}) inside {member}: hit = 1;{item} default: hit = 0; endcase",
            f'{pad}$display("{label} v=%b array=%b listed=%b case=%0d", {v}, '
            f"{show or f'{v} inside {{{member}{extra}}}'}, {v} inside {{{listed}{extra}}}, hit);"]


def setup(c, arr, out, pad):
    out += [f"{pad}{sub(arr, i)} = {c.elem.literal(e)};" for i, e in zip(indices(c), c.elems)]


def place_name(place, k):
    return {"automatic": f"a{k}", "ref": f"r{k}", "package": f"set_pkg::p{k}",
            "interface": f"i.n{k}", "generate": f"g.g{k}", "class": f"C::c{k}",
            "assign": f"m{k}"}[place]


def body(pad):
    """The statements of the container, and the lines they print."""
    out, exp = [], []
    places = place_cases()
    out += [f"{pad}begin"]
    inner = pad + "  "
    out += [f"{inner}automatic {c.elem.name} a{k} {dims(c)};" for k, c in places]
    for k, c in enumerate(CASES):
        setup(c, f"m{k}", out, inner)
        for v in c.lefts:
            out += [f"{inner}l{k} = {c.left.literal(v)};"]
            check(c, f"module {c.label}", f"l{k}", f"m{k}", None, out, inner)
            whole, hit = expect(c, v)
            exp.append(f"module {c.label} v={v} array={whole} listed={whole} case={hit}")
    for place in PLACES:
        for k, c in places:
            arr = place_name(place, k)
            setup(c, arr, out, inner)
            for v in c.lefts:
                label = f"{place} {c.label}"
                if place == "ref":
                    out += [f"{inner}by_ref{k}({arr}, {c.left.literal(v)});"]
                else:
                    out += [f"{inner}l{k} = {c.left.literal(v)};"]
                    if place == "assign":
                        out += [f"{inner}#1;"]
                    check(c, label, f"l{k}", arr, f"w{k}" if place == "assign" else None, out,
                          inner)
                whole, hit = expect(c, v)
                exp.append(f"{label} v={v} array={whole} listed={whole} case={hit}")
    out += [f"{pad}end"]
    return out, exp


def declarations():
    types = sorted({t for c in CASES for t in [c.left, c.elem] + ([c.extra[0]] if c.extra else [])},
                   key=lambda t: (t.width, t.four, t.signed))
    places = place_cases()
    sv = ["package set_pkg;"]
    sv += [f"  {t.typedef()}" for t in types]
    sv += [f"  {c.elem.name} p{k} {dims(c)};" for k, c in places]
    sv += ["endpackage", "interface set_ifc;", "  import set_pkg::*;"]
    sv += [f"  {c.elem.name} n{k} {dims(c)};" for k, c in places]
    sv += ["endinterface", "module top;", "  import set_pkg::*;", "  int hit;"]
    sv += [f"  {c.left.name} l{k};" for k, c in enumerate(CASES)]
    sv += [f"  {c.elem.name} m{k} {dims(c)};" for k, c in enumerate(CASES)]
    sv += [f"  {c.elem.name} r{k} {dims(c)};" for k, c in places]
    for k, c in places:
        extra = f", {c.extra[0].literal(c.extra[1])}" if c.extra else ""
        sv += [f"  wire w{k} = l{k} inside {{m{k}{extra}}};"]
    sv += ["  set_ifc i();", "  if (1) begin : g"]
    sv += [f"    {c.elem.name} g{k} {dims(c)};" for k, c in places]
    sv += ["  end", "  class C;"]
    sv += [f"    static {c.elem.name} c{k} {dims(c)};" for k, c in places]
    sv += ["  endclass"]
    for k, c in places:
        sv += [f"  function automatic void by_ref{k}(ref {c.elem.name} a {dims(c)}, "
               f"input {c.left.name} v);", "    int hit;"]
        lines = []
        check(c, f"ref {c.label}", "v", "a", None, lines, "    ")
        sv += lines + ["  endfunction"]
    return sv


def files():
    out = {}
    for container, what in CONTAINERS.items():
        name = f"{NAME}_{container}"
        sv = [
            "/*",
            f":name: {name}",
            ":description: inside and case inside compare each element of an unpacked array in "
            "the set at the set's common type, extended by that type's sign as a listed member "
            f"is, in {what}",
            ":tags: values",
            ":type: simulation",
            ":lrm: 1800-2023 11.4.13 11.4.6 11.8.1 11.8.2 12.5.4",
            ":oracle: spec",
            ":spec_check: ai",
            ":fourstate: yes",
            ":nondeterministic: no",
            f":generator: gen_inside_member_matrix.py@{VERSION}",
            f":expected_stdout: @{name}.expected",
            "*/",
            "// Generated by tools/gen_inside_member_matrix.py; change the generator, not this "
            "file.",
            "`timescale 1ns/1ns",
        ]
        sv += declarations()
        if container == "initial":
            stmts, exp = body("    ")
            sv += ["  initial"] + stmts
        elif container == "task":
            stmts, exp = body("    ")
            sv += ["  initial run();", "  task automatic run();"] + stmts + ["  endtask"]
        elif container == "always":
            stmts, exp = body("    ")
            sv += ["  logic go = 1'b0;", "  initial #1 go = 1'b1;", "  always @(posedge go)"]
            sv += stmts
        else:
            stmts, exp = body("      ")
            sv += ["  initial", "    fork"] + stmts + ["    join"]
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
                print(f"gen_inside_member_matrix: {path.relative_to(OUT.parent.parent.parent)} is "
                      "out of date; run tools/gen_inside_member_matrix.py", file=sys.stderr)
                bad += 1
        else:
            path.write_text(text, encoding="utf-8")
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main())
