#!/usr/bin/env python3
"""Generates the matrix tests of $ungetc and the type a file is opened
with (TEST_SUITE.md, "Mechanism matrix") in tests/suite/proc/.

  tools/gen_ungetc_matrix.py           # writes the files
  tools/gen_ungetc_matrix.py --check   # exit 1 if a file differs from what it would write

The rules (21.3.4.1, 21.3.4.3, SYSTEM_TASKS.md 6.3 and 6.4):

- $ungetc pushes a character back on any open file and returns 0: one
  opened only for writing is open too. The next reads of the fd return
  the characters pushed back, the last pushed first, whatever the file
  was opened for (21.3.4.3).
- A file opened with "r" or "r+" then goes on with its own bytes.
- A file opened with "w" or "a" only writes: once what was pushed back
  has been read, a read fails with EBADF, and $ferror is not 0. $fgetc
  returns -1; $fgets, $fscanf and $fread return what they read before,
  or 0, -1 and 0 when they read nothing.
- STDOUT is open for writing, so the same holds for it.

The axes: the type of the file ("w", "a", "r", "r+"); the reading
function ($fgetc three times, $fgets, $fscanf with %s, $fread into 16
bits); the characters pushed back first (none, "A", and "A" then "B");
the container of the calls (an initial procedure, a task, a function).
One file per type and container, a line per cell. The file holds "pq"
and a newline before it is opened, except with "w", which empties it.

The expected outputs follow from the rules above, without reading
kirimasim's code. For reference, checked 2026-10-21 by running them on
a file opened with "w": C libraries differ (glibc gives the character
back; macOS fails ungetc with EBADF), and Icarus follows the C library
of its OS (SYSTEM_TASKS.md 6.3).
"""

from __future__ import annotations

import argparse
import sys
from pathlib import Path

OUT = Path(__file__).resolve().parent.parent / "tests" / "suite" / "proc"
PREFIX = "ungetc_"
# The version of the rules below (TEST_SUITE.md, :generator:). Raise it
# whenever they change.
VERSION = 1

TYPES = {"w": "w", "a": "a", "r": "r", "rplus": "r+"}
CONTAINERS = ["initial", "task", "function"]
FUNCTIONS = ["fgetc", "fgets", "fscanf", "fread"]
CONTENT = "pq\n"
PUSHED = ["", "A", "AB"]  # in the order pushed; read back last first

DECLS = ["integer u1, u2, c1, c2, c3, n, e;", "string s, why;", "reg [15:0] w;"]


def reads(mode: str, pushed: str) -> tuple[list[int], bool]:
    """The bytes the fd gives in turn, and whether it fails after them."""
    back = [ord(c) for c in reversed(pushed)]
    if mode in ("r", "r+"):
        return back + [ord(c) for c in CONTENT], False
    return back, True


def cell(function: str, pushed: str) -> list[str]:
    """The statements of a cell, on fd."""
    lines = ["u1 = -9; u2 = -9; s = \"\"; w = 16'h0000;"]
    if len(pushed) >= 1:
        lines.append(f"u1 = $ungetc({ord(pushed[0])}, fd);")
    if len(pushed) == 2:
        lines.append(f"u2 = $ungetc({ord(pushed[1])}, fd);")
    k = len(pushed)
    if function == "fgetc":
        lines += ["c1 = $fgetc(fd); c2 = $fgetc(fd); c3 = $fgetc(fd); e = $ferror(fd, why);",
                  f'$display("fgetc {k}: %0d %0d | %0d %0d %0d | %0d", u1, u2, c1, c2, c3, '
                  "e != 0);"]
    elif function == "fgets":
        lines += ["n = $fgets(s, fd); e = $ferror(fd, why);",
                  "if (s.len() > 0 && s.getc(s.len() - 1) == 10) s = s.substr(0, s.len() - 2);",
                  f'$display("fgets {k}: %0d %0d | %0d [%s] | %0d", u1, u2, n, s, e != 0);']
    elif function == "fscanf":
        lines += ["n = $fscanf(fd, \"%s\", s); e = $ferror(fd, why);",
                  f'$display("fscanf {k}: %0d %0d | %0d [%s] | %0d", u1, u2, n, s, e != 0);']
    else:
        lines += ["n = $fread(w, fd); e = $ferror(fd, why);",
                  f'$display("fread {k}: %0d %0d | %0d %h | %0d", u1, u2, n, w, e != 0);']
    return lines


