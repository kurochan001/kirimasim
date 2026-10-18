#!/usr/bin/env python3
"""Generates the real number matrix test of TEST_SUITE.md ("Mechanism matrix") in
tests/suite/values/.

  tools/gen_real_matrix.py           # writes the files
  tools/gen_real_matrix.py --check   # exit 1 if a file differs from what it would write

The rules: the math functions of 20.8.2 (those of C, with NaN and the
infinities where C gives them), the truth of a real (VALUE_SEMANTICS.md
7.5: 0.0 and -0.0 are false, anything else, NaN included, is true) under
!, &&, ||, ->, <->, if and ?:, the comparisons (11.4.4; NaN compares
unequal to everything), and the conversions (VALUE_SEMANTICS.md 1.5,
7.4: assignment rounds half away from zero, $rtoi truncates (20.5),
$realtobits and the like keep the bits (20.5), shortreal keeps a float);
the values print with %f and %h, and the infinities and NaN as inf, -inf
and nan (SYSTEM_TASKS.md 3.6). The axes: the function, a real and an
integer argument, values in and out of the function's domain, and the
container (module variables and automatic variables of a function).
Conversions of reals beyond 64 bits, NaN and the infinities are the
subject of gen_real_convert_matrix.py.

The expected outputs are computed with Python's math, which gives the C
results. A math result is printed only with as many digits as stay the
same when the result moves by a few units in the last place, so that
another platform's library gives the same text. A person checks them
once for all the tests made from them (TEST_SUITE.md, "Sampling by people").
"""

import argparse
import math
import struct
import sys
from pathlib import Path

OUT = Path(__file__).resolve().parent.parent / "tests" / "suite" / "values"
NAME = "real_matrix"
# The version of the rules below (TEST_SUITE.md, :generator:). Raise it
# whenever they change.
VERSION = 1

INF = float("inf")
NAN = float("nan")


def show(x, digits=6):
    """%.<digits>f, with inf, -inf and nan as SYSTEM_TASKS.md 3.6 says."""
    if math.isnan(x):
        return "nan"
    if math.isinf(x):
        return "inf" if x > 0 else "-inf"
    return f"%.{digits}f" % x


def stable(x, digits):
    """Whether x prints the same after moving a few units in the last place."""
    if math.isnan(x) or math.isinf(x):
        return True
    texts = set()
    for steps in (-4, 0, 4):
        y = x
        for _ in range(abs(steps)):
            y = math.nextafter(y, INF if steps > 0 else -INF)
        texts.add(show(y, digits))
    return len(texts) == 1


def c_real(x):
    """A real literal the parser reads back exactly."""
    return repr(float(x))


# (name, function, arguments): the arguments are real values; the
# integer column passes the first one through an int variable when it is
# a whole number.
FUNCTIONS = [
    ("ln", math.log, [(2.5,), (0.0,), (-1.0,)]),
    ("log10", math.log10, [(250.0,), (0.0,)]),
    ("exp", math.exp, [(1.25,), (1000.0,)]),
    ("sqrt", math.sqrt, [(2.0,), (-1.0,)]),
    ("pow", math.pow, [(2.0, 0.5), (-8.0, 1.0 / 3.0), (0.0, -1.0)]),
    ("floor", math.floor, [(-2.5,), (2.5,)]),
    ("ceil", math.ceil, [(-2.5,), (2.5,)]),
    ("sin", math.sin, [(0.75,)]),
    ("cos", math.cos, [(0.75,)]),
    ("tan", math.tan, [(0.75,)]),
    ("asin", math.asin, [(0.5,), (2.0,)]),
    ("acos", math.acos, [(0.5,), (2.0,)]),
    ("atan", math.atan, [(3.0,)]),
    ("atan2", math.atan2, [(1.0, -1.0), (0.0, -1.0)]),
    ("hypot", math.hypot, [(3.0, 4.0)]),
    ("sinh", math.sinh, [(1.5,)]),
    ("cosh", math.cosh, [(1.5,)]),
    ("tanh", math.tanh, [(1.5,)]),
    ("asinh", math.asinh, [(1.5,)]),
    ("acosh", math.acosh, [(1.5,), (0.5,)]),
    ("atanh", math.atanh, [(0.5,), (1.0,)]),
]


