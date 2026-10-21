#!/usr/bin/env python3
# Copyright (c) 2026 The kirimasim Authors
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

"""Generates the matrix tests of the VCD dump (WAVEFORM.md; 21.7.1,
21.7.2; TEST_SUITE.md, "Mechanism matrix") in tests/suite/proc/.

  tools/gen_dump_matrix.py           # writes the files
  tools/gen_dump_matrix.py --check   # exit 1 if a file differs from what it would write

Each design dumps to d.vcd, then reads the file back after $dumpflush and
prints it, leaving out the line in $date, which changes with every run,
and the line "kirimasim <version>" in $version, which changes with every
release (WAVEFORM.md 6). The expected outputs are made here from the rules
of WAVEFORM.md, without reading kirimasim's code:

- names: what $dumpvars chooses (21.7.1.2, WAVEFORM.md 4), by how its
  arguments are spelled: none, levels alone, levels and the top, a scope
  below, a name relative to the caller, scopes and variables, an unpacked
  structure, an unpacked array (not dumped, 21.7.5), two calls in one time
  slot. Each spelling is called from an initial procedure; three of them
  from a task, a function, a fork branch and a task in a generate block
  too, which change nothing but the generate block, a scope of its own.
- types: the VCD type, size and range of each kind of variable and net,
  and the shortest form of their values (WAVEFORM.md 2, 3).
- control: when $dumpvars, $dumpoff, $dumpon, $dumpall and $dumpfile take
  effect, and the warnings of those called too late (WAVEFORM.md 4).
- limit: $dumplimit.
- wave, wave_scope, wave_glitches: --wave, --wave-scope and
  --wave-glitches (WAVEFORM.md 5).
"""

import argparse
import struct
import sys
from pathlib import Path

OUT = Path(__file__).resolve().parent.parent / "tests" / "suite" / "proc"
# The version of the rules below (TEST_SUITE.md, :generator:). Raise it
# whenever they change.
VERSION = 1

CONTAINERS = ["initial", "task", "function", "fork", "generate"]

# The task that reads the dump back. Its variables are automatic, so they
# are not dumped (21.7.5).
SHOW = """  task automatic show_vcd;
    int fd;
    string line, last;
    $dumpflush;
    fd = $fopen("d.vcd", "r");
    while ($fgets(line, fd) != 0) begin
      if (last != "$date\\n" && line.substr(0, 9) != "\\tkirimasim") $write("%s", line);
      last = line;
    end
    $fclose(fd);
  endtask"""


# --- The model of a dump ----------------------------------------------------


class Var:
    """A variable: its VCD type, size and range, and its value at each time
    step as the VCD writes it without the code (None: no change), or, for
    an event, whether it was triggered."""

    def __init__(self, name, vtype, size, rng, values, net=None):
        self.name, self.vtype, self.size, self.rng = name, vtype, size, rng
        self.values = values
        self.net = net  # names of one net share it, and its code


class Scope:
    def __init__(self, kind, name, variables=(), subs=()):
        self.kind, self.name = kind, name
        self.vars = list(variables)
        self.subs = list(subs)


def rows(top):
    """The scopes depth first: (scope, path, parent row)."""
    out = []

    def walk(s, path, parent):
        out.append((s, path, parent))
        me = len(out) - 1
        for c in s.subs:
            walk(c, f"{path}.{c.name}", me)

    walk(top, top.name, None)
    return out


def choose(top, requests, patterns=None):
    """The scopes and variables chosen (WAVEFORM.md 4, 5): each request is
    (levels, scope paths, variable paths), scope paths None for every top
    module."""
    table = rows(top)
    scopes = set()
    variables = set()
    for levels, names, var_names in requests:
        roots = [0] if names is None else [i for i, r in enumerate(table) if r[1] in names]
        for root in roots:
            for i, (s, _, _) in enumerate(table):
                # The level of row i below the root: 1, and one more for
                # each module instance on the way down.
                level, at, inside = 1, i, False
                while at is not None:
                    if at == root:
                        inside = True
                        break
                    if table[at][0].kind == "module":
                        level += 1
                    at = table[at][2]
                if inside and (levels == 0 or level <= levels):
                    scopes.add(i)
        for i, (s, path, _) in enumerate(table):
            for v in s.vars:
                if f"{path}.{v.name}" in var_names:
                    variables.add((i, v.name))
    for i in scopes:
        for v in table[i][0].vars:
            variables.add((i, v.name))
    if patterns is not None:
        keep = set()
        for i, (s, path, parent) in enumerate(table):
            if parent in keep or any(glob(p, path) for p in patterns):
                keep.add(i)
        scopes &= keep
        variables = {(i, n) for i, n in variables if i in keep}
    return scopes, variables


