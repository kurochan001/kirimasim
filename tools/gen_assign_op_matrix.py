#!/usr/bin/env python3
# Copyright (c) 2026 The kirimasim Authors
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

"""Generates the assignment operator matrix tests of TEST_SUITE.md
("4-state matrix", "Mechanism matrix") in tests/suite/values/.

  tools/gen_assign_op_matrix.py           # writes the files
  tools/gen_assign_op_matrix.py --check   # exit 1 if a file differs from what it would write

The increment and decrement operators and the assignment operators read,
change and write their operand: `a op= b` is `a = a op (b)` with the index
expressions of `a` evaluated once (11.4.1), `++` and `--` behave as
blocking assignments and step a real by 1.0 (11.4.2). The axes: every
operator, the types int, byte, logic [7:0] (with and without x bits),
real and shortreal, the target (a whole variable, a part, a bit, an
element of an unpacked array with a constant index and with an index
whose function counts its calls, a part of an element), and four
containers (module variables by short and by hierarchical names, the
variables of a static task, and of an automatic function). The automatic
function has whole variables only, since automatic arrays and parts of
automatic variables are not supported yet; they join the matrix with
their support. Increments and decrements evaluate the index of their
target once too (VALUE_SEMANTICS.md 3.12).

The expected outputs follow from the model below (11.4.1 to 11.4.3,
11.4.8, 11.4.10, 11.8). A person checks the model against the standard
once for all the tests made from it (TEST_SUITE.md, "Sampling by people").
"""

import argparse
import re
import struct
import sys
from pathlib import Path

OUT = Path(__file__).resolve().parent.parent / "tests" / "suite" / "values"
# The version of the rules below (TEST_SUITE.md, :generator:). Raise it
# whenever they change.
VERSION = 2

# Integral types: name -> (declaration, width, signed, four-state, display).
INTEGRAL = {
    "int": ("int", 32, True, False, "%0d"),
    "byte": ("byte", 8, True, False, "%0d"),
    "logic8": ("logic [7:0]", 8, False, True, "%b"),
}
COMPOUND = ["+=", "-=", "*=", "/=", "%=", "&=", "|=", "^=", "<<=", ">>=", "<<<=", ">>>="]


# --- The model -----------------------------------------------------------------

def bits_of(v, w):
    return format(v % (1 << w), f"0{w}b")


def known(b):
    return all(c in "01" for c in b)


def to_int(b, signed):
    v = int(b, 2)
    return v - (1 << len(b)) if signed and b[0] == "1" else v


def extend(b, w, signed):
    if len(b) >= w:
        return b[len(b) - w:]
    return (b[0] if signed else "0") * (w - len(b)) + b


def bitwise(op, a, b):
    def one(x, y):
        x = "x" if x == "z" else x
        y = "x" if y == "z" else y
        if op == "&":
            return "0" if "0" in (x, y) else ("1" if x == y == "1" else "x")
        if op == "|":
            return "1" if "1" in (x, y) else ("0" if x == y == "0" else "x")
        return "x" if "x" in (x, y) else str(int(x) ^ int(y))
    return "".join(one(x, y) for x, y in zip(a, b))


def binary(op, a, sa, b, sb):
    """a op b for bit strings a and b with signs sa and sb (11.8.1, 11.8.2):
    the result is (bits, width, signed)."""
    if op in ("<<", ">>", "<<<", ">>>"):
        w, signed = len(a), sa  # the amount is self-determined (11.6.1)
        if not known(b):
            return "x" * w, w, signed
        n = min(int(b, 2), w)
        if op in ("<<", "<<<"):
            return a[n:] + "0" * n, w, signed
        fill = a[0] if op == ">>>" and signed else "0"  # 11.4.10
        return fill * n + a[:w - n], w, signed
    w, signed = max(len(a), len(b)), sa and sb
    a, b = extend(a, w, signed), extend(b, w, signed)
    if op in "&|^":
        return bitwise(op, a, b), w, signed
    if not (known(a) and known(b)):
        return "x" * w, w, signed  # 11.4.3
    x, y = to_int(a, signed), to_int(b, signed)
    if op in "/%" and y == 0:
        return "x" * w, w, signed
    if op == "+":
        r = x + y
    elif op == "-":
        r = x - y
    elif op == "*":
        r = x * y
    else:
        q = abs(x) // abs(y) * (1 if (x < 0) == (y < 0) else -1)  # toward zero
        r = q if op == "/" else x - y * q  # the remainder takes the sign of x
    return bits_of(r, w), w, signed