def math_value(fn, args):
    try:
        r = fn(*args)
    except (ValueError, OverflowError) as e:
        name = fn.__name__
        if isinstance(e, OverflowError):
            return INF
        if name in ("log", "log10") and args[0] == 0.0:
            return -INF
        if name == "atanh" and abs(args[0]) == 1.0:
            return math.copysign(INF, args[0])
        if name == "pow" and args[0] == 0.0 and args[1] < 0:
            return INF
        return NAN
    return float(r)


def digits_for(x):
    for d in (9, 6, 3):
        if stable(x, d):
            return d
    raise SystemExit(f"no stable digits for {x}")


def math_cases():
    """(tag, expression over x, y and i, expected text)."""
    out = []
    for name, fn, arglists in FUNCTIONS:
        for n, args in enumerate(arglists):
            val = math_value(fn, args)
            d = digits_for(val)
            call = f"${name}({', '.join(f'x{k}' for k in range(len(args)))})"
            out.append((f"{name}{n}", call, args, f"%.{d}f", show(val, d)))
            if float(args[0]).is_integer() and len(args) == 1:
                # The same with an integer argument, converted to real.
                out.append((f"{name}{n}int", f"${name}(i0)", args, f"%.{d}f", show(val, d)))
    return out


TRUTH_VALUES = [0.0, -0.0, 0.25, -3.5, NAN, INF]


def truth(x):
    return x != 0.0 or math.isnan(x)


def logic_lines(x):
    t = truth(x)
    return "".join(str(int(b)) for b in (
        not t, t and True, t or False, (not t) or False, t == True, t))


COMPARE = [(1.5, 2.5), (2.5, 2.5), (-0.0, 0.0), (NAN, 1.0), (NAN, NAN), (INF, 1e308)]


def compare_bits(a, b):
    return "".join(str(int(v)) for v in (a < b, a <= b, a > b, a >= b, a == b, a != b))


def round_half_away(x):
    """The nearest integer, half away from zero. floor(x + 0.5) would be
    wrong: for the double just below 0.5 the sum rounds up to 1.0."""
    whole = math.floor(abs(x))
    frac = abs(x) - whole  # exact for a double
    return int(whole + (1 if frac >= 0.5 else 0)) * (1 if x >= 0 else -1)


CONVERT = [2.5, -2.5, 0.49999999999999994, -0.5, 123456.5, -7.25]


