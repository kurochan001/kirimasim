#!/usr/bin/env python3
# Copyright (c) 2026 The kirimasim Authors
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

"""Generates the $typename matrix test of TEST_SUITE.md ("Mechanism matrix")
in tests/suite/values/.

  tools/gen_typename_matrix.py           # writes the files
  tools/gen_typename_matrix.py --check   # exit 1 if a file differs from what it would write

$typename gives the resolved type as a string (20.6.1, SYSTEM_TASKS.md
8): typedefs resolve to the types they name, and a structure, union or
enumeration keeps the name of the typedef that declares it, prefixed with
its package (pkg::) or the hierarchical name of its scope (top.); default
signing is left out; anonymous enumerations, structures and unions are
named e$N, s$N and u$N in the order they appear in their namespace;
enumeration constants come with their sized values (32'sd0); "$" stands
for the name of an unpacked array, followed by its ranges; ranges are
unsized decimal, with one space between a keyword and the packed ranges
after it. The axes: the kind of type, how the argument names it (a
variable, a type, a typedef, a typedef of a typedef, an expression), and
where the type is declared (the module, a package, a named block, the
compilation unit).

The expected strings follow from the rules above. A person checks them
once for all the tests made from them (TEST_SUITE.md, "Sampling by people").
"""

import argparse
import sys
from pathlib import Path

OUT = Path(__file__).resolve().parent.parent / "tests" / "suite" / "values"
NAME = "typename_matrix"
# The version of the rules below (TEST_SUITE.md, :generator:). Raise it
# whenever they change.
VERSION = 2

# Declarations, in order, by where they are.
UNIT = ["typedef struct {byte b;} unit_t;"]
PACKAGE = ["typedef enum {RED, GREEN = 5} color_t;", "enum bit [1:0] {LO, HI} pk_anon;",
           "typedef logic [3:0] nib_t;"]
MODULE = [
    "typedef bit flag_t;", "typedef flag_t [2:0] trio_t;", "typedef trio_t trio2_t;",
    "typedef struct {flag_t lo, hi; byte n;} rec_t;", "typedef struct packed signed {logic [3:0] hi, lo;} sps_t;",
    "typedef union packed {logic [7:0] u8; byte s8;} un_t;",
    "typedef enum logic [1:0] {IDLE, RUN = 2'b10} state_t;",
    "bit b1;", "logic l1;", "reg r1;", "bit signed bs1;",
    "byte by;", "byte unsigned byu;", "shortint si;", "int i;", "int signed is;",
    "int unsigned iu;", "longint li;", "integer ig;", "integer unsigned igu;", "time tm;",
    "time signed tms;",
    "logic [7:0] l8;", "logic signed [7:0] ls8;", "bit [0:3] b03;", "logic [3:0][7:0] l2d;",
    "real r;", "shortreal sr;", "realtime rt;", "string s;", "chandle ch;", "event ev;",
    "trio_t t3;", "trio2_t t3b;", "rec_t recs[5];", "rec_t grid[3][2];", "sps_t sps;", "un_t un;",
    "state_t st;", "color_t col;", "nib_t nib;", "unit_t ut;",
    "enum {X1, X2} anon1;", "struct {int a; enum {Y1} e;} anon2;", "enum {Z1} anon3;",
    "union packed {bit [1:0] p; bit [1:0] q;} anon4;", "int ua[4];", "logic [1:0] ub[0:2][1];",
]
BLOCK = ["typedef struct {int k;} blk_t;", "blk_t bv;", "enum {B1} banon;"]