def glob(pattern, text):
    if not pattern:
        return not text
    if pattern[0] == "*":
        return any(glob(pattern[1:], text[k:]) for k in range(len(text) + 1))
    return bool(text) and pattern[0] in ("?", text[0]) and glob(pattern[1:], text[1:])


def code(n):
    """The n-th code: base 94 with the digits ! to ~ (WAVEFORM.md 2)."""
    digits = ""
    while True:
        digits = chr(33 + n % 94) + digits
        n //= 94
        if n == 0:
            return digits


def value_line(v, text, c):
    if v.vtype in ("real", "realtime"):
        return f"{text} {c}"
    if v.size == 1:
        return f"{text}{c}"
    # The shortest form (Tables 21-8, 21-9).
    k = 0
    while k + 1 < len(text) and ((text[k] == "0" and text[k + 1] in "01") or
                                 (text[k] in "xz" and text[k + 1] == text[k])):
        k += 1
    return f"b{text[k:]} {c}"


def unknown(v):
    return "x" * v.size if v.vtype not in ("real", "realtime") else None


class Vcd:
    """Writes the expected lines of a dump."""

    def __init__(self, top, chosen, call):
        scopes, variables = chosen
        table = rows(top)
        needed = set(scopes) | {i for i, _ in variables}
        for i in reversed(range(len(table))):
            if i in needed and table[i][2] is not None:
                needed.add(table[i][2])
        self.lines = ["$date", "$end", "$version", f"\t{call}", "$end", "$timescale", "\t1 ns",
                      "$end"]
        self.codes = []  # (var, code), in the order of the codes
        by_net = {}
        open_rows = []
        for i, (s, _, parent) in enumerate(table):
            if i not in needed:
                continue
            while open_rows and open_rows[-1] != parent:
                self.lines.append("$upscope $end")
                open_rows.pop()
            self.lines.append(f"$scope {s.kind} {s.name} $end")
            open_rows.append(i)
            for v in s.vars:
                if (i, v.name) not in variables:
                    continue
                key = v.net or (i, v.name)
                if key not in by_net:
                    by_net[key] = code(len(by_net))
                    self.codes.append((v, by_net[key]))
                rng = f" {v.rng}" if v.rng else ""
                self.lines.append(f"$var {v.vtype} {v.size} {by_net[key]} {v.name}{rng} $end")
        self.lines += ["$upscope $end"] * len(open_rows)
        self.lines.append("$enddefinitions $end")
        self.last = {}
        self.stamped = None

    def stamp(self, t):
        if self.stamped != t:
            self.lines.append(f"#{t}")
            self.stamped = t

    def now(self, v, step):
        for k in range(min(step, len(v.values) - 1), -1, -1):
            if v.values[k] is not None:
                return v.values[k]
        return None

    def section(self, name, t, step, off=False):
        self.stamp(t)
        self.lines.append(name)
        for v, c in self.codes:
            if v.vtype == "event":
                continue
            if off:
                if unknown(v) is not None:
                    self.lines.append(value_line(v, unknown(v), c))
                continue
            self.last[c] = self.now(v, step)
            self.lines.append(value_line(v, self.last[c], c))
        self.lines.append("$end")

    def changes(self, t, step):
        for v, c in self.codes:
            if v.vtype == "event":
                if step < len(v.values) and v.values[step]:
                    self.stamp(t)
                    self.lines.append(f"1{c}")
                continue
            if self.now(v, step) != self.last[c]:
                self.stamp(t)
                self.last[c] = self.now(v, step)
                self.lines.append(value_line(v, self.last[c], c))


def header(name, description, lrm, args=None):
    lines = ["/*", f":name: {name}", f":description: {description}", ":tags: proc",
             ":type: simulation", f":lrm: 1800-2023 {lrm}", ":oracle: spec", ":spec_check: ai",
             ":fourstate: yes", ":nondeterministic: no", f":generator: gen_dump_matrix.py@{VERSION}",
             ":top_module: top"]
    if args:
        lines.append(f":args: {args}")
    lines += [f":expected_stdout: @{name}.expected", f":expected_stderr: @{name}.stderr", "*/",
              "// Generated by tools/gen_dump_matrix.py; change the generator, not this file.",
              "`timescale 1ns/1ns"]
    return lines


