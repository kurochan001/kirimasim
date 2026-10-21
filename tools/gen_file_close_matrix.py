#!/usr/bin/env python3
# Copyright (c) 2026 The kirimasim Authors
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

"""Generates the matrix tests of how $fclose ends the output of $fstrobe
and $fmonitor (TEST_SUITE.md, "Mechanism matrix") in tests/suite/proc/.

  tools/gen_file_close_matrix.py           # writes the files
  tools/gen_file_close_matrix.py --check   # exit 1 if a file differs from what it would write

The rules (21.3.1, SYSTEM_TASKS.md 6.1 and 6.2): $fstrobe and $fmonitor
write to the files their descriptor named when they were called.
$fclose takes the files it closes out of them, without a warning: a
pending $fstrobe whose file is closed writes nothing, a $fmonitor goes
on with the files left and ends when none is left, and a file opened
later under the number that was freed (the lowest free bit of an mcd,
the lowest free fd from 32'h8000_0003) gets nothing of theirs.

The axes: the kind of descriptor (an mcd, an fd) and the case (a strobe
whose file is closed in the same time slot; the same, with the number
opened again for another file; a monitor whose file is closed after it
printed; the same, with the number opened again; and for an mcd, a
strobe and a monitor of two files one of which is closed). Each case
prints the files with $fgets at its end. Every test runs in an empty
directory of its own (TEST_SUITE.md, "What is judged now").

The expected outputs follow from the rules above, without reading
kirimasim's code; each case says what it expects. Where Icarus Verilog
differs, the tests say so (ICARUS).
"""

import argparse
import sys
from pathlib import Path

OUT = Path(__file__).resolve().parent.parent / "tests" / "suite" / "proc"
# The version of the rules below (TEST_SUITE.md, :generator:). Raise it
# whenever they change.
VERSION = 1

KINDS = ["mcd", "fd"]
# Where Icarus Verilog differs (run 2026-10-20, 14.0 devel), as the tests say.
ICARUS = {
    "mcd": "// Icarus Verilog 14.0 devel writes the pending $fstrobe of s2 into the file opened\n"
           "// under the freed bit, and drops the $fstrobe of p1 and the $fmonitor of p2 once one\n"
           "// of their two files is closed. 21.3.1 has $fclose cancel them; SYSTEM_TASKS.md 6.2\n"
           "// keeps them for the file left.\n",
    "fd": "// Icarus Verilog 14.0 devel writes the pending $fstrobe of s2 into the file opened\n"
          "// under the freed number; 21.3.1 has $fclose cancel it (SYSTEM_TASKS.md 6.2).\n",
}


def opener(kind):
    return (lambda n: f'$fopen("{n}")') if kind == "mcd" else (lambda n: f'$fopen("{n}", "w")')


