#!/usr/bin/env python3
"""Generates the matrix test of assignment operators and increments on a
character of a string, of TEST_SUITE.md ("Mechanism matrix"), in
tests/suite/values/.

  tools/gen_string_char_op_matrix.py           # writes the files
  tools/gen_string_char_op_matrix.py --check   # exit 1 if a file differs from what it would write

s[i] op= v is s[i] = s[i] op (v), with s[i] read and written once
(11.4.1, VALUE_SEMANTICS.md 3.12); s[i] is a byte, 0 outside the string,
and a write outside the string, or of 0, changes nothing (6.16, Table
6-9; VALUE_SEMANTICS.md 7.6). ++ and -- are the same with 1, the postfix
form giving the old value (11.4.2).

The axes: the operator (+=, -=, *=, /=, %=, &=, |=, ^=, <<=, >>=, <<<=,
>>>=, prefix and postfix ++ and --); the character (two, and the result
0, which changes nothing); the position (inside, past the end, -1 and x,
where nothing changes); the place (a module variable, an element of a
module's array of strings, an automatic variable of a task, and a string
through a ref argument). Each line prints the value an increment gives
inside the string, and the length and the bytes of the string after it.

Two things are left out, as no document of kirimasim decides them yet
(not confirmed): whether s[i] is signed, as a byte is (6.11; it matters
for bytes of 128 and more), and the value of ++s[i] and --s[i] where the
write changes nothing. Every character and result here is from 1 to 127.

The expected outputs follow from the rules above, computed below. For
reference (checked 2026-10-18): Icarus Verilog 13.0 and Verilator 5.052
do not take these forms (++ on a character, and some of the operators);
with each operator written out as an assignment, both print the 65 lines
of the module variable at the positions other than x as expected.
"""

from __future__ import annotations

import argparse
import sys
from pathlib import Path

OUT = Path(__file__).resolve().parent.parent / "tests" / "suite" / "values"
NAME = "string_char_op_matrix"
# The version of the rules below (TEST_SUITE.md, :generator:). Raise it
# whenever they change.
VERSION = 1

# The string each case starts from.
BASE = [0x61, 0x30, 0x42, 0x25]
BASE_TEXT = "a0B%"
# (name, the operator, its right side, the operation on a signed byte and
# the right side)
OPS = [
    ("add", "+=", "2", lambda a: a + 2),
    ("sub", "-=", "1", lambda a: a - 1),
    ("mul", "*=", "2", lambda a: a * 2),
    ("div", "/=", "2", lambda a: int(a / 2)),
    ("mod", "%=", "7", lambda a: a - 7 * int(a / 7)),
    ("and", "&=", "8'h5d", lambda a: (a & 0xFF) & 0x5D),
    ("or", "|=", "8'h21", lambda a: (a & 0xFF) | 0x21),
    ("xor", "^=", "8'h21", lambda a: (a & 0xFF) ^ 0x21),
    ("shl", "<<=", "1", lambda a: a << 1),
    ("shr", ">>=", "1", lambda a: (a & 0xFF) >> 1),
    ("ashl", "<<<=", "1", lambda a: a << 1),
    ("ashr", ">>>=", "1", lambda a: a >> 1),
]
# (name, the statement with S for the string's character, whether its
# value is printed, the new byte, the value)
INCS = [
    ("post_inc", "n = S++;", lambda a: a + 1, lambda a: a),
    ("pre_inc", "n = ++S;", lambda a: a + 1, lambda a: a + 1),
    ("post_dec", "n = S--;", lambda a: a - 1, lambda a: a),
    ("pre_dec", "n = --S;", lambda a: a - 1, lambda a: a - 1),
]
# (name, the index text, the character position or None outside)
POSITIONS = [("p1", "1", 1), ("p3", "3", 3), ("end", "4", None), ("neg", "-1", None),
             ("x", "ix", None)]
PLACES = ["module", "element", "task", "ref"]


def sbyte(v):
    v &= 0xFF
    return v - 256 if v & 0x80 else v


def apply(pos, op):
    """The bytes after `op` at `pos`, and the value of the expression."""
    after = list(BASE)
    old = sbyte(BASE[pos]) if pos is not None else 0
    new, value = op(old)
    if pos is not None and new & 0xFF != 0:
        after[pos] = new & 0xFF
    return after, value