def warnings_at(name, lines, marks):
    """The warning lines of the calls marked with «» (removed here)."""
    out, text = [], []
    for i, line in enumerate("\n".join(lines).split("\n"), 1):
        while "«" in line:
            col = line.index("«")
            end = line.index("»")
            call = line[col + 1:end]
            task = call.split("(")[0].rstrip(";")
            out.append(f"{name}.sv:{i}:{col + 1}: warning: {task}: {marks[task]}")
            line = line[:col] + call + line[end + 1:]
        text.append(line)
    return text, out


# --- names: what $dumpvars chooses --------------------------------------------


def names_top(container):
    """The hierarchy of the names tests, with the values at times 0 and 1."""
    def i32(name, a, b):
        return Var(name, "integer", 32, "[31:0]", [format(a, "032b"), format(b, "032b")])

    subs = [Scope("fork", "s", [i32("x", 3, 4)]),
            Scope("module", "m", [i32("ma", 10, 11)], [Scope("module", "l", [i32("la", 20, 21)])]),
            Scope("begin", "g", [Var("tg", "reg", 1, "", ["0", "1"])])]
    if container == "generate":
        subs.append(Scope("begin", "gd"))
    subs.append(Scope("begin", "blk", [i32("tb", 5, 6)]))
    return Scope("module", "top", [i32("ta", 1, 2)], subs)


# Each cell: (name, the calls, the requests of the model).
NAMES = [
    ("all", ["$dumpvars;"], [(0, None, [])]),
    ("levels_0", ["$dumpvars(0);"], [(0, None, [])]),
    ("levels_1", ["$dumpvars(1);"], [(1, None, [])]),
    ("top_2", ["$dumpvars(2, top);"], [(2, ["top"], [])]),
    ("below", ["$dumpvars(0, top.m);"], [(0, ["top.m"], [])]),
    ("relative", ["$dumpvars(1, m);"], [(1, ["top.m"], [])]),
    ("variables", ["$dumpvars(1, top.m.l, ta, top.g.tg);"],
     [(1, ["top.m.l"], ["top.ta", "top.g.tg"])]),
    ("struct", ["$dumpvars(0, s);"], [(0, ["top.s"], [])]),
    ("array", ["$dumpvars(0, arr);"], [(0, [], [])]),
    ("added_up", ["$dumpvars(1, top.m);", "$dumpvars(0, top.blk.tb);"],
     [(1, ["top.m"], []), (0, [], ["top.blk.tb"])]),
]
NAMES_IN_EVERY_CONTAINER = ["all", "top_2", "variables"]


def names_test(cell, container):
    key, calls, requests = cell
    name = f"dump_names_{key}" + ("" if container == "initial" else f"_{container}")
    lines = header(name, f"what {' '.join(calls)} dumps (21.7.1.2), called from "
                         f"{'an' if container[0] in 'aeiou' else 'a'} {container}",
                   "21.7.1.1 21.7.1.2 21.7.2 21.7.5")
    body = ['$dumpfile("d.vcd");'] + calls
    lines += ["module leaf;", "  int la = 20;", "endmodule", "module mid;", "  int ma = 10;",
              "  leaf l();", "endmodule", "module top;", "  int ta = 1;",
              "  struct { int x; } s = '{x: 3};", "  int arr [0:1];", "  mid m();",
              "  if (1) begin : g", "    logic tg = 1'b0;", "  end"]
    call = {"initial": body, "fork": ["fork begin"] + ["  " + b for b in body] + ["end join"],
            "task": ["dump_calls;"], "function": ["dump_calls();"], "generate": ["gd.run;"]}
    if container == "task":
        lines += ["  task dump_calls;"] + ["    " + b for b in body] + ["  endtask"]
    elif container == "function":
        lines += ["  function void dump_calls();"] + ["    " + b for b in body] + ["  endfunction"]
    elif container == "generate":
        lines += ["  if (1) begin : gd", "    task run;"] + ["      " + b for b in body] + [
            "    endtask", "  end"]
    lines += ["  initial begin : blk", "    static int tb = 5;", "    #1 tb = 6;", "  end", SHOW,
              "  initial begin"]
    lines += ["    " + b for b in call[container]]
    lines += ["    #1 ta = 2; s.x = 4; m.ma = 11; m.l.la = 21; g.tg = 1'b1;", "    #1 show_vcd;",
              "  end", "endmodule"]
    top = names_top(container)
    vcd = Vcd(top, choose(top, requests), '$dumpfile("d.vcd")')
    vcd.section("$dumpvars", 0, 0)
    vcd.changes(1, 1)
    return name, lines, vcd.lines, []


