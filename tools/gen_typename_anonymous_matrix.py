#!/usr/bin/env python3
# Copyright (c) 2026 The kirimasim Authors
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

"""Generates the matrix tests of the names $typename gives to anonymous
types, of TEST_SUITE.md ("Mechanism matrix"), in tests/suite/values/.

  tools/gen_typename_anonymous_matrix.py           # writes the files
  tools/gen_typename_anonymous_matrix.py --check   # exit 1 if a file differs from what it would write

$typename names an anonymous enumeration, structure or union by the
namespace it is declared in and its order of appearance there, as e$N,
s$N and u$N, and the same type gives the same string (20.6.1;
SYSTEM_TASKS.md 8). The variables of one declaration have one anonymous
type, and a second declaration with the same members has another (6.22.1
c). So the variables of one declaration get one name, with them the
anonymous members inside them, and the next declaration gets the next
number. A type declared in another namespace and named here through
type() keeps the name it has there and takes no number here.

The axes: the kind of type (structure, packed structure, packed union,
enumeration); the shape of the declaration (two variables of one
declaration, a second declaration, two unpacked arrays of one
declaration, an anonymous member of two variables of one declaration, two
members of one member declaration, a variable of the type of another
namespace's variable); how the argument names the type (a variable, a
member, through type()); and the namespace (the module, a package, the
compilation unit, a named block, an unnamed block, an automatic function,
a static task, a generate loop, an interface, a class).

Each row compares two strings at run time and prints them with the
result. In a class and in an unnamed block only the result is printed:
what their types' names are prefixed with is not settled by
SYSTEM_TASKS.md 8 (not confirmed), and the identity of the types within
one of them does not depend on it. A class has no test of
unpacked structures, and there an enumeration sits in a packed structure;
no test has an unpacked union: kirimasim does not run these yet. An
enumeration's second declaration has other constant names, as one scope
cannot declare a name twice.

The expected outputs follow from the rules above, computed below.
"""

from __future__ import annotations

import argparse
import sys
from pathlib import Path

OUT = Path(__file__).resolve().parent.parent / "tests" / "suite" / "values"
PREFIX = "typename_anonymous_"
# The version of the rules below (TEST_SUITE.md, :generator:). Raise it
# whenever they change.
VERSION = 1

# What the type of the other namespace's variable is called.
OTHER = "struct packed{int f;}other::s$1"


class Anon:
    """An anonymous type: how it is written, the letter of its kind and its
    $typename body, with the anonymous types of its members in order."""

    def __init__(self, text, letter, body, inner=None):
        self.text, self.letter, self.body, self.inner = text, letter, body, inner


def leaf(kind, n):
    """The type of the rows without anonymous members; n makes the names of
    an enumeration's constants unique."""
    if kind == "struct":
        return Anon("struct {int f; byte g;}", "s", "struct{int f;byte g;}")
    if kind == "struct_packed":
        return Anon("struct packed {int f; byte g;}", "s", "struct packed{int f;byte g;}")
    if kind == "union_packed":
        return Anon("union packed {int f; bit [31:0] g;}", "u",
                    "union packed{int f;bit [31:0] g;}")
    return Anon(f"enum {{A{n}, B{n}}}", "e", f"enum{{A{n}=32'sd0,B{n}=32'sd1}}")


def inner_of(kind, n):
    if kind == "struct":
        return Anon("struct {int x;}", "s", "struct{int x;}")
    if kind == "struct_packed":
        return Anon("struct packed {int x;}", "s", "struct packed{int x;}")
    if kind == "union_packed":
        return Anon("union packed {int x; bit [31:0] z;}", "u",
                    "union packed{int x;bit [31:0] z;}")
    return Anon(f"enum {{P{n}, Q{n}}}", "e", f"enum{{P{n}=32'sd0,Q{n}=32'sd1}}")


def outer_of(kind, packed, inner, members):
    """A structure or union whose members, of one member declaration, have
    the anonymous type inner. An enumeration sits in a structure."""
    if kind == "union_packed":
        word, letter = "union packed", "u"
    elif kind == "struct_packed" or packed:
        word, letter = "struct packed", "s"
    else:
        word, letter = "struct", "s"
    text = f"{word} {{{inner.text} {', '.join(members)};}}"
    return Anon(text, letter, (word, members), inner)