def files():
    head = [
        "/*",
        f":name: {NAME}",
        ":description: real numbers: the math functions with real and integer arguments in "
        "and out of their domains, the truth of reals under logical operators, if and ?:, "
        "comparisons with NaN, and conversions between real, shortreal, integers and bits, in "
        "module and automatic variables",
        ":tags: values real",
        ":type: simulation",
        ":lrm: 1800-2023 6.12 11.3.1 11.4.4 11.4.7 12.4 20.5 20.8.2",
        ":oracle: spec",
        ":spec_check: ai",
        ":fourstate: no",
        ":nondeterministic: no",
        f":generator: gen_real_matrix.py@{VERSION}",
        f":expected_stdout: @{NAME}.expected",
        "*/",
        "// Generated by tools/gen_real_matrix.py; change the generator, not this file.",
        "// Icarus Verilog 14.0 (devel) f190dbb keeps a shortreal as a double (shortreal lines);",
        "// 6.12 makes it a single-precision float.",
        "module top;",
        "  real x0, x1, z, nan_, inf_;",
        "  int i0, n;",
        "  shortreal s;",
        "  logic [63:0] bits64;",
        "  logic [31:0] bits32;",
    ]
    body, exp = [], []

    def emit(prefix, pad):
        st = [f"{pad}z = 0.0;", f"{pad}nan_ = 0.0 / z;", f"{pad}inf_ = 1.0 / z;"]
        ex = []
        for tag, call, args, fmt, text in math_cases():
            st.append(f"{pad}x0 = {c_real(args[0])};")
            if len(args) > 1:
                st.append(f"{pad}x1 = {c_real(args[1])};")
            if float(args[0]).is_integer():
                st.append(f"{pad}i0 = {int(args[0])};")
            st.append(f"{pad}$display(\"{prefix} {tag} {fmt}\", {call});")
            ex.append(f"{prefix} {tag} {text}")
        for k, x in enumerate(TRUTH_VALUES):
            xs = "nan_" if math.isnan(x) else "inf_" if math.isinf(x) else c_real(x)
            st += [f"{pad}x0 = {xs};",
                   f"{pad}$display(\"{prefix} truth{k} %b%b%b%b%b\", !x0, x0 && 1.0, x0 || 0.0,"
                   " x0 -> 0.0, x0 <-> 1.0);",
                   f"{pad}if (x0) $display(\"{prefix} if{k} t\"); else $display(\"{prefix} if{k} e\");",
                   f"{pad}$display(\"{prefix} sel{k} %0d\", x0 ? 1 : 2);"]
            t = truth(x)
            ex += [f"{prefix} truth{k} {int(not t)}{int(t)}{int(t)}{int(not t)}{int(t)}",
                   f"{prefix} if{k} {'t' if t else 'e'}",
                   f"{prefix} sel{k} {1 if t else 2}"]
        for k, (a, b) in enumerate(COMPARE):
            asv = "nan_" if math.isnan(a) else c_real(a)
            bsv = "nan_" if math.isnan(b) else "inf_" if math.isinf(b) else c_real(b)
            if math.isinf(a):
                asv = "inf_"
            st += [f"{pad}x0 = {asv}; x1 = {bsv};",
                   f"{pad}$display(\"{prefix} cmp{k} %b%b%b%b%b%b\", x0 < x1, x0 <= x1, x0 > x1,"
                   " x0 >= x1, x0 == x1, x0 != x1);"]
            ex.append(f"{prefix} cmp{k} {compare_bits(a, b)}")
        for k, x in enumerate(CONVERT):
            st += [f"{pad}x0 = {c_real(x)};", f"{pad}n = x0;",
                   f"{pad}$display(\"{prefix} round{k} %0d %0d\", n, $rtoi(x0));"]
            ex.append(f"{prefix} round{k} {round_half_away(x)} {int(x)}")
        big = 2 ** 53 + 1
        st += [f"{pad}bits64 = 64'd{big};", f"{pad}x0 = bits64;",
               f"{pad}$display(\"{prefix} itor %.1f %.1f\", x0, $itor(-7));",
               f"{pad}$display(\"{prefix} tobits %h %h\", $realtobits(1.5), $shortrealtobits(1.5));",
               f"{pad}bits64 = 64'h4000000000000000; bits32 = 32'h40490fdb;",
               f"{pad}$display(\"{prefix} frombits %f %f\", $bitstoreal(bits64),"
               " $bitstoshortreal(bits32));",
               f"{pad}x0 = 0.1; s = x0; x1 = s;",
               f"{pad}$display(\"{prefix} shortreal %.12f %0d\", x1, x1 == x0);",
               f"{pad}$display(\"{prefix} arith %f %f %f\", 1.0 / z, -1.0 / z, 2.0 ** 0.5);"]
        f32 = struct.unpack("<f", struct.pack("<I", 0x40490fdb))[0]
        sh = struct.unpack("<f", struct.pack("<f", 0.1))[0]
        ex += [f"{prefix} itor {float(big):.1f} -7.0",
               f"{prefix} tobits 3ff8000000000000 3fc00000",
               f"{prefix} frombits 2.000000 {f32:.6f}",
               f"{prefix} shortreal {sh:.12f} 0",
               f"{prefix} arith inf -inf {math.sqrt(2.0):.6f}"]
        return st, ex

    st, ex = emit("module", "    ")
    body += ["  initial begin"] + st
    exp += ex
    fst, fex = emit("function", "    ")
    decls = ["  function automatic void auto();", "    real x0, x1, z, nan_, inf_;",
             "    int i0, n;", "    shortreal s;", "    logic [63:0] bits64;",
             "    logic [31:0] bits32;"]
    func = decls + fst + ["  endfunction"]
    body += ["    auto();", "  end"]
    exp += fex
    sv = head + func + body + ["endmodule", ""]
    return {f"{NAME}.sv": "\n".join(sv), f"{NAME}.expected": "\n".join(exp) + "\n"}


def main():
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("--check", action="store_true")
    args = ap.parse_args()
    bad = 0
    for name, text in files().items():
        path = OUT / name
        if args.check:
            if not path.exists() or path.read_text(encoding="utf-8") != text:
                print(f"gen_real_matrix: {path.relative_to(OUT.parent.parent.parent)} is out of "
                      "date; run tools/gen_real_matrix.py", file=sys.stderr)
                bad += 1
        else:
            path.write_text(text, encoding="utf-8")
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main())