def assign(t, bits):
    """The value of an expression stored in a variable of type t (10.7):
    truncated or extended, and x and z become 0 in a 2-state type."""
    _, w, _, four, _ = INTEGRAL[t]
    bits = bits[len(bits) - w:] if len(bits) >= w else bits.rjust(w, "0")
    return bits if four else "".join(c if c in "01" else "0" for c in bits)


def show(t, bits):
    _, _, signed, _, fmt = INTEGRAL[t]
    if fmt == "%b":
        return bits
    return str(to_int(bits, signed))


def literal(bits, signed):
    return f"{len(bits)}'{'s' if signed else ''}b{bits}"


def f32(x):
    return struct.unpack("f", struct.pack("f", x))[0]


# --- The cases -----------------------------------------------------------------
# A case is (label, declarations, statements, $display arguments, expected):
# the declarations as (type, name, initial value); the statements use the
# names; the $display format is "<label> %..." with one specifier per
# argument.

def integral_cases(t, init_bits, operands, ops):
    """`v op= b` on a whole variable v of type t."""
    decl, w, signed, four, fmt = INTEGRAL[t]
    out = []
    for op in ops:
        for bname, (bbits, bsigned) in operands.items():
            label = f"{t} {op} {bname}"
            r, _, _ = binary(op[:-1], init_bits, signed, bbits, bsigned)
            out.append((label, [(t, "v", init_bits)], [f"v {op} {bname};"],
                        [("v", fmt)], show(t, assign(t, r))))
    return out


def incdec_cases(t, init_bits):
    decl, w, signed, four, fmt = INTEGRAL[t]
    out = []
    one = bits_of(1, 32)
    up, _, _ = binary("+", init_bits, signed, one, True)
    down, _, _ = binary("-", init_bits, signed, one, True)
    up, down = assign(t, up), assign(t, down)
    old = assign(t, init_bits)
    for form, new, val in [("++v", up, up), ("v++", up, old), ("--v", down, down),
                           ("v--", down, old)]:
        out.append((f"{t} {form};", [(t, "v", init_bits)], [f"{form};"], [("v", fmt)],
                    show(t, new)))
        out.append((f"{t} y = {form}", [(t, "v", init_bits), (t, "y", bits_of(0, w))],
                    [f"y = {form};"], [("v", fmt), ("y", fmt)],
                    f"{show(t, new)} {show(t, val)}"))
    return out


def real_cases():
    out = []
    for t, conv in (("real", float), ("shortreal", f32)):
        init = 1.5
        for form, new, val in [("++v", 2.5, 2.5), ("v++", 2.5, 1.5), ("--v", 0.5, 0.5),
                               ("v--", 0.5, 1.5)]:
            out.append((f"{t} y = {form}", [(t, "v", init), (t, "y", 0.0)], [f"y = {form};"],
                        [("v", "%f"), ("y", "%f")], f"{conv(new):f} {conv(val):f}"))
        for op, b, r in [("+=", 2.25, 3.75), ("-=", 0.5, 1.0), ("*=", 4.0, 6.0),
                         ("/=", 4.0, 0.375)]:
            out.append((f"{t} {op} {b}", [(t, "v", init)], [f"v {op} {b};"], [("v", "%f")],
                        f"{conv(r):f}"))
    return out


def whole_cases():
    ints = {"7": (bits_of(7, 32), True), "0": (bits_of(0, 32), True),
            "-3": (bits_of(-3, 32), True)}
    out = []
    out += incdec_cases("int", bits_of(100, 32))
    out += incdec_cases("int", bits_of(-2147483648, 32))  # wraps to the largest int
    out += integral_cases("int", bits_of(100, 32), ints, COMPOUND)
    out += integral_cases("int", bits_of(-100, 32), {"7": ints["7"], "2": (bits_of(2, 32), True)},
                          ["/=", "%=", ">>=", ">>>="])
    out += incdec_cases("byte", bits_of(-8, 8))
    out += incdec_cases("byte", bits_of(127, 8))
    out += integral_cases("byte", bits_of(-8, 8), {"1": (bits_of(1, 32), True),
                                                   "200": (bits_of(200, 32), True)},
                          ["+=", ">>=", ">>>="])
    out += incdec_cases("logic8", "11000101")
    out += incdec_cases("logic8", "1100x101")
    out += integral_cases("logic8", "11000101",
                          {"8'h40": ("01000000", False), "300": (bits_of(300, 32), True),
                           "8'b0000_x011": ("0000x011", False)},
                          ["+=", "-=", "*=", "&=", "|=", "^=", "<<=", ">>>="])
    out += integral_cases("logic8", "1100x101",
                          {"8'h0f": ("00001111", False), "8'hf0": ("11110000", False)},
                          ["+=", "&=", "|=", "^=", ">>="])
    out += real_cases()
    return out


