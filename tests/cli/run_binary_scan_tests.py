#!/usr/bin/env python3
# Copyright (c) 2026 The kirimasim Authors
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

"""Check original binary-input matrices (SYSTEM_TASKS.md 6.3).

Independent Python integers predict values and byte consumption. Exercise
constant/runtime formats, packed/string/file inputs, typed targets, partial
and array targets, automatic/ref storage, and every execution container.
No expected input is made with the simulator's binary output formatter.
"""

import argparse
import sys
import tempfile
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / "tools"))
import bounded_run  # noqa: E402

CONTAINERS = ("initial", "always", "final", "static_task", "automatic", "function",
              "ref", "fork", "generate", "interface")
FORMATS = ("constant", "string", "packed", "function")
WIDTHS = (4, 31, 32, 33, 65, 129)


def decoded(data, width, field, conv, two=False):
    transfer = int(field or 0) or width
    needed = (transfer + 31) // 32 * (4 if conv.lower() == "u" else 8)
    if len(data) < needed:
        return "1" * width, -1, len(data), 1
    a = b = 0
    step = 4 if conv.lower() == "u" else 8
    for bit in range(0, transfer, 32):
        offset = bit // 32 * step
        a |= int.from_bytes(data[offset:offset + 4], sys.byteorder) << bit
        if step == 8:
            b |= int.from_bytes(data[offset + 4:offset + 8], sys.byteorder) << bit
    a &= (1 << transfer) - 1
    b &= (1 << transfer) - 1
    result = "".join("0" if two and b >> i & 1 else
                     "x" if b >> i & a >> i & 1 else
                     "z" if b >> i & 1 else str(a >> i & 1)
                     for i in reversed(range(width)))
    return result, 1, needed, 0


def rows():
    result = []
    payload = b"\x20\x09\x00\x81\xff\x0a\x00B" * 8
    for mode in ("file", "packed", "string"):
        data = payload if mode != "string" else payload.replace(b"\0", b"C")
        for width in WIDTHS:
            for field in ("", "0", "4", "65"):
                for conv in "uzUZ":
                    result.append((mode, width, field, conv, data, len(result) % 7))
    # Each byte of short fields, including empty input and complete-field EOF.
    for width in (4, 33, 65, 129):
        for conv in "uz":
            needed = (width + 31) // 32 * (4 if conv == "u" else 8)
            for length in range(needed + 1):
                result.append(("file", width, "", conv, bytes(range(0x41, 0x41 + length)), 0))
    return result


