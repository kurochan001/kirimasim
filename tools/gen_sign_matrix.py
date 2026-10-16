#!/usr/bin/env python3
"""Generates the sign propagation matrix tests of TEST_SUITE.md ("4 値の行列")
in tests/suite/values/.

  tools/gen_sign_matrix.py           # writes the files
  tools/gen_sign_matrix.py --check   # exit 1 if a file differs from what it would write

How the operands of a context-determined expression are extended (11.8.1,
11.8.2): the expression is signed only if every operand is, its width is
the widest of its operands and its context, and each operand is extended
to that width by the sign of the expression, not its own. The axes: the
operators of 11.4 whose operands are context-determined, a signed operand
mixed with an unsigned one or with another signed one, operands with x
and z bits, three contexts (an assignment to a wider variable, and an
operand of a wider unsigned or signed bitwise OR), and four containers
(module variables by short and by hierarchical names, the variables of a
static task and of an automatic function).

The expected outputs follow from the model below. A person checks the
model against the standard once for all the tests made from it
(TEST_SUITE.md, "人の抜き取り").
"""

import argparse
import sys
from pathlib import Path

OUT = Path(__file__).resolve().parent.parent / "tests" / "suite" / "values"
# The version of the rules below (TEST_SUITE.md, :generator:). Raise it
# whenever they change.
VERSION = 1

# The operands: name -> (bits, most significant first, signed).
LEAVES = {
    "s": ("10000101", True),   # -123
    "t": ("01111010", True),   # 122
    "u": ("00001111", False),  # 15
    "x": ("x0110101", True),   # the sign bit is x
    "z": ("z1010011", True),   # the sign bit is z
    "c1": ("1", False),
    "c0": ("0", False),
}
ARITH = ["+", "-", "*", "/", "%"]
BITWISE = ["&", "|", "^", "~^"]
PAIRS = [("s", "u"), ("u", "s"), ("s", "t"), ("x", "u"), ("x", "t")]


def leaf(n):
    return ("leaf", n)


def templates():
    out = []
    for op in ARITH + BITWISE:
        for a, b in PAIRS:
            out.append(("bin", op, leaf(a), leaf(b)))
    for a, op in [("s", ">>>"), ("s", ">>"), ("s", "<<"), ("x", ">>>")]:
        out.append(("shift", op, leaf(a), 2))
    out.append(("bin", "+", ("shift", ">>>", leaf("s"), 2), leaf("u")))
    out.append(("bin", "+", ("shift", ">>>", leaf("s"), 2), leaf("t")))
    out.append(("un", "-", leaf("s")))
    out.append(("un", "~", leaf("s")))
    out.append(("bin", "+", ("un", "-", leaf("s")), leaf("u")))
    out.append(("bin", "+", ("un", "~", leaf("s")), leaf("u")))
    out.append(("un", "~", leaf("x")))
    out.append(("bin", "&", ("un", "~", leaf("x")), leaf("u")))
    for c, a, b in [("c1", "s", "u"), ("c0", "u", "s"), ("c1", "s", "t"), ("c1", "z", "u"),
                    ("c1", "z", "t"), ("c1", "x", "t")]:
        out.append(("cond", leaf(c), leaf(a), leaf(b)))
    return out


def text(e, top=True):
    k = e[0]
    if k == "leaf":
        return e[1]
    if k == "bin":
        s = f"{text(e[2], False)} {e[1]} {text(e[3], False)}"
    elif k == "shift":
        s = f"{text(e[2], False)} {e[1]} {e[3]}"
    elif k == "un":
        s = f"{e[1]}{text(e[2], False)}"
    else:
        s = f"{text(e[1], False)} ? {text(e[2], False)} : {text(e[3], False)}"
    return s if top or k == "un" else f"({s})"


# --- The model of 11.8.1 and 11.8.2 -----------------------------------------

def self_type(e):
    """(width, signed) of an expression on its own (11.6.1, 11.8.1)."""
    k = e[0]
    if k == "leaf":
        bits, signed = LEAVES[e[1]]
        return len(bits), signed
    if k == "bin":
        (wl, sl), (wr, sr) = self_type(e[2]), self_type(e[3])
        return max(wl, wr), sl and sr
    if k in ("shift", "un"):
        return self_type(e[2])
    (wl, sl), (wr, sr) = self_type(e[2]), self_type(e[3])
    return max(wl, wr), sl and sr


def extend(bits, w, signed):
    """An operand extended to w bits: by its sign bit (x and z included,
    11.8.4) if the propagated type is signed, else by zeros (11.8.2)."""
    if len(bits) >= w:
        return bits[len(bits) - w:]
    return (bits[0] if signed else "0") * (w - len(bits)) + bits


def known(bits):
    return all(b in "01" for b in bits)


def to_int(bits, signed):
    v = int(bits, 2)
    return v - (1 << len(bits)) if signed and bits[0] == "1" else v


def from_int(v, w):
    return format(v % (1 << w), f"0{w}b")


def bitwise(op, a, b):
    def one(x, y):
        x = "x" if x == "z" else x
        y = "x" if y == "z" else y
        if op == "&":
            return "0" if "0" in (x, y) else ("1" if x == y == "1" else "x")
        if op == "|":
            return "1" if "1" in (x, y) else ("0" if x == y == "0" else "x")
        r = "x" if "x" in (x, y) else str(int(x) ^ int(y))  # 11.4.8, table 11-14
        return r if op == "^" or r == "x" else str(1 - int(r))
    return "".join(one(x, y) for x, y in zip(a, b))