class Namespace:
    """Numbers the anonymous types of one namespace in the order they
    appear, by kind."""

    def __init__(self, prefix):
        self.prefix, self.count = prefix, {"s": 0, "u": 0, "e": 0}

    def name(self, anon):
        """The string of a type that appears now, and of its members."""
        self.count[anon.letter] += 1
        own = f"{self.prefix}{anon.letter}${self.count[anon.letter]}"
        if anon.inner is None:
            return anon.body + own, None
        member, _ = self.name(anon.inner)
        word, members = anon.body
        return word + "{" + "".join(f"{member} {m};" for m in members) + "}" + own, member


def rows(kind, prefix, in_class):
    """The declarations of one namespace, and the rows: (label, first
    argument, second argument, the two strings, whether they are equal)."""
    ns = Namespace(prefix)
    decls = ["var type(other::o) t;"]
    out = [("type of another namespace", "t", "other::o", OTHER, OTHER)]
    a, _ = ns.name(leaf(kind, 1))
    decls.append(f"{leaf(kind, 1).text} a1, a2;")
    out.append(("one declaration", "a1", "a2", a, a))
    b, _ = ns.name(leaf(kind, 2))
    decls.append(f"{leaf(kind, 2).text} b;")
    out.append(("second declaration", "a1", "b", a, b))
    c, _ = ns.name(leaf(kind, 3))
    decls.append(f"{leaf(kind, 3).text} c1 [2], c2 [2];")
    out.append(("arrays of one declaration", "c1", "c2", c + "$[0:1]", c + "$[0:1]"))
    nested = outer_of(kind, in_class, inner_of(kind, 4), ["in"])
    d, d_in = ns.name(nested)
    decls.append(f"{nested.text} d1, d2;")
    out.append(("member of one declaration", "d1", "d2", d, d))
    out.append(("member of one declaration, selected", "d1.in", "d2.in", d_in, d_in))
    pair = outer_of(kind, in_class, inner_of(kind, 5), ["p", "q"])
    _, e_in = ns.name(pair)
    decls.append(f"{pair.text} e;")
    out.append(("two members of one declaration", "e.p", "e.q", e_in, e_in))
    return decls, out


def compare(row, qualifier, identity_only, indent):
    label, x, y, _, _ = row
    x = x if x.startswith("other::") else qualifier + x
    y = y if y.startswith("other::") else qualifier + y
    show = (f'$display("{label}: equal %0d", sx == sy);' if identity_only else
            f'$display("{label}: %s | %s equal %0d", sx, sy, sx == sy);')
    return [f"{indent}sx = $typename({x});", f"{indent}sy = $typename({y});", f"{indent}{show}"]


def expected(row, identity_only):
    label, _, _, sx, sy = row
    if identity_only:
        return f"{label}: equal {int(sx == sy)}"
    return f"{label}: {sx} | {sy} equal {int(sx == sy)}"


# (name, what the description calls it)
PLACES = [
    ("module", "the module"), ("package", "a package"), ("unit", "the compilation unit"),
    ("block", "a named block"), ("unnamed_block", "an unnamed block"),
    ("function", "an automatic function"), ("task", "a static task"),
    ("generate", "a generate loop"), ("interface", "an interface"), ("class", "a class"),
]
KINDS = [("struct", "structures"), ("struct_packed", "packed structures"),
         ("union_packed", "packed unions"), ("enum", "enumerations")]


def header(name, description):
    return [
        "/*",
        f":name: {name}",
        f":description: {description}",
        ":tags: values",
        ":type: simulation",
        ":lrm: 1800-2023 20.6.1 6.22.1",
        ":oracle: spec",
        ":spec_check: ai",
        ":fourstate: no",
        ":nondeterministic: no",
        f":generator: gen_typename_anonymous_matrix.py@{VERSION}",
        ":issue: 133",
        ":top_module: top",
        f":expected_stdout: @{name}.expected",
        "*/",
        "// Generated by tools/gen_typename_anonymous_matrix.py; change the generator, not this "
        "file.",
        "// The form of the names is kirimasim's (SYSTEM_TASKS.md 8); 20.6.1 leaves it to the",
        "// tool. What the standard fixes is which of the strings are equal.",
        "`timescale 1ns/1ns",
        "package other;",
        "  struct packed {int f;} o;",
        "endpackage",
    ]