def design(container, kind, cases):
    types, declarations, formals, actuals = [], [], [], []
    for width in WIDTHS:
        types += [f"  typedef logic signed [{width-1}:0] logic{width}_t;",
                  f"  typedef bit [{width-1}:0] bit{width}_t;",
                  f"  typedef enum logic [{width-1}:0] {{ E{width}=0 }} enum{width}_t;",
                  f"  typedef struct packed {{ logic [{width-1}:0] data; }} struct{width}_t;"]
        for name, type_, suffix in [(f"v{width}", f"logic{width}_t", ""),
                                    (f"b{width}", f"bit{width}_t", ""),
                                    (f"e{width}", f"enum{width}_t", ""),
                                    (f"s{width}", f"struct{width}_t", ""),
                                    (f"a{width}", f"logic{width}_t", "[2]"),
                                    (f"p{width}", f"logic [{width+7}:0]", "")]:
            declarations.append(f"  {type_} {name}{suffix};")
            formals.append(f"ref {type_} {name}{suffix}")
            actuals.append(name)
    locals_ = ["    int n, fd, pos, eof, next_byte;", "    string fmt;", "    bit [127:0] packed_fmt;"]
    body, expected = [], []
    for i, (mode, width, field, conv, data, target_kind) in enumerate(cases):
        targets = [f"v{width}", f"b{width}", f"e{width}", f"s{width}", f"a{width}[0]",
                   f"p{width}[4 +: {width}]", f"{{v{width}[{width-1}:{width//2}],v{width}[{width//2-1}:0]}}"]
        target = targets[target_kind]
        value, count, position, eof = decoded(data, width, field, conv, target_kind == 1)
        display_target = f"v{width}" if target_kind == 6 else f"p{width}" if target_kind == 5 else target
        if target_kind == 5:
            value = "1111" + value + "1111"
        fmt = "%" + field + conv
        fmt_expr = {"constant": f'"{fmt}"', "string": "fmt", "packed": "packed_fmt",
                    "function": "format_value(fmt)"}[kind]
        if target_kind == 2:
            body.append(f"    e{width} = enum{width}_t'('1);")
        elif target_kind == 5:
            body.append(f"    p{width} = '1;")
        elif target_kind == 6:
            body.append(f"    v{width} = '1;")
        else:
            body.append(f"    {target} = '1;")
        if target_kind == 4:
            body.append(f"    a{width}[1] = '1;")
        body.append(f'    fmt = "{fmt}"; packed_fmt = "{fmt}";')
        if mode == "file":
            body += [f'    fd = $fopen("input_{i}.bin", "rb");',
                     f"    n = $fscanf(fd, {fmt_expr}, {target});",
                     "    pos=$ftell(fd); eof=$feof(fd); next_byte=$fgetc(fd); $fclose(fd);"]
            extra_fmt, extra_args = " %0d %0d %0d", ", pos, eof, next_byte"
            next_byte = data[position] if position < len(data) else -1
            extra = f" {position} {eof} {next_byte}"
        else:
            text = (f"{len(data)*8}'h{data.hex()}" if mode == "packed" else
                    '"' + "".join(f"\\{c:03o}" for c in data) + '"')
            body.append(f"    n = $sscanf({text}, {fmt_expr}, {target});")
            extra_fmt = extra_args = extra = ""
        neighbor_fmt = " %b" if target_kind == 4 else ""
        neighbor_args = f", a{width}[1]" if target_kind == 4 else ""
        neighbor = " " + "1" * width if target_kind == 4 else ""
        body.append(f'    $display("ROW {i} %0d %b{neighbor_fmt}{extra_fmt}", n, {display_target}{neighbor_args}{extra_args});')
        expected.append(f"ROW {i} {count} {value}{neighbor}{extra}")
    lines = ["interface worker;" if container == "interface" else "module top;"] + types
    lines += ["  function automatic string format_value(input string f); return f; endfunction"]
    if container == "ref":
        lines += declarations + ["  task automatic run(" + ", ".join(formals) + ");"] + locals_ + body
        lines += ["  endtask", "  initial run(" + ", ".join(actuals) + ");"]
    elif container in ("automatic", "static_task", "function"):
        opening = {"automatic": "task automatic run();", "static_task": "task run();",
                   "function": "function automatic void run();"}[container]
        lines += ["  " + opening] + declarations + locals_ + body
        lines += ["  endfunction" if container == "function" else "  endtask", "  initial run();"]
    else:
        lines += declarations
        if container == "generate":
            lines += ["  if (1) begin : g"]
        lines += (["  event go;", "  initial #1 ->go;", "  always @(go) begin"] if container == "always" else
                  ["  final begin" if container == "final" else "  initial begin"])
        lines += locals_
        lines += (["    fork", "      begin"] + body + ["      end", "    join"] if container == "fork" else body)
        lines += ["  end"]
        if container == "generate":
            lines += ["  end"]
    lines += (["endinterface", "module top; worker u(); endmodule"] if container == "interface" else ["endmodule"])
    return "\n".join(lines) + "\n", "\n".join(expected) + "\n"


def continuation_design(runtime):
    expression = "fmt" if runtime else None
    lines = ["module top;", "  int fd,n,a,trace; logic [64:0] values[2]; string fmt;",
             "  function automatic int choose(); trace++; return 0; endfunction", "  initial begin"]
    expected = []
    for suppressed in (False, True):
        fmt = "%*u%65z" if suppressed else "%d:%65z"
        expr = expression or '"' + fmt + '"'
        lines += [f"    fmt=\"{fmt}\"; values[0]=65'h123; values[1]=65'h456; trace=0;",
                  f'    fd=$fopen("prefix_{int(suppressed)}.bin","rb");',
                  f"    n=$fscanf(fd,{expr}," + ("" if suppressed else "a,") + "values[choose()]);",
                  '    $display("partial %0d %0d %0d %0d %h %h",n,trace,$ftell(fd),$feof(fd),values[0],values[1]);',
                  "    $fclose(fd);"]
        expected.append(f"partial {0 if suppressed else 1} 0 {11 if suppressed else 9} 1 00000000000000123 00000000000000456")
    # Every byte of a pushed-back word, including NUL and whitespace, is payload.
    word = bytes((0, 0x20, 0x0a, 0xff))
    lines += ['    fd=$fopen("pushback.bin","rb");']
    for byte in reversed(word):
        lines += [f"    n=$ungetc({byte},fd);"]
    expr = expression or '"%u"'
    lines += ['    fmt="%u";', f"    n=$fscanf(fd,{expr},a);",
              '    $display("back %0d %h %0d",n,a,$fgetc(fd));', '    $fclose(fd);']
    expected += [f"back 1 {int.from_bytes(word, sys.byteorder):08x} 81"]
    # A suppressed complete field counts as conversion for a following EOF.
    expr = expression or '"%*u%u"'
    lines += ['    fd=$fopen("suppression.bin","rb");', '    fmt="%*u%u"; a=77;',
              f"    n=$fscanf(fd,{expr},a);",
              '    $display("suppressed %0d %0d %0d %0d",n,a,$ftell(fd),$feof(fd));',
              '    $fclose(fd);', '  end', 'endmodule']
    expected += ["suppressed 0 77 4 1"]
    return "\n".join(lines) + "\n", "\n".join(expected) + "\n"


