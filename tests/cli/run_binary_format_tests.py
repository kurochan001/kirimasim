#!/usr/bin/env python3
# Copyright (c) 2026 The kirimasim Authors
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

"""Generate and check binary output matrices (SYSTEM_TASKS.md 3.11).

Expected bytes come from independent Python integers, including every NUL.
The optional Icarus executable is only run; its source is never read.
"""

import argparse
import sys
import tempfile
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / "tools"))
import bounded_run  # noqa: E402

CONTAINERS = ("initial", "always", "final", "static_task", "automatic", "function",
              "ref", "fork", "generate", "interface")
APIS = ("write", "display", "fwrite", "fdisplay", "swrite", "sformat", "sformatf",
        "runtime_string", "runtime_packed", "packed_target", "strobe", "monitor",
        "fstrobe", "fmonitor")


def values():
    cases = []
    for width in (1, 7, 8, 31, 32, 33, 63, 64, 65, 127, 129):
        for four in (False, True):
            bits = ("10xz0011" if four else "10000001") * ((width + 7) // 8)
            bits = bits[-width:]
            a = int("".join("1" if c in "1x" else "0" for c in bits), 2)
            b = int("".join("1" if c in "xz" else "0" for c in bits), 2)
            cases.append((f"{width}'b{bits}", width, a, b))
    cases += [("signed_value", 5, 31, 0), ("enum_value", 8, 0x81, 0),
              ("record_value", 12, 0xa53, 0x0f0), ("mem[0]", 65, 1 << 64 | 0x41, 0),
              ("read_value()", 32, 0x00414200, 0)]
    return cases


def binary(width, a, b, conv):
    result = bytearray()
    for low in range(0, width, 32):
        if conv.lower() == "u":
            result += ((a & ~b) >> low & 0xffffffff).to_bytes(4, sys.byteorder)
        else:
            result += (a >> low & 0xffffffff).to_bytes(4, sys.byteorder)
            result += (b >> low & 0xffffffff).to_bytes(4, sys.byteorder)
    return bytes(result)


def source(container, api):
    cases = [(expr, width, a, b, conv) for expr, width, a, b in values()
             for conv in ("u", "z", "U", "Z")]
    fmt = "|" + "|".join("%" + c[4] for c in cases) + "|"
    expected = b"|" + b"|".join(binary(w, a, b, conv) for _, w, a, b, conv in cases) + b"|"
    args = ", ".join("v" if expr == "mem[0]" and container == "ref" else expr
                     for expr, *_ in cases)
    literal = '"' + fmt + '"'
    declarations = ["  typedef enum bit [7:0] { E = 8'h81 } enum_t;",
                    "  enum_t enum_value = E;", "  logic signed [4:0] signed_value = -1;",
                    "  struct packed { logic [3:0] a, b, c; } record_value = 12'b1010_zxzx_0011;",
                    "  logic [64:0] mem[1] = '{65'h10000000000000041};",
                    "  function automatic bit [31:0] read_value(); return 32'h00414200; endfunction",
                    "  string text, fmt;", f"  bit [{len(fmt) * 8 - 1}:0] packed_fmt;",
                    f"  bit [{len(expected) * 8 - 1}:0] packed_result;", "  int fd;"]
    body = []
    if api in ("fwrite", "fdisplay"):
        body += ['    fd = $fopen("result.bin", "wb");',
                 f"    ${api}(fd, {literal}, {args});", "    $fclose(fd);"]
    elif api in ("fstrobe", "fmonitor"):
        body += [f"    ${api}(32'h80000001, {literal}, {args});"]
    elif api in ("write", "display", "strobe", "monitor"):
        body += [f"    ${api}({literal}, {args});"]
    elif api == "packed_target":
        body += [f"    $sformat(packed_result, {literal}, {args});",
                 '    $write("%h", packed_result);']
        expected = expected.hex().encode()
    else:
        if api == "swrite":
            body += [f"    $swrite(text, {literal}, {args});"]
        elif api == "sformat":
            body += [f"    $sformat(text, {literal}, {args});"]
        elif api == "sformatf":
            body += [f"    text = $sformatf({literal}, {args});"]
        else:
            body += [f"    fmt = {literal}; packed_fmt = {literal};",
                     f"    text = $sformatf({'fmt' if api == 'runtime_string' else 'packed_fmt'}, {args});"]
        body += ['    $write("%s", text);']
        expected = expected.replace(b"\0", b"")  # 6.16: SV strings contain no NUL.
    if api in ("display", "fdisplay", "strobe", "monitor", "fstrobe", "fmonitor"):
        expected += b"\n"
    lines = ["interface worker;" if container == "interface" else "module top;"] + declarations
    if container in ("static_task", "automatic", "function", "ref"):
        opening = {"static_task": "task run();", "automatic": "task automatic run();",
                   "function": "function automatic void run();",
                   "ref": "task automatic run(ref logic [64:0] v);"}[container]
        lines += ["  " + opening] + body
        lines += ["  endfunction" if container == "function" else "  endtask",
                  "  initial run(mem[0]);" if container == "ref" else "  initial run();"]
    else:
        if container == "generate":
            lines += ["  if (1) begin : g"]
        if container == "always":
            lines += ["  event go;", "  initial #1 ->go;", "  always @(go) begin"]
        else:
            lines += ["  final begin" if container == "final" else "  initial begin"]
        lines += (["    fork", "      begin"] + body + ["      end", "    join"]
                  if container == "fork" else body)
        lines += ["  end"]
        if container == "generate":
            lines += ["  end"]
    lines += (["endinterface", "module top; worker u(); endmodule"]
              if container == "interface" else ["endmodule"])
    return "\n".join(lines) + "\n", expected, len(cases)


def main():
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--kirimasim", required=True)
    parser.add_argument("--iverilog")
    parser.add_argument("--vvp", default="vvp")
    parser.add_argument("--container", choices=CONTAINERS, action="append")
    parser.add_argument("--api", choices=APIS, action="append")
    args = parser.parse_args()
    simulator = str(Path(args.kirimasim).resolve())
    checks = conversions = 0
    with tempfile.TemporaryDirectory(prefix="kirimasim-binary-") as tmp:
        directory = Path(tmp)
        for container in args.container or CONTAINERS:
            for api in args.api or APIS:
                if api in ("strobe", "monitor", "fstrobe", "fmonitor") and container in ("ref", "final"):
                    continue  # Automatic references cannot outlive the call; final cannot schedule.
                text, expected, count = source(container, api)
                path = directory / f"{container}_{api}.sv"
                path.write_text(text, encoding="utf-8")
                result_file = directory / "result.bin"
                result_file.unlink(missing_ok=True)
                if args.iverilog:
                    compiled = bounded_run.run([args.iverilog, "-g2012", "-s", "top", "-o",
                                                str(directory / "test.vvp"), str(path)],
                                               cwd=tmp, text=False, timeout=30)
                    if compiled.returncode:
                        raise AssertionError(f"{path.name}: compile: {compiled.stderr.decode(errors='replace')}")
                    cmd = [args.vvp, str(directory / "test.vvp")]
                else:
                    cmd = [simulator, str(path)]
                ran = bounded_run.run(cmd, cwd=tmp, text=False, timeout=30)
                diagnostics = b"\n".join(line for line in ran.stderr.splitlines()
                                         if not line.startswith((b"kirimasim: info: kirimasim ",
                                                                 b"kirimasim: info: quiescent at ")))
                actual = result_file.read_bytes() if api in ("fwrite", "fdisplay") and result_file.exists() else ran.stdout
                if ran.returncode or diagnostics or actual != expected:
                    mismatch = next((i for i, (a, b) in enumerate(zip(actual, expected)) if a != b),
                                    min(len(actual), len(expected)))
                    raise AssertionError(f"{path.name}: exit={ran.returncode} stderr={ran.stderr!r}\n"
                                         f"expected {len(expected)} bytes, actual {len(actual)}; "
                                         f"first mismatch at {mismatch}: "
                                         f"{actual[mismatch:mismatch+16].hex()} != "
                                         f"{expected[mismatch:mismatch+16].hex()}")
                if api in ("fwrite", "fdisplay") and ran.stdout:
                    raise AssertionError(f"{path.name}: unexpected console output {ran.stdout!r}")
                checks += 1
                conversions += count
    print(f"binary formats: {checks} programs, {conversions} conversions checked")
    return 0


if __name__ == "__main__":
    sys.exit(main())
