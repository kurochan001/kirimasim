#!/usr/bin/env python3
"""Generates the matrix tests of file input (TEST_SUITE.md, "Mechanism
matrix") in tests/suite/proc/.

  tools/gen_file_input_matrix.py           # writes the files
  tools/gen_file_input_matrix.py --check   # exit 1 if a file differs from what it would write

The rules (21.3.4 to 21.3.8, SYSTEM_TASKS.md 6.3 and 6.4):

- $fgetc returns the next byte, or -1 at the end; $feof is 1 once a read
  has met the end, until $fseek or $rewind. Meeting the end is not an
  error: $ferror then returns 0 and empties its string.
- $ungetc pushes back any number of characters, the last one pushed read
  first; $ftell counts them as not read yet.
- $fgets reads up to a newline, which it includes, or the end. Into a
  string it reads any number; into an integral variable at most
  floor(width / 8) characters, placed as a string literal is, with 0
  above. It returns the number read, and leaves the target when it is 0.
- $fscanf reads as $sscanf does, and takes from the file exactly what its
  conversions read: %f on "1e+x" reads 1.0 and leaves "e+x". It returns
  -1 when the file ends before the first conversion.
- $fread fills an integral variable from ceil(width / 8) bytes, the first
  one most significant, with the bits above the width dropped; the bytes
  not read keep their value. A memory is read element by element from
  start (the lowest address by default) toward the highest, at most count
  elements, until one is not read whole. It returns the bytes read.
- $fseek from the start, the position or the end (0, 1, 2) and $rewind
  return 0, or -1 for another operation, after which $ferror is not 0.
- A file opened with "w" does not read: $fgetc and $fscanf give -1, $fgets
  and $fread 0, and $ferror is not 0. "w+" and "a+" read and write; a
  write after a read goes where the next read would have read, as reading
  and writing share one position (21.3.5).
- STDIN reads standard input, which the test tools leave empty
  (TEST_SUITE.md, "Where a test runs").

The axes: the way of reading (the cells below) and the container of the
calls (an initial procedure, a task, a function, a fork branch, a task in
a generate block). Each cell writes its file, opens it, gives the
descriptor to the container's code, and closes it. Every test runs in an
empty directory of its own.

The expected outputs follow from the rules above, without reading
kirimasim's code. For reference, checked 2026-10-21 with Icarus 14.0
devel, which does not accept the empty start of $fread(m, fd, , 1), with
a start of 0 there: it differs in three cells. In fgets_integral it sets
a target narrower than a byte to 0; in fscanf it takes all of "1e+x" and
assigns nothing, as C's fscanf may; in update its write after a read does
not land where the next read would have read.
"""

import argparse
import sys
from pathlib import Path

OUT = Path(__file__).resolve().parent.parent / "tests" / "suite" / "proc"
# The version of the rules below (TEST_SUITE.md, :generator:). Raise it
# whenever they change.
VERSION = 1

CONTAINERS = ["initial", "task", "function", "fork", "generate"]


def text(s):
    """A string literal of SystemVerilog for s (\\n for a newline)."""
    return '"' + s.replace("\\", "\\\\").replace("\n", "\\n").replace('"', '\\"') + '"'


