#!/usr/bin/env python3
# Copyright (c) 2026 The kirimasim Authors
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

"""Generate runtime sscanf/fscanf formats (21.3.4.3, SYSTEM_TASKS.md 6.3).

Axes: string, 256-bit packed format, function, array element and conditional;
initial/always/final, static/automatic task, function, ref, fork, generate,
interface; string/file input; radix, suppression, width, real and string
fields, a 129-bit output, partial conversion, EOF, extra outputs and %%.
Check unchanged outputs, partial-target neighbours, the next unread byte,
and single, left-to-right evaluation including copy-back's index.
The expected values below follow from the spec, independently of SIR.
"""

import argparse
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
OUT = ROOT / "tests" / "suite" / "proc"
VERSION = 1
KINDS = ("string", "packed", "function", "array", "conditional")
CONTAINERS = ("initial", "always", "final", "static_task", "automatic", "function", "ref",
              "fork", "generate", "interface")
HEX = "1z23456789abcdef0123456789abcdef01"
WIDE = "".join("zzzz" if c == "z" else format(int(c, 16), "04b") for c in HEX)[-129:]
# tag, format, input, output arguments, display format/arguments, result count,
# expected displayed values, next unread file byte
CASES = [
    ("radix", "%d %h %b", "42 ff 10xz!", "a, b, bits[5:2]", "%0d %0d %b", "a, b, bits",
     3, "42 255 1110xz11", 33),
    ("suppressed", "%*d,%3s/%f", "99,abc/1.5!", "s, r", "%s %0.1f", "s, r",
     2, "abc 1.5", 33),
    ("wide", "%h", HEX + "!", "wide", "%b", "wide", 1, WIDE, 33),
    ("partial", "%d %d", "7 nope!", "a, b", "%0d %0d", "a, b", 1, "7 -9", 110),
    ("packed_text", "%s", "hi!", "text_bits", "%h", "text_bits", 1, "00686921", -1),
    ("eof", "%d", "", "a", "%0d", "a", -1, "-9", -1),
    ("copyback", "%% %d", "% -12!", "items[choose()]", "%0d %0d", "items[0], items[1]",
     1, "77 -12", 33),
    ("extra", "%d", "11!", "a, s, r", "%0d %s %0.1f", "a, s, r", 1, "11 seed 2.5", 33),
    ("empty", "", "x", "a", "%0d", "a", 0, "-9", 120),
]


def quote(text):
    return '"' + text.replace('\\', '\\\\').replace('"', '\\"') + '"'


