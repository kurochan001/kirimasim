#!/usr/bin/env python3
# Copyright (c) 2026 The kirimasim Authors
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

"""Generate ref-memory file operations (21.4, 21.5, 13.5.2).

Axes: slot/frame/subarray homes, forwarding through ref arguments, waiting
tasks, functions, declaration direction, dimensions, 2/4 states, width,
enumerations, both radixes, default/forward/reverse/partial ranges, and
const-ref writing. Expected values follow address and digit rules from
SYSTEM_TASKS.md 7, independently of the implementation.
"""

import argparse
import sys
from pathlib import Path

OUT = Path(__file__).resolve().parent.parent / "tests" / "suite" / "proc"
VERSION = 1
HOMES = ("slot", "frame", "subarray", "frame_subarray", "forward_subarray",
         "frame_forward_subarray", "function", "waiting", "interface")
SHAPES = (("bit5", "bit [4:0]", 5, False, False),
          ("logic8", "logic [7:0]", 8, True, False),
          ("logic33_down", "logic [32:0]", 33, True, False),
          ("bit65", "bit [64:0]", 65, False, False),
          ("logic129", "logic [128:0]", 129, True, False),
          ("logic65_multi", "logic [64:0]", 65, True, True),
          ("enum8", "enum logic [7:0] { A=8'h12, B=8'h34, C=8'h56 }", 8, True, False))


def digits(bits, radix):
    if radix == "b":
        return bits
    out = []
    while bits:
        part, bits = bits[-4:], bits[:-4]
        if set(part) == {"x"}:
            out.append("x")
        elif set(part) == {"z"}:
            out.append("z")
        elif "x" in part:
            out.append("X")
        elif "z" in part:
            out.append("Z")
        else:
            out.append(format(int(part, 2), "x"))
    return "".join(reversed(out))