# Each cell: (name, how the file is made, the mode it is opened with, the
# declarations, the statements, the lines they print). The file is made by
# writing a text, or bytes, with "w"; None makes no file (STDIN, or a file
# opened with "w" or "w+" by the cell itself).
CELLS = [
    ("fgetc", ("text", "ab"), "r",
     ["integer c, e, err;", "string why;"],
     ["c = $fgetc(fd); e = $feof(fd); $display(\"fgetc %0d %0d\", c, e);"] * 3 +
     ["err = $ferror(fd, why); $display(\"fgetc error %0d %0d\", err, why.len());"],
     ["fgetc 97 0", "fgetc 98 0", "fgetc -1 1", "fgetc error 0 0"]),
    ("ungetc", ("text", "xy"), "r",
     ["integer c, u1, u2, t, c1, c2, c3, t2, u3, k, c4;"],
     ["c = $fgetc(fd); c = $fgetc(fd);", "u1 = $ungetc(65, fd); u2 = $ungetc(66, fd);",
      "t = $ftell(fd); c1 = $fgetc(fd); c2 = $fgetc(fd); c3 = $fgetc(fd); t2 = $ftell(fd);",
      "u3 = $ungetc(67, fd); k = $fseek(fd, 0, 1); c4 = $fgetc(fd);",
      "$display(\"ungetc %0d %0d %0d %0d %0d %0d %0d %0d %0d\", u1, u2, t, c1, c2, c3, t2, k, "
      "c4);"],
     ["ungetc 0 0 0 66 65 -1 2 0 121"]),
    ("fgets_string", ("text", "ab\ncd"), "r",
     ["integer n;", "string s;"],
     ["n = $fgets(s, fd); $display(\"fgets %0d [%s]\", n, s);"] * 3,
     ["fgets 3 [ab", "]", "fgets 2 [cd]", "fgets 0 [cd]"]),
    ("fgets_integral", ("text", "abcdefghijklmnop\n"), "r",
     ["integer n1, n2, n3, n4, n5;", "reg [31:0] r32;", "reg [35:0] r36;", "bit [11:0] b12;",
      "reg [71:0] r72;", "reg [6:0] r7;"],
     ["r32 = '1; r36 = '1; b12 = '1; r72 = '1; r7 = 7'h55;",
      "n1 = $fgets(r32, fd); n2 = $fgets(r36, fd); n3 = $fgets(b12, fd);",
      "n4 = $fgets(r72, fd); n5 = $fgets(r7, fd);",
      "$display(\"fgets %0d %h\", n1, r32);", "$display(\"fgets %0d %h\", n2, r36);",
      "$display(\"fgets %0d %h\", n3, b12);", "$display(\"fgets %0d %h\", n4, r72);",
      "$display(\"fgets %0d %h\", n5, r7);"],
     ["fgets 4 61626364", "fgets 4 065666768", "fgets 1 069", "fgets 8 006a6b6c6d6e6f700a",
      "fgets 0 55"]),
    ("fscanf", ("text", "12 ab 3.5 x\n1e+x 7"), "r",
     ["integer s1, i1, i2, s2, c, n, s3, i3;", "string str, line;", "real rv, rv2;"],
     ["i3 = 5;",
      "s1 = $fscanf(fd, \"%d %s %f %d\", i1, str, rv, i2);",
      "$display(\"fscanf %0d %0d %s %0.1f %0d\", s1, i1, str, rv, i2);",
      "s2 = $fscanf(fd, \"%f\", rv2); c = $fgetc(fd);",
      "$display(\"fscanf %0d %0.1f %0d\", s2, rv2, c);",
      "n = $fgets(line, fd); $display(\"fscanf rest %0d [%s]\", n, line);",
      "s3 = $fscanf(fd, \"%d\", i3); $display(\"fscanf %0d %0d\", s3, i3);"],
     ["fscanf 4 12 ab 3.5 x", "fscanf 1 1.0 101", "fscanf rest 4 [+x 7]", "fscanf -1 5"]),
    ("fread_integral", ("bytes", [0x81, 0x82, 0x83, 0x84, 0x85]), "r",
     ["integer r1, r2, r3, r4;", "reg [11:0] w12;", "reg [15:0] w16;", "logic [31:0] w32;"],
     ["w12 = '1; w16 = '1; w32 = 0;",
      "r1 = $fread(w12, fd); $display(\"fread %0d %h\", r1, w12);",
      "r2 = $fread(w16, fd); $display(\"fread %0d %h\", r2, w16);",
      "r3 = $fread(w32, fd); $display(\"fread %0d %h\", r3, w32);",
      "r4 = $fread(w12, fd); $display(\"fread %0d %h\", r4, w12);"],
     ["fread 2 182", "fread 2 8384", "fread 1 85000000", "fread 0 182"]),
    ("fread_memory", ("bytes", [1, 2, 3, 4, 5, 6, 7, 0x0a, 0x0b, 0x0d]), "r",
     ["integer r1, r2, r3, r4, r5, r6;", "reg [7:0] m [0:3];", "reg [8:0] m9 [1:2];",
      "reg [7:0] dm [3:1];"],
     ["foreach (m[i]) m[i] = 8'hee; foreach (m9[i]) m9[i] = 0; foreach (dm[i]) dm[i] = 8'hee;",
      "r1 = $fread(m, fd, 1, 2); r2 = $fread(m, fd, , 1); r3 = $fread(m9, fd);",
      "r4 = $fread(dm, fd, 2);",
      "$display(\"m %h %h %h %h m9 %h %h dm %h %h %h\", m[0], m[1], m[2], m[3], m9[1], m9[2], "
      "dm[1], dm[2], dm[3]);",
      "r5 = $fread(m9, fd); r6 = $fread(m, fd, 3);",
      "$display(\"fread memory %0d %0d %0d %0d %0d %0d m9 %h %h\", r1, r2, r3, r4, r5, r6, "
      "m9[1], m9[2]);"],
     ["m 03 01 02 ee m9 005 007 dm ee 0a 0b", "fread memory 2 1 4 2 1 0 m9 105 007"]),
    ("seek", ("text", "0123456789"), "r",
     ["integer t0, k1, c1, t1, k2, c2, k3, c3, c4, e1, w, e2, c5, k4, err;", "string why;"],
     ["t0 = $ftell(fd); k1 = $fseek(fd, 5, 0); c1 = $fgetc(fd); t1 = $ftell(fd);",
      "k2 = $fseek(fd, -2, 1); c2 = $fgetc(fd); k3 = $fseek(fd, -1, 2); c3 = $fgetc(fd);",
      "c4 = $fgetc(fd); e1 = $feof(fd); w = $rewind(fd); e2 = $feof(fd); c5 = $fgetc(fd);",
      "k4 = $fseek(fd, 0, 3); err = $ferror(fd, why);",
      "$display(\"seek %0d %0d %0d %0d %0d %0d %0d %0d %0d %0d %0d %0d %0d %0d %0d\", t0, k1, "
      "c1, t1, k2, c2, k3, c3, c4, e1, w, e2, c5, k4, err != 0);"],
     ["seek 0 0 53 6 0 52 0 57 -1 1 0 0 48 -1 1"]),
    ("write_only", None, "w",
     ["integer c, e1, n, s, r, e, e2;", "string str, why;", "reg [11:0] w12;"],
     ["c = $fgetc(fd); e1 = $ferror(fd, why); n = $fgets(str, fd);",
      "s = $fscanf(fd, \"%d\", c); r = $fread(w12, fd); e = $feof(fd);",
      "$fwrite(fd, \"ok\"); e2 = $ferror(fd, why);",
      "$display(\"write only %0d %0d %0d %0d %0d %0d %0d %0d\", c, e1 != 0, n, s, r, e, e2, "
      "why.len());"],
     ["write only -1 1 0 -1 0 0 0 0"]),
    ("update", None, "w+",
     ["integer w, n, w2, n2, n3;", "string s, s2;"],
     ["$fwrite(fd, \"hi\\nthere\\n\"); w = $rewind(fd); n = $fgets(s, fd);",
      "$fwrite(fd, \"X\"); w2 = $rewind(fd); n2 = $fgets(s, fd); n3 = $fgets(s2, fd);",
      "$display(\"update %0d %0d %0d %0d %0d %s\", w, n, w2, n2, n3, s2.substr(0, 4));"],
     ["update 0 3 0 3 6 Xhere"]),
    ("append", ("text", "one\n"), "a+",
     ["integer w, n1, n2;", "string s1, s2;"],
     ["$fwrite(fd, \"two\\n\"); w = $rewind(fd); n1 = $fgets(s1, fd); n2 = $fgets(s2, fd);",
      "$display(\"append %0d %0d %0d %s %s\", w, n1, n2, s1.substr(0, 2), s2.substr(0, 2));"],
     ["append 0 4 4 one two"]),
    ("stdin", None, None,
     ["integer c, e, n;", "string s;"],
     ["c = $fgetc(fd); e = $feof(fd); n = $fgets(s, fd);",
      "$display(\"stdin %0d %0d %0d\", c, e, n);"],
     ["stdin -1 1 0"]),
]


