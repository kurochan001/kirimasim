#!/usr/bin/env python3
# Copyright (c) 2026 The kirimasim Authors
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

"""Generates the matrix tests of $readmemb, $readmemh, $writememb and
$writememh (TEST_SUITE.md, "Mechanism matrix") in tests/suite/proc/.

  tools/gen_memory_file_matrix.py           # writes the files
  tools/gen_memory_file_matrix.py --check   # exit 1 if a file differs from what it would write

The rules (21.4, 21.5, SYSTEM_TASKS.md 7, UNSPECIFIED_BEHAVIOR.md P4):

- A file holds white space, comments of both kinds, numbers of the radix
  (with x, z and _; no width or radix), and addresses: @ and a
  hexadecimal number. A number with fewer digits than the word leaves 0
  above them; one with more keeps its low digits.
- With no address arguments the words go up from the lowest address of
  the memory; with a start alone, up from it to the highest address; with
  start and finish, from start to finish, down when start is larger. An
  address in the file changes where the next word goes, and must lie in
  that range, or the reading stops with the warning kirima-readmem-range.
- A file with no address whose number of words differs from the number of
  words in the range gives the warning kirima-readmem-short; words it
  does not give are not changed, and words past the range are not loaded.
- A memory of more than one dimension takes one word per element below an
  address of its first dimension, each lower dimension from its low to its
  high index (21.4.3).
- A 2-state memory takes x and z as 0.
- $writememh and $writememb write one word per line, every digit of the
  width, in the order $readmem reads the same range, so that reading the
  file back with the same arguments gives the same values.
- Each warning comes once for each place.

The axes: the way of reading or writing (the cells below: task, memory,
arguments, what the file holds) and the container of the calls (an
initial procedure, a task, a function, a fork branch, a task in a
generate block); one more cell reads a memory of an automatic task. Every
test runs in an empty directory of its own.

The expected outputs follow from the rules above, without reading
kirimasim's code. For reference, checked 2026-10-21: Icarus 14.0 devel
prints the same but in the cell dimensions_address, where it takes an
address in the file as the number of an element, not of a word of the
first dimension (21.4.3).
"""

import argparse
import sys
from pathlib import Path

OUT = Path(__file__).resolve().parent.parent / "tests" / "suite" / "proc"
# The version of the rules below (TEST_SUITE.md, :generator:). Raise it
# whenever they change.
VERSION = 1

CONTAINERS = ["initial", "task", "function", "fork", "generate"]

# The memories: name -> (declaration, element width, dimensions, 4-state).
MEMS = {
    "m": ("logic [7:0] m [0:7];", 8, [(0, 7)], True),
    "d": ("logic [7:0] d [7:0];", 8, [(7, 0)], True),
    "b": ("bit [7:0] b [0:3];", 8, [(0, 3)], False),
    "n": ("logic [3:0] n [0:3];", 4, [(0, 3)], True),
    "w": ("logic [71:0] w [0:1];", 72, [(0, 1)], True),
    "q": ("logic [7:0] q [0:1][2:0];", 8, [(0, 1), (2, 0)], True),
}
FILL = {8: 0xEE, 4: 0xE, 72: 0xEE}

# Reading cells: (name, task, memory, the file's text, address arguments).
READS = [
    ("plain", "$readmemh", "m", "0a 0B c_c\n", []),
    ("comments_addresses", "$readmemh", "m", "// words\n11 /* skipped */ @5 55\n66 77\n", []),
    ("start_finish_down", "$readmemh", "m", "a b c d\n", [6, 3]),
    ("start_only", "$readmemh", "m", "1 2 3\n", [5]),
    ("x_and_z", "$readmemh", "m", "x z5 1x\n", [0, 2]),
    ("two_state", "$readmemh", "b", "x z5 1x 4\n", []),
    ("declared_down", "$readmemh", "d", "1 2\n", []),
    ("digits", "$readmemh", "m", "fff 1\n", [0, 1]),
    ("binary", "$readmemb", "n", "1x01 0z11 _1_0 1\n", []),
    ("wide", "$readmemh", "w", "123456789abcdef012 x1\n", []),
    ("dimensions", "$readmemh", "q", "10 11 12\n20 21 22\n", []),
    ("dimensions_address", "$readmemh", "q", "@1 a0 a1\n", []),
    ("address_outside", "$readmemh", "m", "5 @9 6\n", [0, 3]),
    ("past_the_range", "$readmemh", "m", "1 2 3 4\n", [6, 7]),
]
# Round trips: (name, radix h or b, memory, the values put in by index
# tuple, address arguments); written out, the memory cleared, read back.
TRIPS = [
    ("write_read_hex", "h", "m", {(i,): (0x10 * i + 1, 0) for i in range(7)} | {(7,): (0xF0, 0xF0)},
     []),
    ("write_read_binary_down", "b", "n", {(0,): (5, 0), (1,): (0b1010, 0b0011), (2,): (3, 0),
                                          (3,): (0b0110, 0b0110)}, [3, 1]),
    ("write_read_dimensions", "h", "q", {(i, j): (0x10 * i + j, 0) for i in range(2)
                                         for j in range(3)}, []),
]


