#!/usr/bin/env python3
# Copyright (c) 2026 The kirimasim Authors
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

"""Generates the matrix tests of foreach loops with omitted loop
variables (TEST_SUITE.md, "Mechanism matrix") in tests/suite/proc/.

  tools/gen_foreach_omitted_matrix.py           # writes the files
  tools/gen_foreach_omitted_matrix.py --check   # exit 1 if a file differs from what it would write

The rule: 12.7.3 of the standard. A loop variable may be omitted, and
then that dimension is not iterated; trailing commas may be left out
too. The loop variables are nested loops, the outer one for the lower
dimension number (20.7: unpacked dimensions first, then packed), and
each goes from the left bound of its range to the right bound; a
dynamic array or a queue goes from 0 up. break leaves the whole loop
and continue goes on with the next iteration (12.8).

When every loop variable is omitted, this generator expects that the
body does not run, as kirimasim-tests' ch12/test_syntax_loop_statement_forms
does and as Icarus Verilog 13.0 and Verilator 5.052 do. The standard does
not speak of that case; issue 126 asks a person to read it, and slang's
constant evaluation runs the body once.

The axes: the array (fixed unpacked arrays of one, two and three
dimensions and one with a descending range, packed vectors of one and
two dimensions, a dynamic array, a queue, and a fixed array of dynamic
arrays, a dynamic array of dynamic arrays and a queue of queues whose
inner sizes differ), the loop variables (every subset of the dimensions,
and trailing commas left out), the body
(counting with the first and the last indices, break at the second
iteration, continue, and a delay of 1 ns where the place may wait), and
the place: an initial procedure, an automatic task, an automatic
function, a class method and a fork branch; the arrays are variables of
the module, or locals of the task, the function or the method.

The expected output is computed here from the rules, without reading
kirimasim's code. A person checks it once for all the tests made from
it (TEST_SUITE.md, "Sampling by people").
"""

import argparse
import itertools
import sys
from pathlib import Path

OUT = Path(__file__).resolve().parent.parent / "tests" / "suite" / "proc"
# The version of the rules below (TEST_SUITE.md, :generator:). Raise it
# whenever they change.
VERSION = 1

# name: (declaration, setup statement or None, the elements' indices in
# iteration order: the dimensions in the order of 20.7, each from its left
# bound to its right bound, a dynamic one from 0 up).
ARRAYS = {
    "f1": ("int f1[3];", None, [[0, 1, 2]]),
    "f2": ("int f2[2][3];", None, [[0, 1], [0, 1, 2]]),
    "f3": ("int f3[2][3][4];", None, [[0, 1], [0, 1, 2], [0, 1, 2, 3]]),
    "fr": ("int fr[3:1][0:1];", None, [[3, 2, 1], [0, 1]]),
    "p1": ("bit [7:0] p1;", None, [list(range(7, -1, -1))]),
    "p2": ("bit [1:0][3:0] p2;", None, [[1, 0], [3, 2, 1, 0]]),
    "d": ("int d[];", "d = new[3];", [[0, 1, 2]]),
    "q": ("int q[$];", "q = '{5, 6, 7, 8};", [[0, 1, 2, 3]]),
    # Arrays whose inner sizes differ by element, given as their elements.
    "fd": ("int fd[2][];", "fd[0] = new[3]; fd[1] = new[2];",
           [(0, 0), (0, 1), (0, 2), (1, 0), (1, 1)]),
    "dd": ("int dd[][];", "dd = new[2]; dd[0] = new[3]; dd[1] = new[1];",
           [(0, 0), (0, 1), (0, 2), (1, 0)]),
    "qq": ("int qq[$][$];", "qq = '{'{1, 2}, '{3, 4, 5}};",
           [(0, 0), (0, 1), (1, 0), (1, 1), (1, 2)]),
}
VARS = ["i", "j", "k"]
PLACES = ["initial", "task", "function", "class", "fork"]
WAITS = {"initial", "task", "fork"}


def elements(spec):
    """The index tuples of every element, in iteration order."""
    if isinstance(spec[0], tuple):
        return spec
    return list(itertools.product(*spec))


def ragged(spec):
    return isinstance(spec[0], tuple)


def patterns(dims, prefix_only):
    """(the text inside the brackets, the named dimensions) for every subset,
    and the same with trailing commas left out where that differs. Where the
    inner sizes differ, only the subsets that name a leading run of
    dimensions: a dimension named after an omitted one has no one size."""
    out = []
    for mask in itertools.product([False, True], repeat=dims):
        named = [d for d in range(dims) if mask[d]]
        if prefix_only and named != list(range(len(named))):
            continue
        full = ",".join(VARS[d] if mask[d] else "" for d in range(dims))
        out.append((full, named))
        if named and named[-1] < dims - 1:
            out.append((",".join(VARS[d] if mask[d] else "" for d in range(named[-1] + 1)), named))
    return out


def cases():
    out = []
    for name, (_, _, spec) in ARRAYS.items():
        for text, named in patterns(len(elements(spec)[0]), ragged(spec)):
            out.append((name, text, named, spec))
    return out