def expected(mode: str, function: str, pushed: str) -> str:
    k = len(pushed)
    u1 = 0 if k >= 1 else -9
    u2 = 0 if k == 2 else -9
    got, fails = reads(mode, pushed)
    if function == "fgetc":
        cs = (got + [-1, -1, -1])[:3]
        err = fails and len(got) < 3
        return f"fgetc {k}: {u1} {u2} | {cs[0]} {cs[1]} {cs[2]} | {int(err)}"
    if function == "fgets":
        line = []
        for c in got:
            line.append(c)
            if c == 10:
                break
        err = fails and 10 not in line
        text = "".join(chr(c) for c in line).rstrip("\n")
        return f"fgets {k}: {u1} {u2} | {len(line)} [{text}] | {int(err)}"
    if function == "fscanf":
        word = []
        for c in got:
            if chr(c).isspace():
                break
            word.append(c)
        err = fails and len(word) == len(got)
        n = 1 if word else -1
        return f"fscanf {k}: {u1} {u2} | {n} [{''.join(chr(c) for c in word)}] | {int(err)}"
    take = got[:2]
    value = 0
    for i, c in enumerate(take):
        value |= c << (8 * (1 - i))
    err = fails and len(take) < 2
    return f"fread {k}: {u1} {u2} | {len(take)} {value:04x} | {int(err)}"


def indent(lines: list[str], by: int) -> str:
    return "".join(" " * by + line + "\n" for line in lines)


def test(kind: str, container: str) -> tuple[str, str, str]:
    mode = TYPES[kind]
    name = f"{PREFIX}{kind}_{container}"
    sv = (f"/*\n:name: {name}\n:description: $ungetc on a file opened with \"{mode}\", read back "
          f"with $fgetc, $fgets, $fscanf and $fread, called from "
          f"{'an' if container[0] in 'aeiou' else 'a'} {container}\n:tags: proc\n"
          ":type: simulation\n:lrm: 1800-2023 21.3.4.1 21.3.4.3 21.3.7\n:oracle: spec\n"
          ":spec_check: ai\n:fourstate: no\n:nondeterministic: no\n"
          f":generator: gen_ungetc_matrix.py@{VERSION}\n:top_module: top\n"
          f":expected_stdout: @{name}.expected\n*/\n"
          "// Generated by tools/gen_ungetc_matrix.py; change the generator, not this file.\n"
          "`timescale 1ns/1ns\nmodule top;\n  integer fd, mk;\n")
    cells = [(f, p) for f in FUNCTIONS for p in PUSHED]
    for i, (function, pushed) in enumerate(cells):
        if container == "task":
            sv += f"  task t{i}(input integer fd);\n" + indent(DECLS + cell(function, pushed), 4)
            sv += "  endtask\n"
        elif container == "function":
            sv += f"  function void f{i}(input integer fd);\n"
            sv += indent(DECLS + cell(function, pushed), 4) + "  endfunction\n"
    sv += "  initial begin\n"
    if mode != "w":
        sv += ('    mk = $fopen("u.txt", "w");\n    $fwrite(mk, "pq\\n");\n'
               "    $fclose(mk);\n")
    exp = []
    for i, (function, pushed) in enumerate(cells):
        sv += f'    fd = $fopen("u.txt", "{mode}");\n'
        if container == "initial":
            sv += "    begin\n" + indent(DECLS + cell(function, pushed), 6) + "    end\n"
        else:
            sv += f"    {'t' if container == 'task' else 'f'}{i}(fd);\n"
        sv += "    $fclose(fd);\n"
        exp.append(expected(mode, function, pushed))
    if mode == "w":
        # STDOUT is open for writing too.
        sv += ("    begin\n      integer u, c1, c2;\n      u = $ungetc(65, 32'h8000_0001);\n"
               "      c1 = $fgetc(32'h8000_0001); c2 = $fgetc(32'h8000_0001);\n"
               '      $display("stdout: %0d | %0d %0d", u, c1, c2);\n    end\n')
        exp.append("stdout: 0 | 65 -1")
    sv += "  end\nendmodule\n"
    return name, sv, "".join(line + "\n" for line in exp)


def files() -> dict[str, str]:
    out = {}
    for kind in TYPES:
        for container in CONTAINERS:
            name, sv, exp = test(kind, container)
            out[f"{name}.sv"] = sv
            out[f"{name}.expected"] = exp
    return out


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("--check", action="store_true")
    args = ap.parse_args()
    bad = 0
    for name, content in files().items():
        path = OUT / name
        if args.check:
            if not path.exists() or path.read_text(encoding="utf-8") != content:
                print(f"gen_ungetc_matrix: {path.relative_to(OUT.parent.parent.parent)} is "
                      "out of date; run tools/gen_ungetc_matrix.py", file=sys.stderr)
                bad += 1
        else:
            path.write_text(content, encoding="utf-8")
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main())