def case(place, kind):
    pname, where = place
    kname, what = kind
    name = f"{PREFIX}{pname}_{kname}"
    in_class = pname == "class"
    identity_only = pname in ("class", "unnamed_block")
    strings = "string sx, sy;"

    def body(prefix, qualifier, indent):
        decls, table = rows(kname, prefix, in_class)
        code = [line for row in table
                for line in compare(row, qualifier, identity_only, indent)]
        return decls, code, [expected(row, identity_only) for row in table]

    if pname == "module":
        decls, code, exp = body("top.", "", "    ")
        lines = (["module top;"] + [f"  {d}" for d in decls] +
                 [f"  {strings}", "  initial begin"] + code + ["  end", "endmodule"])
    elif pname == "package":
        decls, code, exp = body("pk::", "pk::", "    ")
        lines = (["package pk;"] + [f"  {d}" for d in decls] + ["endpackage", "module top;",
                 f"  {strings}", "  initial begin"] + code + ["  end", "endmodule"])
    elif pname == "unit":
        decls, code, exp = body("$unit::", "", "    ")
        lines = (decls + ["module top;", f"  {strings}", "  initial begin"] + code +
                 ["  end", "endmodule"])
    elif pname == "block":
        decls, code, exp = body("top.blk.", "", "      ")
        lines = (["module top;", "  initial begin", "    begin : blk"] +
                 [f"      {d}" for d in decls] + [f"      {strings}"] + code +
                 ["    end", "  end", "endmodule"])
    elif pname == "unnamed_block":
        decls, code, exp = body("", "", "    ")
        lines = (["module top;", "  initial begin"] + [f"    {d}" for d in decls] +
                 [f"    {strings}"] + code + ["  end", "endmodule"])
    elif pname in ("function", "task"):
        sub = "function automatic void sub();" if pname == "function" else "task sub();"
        decls, code, exp = body("top.sub.", "", "    ")
        lines = (["module top;", f"  {sub}"] + [f"    {d}" for d in decls] + [f"    {strings}"] +
                 code + [f"  end{pname}", "  initial sub();", "endmodule"])
    elif pname == "generate":
        lines = ["module top;", "  for (genvar g = 0; g < 2; g++) begin : gen"]
        exp = []
        for g in range(2):
            decls, code, e = body(f"top.gen[{g}].", "", "      ")
            exp += e
        lines += ([f"    {d}" for d in decls] + [f"    {strings}", "    initial begin", "      #g;"] +
                  code + ["    end", "  end", "endmodule"])
    elif pname == "interface":
        decls, code, exp = body("top.i0.", "", "    ")
        lines = (["interface ifc;"] + [f"  {d}" for d in decls] +
                 [f"  {strings}", "  initial begin"] + code +
                 ["  end", "endinterface", "module top;", "  ifc i0();", "endmodule"])
    else:
        decls, code, exp = body("", "", "    ")
        lines = (["class C;"] + [f"  {d}" for d in decls] + ["  function void show();",
                 f"    {strings}"] + code + ["  endfunction", "endclass", "module top;",
                 "  C c = new;", "  initial c.show();", "endmodule"])
    description = (f"$typename of anonymous {what} declared in {where}: the variables of one "
                   "declaration and the anonymous members inside them get one name, a second "
                   "declaration gets the next one, and a type of another namespace keeps its "
                   "name and takes no number")
    return name, header(name, description) + lines, exp


def files():
    out = {}
    for place in PLACES:
        for kind in KINDS:
            if place[0] == "class" and kind[0] == "struct":
                continue  # kirimasim does not run unpacked structures as class properties yet
            name, text, exp = case(place, kind)
            out[f"{name}.sv"] = "\n".join(text) + "\n"
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
                print(f"gen_typename_anonymous_matrix: {path.relative_to(OUT.parent.parent.parent)} "
                      "is out of date; run tools/gen_typename_anonymous_matrix.py",
                      file=sys.stderr)
                bad += 1
        else:
            path.write_text(text, encoding="utf-8")
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main())