def rejection_design(api, conv, type_, runtime):
    declaration = {"integral": "int value=77;", "real": "real value=2.5;", "string": 'string value="seed";'}[type_]
    fmt = f"key=%{conv}" if api == "value$plusargs" else f"%{conv}"
    expression = "fmt" if runtime else '"' + fmt + '"'
    call = (f"$value$plusargs({expression},value)" if api == "value$plusargs" else
            f"$sscanf(32'h41424344,{expression},value)")
    return f"module top; {declaration} string fmt; int n; initial begin fmt=\"{fmt}\"; n={call}; end endmodule\n"


def main():
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--kirimasim", required=True, type=Path)
    parser.add_argument("--container", choices=CONTAINERS, action="append")
    parser.add_argument("--format", choices=FORMATS, action="append")
    args = parser.parse_args()
    cases = rows()
    programs = 0
    with tempfile.TemporaryDirectory(prefix="kirimasim-binary-scan-matrix-") as tmp:
        root = Path(tmp)
        for i, row in enumerate(cases):
            (root / f"input_{i}.bin").write_bytes(row[4])
        for container in args.container or CONTAINERS:
            for kind in args.format or FORMATS:
                text, expected = design(container, kind, cases)
                path = root / f"{container}_{kind}.sv"
                path.write_text(text)
                ran = bounded_run.run([str(args.kirimasim.resolve()), str(path)], cwd=root, timeout=60)
                diagnostics = "\n".join(line for line in ran.stderr.splitlines()
                                        if not line.startswith(("kirimasim: info: kirimasim ",
                                                                "kirimasim: info: quiescent at ")))
                if ran.returncode or diagnostics or ran.stdout != expected:
                    actual = ran.stdout.splitlines()
                    want = expected.splitlines()
                    mismatch = next((i for i, (a, b) in enumerate(zip(actual, want)) if a != b), min(len(actual), len(want)))
                    raise AssertionError(f"{path.name}: exit {ran.returncode}\n{ran.stderr}\n"
                                         f"row {mismatch}: {actual[mismatch:mismatch+1]} != {want[mismatch:mismatch+1]}")
                programs += 1
        (root / "prefix_0.bin").write_bytes(b"7:" + b"A" * 7)
        (root / "prefix_1.bin").write_bytes(b"ABCD" + b"A" * 7)
        (root / "pushback.bin").write_bytes(b"Q")
        (root / "suppression.bin").write_bytes(b"ABCD")
        for runtime in (False, True):
            text, expected = continuation_design(runtime)
            path = root / f"continuation_{runtime}.sv"
            path.write_text(text)
            ran = bounded_run.run([str(args.kirimasim.resolve()), str(path)], cwd=root, timeout=30)
            if ran.returncode or ran.stdout != expected:
                raise AssertionError(f"{path.name}: {ran.returncode}\n{ran.stderr}\n{ran.stdout} != {expected}")
        for conv in "uzUZ":
            for api in ("sscanf", "value$plusargs"):
                for type_ in (("real", "string", "integral") if api == "value$plusargs" else ("real", "string")):
                    for runtime in (False, True):
                        path = root / "invalid.sv"
                        path.write_text(rejection_design(api, conv, type_, runtime))
                        ran = bounded_run.run([str(args.kirimasim.resolve()), str(path), "+key=ABCD"],
                                              cwd=root, timeout=30)
                        if ran.returncode != 2 or "internal error" in ran.stderr or ran.stdout:
                            raise AssertionError(f"invalid {api}/{conv}/{type_}/{runtime}: {ran.returncode}\n{ran.stderr}")
    print(f"binary scans: {programs} programs, {programs * len(cases)} conversions checked; 2 continuation programs, 40 rejections")
    return 0


if __name__ == "__main__":
    sys.exit(main())
