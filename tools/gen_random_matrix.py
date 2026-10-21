#!/usr/bin/env python3
"""Generates the matrix tests of $random and the $dist_* functions
(TEST_SUITE.md, "Mechanism matrix") in tests/suite/proc/.

  tools/gen_random_matrix.py           # writes the files
  tools/gen_random_matrix.py --check   # exit 1 if a file differs from what it would write

The rules (20.14, Annex N, SYSTEM_TASKS.md 12):

- Each function computes as SYSTEM_TASKS.md 12 says, and the values to
  check against of 12.7 are what the cells below expect.
- The seed variable gives its low 32 bits, x and z as 0, and gets the new
  seed back, sign-extended, when it changed; a function that does not
  draw leaves it as it was.
- $random with no argument uses one seed of the run, starting at 0.
- A mean, degree of freedom or k_stage of 0 or less gives 0, leaves the
  seed, and the run-time warning kirima-dist-argument, once for each place.
- A seed that is not a variable is an error (20.14.1).

The axes: the function and its seed (the cells below: each function, a
4-state integer seed, a 64-bit seed, an x seed, an element of an array,
no seed) and the container of the calls (an initial procedure, a task, a
function, a fork branch, a task in a generate block). Each cell starts
from its own seed, so its values do not depend on the others; the cell
without a seed is the only user of the run's seed.

The expected outputs are the values of SYSTEM_TASKS.md 12.7, without
reading kirimasim's code. For reference, checked 2026-10-21: Icarus 14.0
devel, which does not take an element of an array as a seed, prints the
same with a variable there, but in fusion_point, where its arm64 build
fuses a multiply-add (SYSTEM_TASKS.md 12.2).
"""

import argparse
import sys
from pathlib import Path

OUT = Path(__file__).resolve().parent.parent / "tests" / "suite" / "proc"
# The version of the rules below (TEST_SUITE.md, :generator:). Raise it
# whenever they change.
VERSION = 1

CONTAINERS = ["initial", "task", "function", "fork", "generate"]

# Each cell: (name, the seed's declaration, its start, the call with S for
# the seed, the values and seeds after each call; None for "not printed").
CELLS = [
    ("random_from_0", "integer s", "0", "$random(S)",
     [(303379748, -1844104698), (-1064739199, None), (-2071669239, None),
      (-1309649309, None), (112818957, None)]),
    ("random_from_1", "int s", "1", "$random(S)",
     [(-2147414528, 69070), (-1671855048, 475628535), (1129920902, -1017563188)]),
    ("uniform", "int s", "1", "$dist_uniform(S, -5, 5)",
     [(-5, 69070), (-4, 475628535), (3, -1017563188)]),
    ("normal", "int s", "1", "$dist_normal(S, 100, 10)",
     [(106, 772999773), (76, 2044158073), (102, 797919023)]),
    ("exponential", "int s", "1", "$dist_exponential(S, 10)",
     [(110, 69070), (22, 475628535), (3, -1017563188)]),
    ("poisson", "int s", "1", "$dist_poisson(S, 5)",
     [(0, 69070), (5, 1662200408), (5, 1059494674)]),
    ("chi_square", "int s", "1", "$dist_chi_square(S, 3)",
     [(1, -417135238), (2, 2044158073), (1, -1551342684)]),
    ("t", "int s", "1", "$dist_t(S, 5)",
     [(-6, 2044158073), (0, 584849259), (2, 1692139853)]),
    ("erlang", "int s", "1", "$dist_erlang(S, 3, 100)",
     [(450, -1017563188), (64, -473131853), (61, -505977370)]),
    ("fusion_point", "int s", "-1709352709", "$random(S)", [(-1073741185, None)]),
    ("outside_32_bits", "int s", "-1271221770", "$random(S)", [(2147483647, -1)]),
    ("zero_inside_a_call", "int s", "1511872763", "$dist_normal(S, 100, 10)",
     [(86, -2034665166), (106, 851608272)]),
    ("x_seed", "integer s", "'x", "$random(S)", [(303379748, -1844104698)]),
    ("array_element", "int s [0:1]", None, "$random(S)", [(-2147414528, 69070)]),
]