def cases(kind):
    """(code, expected lines) of each case, in order."""
    op = opener(kind)
    out = [
        # A strobe whose file is closed before the Postponed region: nothing.
        (f'    d = {op("s1.txt")};\n    $fstrobe(d, "s1 strobe");\n    $fclose(d);\n'
         '    #1 show("s1.txt");\n',
         ["s1.txt: end"]),
        # The freed number opened again: the new file gets nothing either.
        (f'    d = {op("s2a.txt")};\n    $fstrobe(d, "s2 strobe");\n    $fclose(d);\n'
         f'    e = {op("s2b.txt")};\n    $display("s2 reused %0d", e == d);\n'
         '    #1 $fclose(e);\n    show("s2a.txt");\n    show("s2b.txt");\n',
         ["s2 reused 1", "s2a.txt: end", "s2b.txt: end"]),
        # A monitor prints until its file is closed, and then ends.
        (f'    v = 1;\n    d = {op("m1.txt")};\n    $fmonitor(d, "m1 v=%0d", v);\n'
         '    #1 v = 2;\n    #1 $fclose(d);\n    #1 v = 3;\n    #1 show("m1.txt");\n',
         ["m1.txt: m1 v=1", "m1.txt: m1 v=2", "m1.txt: end"]),
        # The freed number opened again: the new file gets nothing of it.
        (f'    v = 1;\n    d = {op("m2a.txt")};\n    $fmonitor(d, "m2 v=%0d", v);\n'
         f'    #1 $fclose(d);\n    e = {op("m2b.txt")};\n'
         '    $display("m2 reused %0d", e == d);\n    v = 2;\n    #1 v = 3;\n'
         '    #1 $fclose(e);\n    show("m2a.txt");\n    show("m2b.txt");\n',
         ["m2 reused 1", "m2a.txt: m2 v=1", "m2a.txt: end", "m2b.txt: end"]),
    ]
    if kind == "mcd":
        out += [
            # Two files, one closed: the strobe writes to the other.
            ('    d = $fopen("p1a.txt");\n    e = $fopen("p1b.txt");\n'
             '    $fstrobe(d | e, "p1 strobe");\n    $fclose(d);\n'
             '    #1 $fclose(e);\n    show("p1a.txt");\n    show("p1b.txt");\n',
             ["p1a.txt: end", "p1b.txt: p1 strobe", "p1b.txt: end"]),
            # Two files: the monitor goes on with the one left, and ends
            # when both are closed.
            ('    v = 1;\n    d = $fopen("p2a.txt");\n    e = $fopen("p2b.txt");\n'
             '    $fmonitor(d | e, "p2 v=%0d", v);\n    #1 $fclose(d);\n    v = 2;\n'
             '    #1 $fclose(e);\n    v = 3;\n    #1 show("p2a.txt");\n    show("p2b.txt");\n',
             ["p2a.txt: p2 v=1", "p2a.txt: end", "p2b.txt: p2 v=1", "p2b.txt: p2 v=2",
              "p2b.txt: end"]),
        ]
    return out


def test(kind):
    name = f"file_close_{kind}"
    sv = (f"/*\n:name: {name}\n:description: $fclose of the file of a pending $fstrobe or an "
          f"active $fmonitor, through an {kind}: they write nothing more to it, nor to a file "
          f"opened later under the freed number, and go on with the files left\n:tags: proc\n"
          f":type: simulation\n:lrm: 1800-2023 21.3.1 21.3.2 21.3.4.2\n:oracle: spec\n"
          f":spec_check: ai\n:fourstate: no\n:nondeterministic: no\n"
          f":generator: gen_file_close_matrix.py@{VERSION}\n:top_module: top\n"
          f":expected_stdout: @{name}.expected\n*/\n"
          "// Generated by tools/gen_file_close_matrix.py; change the generator, not this file.\n"
          + ICARUS[kind] +
          "`timescale 1ns/1ns\nmodule top;\n  integer d, e;\n  reg [7:0] v;\n"
          "  task show(input string name);\n    integer r, n;\n    string line;\n"
          "    r = $fopen(name, \"r\");\n    n = $fgets(line, r);\n"
          "    while (n > 0) begin\n      $write(\"%s: %s\", name, line);\n"
          "      n = $fgets(line, r);\n    end\n    $display(\"%s: end\", name);\n"
          "    $fclose(r);\n  endtask\n  initial begin\n")
    exp = ""
    for code, lines in cases(kind):
        sv += code
        exp += "".join(line + "\n" for line in lines)
    sv += "  end\nendmodule\n"
    return name, sv, exp


def files():
    out = {}
    for kind in KINDS:
        name, sv, exp = test(kind)
        out[f"{name}.sv"] = sv
        out[f"{name}.expected"] = exp
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
                print(f"gen_file_close_matrix: {path.relative_to(OUT.parent.parent.parent)} is "
                      "out of date; run tools/gen_file_close_matrix.py", file=sys.stderr)
                bad += 1
        else:
            path.write_text(text, encoding="utf-8")
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main())