def part_cases():
    """Targets inside a variable: a part, a bit, elements of an unpacked
    array (with a constant index and with an index from a function that
    counts its calls), and a part of an element."""
    out = []
    w = "11000101"
    # v[3:0] += 4'd9: 0101 + 1001 = 1110; v[7] ++ ; v[5:2] -= 1
    out.append(("part v[3:0] += 4'd9", [("logic8", "v", w)], ["v[3:0] += 4'd9;"], [("v", "%b")],
                "11001110"))
    out.append(("part v[6:3]++", [("logic8", "v", w)], ["v[6:3]++;"], [("v", "%b")],
                "11001101"))
    out.append(("bit v[7]--", [("logic8", "v", w)], ["v[7]--;"], [("v", "%b")], "01000101"))
    out.append(("bit v[1] |= 1", [("logic8", "v", w)], ["v[1] |= 1'b1;"], [("v", "%b")],
                "11000111"))
    arr = [("intarr", "a", [10, 20, 30, 40])]
    out.append(("elem a[2] += 5", arr, ["a[2] += 5;"],
                [("a[0]", "%0d"), ("a[1]", "%0d"), ("a[2]", "%0d"), ("a[3]", "%0d")],
                "10 20 35 40"))
    out.append(("elem a[1]++", arr, ["a[1]++;"],
                [("a[0]", "%0d"), ("a[1]", "%0d"), ("a[2]", "%0d"), ("a[3]", "%0d")],
                "10 21 30 40"))
    out.append(("idx a[idx()] *= 3", arr, ["calls = 0;", "a[idx()] *= 3;"],
                [("a[1]", "%0d"), ("a[2]", "%0d"), ("calls", "%0d")], "20 90 1"))
    out.append(("idx a[idx()]++", arr, ["calls = 0;", "a[idx()]++;"],
                [("a[1]", "%0d"), ("a[2]", "%0d"), ("calls", "%0d")], "20 31 1"))
    out.append(("idx y = a[idx()]--", arr + [("int", "y", bits_of(0, 32))],
                ["calls = 0;", "y = a[idx()]--;"],
                [("a[2]", "%0d"), ("y", "%0d"), ("calls", "%0d")], "29 30 1"))
    out.append(("idx a[idx()] <<= idx()", arr, ["calls = 0;", "a[idx()] <<= idx();"],
                [("a[2]", "%0d"), ("calls", "%0d")], "120 2"))
    marr = [("logic8arr", "m", ["00010010", "00110100"])]
    out.append(("elem part m[1][3:0] += 4'd13", marr, ["m[1][3:0] += 4'd13;"],
                [("m[0]", "%b"), ("m[1]", "%b")], "00010010 00110001"))
    out.append(("elem part m[idx()-1][7:4] -= 1", marr, ["calls = 0;", "m[idx()-1][7:4] -= 1;"],
                [("m[0]", "%b"), ("m[1]", "%b"), ("calls", "%0d")], "00010010 00100100 1"))
    out.append(("elem part m[idx()-1][7:4]--", marr, ["calls = 0;", "m[idx()-1][7:4]--;"],
                [("m[0]", "%b"), ("m[1]", "%b"), ("calls", "%0d")], "00010010 00100100 1"))
    return out