def test(kind, container):
    name = f"runtime_scan_{kind}_{container}"
    format_expr = {"string": "fmt", "packed": "packed_fmt", "function": "get_format(fmt)",
                   "array": "formats[0]", "conditional": '(select_format ? fmt : "%q")'}[kind]
    declarations = ["    int a, b, items[2];", "    byte_bits_t bits;", "    wide_t wide;",
                    "    text_t text_bits;", "    string s;", "    real r;"]
    locals_ = ["    int n, fd, next_byte;", "    string fmt, formats[1];",
               "    logic [255:0] packed_fmt;", "    bit select_format;"]
    body, expected = [], []
    for mode in ("string", "file"):
        for tag, fmt, data, outputs, display, args, n, values, next_byte in CASES:
            body += ['    a = -9; b = -9; s = "seed"; r = 2.5;',
                     "    items[0] = 77; items[1] = -9; bits = 8'b11000011;",
                     "    wide = '0; text_bits = '0;",
                     f"    fmt = {quote(fmt)}; packed_fmt = {quote(fmt)}; formats[0] = fmt; select_format = 1;"]
            if mode == "file":
                body += ['    fd = $fopen("scan.txt", "w");',
                         f'    $fwrite(fd, "%s", {quote(data)});', "    $fclose(fd);",
                         '    fd = $fopen("scan.txt", "r");']
            body += ["    trace = 0;"]
            source = f"input_text({quote(data)})" if mode == "string" else "input_fd(fd)"
            call = "$sscanf" if mode == "string" else "$fscanf"
            body += [f"    n = {call}({source}, {format_expr}, {outputs});"]
            if mode == "file":
                body += ["    next_byte = $fgetc(fd);", "    $fclose(fd);"]
            trace = "12" if kind == "function" else "1"
            if tag == "copyback":
                trace += "3"
            body += [f'    $display("{mode} {tag} %0d {display} %0d'
                     + (' %0d' if mode == "file" else '') + f'", n, {args}, trace'
                     + (", next_byte" if mode == "file" else "") + ");"]
            expected += [f"{mode} {tag} {n} {values} {trace}"
                         + (f" {next_byte}" if mode == "file" else "")]
        if kind == "packed":
            # Unknown format bits give -1, and a file's position stays put.
            body += ["    packed_fmt = 'x; a = -9; trace = 0;"]
            if mode == "file":
                body += ['    fd = $fopen("scan.txt", "w");', '    $fwrite(fd, "42!");',
                         "    $fclose(fd);", '    fd = $fopen("scan.txt", "r");',
                         "    n = $fscanf(input_fd(fd), packed_fmt, a);",
                         "    next_byte = $ftell(fd);", "    $fclose(fd);"]
            else:
                body += ['    n = $sscanf(input_text("42!"), packed_fmt, a);']
            body += [f'    $display("{mode} unknown %0d %0d %0d'
                     + (' %0d' if mode == "file" else '') + '", n, a, trace'
                     + (", next_byte" if mode == "file" else "") + ");"]
            expected += [f"{mode} unknown -1 -9 1" + (" 0" if mode == "file" else "")]
    lines = ["/*", f":name: {name}",
             f":description: Runtime scan formats from {kind} in {container}, string and file input",
             ":tags: proc scan", ":type: simulation", ":lrm: 1800-2023 21.3.4.3",
             ":oracle: spec", ":spec_check: ai", ":fourstate: yes", ":nondeterministic: no",
             f":generator: gen_runtime_scan_matrix.py@{VERSION}", ":top_module: top",
             f":expected_stdout: @{name}.expected", f":expected_stderr: @{name}.stderr", "*/",
             "// Generated by tools/gen_runtime_scan_matrix.py; edit the generator.",
             "`timescale 1ns/1ns", "interface worker;" if container == "interface" else "module top;",
             "  typedef logic [7:0] byte_bits_t;", "  typedef logic [128:0] wide_t;",
             "  typedef logic [31:0] text_t;", "  int trace;",
             "  function automatic string input_text(input string data);",
             "    trace = trace * 10 + 1; return data;", "  endfunction",
             "  function automatic int input_fd(input int fd);",
             "    trace = trace * 10 + 1; return fd;", "  endfunction",
             "  function automatic string get_format(input string f);",
             "    trace = trace * 10 + 2; return f;", "  endfunction",
             "  function automatic int choose();",
             "    trace = trace * 10 + 3; return 1;", "  endfunction"]
    if container == "ref":
        lines += declarations + ["  task automatic run(ref int a, b, ref int items[2],",
                                 "      ref byte_bits_t bits, ref wide_t wide, ref text_t text_bits,",
                                 "      ref string s, ref real r);"]
        lines += locals_ + body + ["  endtask", "  initial run(a, b, items, bits, wide, text_bits, s, r);"]
    elif container in ("automatic", "static_task", "function"):
        opening = {"automatic": "task automatic run();", "static_task": "task run();",
                   "function": "function automatic void run();"}[container]
        lines += ["  " + opening] + declarations + locals_ + body
        lines += ["  endfunction" if container == "function" else "  endtask", "  initial run();"]
    else:
        lines += declarations
        if container == "generate":
            lines += ["  if (1) begin : g"]
        if container == "always":
            lines += ["  event go;", "  initial #1 ->go;", "  always @(go) begin"]
        else:
            lines += ["  final begin" if container == "final" else "  initial begin"]
        lines += locals_
        if container == "fork":
            lines += ["    fork", "      begin"] + body + ["      end", "    join"]
        else:
            lines += body
        lines += ["  end"]
        if container == "generate":
            lines += ["  end"]
    lines += (["endinterface", "module top;", "  worker u();", "endmodule"]
              if container == "interface" else ["endmodule"])
    return {name + ".sv": "\n".join(lines) + "\n",
            name + ".expected": "\n".join(expected) + "\n", name + ".stderr": ""}


def files():
    return {name: text for kind in KINDS for container in CONTAINERS
            for name, text in test(kind, container).items()}


def main():
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--check", action="store_true")
    args = parser.parse_args()
    bad = False
    for name, text in files().items():
        path = OUT / name
        if args.check:
            if not path.exists() or path.read_text(encoding="utf-8") != text:
                print(f"{name}: run tools/gen_runtime_scan_matrix.py", file=sys.stderr)
                bad = True
        else:
            path.write_text(text, encoding="utf-8")
    return int(bad)


if __name__ == "__main__":
    sys.exit(main())