def cell_lines(k, cell):
    """The statements of a cell: the seed, the calls, one line printed."""
    name, decl, start, call, steps = cell
    seed = "a[1]" if decl.startswith("int s [") else f"s{k}"
    lines = []
    if decl.startswith("int s ["):
        lines.append("a[0] = 5; a[1] = 1;")
    else:
        lines.append(f"{seed} = {start};")
    fmt, refs = [], []
    for i, (_, after) in enumerate(steps):
        lines.append(f"v{k}[{i}] = {call.replace('S', seed)};")
        if after is not None:
            lines.append(f"w{k}[{i}] = {seed};")
            fmt.append("%0d (%0d)")
            refs += [f"v{k}[{i}]", f"w{k}[{i}]"]
        else:
            fmt.append("%0d")
            refs.append(f"v{k}[{i}]")
    lines.append(f'$display("c{k} {name}: {" ".join(fmt)}", {", ".join(refs)});')
    return lines


def expected(k, cell):
    name, _, _, _, steps = cell
    parts = [f"{v} ({a})" if a is not None else str(v) for v, a in steps]
    return f"c{k} {name}: " + " ".join(parts)


# Cells that print by themselves: the run's seed, a 64-bit seed, and the
# arguments out of their range, with their warnings.
def extra_lines(k):
    return {
        "no_seed": [f'$display("c{k} no_seed: %0d %0d %0d %0d %0d", $random, $random, $random, '
                    "$random, $random);"],
        "wide_seed": ["w64 = 64'hffff_0000_0000_0001;", "e = $random(w64);",
                      f'$display("c{k + 1} wide_seed: %0d %h", e, w64);'],
        "bad_arguments": ["d = 7;", "e = $dist_exponential(d, 0);", "e = e + $dist_poisson(d, -3);",
                          "e = e + $dist_chi_square(d, 0);", "e = e + $dist_t(d, -1);",
                          "e = e + $dist_erlang(d, 0, 5);",
                          f'$display("c{k + 2} bad_arguments: %0d %0d", e, d);'],
    }


EXTRA_EXPECTED = ["no_seed: 303379748 -1064739199 -2071669239 -1309649309 112818957",
                  "wide_seed: -2147414528 0000000000010dce", "bad_arguments: 0 7"]
BAD = [("$dist_exponential", "the mean"), ("$dist_poisson", "the mean"),
       ("$dist_chi_square", "the degree of freedom"), ("$dist_t", "the degree of freedom"),
       ("$dist_erlang", "k_stage")]