def elements(dims):
    """The index tuples of a memory, in slot order (row-major, each
    dimension from its left bound)."""
    out = [()]
    for left, right in dims:
        step = 1 if left <= right else -1
        out = [e + (i,) for e in out for i in range(left, right + step, step)]
    return out


def below(dims):
    """The index tuples under one address, each dimension from low to high."""
    out = [()]
    for left, right in dims[1:]:
        out = [e + (i,) for e in out for i in range(min(left, right), max(left, right) + 1)]
    return out


def span(dims, args):
    """The first-dimension range (from, to, step) of the address arguments."""
    lo, hi = min(dims[0]), max(dims[0])
    if not args:
        return lo, hi, 1
    if len(args) == 1:
        return args[0], hi, 1
    return args[0], args[1], 1 if args[0] <= args[1] else -1


def visited(dims, args):
    """The index tuples a range reads or writes, in order."""
    frm, to, step = span(dims, args)
    return [(a,) + e for a in range(frm, to + step, step) for e in below(dims)]


def number(token, hex_, width, four):
    """A word: (value bits, x/z bits) of width bits."""
    a = b = 0
    pos = 0
    bits = 4 if hex_ else 1
    for ch in reversed(token.replace("_", "")):
        if ch in "xX":
            da = db = (1 << bits) - 1
        elif ch in "zZ":
            da, db = 0, (1 << bits) - 1
        else:
            da, db = int(ch, 16 if hex_ else 2), 0
        for k in range(bits):
            if pos < width:
                a |= ((da >> k) & 1) << pos
                b |= ((db >> k) & 1) << pos
                pos += 1
    return (a & ~b, 0) if not four else (a, b)


def tokens(text):
    out = []
    i = 0
    while i < len(text):
        if text[i].isspace():
            i += 1
        elif text.startswith("//", i):
            j = text.find("\n", i)
            i = len(text) if j < 0 else j
        elif text.startswith("/*", i):
            i = text.find("*/", i) + 2
        else:
            j = i
            while j < len(text) and not text[j].isspace() and not text.startswith("/", j):
                j += 1
            out.append(text[i:j])
            i = j
    return out


def load(mem, task, text, args, values):
    """Applies a $readmem to values; returns its warning, if any."""
    _, width, dims, four = MEMS[mem]
    frm, to, step = span(dims, args)
    held = (lambda x: frm <= x <= to) if step > 0 else (lambda x: to <= x <= frm)
    under = below(dims)
    at, k, count, addressed, loading = frm, 0, 0, False, True
    for tok in tokens(text):
        if tok.startswith("@"):
            a = int(tok[1:], 16)
            if not held(a):
                return f"address @{a:x} in the file is outside the range {frm} to {to}"
            addressed, at, k, loading = True, a, 0, True
            continue
        count += 1
        if loading and not held(at):
            loading = False
        if loading:
            values[(at,) + under[k]] = number(tok, task.endswith("h"), width, four)
            k += 1
            if k == len(under):
                k, at = 0, at + step
    size = (abs(to - frm) + 1) * len(under)
    if not addressed and count != size:
        return f"the file has {count} words for the {size} of the range {frm} to {to}"
    return None