# (argument, expected string)
ROWS = [
    ("b1", "bit"), ("l1", "logic"), ("r1", "reg"), ("bs1", "bit signed"),
    ("by", "byte"), ("byu", "byte unsigned"), ("si", "shortint"), ("i", "int"), ("is", "int"),
    ("iu", "int unsigned"), ("li", "longint"), ("ig", "integer"), ("igu", "integer unsigned"),
    ("tm", "time"), ("tms", "time signed"),
    ("l8", "logic [7:0]"), ("ls8", "logic signed [7:0]"), ("b03", "bit [0:3]"),
    ("l2d", "logic [3:0][7:0]"),
    ("r", "real"), ("sr", "shortreal"), ("rt", "realtime"), ("s", "string"), ("ch", "chandle"),
    ("ev", "event"),
    # Typedefs resolve to what they name.
    ("t3", "bit [2:0]"), ("t3b", "bit [2:0]"), ("trio_t", "bit [2:0]"), ("flag_t", "bit"),
    ("nib", "logic [3:0]"),
    # Structures, unions and enumerations keep their typedef's name.
    ("recs", "struct{bit lo;bit hi;byte n;}top.rec_t$[0:4]"),
    ("grid", "struct{bit lo;bit hi;byte n;}top.rec_t$[0:2][0:1]"),
    ("rec_t", "struct{bit lo;bit hi;byte n;}top.rec_t"),
    ("sps", "struct packed signed{logic [3:0] hi;logic [3:0] lo;}top.sps_t"),
    ("un", "union packed{logic [7:0] u8;byte s8;}top.un_t"),
    ("st", "enum logic [1:0]{IDLE=2'd0,RUN=2'd2}top.state_t"),
    ("IDLE", "enum logic [1:0]{IDLE=2'd0,RUN=2'd2}top.state_t"),
    ("col", "enum{RED=32'sd0,GREEN=32'sd5}pk::color_t"),
    ("pk::color_t", "enum{RED=32'sd0,GREEN=32'sd5}pk::color_t"),
    ("pk::pk_anon", "enum bit [1:0]{LO=2'd0,HI=2'd1}pk::e$1"),
    ("ut", "struct{byte b;}$unit::unit_t"),
    # Anonymous ones, in the order they appear in the module.
    ("anon1", "enum{X1=32'sd0,X2=32'sd1}top.e$1"),
    ("anon2", "struct{int a;enum{Y1=32'sd0}top.e$2 e;}top.s$1"),
    ("anon3", "enum{Z1=32'sd0}top.e$3"),
    ("anon4", "union packed{bit [1:0] p;bit [1:0] q;}top.u$1"),
    # Unpacked arrays.
    ("ua", "int$[0:3]"), ("ub", "logic [1:0]$[0:2][0:0]"),
    # Expressions: their self-determined types.
    ("l8 + 1", "logic [31:0]"), ("l8 + 8'd1", "logic [7:0]"), ("{l8, b03}", "logic [11:0]"),
    ("l8 == 0", "logic"), ("b03 == 4'd1", "bit"), ("l8[3:0]", "logic [3:0]"), ("l8[2]", "logic"),
    ("ua[1]", "int"), ("r * 2", "real"), ("iu + iu", "int unsigned"),
    # Types written in the call.
    ("logic [7:0]", "logic [7:0]"), ("int unsigned", "int unsigned"),
    ("bit signed [3:0]", "bit signed [3:0]"),
]
# In a named block.
BLOCK_ROWS = [("bv", "struct{int k;}top.blk.blk_t"), ("banon", "enum{B1=32'sd0}top.blk.e$1")]


def files():
    sv = (f"/*\n:name: {NAME}\n:description: $typename resolves typedefs, leaves out default "
          "signing, names structures, unions and enumerations by their typedefs and scopes or "
          "as e$N, s$N and u$N, gives enumeration values and unpacked array ranges, for "
          "variables, types, typedefs and expressions declared in the module, a package, a "
          "named block and the compilation unit\n:tags: values\n:type: simulation\n"
          ":lrm: 1800-2023 20.6.1\n:oracle: spec\n:spec_check: ai\n:fourstate: no\n"
          ":nondeterministic: no\n"
          f":generator: gen_typename_matrix.py@{VERSION}\n"
          f":expected_stdout: @{NAME}.expected\n*/\n"
          "// Generated by tools/gen_typename_matrix.py; change the generator, not this file.\n"
          "// The space between a keyword and its packed ranges is kirimasim's reading of\n"
          "// 20.6.1 (SYSTEM_TASKS.md 8). Verilator 5.052 writes no space there, leaves out\n"
          "// unsigned and the enumeration values' signing, and names anonymous types\n"
          "// differently. Icarus Verilog 13.0 does not take $typename.\n")
    sv += "".join(f"{d}\n" for d in UNIT)
    sv += "package pk;\n" + "".join(f"  {d}\n" for d in PACKAGE) + "endpackage\n"
    sv += "module top;\n  import pk::*;\n" + "".join(f"  {d}\n" for d in MODULE)
    sv += "  initial begin\n"
    exp = []
    for arg, want in ROWS:
        sv += f"    $display(\"{arg}: %s\", $typename({arg}));\n"
        exp.append(f"{arg}: {want}")
    sv += "    begin : blk\n" + "".join(f"      {d}\n" for d in BLOCK)
    for arg, want in BLOCK_ROWS:
        sv += f"      $display(\"{arg}: %s\", $typename({arg}));\n"
        exp.append(f"{arg}: {want}")
    sv += "    end\n  end\nendmodule\n"
    return {f"{NAME}.sv": sv, f"{NAME}.expected": "\n".join(exp) + "\n"}


def main():
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("--check", action="store_true")
    args = ap.parse_args()
    bad = 0
    for name, text in files().items():
        path = OUT / name
        if args.check:
            if not path.exists() or path.read_text(encoding="utf-8") != text:
                print(f"gen_typename_matrix: {path.relative_to(OUT.parent.parent.parent)} is "
                      "out of date; run tools/gen_typename_matrix.py", file=sys.stderr)
                bad += 1
        else:
            path.write_text(text, encoding="utf-8")
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main())