# --- types: the type, size, range and value of each kind ----------------------


def bits(v, w):
    return format(v & ((1 << w) - 1), f"0{w}b")


def r16g(x):
    return "r" + "%.16g" % x


def f32(x):
    return struct.unpack("f", struct.pack("f", x))[0]


# Each: (name, declaration, VCD type, size, range, values at times 0, 1 and 2
# (None: no change; for an event, whether it is triggered), statements at
# times 1 and 2). The nets come with their continuous assignments.
TYPES = [
    ("a1", "logic a1;", "reg", 1, "", ["x", "z", "1"], ["a1 = 1'bz;", "a1 = 1'b1;"]),
    ("b1", "bit b1;", "reg", 1, "", ["0", "1", None], ["b1 = 1'b1;", "b1 = 1'b0; b1 = 1'b1;"]),
    ("l4", "logic [3:0] l4 = 4'b0010;", "reg", 4, "[3:0]", ["0010", "0x1z", "1111"],
     ["l4 = 4'b0x1z;", "l4 = 4'b1111;"]),
    ("up", "logic [0:7] up = 8'h0f;", "reg", 8, "[0:7]", ["00001111", "xxxx0000", "zzzzzzzz"],
     ["up = 8'bxxxx_0000;", "up = 8'bzzzz_zzzz;"]),
    ("wd", "bit [69:0] wd = 70'd1;", "reg", 70, "[69:0]", ["0" * 69 + "1", "1" + "0" * 68 + "1", None],
     ["wd = {1'b1, 68'd0, 1'b1};", ""]),
    ("x70", "logic [69:0] x70;", "reg", 70, "[69:0]", ["x" * 70, "01" + "z" * 68, "0" * 70],
     ["x70 = {2'b01, {68{1'bz}}};", "x70 = '0;"]),
    ("i", "int i = -2;", "integer", 32, "[31:0]", [bits(-2, 32), bits(7, 32), None],
     ["i = 7;", "i = 7;"]),
    ("ig", "integer ig;", "integer", 32, "[31:0]", ["x" * 32, bits(5, 32), "z" * 32],
     ["ig = 5;", "ig = 'z;"]),
    ("by", "byte by = -1;", "reg", 8, "[7:0]", [bits(-1, 8), bits(0, 8), None], ["by = 0;", ""]),
    ("sh", "shortint sh = 16'h8000;", "reg", 16, "[15:0]", [bits(0x8000, 16), None, bits(1, 16)],
     ["", "sh = 1;"]),
    ("lo", "longint lo = 1;", "reg", 64, "[63:0]", [bits(1, 64), bits(-1, 64), None], ["lo = -1;", ""]),
    ("tm", "time tm;", "time", 64, "[63:0]", ["x" * 64, bits(1, 64), bits(2, 64)],
     ["tm = $time;", "tm = $time;"]),
    ("r", "real r = 1.5;", "real", 64, "", [r16g(1.5), r16g(-0.1), r16g(1e20)],
     ["r = -0.1;", "r = 1.0e20;"]),
    ("rt", "realtime rt = 2.0;", "realtime", 64, "", [r16g(2.0), r16g(1e-5), None], ["rt = 1.0e-5;", ""]),
    ("sr", "shortreal sr = 0.5;", "real", 64, "", [r16g(0.5), r16g(f32(0.1)), None], ["sr = 0.1;", ""]),
    ("ev", "event ev;", "event", 1, "", [False, True, True], ["-> ev;", "-> ev; -> ev;"]),
    ("en", "enum {E0, E1, E2} en = E1;", "integer", 32, "[31:0]", [bits(1, 32), bits(2, 32), None],
     ["en = E2;", ""]),
    ("el", "enum logic [1:0] {L0, L3 = 2'b11} el = L0;", "reg", 2, "[1:0]", ["00", "11", None],
     ["el = L3;", ""]),
    ("eb", "enum byte {B5 = 8'd5} eb = B5;", "reg", 8, "[7:0]", [bits(5, 8), None, None], ["", ""]),
    ("ps", "struct packed { logic [1:0] p; bit q; } ps = '{2'b1x, 1'b0};", "reg", 3, "[2:0]",
     ["1x0", "1x1", None], ["ps.q = 1'b1;", ""]),
    ("pa", "bit [1:0][2:0] pa = 6'b101_010;", "reg", 6, "[5:0]", ["101010", None, None], ["", ""]),
    ("w", "wire [1:0] w; assign w = l4[1:0];", "wire", 2, "[1:0]", ["10", "1z", "11"], ["", ""]),
    ("tr", "tri tr; assign tr = b1;", "tri", 1, "", ["0", "1", None], ["", ""]),
    ("wa", "wand wa; assign wa = b1; assign wa = 1'b1;", "wand", 1, "", ["0", "1", None], ["", ""]),
    ("ta", "triand ta; assign ta = b1; assign ta = 1'b1;", "triand", 1, "", ["0", "1", None], ["", ""]),
    ("wo", "wor wo; assign wo = b1; assign wo = 1'b0;", "wor", 1, "", ["0", "1", None], ["", ""]),
    ("to", "trior to; assign to = b1; assign to = 1'b0;", "trior", 1, "", ["0", "1", None], ["", ""]),
    ("t0", "tri0 t0;", "tri0", 1, "", ["0", None, None], ["", ""]),
    ("t1", "tri1 t1;", "tri1", 1, "", ["1", None, None], ["", ""]),
    ("s0", "supply0 s0;", "supply0", 1, "", ["0", None, None], ["", ""]),
    ("s1", "supply1 s1;", "supply1", 1, "", ["1", None, None], ["", ""]),
    ("uw", "uwire uw; assign uw = b1;", "wire", 1, "", ["0", "1", None], ["", ""]),
]
# An unpacked structure: a fork scope of its members (21.7.5).
STRUCT = "struct { logic [1:0] m1; struct { bit m2; } n2; } us = '{2'b10, '{1'b1}};"
# Not dumped (21.7.5, WAVEFORM.md 2): an unpacked array, dynamic data,
# parameters, and a static variable of an unnamed block.
NOT_DUMPED = ["int arr [0:1];", 'string str = "a";', "parameter int P = 3;",
              "localparam int Q = 4;", "initial begin static int hidden = 9; hidden = 10; end"]