def expected_line(name, text, named, spec, waits):
    # The iterations are the distinct values of the named indices, in the
    # order the elements come; none when no dimension is named.
    seen = []
    if named:
        for e in elements(spec):
            key = tuple(e[d] for d in named)
            if key not in seen:
                seen.append(key)
    n = len(seen)
    first = ",".join(map(str, seen[0])) if n else "-"
    last = ",".join(map(str, seen[-1])) if n else "-"
    line = f"{name}[{text}]: n={n} first={first} last={last} brk={min(n, 2)} cont={n}"
    if waits:
        line += f" t={n}"
    return line


def body(indent, name, text, named, waits):
    loop = f"foreach ({name}[{text}])"
    lines = [f"{indent}n = 0; b = 0; c = 0;"]
    if named:
        firsts = " ".join(f"f{VARS[d]} = {VARS[d]};" for d in named)
        lasts = " ".join(f"l{VARS[d]} = {VARS[d]};" for d in named)
        lines.append(f"{indent}{loop} begin if (n == 0) begin {firsts} end {lasts} n++; end")
    else:
        lines.append(f"{indent}{loop} n++;")
    lines += [f"{indent}{loop} begin if (b == 2) break; b++; end",
              f"{indent}{loop} begin c++; if (c > 0) continue; c += 100; end"]
    if waits:
        lines += [f"{indent}t0 = $time;", f"{indent}{loop} #1;"]
    if named:
        shown = ", ".join([f"f{VARS[d]}" for d in named] + [f"l{VARS[d]}" for d in named])
        pair = ",".join(["%0d"] * len(named))
        lines += [f"{indent}if (n == 0)",
                  f'{indent}  $write("{name}[{text}]: n=%0d first=- last=-", n);',
                  f"{indent}else",
                  f'{indent}  $write("{name}[{text}]: n=%0d first={pair} last={pair}", n, {shown});']
    else:
        lines.append(f'{indent}$write("{name}[{text}]: n=%0d first=- last=-", n);')
    if waits:
        lines.append(f'{indent}$display(" brk=%0d cont=%0d t=%0t", b, c, $time - t0);')
    else:
        lines.append(f'{indent}$display(" brk=%0d cont=%0d", b, c);')
    return lines


def locals_(indent, waits):
    out = [f"{indent}{decl}" for decl, _, _ in ARRAYS.values()]
    out.append(f"{indent}int n, b, c, fi, fj, fk, li, lj, lk;")
    if waits:
        out.append(f"{indent}time t0;")
    return out


def setup(indent):
    return [f"{indent}{s}" for _, s, _ in ARRAYS.values() if s]


def statements(indent, waits):
    out = setup(indent)
    for name, text, named, _ in cases():
        out += body(indent, name, text, named, waits)
    return out


def test(place):
    name = f"foreach_omitted_{place}"
    waits = place in WAITS
    where = {"initial": "an initial procedure", "task": "an automatic task",
             "function": "an automatic function", "class": "a class method",
             "fork": "a fork branch"}[place]
    sv = ["/*", f":name: {name}",
          ":description: foreach over fixed, packed, dynamic and queue arrays with every subset of "
          "loop variables omitted iterates only the named dimensions, in the order of 12.7.3, "
          f"and runs no body when none is named, with break, continue and delays, in {where}",
          ":tags: proc foreach", ":type: simulation", ":lrm: 1800-2023 12.7.3 12.8 20.7",
          ":oracle: spec", ":spec_check: ai", ":fourstate: no", ":nondeterministic: no",
          f":generator: gen_foreach_omitted_matrix.py@{VERSION}", ":top_module: top",
          ":issue: 65", f":expected_stdout: @{name}.expected", "*/",
          "// Generated by tools/gen_foreach_omitted_matrix.py; change the generator, not "
          "this file.", "`timescale 1ns/1ns"]
    if place in ("initial", "fork"):
        sv += ["module top;"] + locals_("  ", waits) + ["  initial begin"]
        if place == "fork":
            sv += ["    fork", "      begin"] + statements("        ", waits) + ["      end",
                                                                               "    join"]
        else:
            sv += statements("    ", waits)
        sv += ["  end", "endmodule"]
    elif place == "task":
        sv += ["module top;", "  task automatic run();"] + locals_("    ", waits)
        sv += statements("    ", waits) + ["  endtask", "  initial run();", "endmodule"]
    elif place == "function":
        sv += ["module top;", "  function automatic void run();"] + locals_("    ", waits)
        sv += statements("    ", waits) + ["  endfunction", "  initial run();", "endmodule"]
    else:
        sv += ["class K;", "  function void run();"] + locals_("    ", waits)
        sv += statements("    ", waits) + ["  endfunction", "endclass", "module top;",
                                            "  K k;", "  initial begin", "    k = new;",
                                            "    k.run();", "  end", "endmodule"]
    expected = [expected_line(n, t, named, dims, waits) for n, t, named, dims in cases()]
    return name, sv, expected


def files():
    out = {}
    for place in PLACES:
        name, sv, expected = test(place)
        out[f"{name}.sv"] = "\n".join(sv) + "\n"
        out[f"{name}.expected"] = "\n".join(expected) + "\n"
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
                print("gen_foreach_omitted_matrix: "
                      f"{path.relative_to(OUT.parent.parent.parent)} is out of date; run "
                      "tools/gen_foreach_omitted_matrix.py", file=sys.stderr)
                bad += 1
        else:
            path.write_text(text, encoding="utf-8")
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main())