def show(after):
    return f"{len(after)} " + " ".join(str(sbyte(b)) for b in after)


def cases():
    """(name, the statement with S for s[i], whether n is printed, the
    function of the old byte giving (new byte, n))."""
    for name, op, rhs, f in OPS:
        yield name, f"S {op} {rhs};", False, (lambda f: lambda a: (f(a), None))(f)
    for name, stmt, f, g in INCS:
        yield name, stmt, True, (lambda f, g: lambda a: (f(a), sbyte(g(a))))(f, g)
    # The result 0: the character minus itself; nothing changes.
    yield "zero", "S -= 8'h30;", False, lambda a: (a - 0x30, None)


def files():
    module, run, helpers, exp, run_exp = [], [], [], [], []
    for place in PLACES:
        lines = run if place in ("task", "ref") else module
        target = {"module": "s", "element": "sa[1]", "task": "t", "ref": "t"}[place]
        for pname, ix, pos in POSITIONS:
            for name, stmt, shows, f in cases():
                if name == "zero" and pos != 1:
                    continue
                shows = shows and pos is not None
                tag = f"{place} {pname} {name}"
                after, value = apply(pos, f)
                lines.append(f'    {target} = "{BASE_TEXT}";')
                if place == "ref":
                    # The operation in a task of its own, on what its ref
                    # argument names.
                    k = len(helpers)
                    helpers.append(f"  task automatic r{k}(ref string r);\n"
                                   f"    {stmt.replace('S', f'r[{ix}]')}\n  endtask")
                    lines.append(f"    r{k}({target});")
                else:
                    lines.append(f"    {stmt.replace('S', f'{target}[{ix}]')}")
                bytes_ = ", ".join(f"{target}[{k}]" for k in range(4))
                fmt = "%0d " * 4 + "%0d"
                if shows:
                    lines.append(f'    $display("{tag} %0d {fmt}", n, {target}.len(), {bytes_});')
                    want = f"{tag} {value} {show(after)}"
                else:
                    lines.append(f'    $display("{tag} {fmt}", {target}.len(), {bytes_});')
                    want = f"{tag} {show(after)}"
                (run_exp if place in ("task", "ref") else exp).append(want)
    head = [
        "/*",
        f":name: {NAME}",
        ":description: assignment operators, ++ and -- on a character of a string: every "
        "operator on a positive and a negative character, a result of 0, and positions past "
        "the end, at -1 and at x, which change nothing, in a module variable, an element of "
        "an array of strings, an automatic variable and a string through a ref argument",
        ":tags: values string",
        ":type: simulation",
        ":lrm: 1800-2023 6.16 11.4.1 11.4.2",
        ":oracle: spec",
        ":spec_check: ai",
        ":fourstate: yes",
        ":nondeterministic: no",
        f":generator: gen_string_char_op_matrix.py@{VERSION}",
        ":top_module: top",
        f":expected_stdout: @{NAME}.expected",
        "*/",
        "// Generated by tools/gen_string_char_op_matrix.py; change the generator, not this "
        "file.",
        "module top;",
        "  string s;",
        "  string sa [0:2];",
        "  integer ix = 'x;",
        "  int n;",
    ]
    sv = head + helpers
    sv += ["  task automatic run();", "    string t;"] + run + ["  endtask"]
    sv += ["  initial begin"] + module + ["    run();", "  end", "endmodule", ""]
    return {f"{NAME}.sv": "\n".join(sv), f"{NAME}.expected": "\n".join(exp + run_exp) + "\n"}


def main():
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("--check", action="store_true")
    args = ap.parse_args()
    bad = 0
    for name, text in files().items():
        path = OUT / name
        if args.check:
            if not path.exists() or path.read_text(encoding="utf-8") != text:
                print(f"gen_string_char_op_matrix: {path.relative_to(OUT.parent.parent.parent)} "
                      "is out of date; run tools/gen_string_char_op_matrix.py", file=sys.stderr)
                bad += 1
        else:
            path.write_text(text, encoding="utf-8")
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main())