def types_test():
    name = "dump_types"
    lines = header(name, "the VCD type, size and range of each kind of variable and net, and the "
                         "shortest form of their values (21.7.2.2, 21.7.2.3, 21.7.5)",
                   "21.7.1.2 21.7.2.2 21.7.2.3 21.7.5")
    lines += ["module top;"] + [f"  {t[1]}" for t in TYPES] + [f"  {STRUCT}"]
    lines += [f"  {d}" for d in NOT_DUMPED]
    lines += [SHOW, "  initial begin", '    $dumpfile("d.vcd");', "    $dumpvars(0, top);"]
    for step in (0, 1):
        statements = " ".join(t[6][step] for t in TYPES if t[6][step])
        lines.append(f"    #1 {statements}" + (" us.n2.m2 = 1'b0;" if step == 0 else ""))
    lines += ["    #1 show_vcd;", "  end", "endmodule"]
    variables = [Var(n, vt, size, rng, values) for n, _, vt, size, rng, values, _ in TYPES]
    us = Scope("fork", "us", [Var("m1", "reg", 2, "[1:0]", ["10"])],
               [Scope("fork", "n2", [Var("m2", "reg", 1, "", ["1", "0"])])])
    top = Scope("module", "top", variables, [us])
    vcd = Vcd(top, choose(top, [(0, ["top"], [])]), '$dumpfile("d.vcd")')
    vcd.section("$dumpvars", 0, 0)
    vcd.changes(1, 1)
    vcd.changes(2, 2)
    return name, lines, vcd.lines, []


# --- control: when the tasks take effect ---------------------------------------

LATE = {"$dumpvars": "dumping started at an earlier time; the call is ignored",
        "$dumpfile": "dumping has started; the call is ignored"}