def body(decls, stmts, indent):
    pad = " " * indent
    return "".join(f"{pad}{line}\n" for line in decls + stmts)


def test(container):
    name = f"file_input_{container}"
    sv = (f"/*\n:name: {name}\n:description: $fgetc, $ungetc, $fgets, $fscanf, $fread, $ftell, "
          f"$fseek, $rewind, $feof and $ferror on files read, written only, for update and for "
          f"append, and on STDIN, called from {'an' if container[0] in 'aeiou' else 'a'} "
          f"{container}\n:tags: proc\n:type: simulation\n"
          f":lrm: 1800-2023 21.3.4 21.3.5 21.3.7 21.3.8 21.3.1\n:oracle: spec\n:spec_check: ai\n"
          f":fourstate: yes\n:nondeterministic: no\n"
          f":generator: gen_file_input_matrix.py@{VERSION}\n:top_module: top\n"
          f":expected_stdout: @{name}.expected\n*/\n"
          "// Generated by tools/gen_file_input_matrix.py; change the generator, not this file.\n"
          "`timescale 1ns/1ns\nmodule top;\n  integer fd, mk;\n")
    for k, (cell, _, _, decls, stmts, _) in enumerate(CELLS):
        if container == "task":
            sv += f"  task t{k}(input integer fd); // {cell}\n{body(decls, stmts, 4)}  endtask\n"
        elif container == "function":
            sv += (f"  function void f{k}(input integer fd); // {cell}\n{body(decls, stmts, 4)}"
                   "  endfunction\n")
        elif container == "generate":
            sv += (f"  if (1) begin : g{k} // {cell}\n    task t(input integer fd);\n"
                   f"{body(decls, stmts, 6)}    endtask\n  end\n")
    sv += "  initial begin\n"
    exp = ""
    for k, (cell, made, mode, decls, stmts, lines) in enumerate(CELLS):
        sv += f"    // {cell}\n"
        file = f"i{k}.txt"
        if made is not None:
            kind, data = made
            sv += f'    mk = $fopen("{file}", "w");\n'
            if kind == "text":
                sv += f'    $fwrite(mk, "%s", {text(data)});\n'
            else:
                sv += (f'    $fwrite(mk, "{"%c" * len(data)}", '
                       + ", ".join(f"8'h{b:02x}" for b in data) + ");\n")
            sv += "    $fclose(mk);\n"
        if mode is None:
            sv += "    fd = 32'h8000_0000;\n"
        else:
            sv += f'    fd = $fopen("{file}", "{mode}");\n'
        if container == "initial":
            sv += "    begin\n" + body(decls, stmts, 6) + "    end\n"
        elif container == "fork":
            sv += "    fork begin\n" + body(decls, stmts, 6) + "    end join\n"
        else:
            sv += "    " + {"task": f"t{k}(fd);", "function": f"f{k}(fd);",
                            "generate": f"g{k}.t(fd);"}[container] + "\n"
        if mode is not None:
            sv += "    $fclose(fd);\n"
        exp += "".join(line + "\n" for line in lines)
    sv += "  end\nendmodule\n"
    return name, sv, exp


def files():
    out = {}
    for container in CONTAINERS:
        name, sv, exp = test(container)
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
                print(f"gen_file_input_matrix: {path.relative_to(OUT.parent.parent.parent)} is "
                      "out of date; run tools/gen_file_input_matrix.py", file=sys.stderr)
                bad += 1
        else:
            path.write_text(content, encoding="utf-8")
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main())