def test(container):
    name = f"random_{container}"
    out = (f"/*\n:name: {name}\n:description: $random and every $dist_* function from the "
           "values to check against of SYSTEM_TASKS.md 12.7, with 4-state, 64-bit, x and array "
           "element seeds, no seed, and arguments out of their range, called from "
           f"{'an' if container[0] in 'aeiou' else 'a'} {container}\n:tags: proc\n"
           ":type: simulation\n:lrm: 1800-2023 20.14 20.14.1 20.14.2\n:oracle: spec\n"
           ":spec_check: ai\n:fourstate: yes\n:nondeterministic: no\n"
           f":generator: gen_random_matrix.py@{VERSION}\n:top_module: top\n"
           f":expected_stdout: @{name}.expected\n:expected_stderr: @{name}.stderr\n*/\n"
           "// Generated by tools/gen_random_matrix.py; change the generator, not this file.\n"
           "`timescale 1ns/1ns\nmodule top;\n  int a [0:1];\n  logic [63:0] w64;\n"
           "  int e, d;\n").split("\n")[:-1]
    lines = list(out)
    for k, (_, decl, _, _, steps) in enumerate(CELLS):
        if not decl.startswith("int s ["):
            lines.append(f"  {decl.split()[0]} s{k};")
        lines.append(f"  int v{k} [0:{len(steps) - 1}];")
        lines.append(f"  int w{k} [0:{len(steps) - 1}];")
    k0 = len(CELLS)
    bodies = [(f"c{k}", cell_lines(k, c)) for k, c in enumerate(CELLS)]
    bodies += [(key, body) for key, body in extra_lines(k0).items()]
    calls = []
    for key, body in bodies:
        if container == "initial":
            calls.append(body)
            continue
        if container == "fork":
            calls.append(["fork begin"] + ["  " + b for b in body] + ["end join"])
            continue
        if container == "task":
            lines.append(f"  task t_{key};")
            lines += ["    " + b for b in body]
            lines.append("  endtask")
            calls.append([f"t_{key};"])
        elif container == "function":
            lines.append(f"  function void f_{key};")
            lines += ["    " + b for b in body]
            lines.append("  endfunction")
            calls.append([f"f_{key}();"])
        else:
            lines.append(f"  if (1) begin : g_{key}")
            lines.append("    task run;")
            lines += ["      " + b for b in body]
            lines.append("    endtask")
            lines.append("  end")
            calls.append([f"g_{key}.run;"])
    lines.append("  initial begin")
    for c in calls:
        lines += ["    " + b for b in c]
    lines.append("  end")
    lines.append("endmodule")
    # The warnings: at each $dist_* call of the bad_arguments cell.
    stderr = []
    for i, text in enumerate(lines, 1):
        for fn, what in BAD:
            col = text.find(fn + "(d,")
            if col >= 0:
                stderr.append(f"{name}.sv:{i}:{col + 1}: warning: {fn}: {what} is 0 or less; "
                              "the result is 0")
    stdout = [expected(k, c) for k, c in enumerate(CELLS)]
    stdout += [f"c{k0 + i} {e}" for i, e in enumerate(EXTRA_EXPECTED)]
    return name, "\n".join(lines) + "\n", "\n".join(stdout) + "\n", "\n".join(stderr) + "\n"


def illegal():
    """A seed that is not a variable (20.14.1)."""
    name = "random_seed_not_a_variable"
    lines = (f"/*\n:name: {name}\n:description: the seed of $random is a variable, so a constant "
             "seed is an error\n:tags: proc error\n:type: elaboration\n:lrm: 1800-2023 20.14.1\n"
             ":oracle: spec\n:spec_check: ai\n:fourstate: no\n:nondeterministic: no\n"
             f":generator: gen_random_matrix.py@{VERSION}\n"
             ":should_fail_because: the seed of $random is a constant, not a variable\n"
             "EXPECTED\n*/\n"
             "// Generated by tools/gen_random_matrix.py; change the generator, not this file.\n"
             "module top;\n  int v;\n  initial v = $random(5);\nendmodule\n").split("\n")
    at = next(i for i, t in enumerate(lines, 1) if "$random(5)" in t)
    col = lines[at - 1].index("$random") + 1
    text = "\n".join(lines).replace(
        "EXPECTED", f":expected_error: {at}:{col}: error: the seed of $random is to be a variable "
        "(20.14.1)")
    return name, text


def files():
    out = {}
    for container in CONTAINERS:
        name, sv, stdout, stderr = test(container)
        out[f"{name}.sv"] = sv
        out[f"{name}.expected"] = stdout
        out[f"{name}.stderr"] = stderr
    name, sv = illegal()
    out[f"{name}.sv"] = sv
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
                print(f"gen_random_matrix: {path.relative_to(OUT.parent.parent.parent)} is out "
                      "of date; run tools/gen_random_matrix.py", file=sys.stderr)
                bad += 1
        else:
            path.write_text(text, encoding="utf-8")
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main())