def control_test():
    name = "dump_control"
    lines = header(name, "$dumpvars, $dumpoff, $dumpon and $dumpall take effect at the end of "
                         "their time slot, after its changes; the tasks do nothing before "
                         "$dumpvars, and a later $dumpvars or $dumpfile is ignored with a warning",
                   "21.7.1.1 21.7.1.2 21.7.1.3 21.7.1.4 21.7.1.6")
    body = ["module top;", "  int v = 0;", "  real r = 1.0;", "  event e;",
            "  logic [1:0] w = 2'b01;", SHOW, "  initial begin",
            "    $dumpoff; $dumpon; $dumpall; $dumpflush;",
            '    $dumpfile("first.vcd");', '    $dumpfile("d.vcd");', "    $dumpvars;",
            "    v = 1;",
            "    #1 v = 2; -> e;",
            "    #1 $dumpoff; v = 3; w = 2'bz1;",
            "    #1 v = 4; r = 2.0; -> e;",
            "    #1 $dumpon; v = 5;",
            "    #1 $dumpall; v = 6;",
            '    #1 «$dumpvars(0, top)»; «$dumpfile("late.vcd")»; «$dumpvars»;',
            "    repeat (2) #1 «$dumpvars(1, top)»;",
            "    #1 $dumpoff; $dumpon;",
            "    #1 $dumpon; $dumpall;",
            "    #1 show_vcd;", "  end", "endmodule"]
    lines += body
    lines, warnings = warnings_at(name, lines, LATE)
    top = Scope("module", "top", [
        Var("v", "integer", 32, "[31:0]", [bits(v, 32) for v in (1, 2, 3, 4, 5, 6)]),
        Var("r", "real", 64, "", [r16g(1.0), None, None, r16g(2.0), None, None]),
        Var("e", "event", 1, "", [False, True, False, True, False, False]),
        Var("w", "reg", 2, "[1:0]", ["01", None, "z1", None, None, None])])
    vcd = Vcd(top, choose(top, [(0, None, [])]), '$dumpfile("d.vcd")')
    vcd.section("$dumpvars", 0, 0)
    vcd.changes(1, 1)
    vcd.changes(2, 2)
    vcd.section("$dumpoff", 2, 2, off=True)
    vcd.section("$dumpon", 4, 4)
    vcd.changes(5, 5)
    vcd.section("$dumpall", 5, 5)
    vcd.section("$dumpoff", 9, 5, off=True)
    vcd.section("$dumpon", 9, 5)
    vcd.section("$dumpall", 10, 5)
    return name, lines, vcd.lines, warnings


def limit_test():
    name = "dump_limit"
    lines = header(name, "$dumplimit stops dumping once the file reaches the size, with a comment",
                   "21.7.1.5")
    lines += ["module top;", "  int v = 0;", SHOW, "  initial begin", "    $dumplimit(10);",
              '    $dumpfile("d.vcd");', "    $dumpvars;", "    #1 v = 2;", "    #1 show_vcd;",
              "  end", "endmodule"]
    top = Scope("module", "top", [Var("v", "integer", 32, "[31:0]", [bits(0, 32), bits(2, 32)])])
    vcd = Vcd(top, choose(top, [(0, None, [])]), '$dumpfile("d.vcd")')
    vcd.section("$dumpvars", 0, 0)
    vcd.lines.append("$comment the dump limit of 10 bytes was reached $end")
    return name, lines, vcd.lines, []


# --- the command line ---------------------------------------------------------

COMMAND_LINE = {"$dumpvars": "--wave sets what is dumped; the call is ignored",
                "$dumpfile": "--wave sets what is dumped; the call is ignored"}


def wave_test():
    name = "dump_wave"
    lines = header(name, "--wave dumps every scope from time 0; the design's $dumpfile and "
                         "$dumpvars are ignored with a warning, its $dumpoff and $dumpon act; "
                         "a port that makes two nets one gives them one code",
                   "21.7.1.1 21.7.1.2 21.7.1.3 21.7.2.3", "--wave=d.vcd")
    lines += ["module sub(input wire p);", "  int sv = 7;", "endmodule", "module top;",
              "  int v = 0;", "  wire n;", "  assign n = v[0];", "  sub u(.p(n));", SHOW,
              "  initial begin",
              '    «$dumpfile("other.vcd")»;', "    «$dumpvars(1, top)»;", "    #1 v = 1;",
              "    #1 $dumpoff;", "    #1 $dumpon;", "    #1 show_vcd;", "  end", "endmodule"]
    lines, warnings = warnings_at(name, lines, COMMAND_LINE)
    n = ["0", "1"]
    top = Scope("module", "top", [Var("v", "integer", 32, "[31:0]", [bits(0, 32), bits(1, 32)]),
                                  Var("n", "wire", 1, "", n, net="n")],
                [Scope("module", "u", [Var("p", "wire", 1, "", n, net="n"),
                                       Var("sv", "integer", 32, "[31:0]", [bits(7, 32)])])])
    vcd = Vcd(top, choose(top, [(0, None, [])]), "--wave=d.vcd")
    vcd.section("$dumpvars", 0, 0)
    vcd.changes(1, 1)
    vcd.section("$dumpoff", 2, 1, off=True)
    vcd.section("$dumpon", 3, 1)
    return name, lines, vcd.lines, warnings