def test(home, shape):
    tag, typename, width, four, multi = shape
    name = f"ref_memfile_{tag}_{home}"
    down = tag.endswith("down")
    dims = "[4:2]" if down else "[2:4]"
    dims += "[1:0]" if multi else ""
    indices = [(i, j) for i in range(2, 5) for j in range(2)] if multi else [(i,) for i in range(2, 5)]
    values = []
    for i in range(len(indices)):
        hexdigits = ("12345xz9" * ((width + 31) // 32 + 1))[-((width + 3) // 4):]
        if tag == "enum8":
            hexdigits = ("12", "34", "56")[i % 3]
        elif i % 3 == 1:
            hexdigits = "z" * len(hexdigits)
        elif i % 3 == 2:
            hexdigits = "x" * len(hexdigits)
        bits = "".join(c * 4 if c in "xz" else format(int(c, 16), "04b") for c in hexdigits)[-width:]
        values.append((hexdigits, bits))
    initial = format(0x12 if tag == "enum8" else 0, f"0{width}b")
    state = dict.fromkeys(indices, initial)
    expected, body = [], []
    refs = lambda mem: ", ".join(mem + "".join(f"[{k}]" for k in index) for index in indices)
    for radix in ("h", "b"):
        for range_name, extra, rows in (("whole", "", [2, 3, 4]),
                                        ("reverse", ", 4, 2", [4, 3, 2]),
                                        ("partial", ", 3, 3", [3])):
            targets = [index for row in rows for index in indices if index[0] == row]
            data = " ".join((v[0] if radix == "h" else v[1]) for v in values[:len(targets)])
            body += [f'    fd = $fopen("in.mem", "w");',
                     f'    $fwrite(fd, "%s", "{data}\\n");', "    $fclose(fd);",
                     f'    $readmem{radix}("in.mem", mem{extra});',
                     f'    save_{radix}(mem{extra});',
                     f'    $readmem{radix}("out.mem", copy{extra});',
                     f'    $display("{radix} {range_name} mem: ' + " ".join("%h" for _ in indices)
                     + '", ' + refs("mem") + ");",
                     f'    $display("{radix} {range_name} copy: ' + " ".join("%h" for _ in indices)
                     + '", ' + refs("copy") + ");"]
            for index, (_, bits) in zip(targets, values):
                state[index] = bits if four else bits.replace("x", "0").replace("z", "0")
            want = " ".join(digits(state[index], "h") for index in indices)
            expected += [f"{radix} {range_name} mem: {want}", f"{radix} {range_name} copy: {want}"]
    # Passing a default range into a nested const-ref task exercises reading
    # through two levels of references, including a reference to a frame.
    declarations = [f"  typedef {typename} elem_t;", f"  elem_t storage {dims}, copy {dims};",
                    "  int fd;"]
    helpers = []
    for radix in ("h", "b"):
        helpers += [f"  function automatic void save_{radix}(const ref elem_t mem {dims}, input int start=2, finish=4);",
                    f'    $writemem{radix}("out.mem", mem, start, finish);', "  endfunction"]
    opening = "function automatic void run" if home == "function" else "task automatic run"
    helpers += [f"  {opening}(ref elem_t mem {dims});"]
    if home == "waiting":
        helpers += ["    #1;"]
    helpers += body + ["  endfunction" if home == "function" else "  endtask"]
    helpers += [f"  task automatic forward(ref elem_t mem {dims});", "    run(mem);", "  endtask"]
    helpers += [f"  task automatic select_row(ref elem_t full [0:1]{dims});",
                "    forward(full[1]);", "  endtask"]
    execution = []
    if home == "frame":
        execution += ["  task automatic invoke;", f"    elem_t local_mem {dims};",
                      "    foreach (local_mem[i]) begin"]
        execution += (["      foreach (local_mem[i][j]) local_mem[i][j] = elem_t'(0);"] if multi else
                      ["      local_mem[i] = elem_t'(18);" if tag == "enum8" else "      local_mem[i] = '0;"])
        execution += ["    end", "    forward(local_mem);", "  endtask", "  initial invoke();"]
    elif "subarray" in home:
        automatic = home.startswith("frame_")
        execution += (["  task automatic invoke;"] if automatic else [])
        execution += [f"  elem_t outer [0:1]{dims};"]
        if not automatic:
            execution += ["  initial begin"]
        execution += ["    foreach (outer[i,j]) begin"]
        execution += (["      foreach (outer[i][j][k]) outer[i][j][k] = '0;"] if multi else
                      ["      outer[i][j] = elem_t'(18);" if tag == "enum8" else "      outer[i][j] = '0;"])
        execution += ["    end", "    select_row(outer);" if "forward" in home else "    forward(outer[1]);"]
        # An adjacent row must keep its value: the reference's offset and
        # element stride must not overwrite the containing array's start.
        neighbor = ["outer[0]" + "".join(f"[{k}]" for k in index) for index in indices]
        execution += ['    $display("neighbor: ' + " ".join("%h" for _ in indices) + '", '
                      + ", ".join(neighbor) + ");"]
        expected += ["neighbor: " + " ".join(digits(initial, "h") for _ in indices)]
        execution += (["  endtask", "  initial invoke();"] if automatic else ["  end"])
    else:
        execution += ["  initial begin"]
        execution += (["    foreach (storage[i,j]) storage[i][j] = '0;"] if multi else
                      ["    foreach (storage[i]) storage[i] = elem_t'(18);" if tag == "enum8" else
                       "    foreach (storage[i]) storage[i] = '0;"])
        execution += ["    forward(storage);", "  end"]
    # Every copy location is written before it is printed, including the
    # first full-range read, so there is no initialization-order dependence.
    lines = ["/*", f":name: {name}", ":description: Memory files through ref and const-ref arguments",
             ":tags: proc", ":type: simulation", ":lrm: 1800-2023 21.4 21.5 13.5.2",
             ":oracle: spec", ":spec_check: ai", ":fourstate: yes", ":nondeterministic: no",
             f":generator: gen_ref_memfile_matrix.py@{VERSION}", ":top_module: top",
             f":expected_stdout: @{name}.expected", f":expected_stderr: @{name}.stderr", "*/",
             "// Generated by tools/gen_ref_memfile_matrix.py; edit the generator.",
             "`timescale 1ns/1ns", "interface worker;" if home == "interface" else "module top;"]
    lines += declarations + helpers + execution
    lines += (["endinterface", "module top; worker u(); endmodule"] if home == "interface" else ["endmodule"])
    return {name + ".sv": "\n".join(lines) + "\n", name + ".expected": "\n".join(expected) + "\n",
            name + ".stderr": ""}


def main():
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--check", action="store_true")
    args = parser.parse_args()
    bad = False
    for home in HOMES:
        for shape in SHAPES:
            for name, text in test(home, shape).items():
                path = OUT / name
                if args.check:
                    if not path.exists() or path.read_text(encoding="utf-8") != text:
                        print(f"{name}: run tools/gen_ref_memfile_matrix.py", file=sys.stderr)
                        bad = True
                else:
                    path.write_text(text, encoding="utf-8")
    return int(bad)


if __name__ == "__main__":
    sys.exit(main())