def string_cases(container):
    """The string methods that change their string (6.16.2, 6.16.11 to
    6.16.15) and s[i] = c, which writes one character (6.16): the string
    is read and written as one target, its index evaluated once."""
    out = [
        ("s.putc(0, \"x\")", [("string", "s", "abc")], ['s.putc(0, "x");'], [("s", "%s")], "xbc"),
        ("s[2] = \"z\"", [("string", "s", "abc")], ['s[2] = "z";'], [("s", "%s")], "abz"),
        ("s.itoa(-42)", [("string", "s", "abc")], ["s.itoa(-42);"], [("s", "%s")], "-42"),
        ("s.hextoa(255)", [("string", "s", "abc")], ["s.hextoa(255);"], [("s", "%s")], "ff"),
    ]
    if container == "function":
        return out
    arr = [("strarr", "sa", ["abc", "def", "ghi"])]
    out += [
        ("sa[1].putc(1, \"y\")", arr, ['sa[1].putc(1, "y");'], [("sa[1]", "%s")], "dyf"),
        ("sa[idx()].putc(0, \"z\")", arr, ["calls = 0;", 'sa[idx()].putc(0, "z");'],
         [("sa[2]", "%s"), ("calls", "%0d")], "zhi 1"),
        ("sa[idx()][1] = \"w\"", arr, ["calls = 0;", 'sa[idx()][1] = "w";'],
         [("sa[2]", "%s"), ("calls", "%0d")], "gwi 1"),
        ("sa[idx()].bintoa(5)", arr, ["calls = 0;", "sa[idx()].bintoa(5);"],
         [("sa[2]", "%s"), ("calls", "%0d")], "101 1"),
    ]
    return out


# --- The containers ------------------------------------------------------------

def decl_text(t, name, init, kw):
    if t in INTEGRAL:
        d, w, signed, four, _ = INTEGRAL[t]
        lit = literal(init, signed) if four else str(to_int(init, signed))
        return f"{kw}{d} {name} = {lit};"
    if t in ("real", "shortreal"):
        return f"{kw}{t} {name} = {init};"
    if t == "string":
        return f'{kw}string {name} = "{init}";'
    if t == "strarr":
        return f"{kw}string {name} [{len(init)}];"
    # Arrays get their values by statements (init_statements).
    if t == "intarr":
        return f"{kw}int {name} [{len(init)}];"
    return f"{kw}logic [7:0] {name} [{len(init)}];"


def init_statements(t, name, init):
    if t == "intarr":
        return [f"{name}[{i}] = {v};" for i, v in enumerate(init)]
    if t == "logic8arr":
        return [f"{name}[{i}] = {literal(v, False)};" for i, v in enumerate(init)]
    if t == "strarr":
        return [f'{name}[{i}] = "{v}";' for i, v in enumerate(init)]
    return []


CONTAINERS = {
    "module": ("module variables, short names", "6.21"),
    "hier": ("module variables, hierarchical names", "6.21 23.6"),
    "task": ("variables of a static task", "6.21 13.3"),
    "function": ("variables of an automatic function", "6.21 13.4.2"),
}


def rename(text, names, prefix):
    """Gives each case its own variables (the name and the case number)
    and spells the module's names with the prefix."""
    def sub(m):
        n = m.group(0)
        if n in names:
            return prefix + names[n]
        return prefix + n if n in ("calls", "idx") else n
    return re.sub(r"(?<![\w'.$])[A-Za-z_]\w*", sub, text)


def is_incdec(label):
    return "++" in label or "--" in label


# The two kinds of test: increments and decrements, and assignment
# operators. Each one has its own files, so that one can run while the
# other is not supported.
KINDS = {
    "incdec": ("incdec", "Increment and decrement operators read, change and write their target "
               "once", "11.4.2 11.4.3 11.8"),
    "compound": ("assign_op", "Assignment operators read, change and write their target with "
                 "its indices evaluated once", "11.4.1 11.4.3 11.4.8 11.4.10 11.8"),
    "string": ("string_change", "String methods that change their string, and the writing of "
               "one character, read and write the string with its indices evaluated once",
               "6.16 6.16.2 6.16.9 6.16.10 6.16.12"),
}