def wave_scope_test():
    name = "dump_wave_scope"
    patterns = ["top.a", "*g?2"]
    lines = header(name, "--wave-scope keeps the scopes whose name or an enclosing scope's "
                         "matches a pattern", "21.7.1.2",
                   f"--wave=d.vcd --wave-scope={','.join(patterns)}")
    lines += ["module sub;", "  int sv = 7;", "endmodule", "module top;", "  int v = 0;",
              "  sub a();", "  sub b();", "  if (1) begin : gx1", "    logic q = 1'b0;", "  end",
              "  if (1) begin : gx2", "    logic q = 1'b1;", "    sub c();", "  end", SHOW,
              "  initial begin", "    #1 v = 1; a.sv = 8; b.sv = 9; gx2.q = 1'b0;",
              "    #1 show_vcd;", "  end", "endmodule"]

    def sub(n, a, b):
        return Scope("module", n, [Var("sv", "integer", 32, "[31:0]", [bits(a, 32), bits(b, 32)])])

    top = Scope("module", "top", [Var("v", "integer", 32, "[31:0]", [bits(0, 32), bits(1, 32)])],
                [sub("a", 7, 8), sub("b", 7, 9),
                 Scope("begin", "gx1", [Var("q", "reg", 1, "", ["0", None])]),
                 Scope("begin", "gx2", [Var("q", "reg", 1, "", ["1", "0"])], [sub("c", 7, 7)])])
    vcd = Vcd(top, choose(top, [(0, None, [])], patterns), "--wave=d.vcd")
    vcd.section("$dumpvars", 0, 0)
    vcd.changes(1, 1)
    return name, lines, vcd.lines, []


def wave_glitches_test():
    name = "dump_wave_glitches"
    lines = header(name, "--wave-glitches writes every change within a time slot as it happens",
                   "21.7.2.1", "--wave=d.vcd --wave-glitches")
    lines += ["module top;", "  int v = 0;", "  event e;", SHOW, "  initial begin",
              "    #1 v = 1; v = 2; v = 1; -> e; -> e;", "    #1 v = 1;", "    #1 show_vcd;",
              "  end", "endmodule"]
    top = Scope("module", "top", [Var("v", "integer", 32, "[31:0]", [bits(0, 32)]),
                                  Var("e", "event", 1, "", [False])])
    vcd = Vcd(top, choose(top, [(0, None, [])]), "--wave=d.vcd")
    vcd.section("$dumpvars", 0, 0)
    vcd.lines += ["#1", "b1 !", "b10 !", "b1 !", '1"', '1"']
    return name, lines, vcd.lines, []


def files():
    tests = [names_test(c, "initial") for c in NAMES]
    tests += [names_test(c, k) for k in CONTAINERS[1:] for c in NAMES
              if c[0] in NAMES_IN_EVERY_CONTAINER]
    tests += [types_test(), control_test(), limit_test(), wave_test(), wave_scope_test(),
              wave_glitches_test()]
    out = {}
    for name, lines, stdout, stderr in tests:
        out[f"{name}.sv"] = "\n".join(lines) + "\n"
        out[f"{name}.expected"] = "\n".join(stdout) + "\n"
        out[f"{name}.stderr"] = "".join(f"{e}\n" for e in stderr)
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
                print(f"gen_dump_matrix: {path.relative_to(OUT.parent.parent.parent)} is out "
                      "of date; run tools/gen_dump_matrix.py", file=sys.stderr)
                bad += 1
        else:
            path.write_text(text, encoding="utf-8")
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main())