def digits(a, b, width, radix):
    """%h or %b of a 4-state value, every digit (SYSTEM_TASKS.md 3.4)."""
    bits = 4 if radix == "h" else 1
    out = []
    for k in range((width + bits - 1) // bits):
        n = min(bits, width - bits * k)
        mask = (1 << n) - 1
        da, db = (a >> bits * k) & mask, (b >> bits * k) & mask
        if db == 0:
            out.append(format(da, "x"))
        elif db == mask:
            out.append("x" if da == mask else "z")
        else:
            out.append("X" if (da & db) != 0 else "Z")
    return "".join(reversed(out))


def literal(a, b, width):
    """A SystemVerilog literal of a 4-state value, in binary."""
    bits = "".join("x" if (b >> i) & 1 and (a >> i) & 1 else "z" if (b >> i) & 1
                   else str((a >> i) & 1) for i in reversed(range(width)))
    return f"{width}'b{bits}"


def ref(mem, e):
    return mem + "".join(f"[{i}]" for i in e)


class Writer:
    """The lines of a test, and the place of each $readmem call."""

    def __init__(self):
        self.lines = []
        self.places = {}

    def add(self, text, key=None):
        self.lines.append(text)
        if key is not None:
            self.places[key] = (len(self.lines), len(text) - len(text.lstrip()) + 1)


def cell_statements(k, kind, cell, mem_name):
    """(statement, is the $readmem call) of a cell."""
    name, task, mem = cell[0], cell[1], cell[2]
    _, width, dims, _ = MEMS[mem]
    m = mem_name
    out = []
    loops = "".join(f"[i{j}]" for j in range(len(dims)))
    fill = f"foreach ({m}[{','.join('i' + str(j) for j in range(len(dims)))}]) {m}{loops} ="
    if kind == "read":
        text, args = cell[3], cell[4]
        out += [(f'fd = $fopen("c{k}.mem", "w");', False),
                (f'$fwrite(fd, "%s", "{text.replace(chr(10), chr(92) + "n")}");', False),
                ("$fclose(fd);", False), (f"{fill} {width}'h{FILL[width]:x};", False),
                (f'{task}("c{k}.mem", {m}' + "".join(f", {x}" for x in args) + ");", True)]
    else:
        radix, vals, args = task, cell[3], cell[4]
        tail = "".join(f", {x}" for x in args)
        out += [(f"{ref(m, e)} = {literal(*vals[e], width)};", False) for e in elements(dims)]
        out += [(f'$writemem{radix}("c{k}.mem", {m}{tail});', False),
                (f"{fill} {width}'h0;", False),
                (f'$readmem{radix}("c{k}.mem", {m}{tail});', True)]
    radix = "b" if task in ("$readmemb", "b") else "h"
    fmt = " ".join(f"%{radix}" for _ in elements(dims))
    out.append((f'$display("c{k} {name}: {fmt}", ' +
                ", ".join(ref(m, e) for e in elements(dims)) + ");", False))
    return out


def cell_expected(kind, cell):
    """The line a cell prints, and its warning."""
    name, task, mem = cell[0], cell[1], cell[2]
    _, width, dims, four = MEMS[mem]
    if kind == "read":
        values = {e: (FILL[width], 0) for e in elements(dims)}
        warn = load(mem, task, cell[3], cell[4], values)
    else:
        written = set(visited(dims, cell[4]))
        values = {e: (cell[3][e] if e in written else (0, 0)) for e in elements(dims)}
        warn = None
    radix = "b" if task in ("$readmemb", "b") else "h"
    return " ".join(digits(*values[e], width, radix) for e in elements(dims)), warn


def test(container):
    name = f"memory_file_{container}"
    w = Writer()
    for line in (f"/*\n:name: {name}\n:description: $readmemh, $readmemb, $writememh and "
                 "$writememb with and without address arguments, addresses and comments in the "
                 "file, x and z, 2-state, wide and two-dimensional memories, too few and too many "
                 f"words, called from {'an' if container[0] in 'aeiou' else 'a'} {container}\n"
                 ":tags: proc\n:type: simulation\n:lrm: 1800-2023 21.4 21.4.3 21.5\n"
                 ":oracle: spec\n:spec_check: ai\n:fourstate: yes\n:nondeterministic: no\n"
                 f":generator: gen_memory_file_matrix.py@{VERSION}\n:top_module: top\n"
                 f":expected_stdout: @{name}.expected\n:expected_stderr: @{name}.stderr\n*/\n"
                 "// Generated by tools/gen_memory_file_matrix.py; change the generator, not "
                 "this file.\n`timescale 1ns/1ns\nmodule top;\n  integer fd;").split("\n"):
        w.add(line)
    for decl, _, _, _ in MEMS.values():
        w.add("  " + decl)
    cells = [("read", c) for c in READS] + [("trip", c) for c in TRIPS]
    calls = []
    for k, (kind, cell) in enumerate(cells):
        stmts = cell_statements(k, kind, cell, cell[2])
        if container == "initial":
            calls.append((k, stmts))
            continue
        if container == "fork":
            calls.append((k, [("fork begin", False)] + [("  " + s, c) for s, c in stmts] +
                          [("end join", False)]))
            continue
        head, pad, end = {"task": (f"  task t{k}; // {cell[0]}", "    ", ["  endtask"]),
                          "function": (f"  function void f{k}; // {cell[0]}", "    ",
                                       ["  endfunction"]),
                          "generate": (f"  if (1) begin : g{k} // {cell[0]}", "      ",
                                       ["    endtask", "  end"])}[container]
        w.add(head)
        if container == "generate":
            w.add("    task t;")
        for s, is_call in stmts:
            w.add(pad + s, k if is_call else None)
        for e in end:
            w.add(e)
        calls.append((k, [({"task": f"t{k};", "function": f"f{k}();",
                            "generate": f"g{k}.t;"}[container], False)]))
    # A memory of an automatic task, read from its frame.
    ka = len(cells)
    auto = ("automatic", "$readmemh", "m", "1 2 3 4 5 6 7 8\n", [])
    w.add("  task automatic ta;")
    w.add("    logic [7:0] am [0:7];")
    w.add("    integer fd;")
    for s, is_call in cell_statements(ka, "read", auto, "am"):
        w.add("    " + s, ka if is_call else None)
    w.add("  endtask")
    w.add("  initial begin")
    for k, stmts in calls:
        for s, is_call in stmts:
            w.add("    " + s, k if is_call else None)
    w.add("    ta;")
    w.add("  end")
    w.add("endmodule")
    stdout, warnings = "", []
    for k, (kind, cell) in enumerate(cells + [("read", auto)]):
        shown, warn = cell_expected(kind, cell)
        stdout += f"c{k} {cell[0]}: {shown}\n"
        if warn:
            warnings.append((w.places[k], f"{cell[1]}: {warn}"))
    stderr = "".join(f"{name}.sv:{line}:{col}: warning: {text}\n"
                     for (line, col), text in sorted(warnings))
    return name, "\n".join(w.lines) + "\n", stdout, stderr


# Run-time errors of the design (CLI.md 5): (name, what the file holds or
# None for no file, the error after the task's name). The run ends there.
ERRORS = [
    ("memory_file_cannot_open", None, 'cannot open "c.mem"'),
    ("memory_file_bad_text", "1 2\n3 8'h4 5\n",
     '"c.mem" line 2: \'\'\' is not part of a number, an address or a comment'),
]


def error_test(name, text, error):
    """A $readmemh that ends the run; the words before the error stay."""
    lines = [f"/*\n:name: {name}\n:description: $readmemh of a file that "
             + ("cannot be opened" if text is None else "has text that is no word")
             + " is a run-time error that ends the run\n:tags: proc\n:type: simulation\n"
             ":lrm: 1800-2023 21.4\n:oracle: spec\n:spec_check: ai\n:fourstate: yes\n"
             f":nondeterministic: no\n:generator: gen_memory_file_matrix.py@{VERSION}\n"
             f":top_module: top\n:expected_stdout: @{name}.expected\n:expected_exit: 2\n"
             f":expected_stderr: @{name}.stderr\n*/",
             "// Generated by tools/gen_memory_file_matrix.py; change the generator, not this "
             "file.", "`timescale 1ns/1ns", "module top;", "  logic [7:0] m [0:3];",
             "  integer fd;", "  initial begin", "    foreach (m[i]) m[i] = 8'hee;"]
    if text is not None:
        lines += ['    fd = $fopen("c.mem", "w");',
                  f'    $fwrite(fd, "%s", "{text.replace(chr(10), chr(92) + "n")}");',
                  "    $fclose(fd);"]
    lines.append('    #1 $readmemh("c.mem", m);')
    at = len("\n".join(lines).split("\n"))
    lines += ['    $display("not reached");', "  end", "  final $display(\"%h %h %h %h\", m[0], "
              "m[1], m[2], m[3]);", "endmodule"]
    kept = ["ee"] * 4 if text is None else ["01", "02", "03", "ee"]
    return ("\n".join(lines) + "\n", " ".join(kept) + "\n",
            f"{name}.sv:{at}: error: $readmemh: {error} [top @ 1ns]\n")


def files():
    out = {}
    for container in CONTAINERS:
        name, sv, stdout, stderr = test(container)
        out[f"{name}.sv"] = sv
        out[f"{name}.expected"] = stdout
        out[f"{name}.stderr"] = stderr
    for name, text, error in ERRORS:
        sv, stdout, stderr = error_test(name, text, error)
        out[f"{name}.sv"] = sv
        out[f"{name}.expected"] = stdout
        out[f"{name}.stderr"] = stderr
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
                print(f"gen_memory_file_matrix: {path.relative_to(OUT.parent.parent.parent)} is "
                      "out of date; run tools/gen_memory_file_matrix.py", file=sys.stderr)
                bad += 1
        else:
            path.write_text(text, encoding="utf-8")
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main())