def arith(op, a, b, signed):
    """11.4.3: any x or z bit makes the whole result x; division by zero too."""
    w = len(a)
    if not (known(a) and known(b)):
        return "x" * w
    x, y = to_int(a, signed), to_int(b, signed)
    if op in "/%" and y == 0:
        return "x" * w
    if op == "+":
        r = x + y
    elif op == "-":
        r = x - y
    elif op == "*":
        r = x * y
    else:
        q = abs(x) // abs(y) * (1 if (x < 0) == (y < 0) else -1)  # toward zero
        r = q if op == "/" else x - y * q  # the remainder takes the sign of x
    return from_int(r, w)


def evaluate(e, w, signed):
    """The value of e as an operand of a context of w bits and the given
    sign (11.8.2)."""
    k = e[0]
    if k == "leaf":
        return extend(LEAVES[e[1]][0], w, signed)
    if k == "bin":
        a, b = evaluate(e[2], w, signed), evaluate(e[3], w, signed)
        return bitwise(e[1], a, b) if e[1] in BITWISE else arith(e[1], a, b, signed)
    if k == "shift":
        a, n = evaluate(e[2], w, signed), e[3]
        if e[1] == "<<":
            return a[n:] + "0" * n
        fill = a[0] if e[1] == ">>>" and signed else "0"  # 11.4.10
        return fill * n + a[:w - n]
    if k == "un":
        a = evaluate(e[2], w, signed)
        if e[1] == "~":
            return bitwise("^", a, "1" * w)
        return arith("-", "0" * w, a, signed)
    cond = LEAVES[e[1][1]][0]  # the condition is self-determined (11.6.1)
    return evaluate(e[2] if cond == "1" else e[3], w, signed)


# --- The containers ------------------------------------------------------------

def decls(indent, kw=""):
    out = ""
    for n, (bits, signed) in LEAVES.items():
        sign = " signed" if signed else ""
        rng = f" [{len(bits) - 1}:0]" if len(bits) > 1 else ""
        lit = f"{len(bits)}'{'s' if signed else ''}b{bits}"
        out += f"{indent}{kw}logic{sign}{rng} {n} = {lit};\n"
    out += f"{indent}{kw}logic [15:0] r;\n"
    return out


CONTAINERS = {
    "module": ("module variables, short names", "6.21"),
    "hier": ("module variables, hierarchical names", "6.21 23.6"),
    "task": ("variables of a static task", "6.21 13.3"),
    "function": ("variables of an automatic function", "6.21 13.4.2"),
}


def body(container, indent):
    prefix = "top." if container == "hier" else ""
    out, exp = "", ""
    for e in templates():
        src = text(e)
        for label, stmt, arg, value in contexts(e, src):
            if stmt is not None:
                out += f"{indent}{spell(stmt, prefix)}\n"
            fmt = f"{label} {src}".replace("%", "%%")
            out += f'{indent}$display("{fmt} %b", {spell(arg, prefix)});\n'
            exp += f"{label} {src} {value}\n"
    return out, exp


def contexts(e, src):
    """The contexts of an expression: the line label, the statement before
    the $display, the $display argument and the expected value. A is an
    assignment to a 16-bit variable, whose sign does not count (11.8.1);
    B and C are an operand of an unsigned and of a signed bitwise OR with
    a 16-bit zero."""
    w, signed = self_type(e)
    full = max(w, 16)
    return [
        ("A", f"r = {src};", "r", evaluate(e, full, signed)),
        ("B", None, f"({src}) | 16'h0", bitwise("|", evaluate(e, full, False), "0" * 16)),
        ("C", None, f"({src}) | 16'sh0", bitwise("|", evaluate(e, full, signed), "0" * 16)),
    ]


def spell(src, prefix):
    if not prefix:
        return src
    out, i = "", 0
    names = sorted(LEAVES, key=len, reverse=True) + ["r"]
    while i < len(src):
        for n in names:
            if src.startswith(n, i) and (i == 0 or not src[i - 1].isalnum()) and (
                    i + len(n) == len(src) or not src[i + len(n)].isalnum()):
                out += prefix + n
                i += len(n)
                break
        else:
            out += src[i]
            i += 1
    return out


def header(name, container):
    what, lrm = CONTAINERS[container]
    return (f"/*\n:name: {name}\n:description: Operands of context-determined expressions are "
            f"extended by the sign of the expression, {what}\n:tags: values\n:type: simulation\n"
            f":lrm: 1800-2023 11.8.1 11.8.2 11.8.4 11.4 {lrm}\n:oracle: spec iverilog@13.0\n"
            f":spec_check: ai\n:fourstate: yes\n:nondeterministic: no\n"
            f":generator: gen_sign_matrix.py@{VERSION}\n:expected_stdout: @{name}.expected\n*/\n"
            "// Generated by tools/gen_sign_matrix.py; change the generator, not this file.\n")


def test(container):
    name = f"sign_propagation_{container}"
    sv = header(name, container) + "module top;\n"
    if container in ("module", "hier"):
        code, exp = body(container, "    ")
        sv += decls("  ") + "  initial begin\n" + code + "  end\n"
    elif container == "task":
        code, exp = body(container, "    ")
        sv += ("  task static run();\n" + decls("    ", "static ") + code + "  endtask\n"
               "  initial run();\n")
    else:
        code, exp = body(container, "    ")
        sv += ("  function automatic void run();\n" + decls("    ", "automatic ") + code +
               "  endfunction\n  initial run();\n")
    sv += "endmodule\n"
    return name, sv, exp


def files():
    out = {}
    for c in CONTAINERS:
        name, sv, exp = test(c)
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
                print(f"gen_sign_matrix: {path.relative_to(OUT.parent.parent.parent)} is out of "
                      "date; run tools/gen_sign_matrix.py", file=sys.stderr)
                bad += 1
        else:
            path.write_text(content, encoding="utf-8")
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main())