def evidence(kind, container, cases):
    """The :oracle: of a test and the comment that says which simulator
    runs which lines (TEST_SUITE.md: where they disagree or fail, the
    standard decides)."""
    if kind == "string":
        note = ["Icarus 13.0 does not accept these string methods called as tasks."]
        if container == "function":
            return "spec verilator@5.052", note + ["The lines match Verilator 5.052."]
        if container == "hier":
            return "spec verilator@5.052", note + [
                "Verilator 5.052 matches the s[2] line and stops with an internal error on the",
                "others, run alone; they follow 6.16."]
        return "spec verilator@5.052", note + [
            "Verilator 5.052 matches the lines on a string variable, run alone, and fails on",
            "the lines on array elements (an internal error, or C++ that does not compile);",
            "they follow 6.16, with the one index evaluated once."]
    if kind == "incdec" and any(c[0].startswith("idx") for c in cases):
        return "spec iverilog@13.0", [
            "Icarus 13.0 cannot run y = a[idx()]-- (an unresolved reference) or the",
            "\"elem part\" line (an internal error); every other line matches its output,",
            "run alone. Verilator 5.052 calls idx() three times in y = a[idx()]-- and",
            "twice in m[idx()-1][7:4]--, where VALUE_SEMANTICS.md 3.12 evaluates the",
            "index once."]
    if any(c[0].startswith("elem part") for c in cases):
        return "spec iverilog@13.0 verilator@5.052", [
            "Icarus 13.0 stops with an internal error on the assignments to a part of an",
            "array element (the \"elem part\" lines); every other line matches its output,",
            "run alone. The \"elem part\" lines match Verilator 5.052, except that it",
            "calls idx() twice in m[idx()-1][7:4] -= 1, where 11.4.1 evaluates the",
            "index once."]
    return "spec iverilog@13.0", []


def test(container, kind):
    prefix_name, description, kind_lrm = KINDS[kind]
    name = f"{prefix_name}_{container}"
    what, lrm = CONTAINERS[container]
    if kind == "string":
        cases = string_cases(container)
    else:
        cases = whole_cases() + ([] if container == "function" else part_cases())
        cases = [c for c in cases if is_incdec(c[0]) == (kind == "incdec")]
    oracle, note = evidence(kind, container, cases)
    sv = (f"/*\n:name: {name}\n:description: {description}, {what}\n:tags: values\n"
          f":type: simulation\n:lrm: 1800-2023 {kind_lrm} {lrm}\n:oracle: {oracle}\n"
          f":spec_check: ai\n:fourstate: {'no' if kind == 'string' else 'yes'}\n"
          f":nondeterministic: no\n:generator: gen_assign_op_matrix.py@{VERSION}\n:expected_stdout: @{name}.expected\n*/\n"
          "// Generated by tools/gen_assign_op_matrix.py; change the generator, not this file.\n"
          + "".join(f"// {line}\n" for line in note)
          + "module top;\n  int calls;\n"
          "  function int idx();\n    calls++;\n    return 2;\n  endfunction\n")
    kw = {"module": "", "hier": "", "task": "static ", "function": "automatic "}[container]
    dind = "  " if container in ("module", "hier") else "    "
    prefix = "top." if container == "hier" else ""
    decls, body, exp = "", "", ""
    for k, (label, ds, stmts, args, expected) in enumerate(cases):
        names = {n: f"{n}{k}" for _, n, _ in ds}
        for t, n, init in ds:
            decls += f"{dind}{decl_text(t, names[n], init, kw)}\n"
        for t, n, init in ds:
            for st in init_statements(t, n, init):
                body += f"    {rename(st, names, prefix)}\n"
        for st in stmts:
            body += f"    {rename(st, names, prefix)}\n"
        fmt = " ".join(f for _, f in args)
        argtext = ", ".join(rename(a, names, prefix) for a, _ in args)
        shown = label.replace("%", "%%").replace('"', '\\"')
        body += f'    $display("{shown} {fmt}", {argtext});\n'
        exp += f"{label} {expected}\n"
    if container in ("module", "hier"):
        sv += decls + "  initial begin\n" + body + "  end\n"
    elif container == "task":
        sv += "  task static run();\n" + decls + body + "  endtask\n  initial run();\n"
    else:
        sv += ("  function automatic void run();\n" + decls + body +
               "  endfunction\n  initial run();\n")
    sv += "endmodule\n"
    return name, sv, exp


def files():
    out = {}
    for kind in KINDS:
        for c in CONTAINERS:
            name, sv, exp = test(c, kind)
            out[f"{name}.sv"] = sv
            out[f"{name}.expected"] = exp
    return out


def main():
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("--check", action="store_true")
    args = ap.parse_args()
    bad = 0
    for name, content in files().items():
        path = OUT / name
        if args.check:
            if not path.exists() or path.read_text(encoding="utf-8") != content:
                print(f"gen_assign_op_matrix: {path.relative_to(OUT.parent.parent.parent)} is out "
                      "of date; run tools/gen_assign_op_matrix.py", file=sys.stderr)
                bad += 1
        else:
            path.write_text(content, encoding="utf-8")
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main())
