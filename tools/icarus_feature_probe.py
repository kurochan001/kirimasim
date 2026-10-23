#!/usr/bin/env python3
# Copyright (c) 2026 The kirimasim Authors
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

"""Run original feature probes with both simulators, without reading their sources.

This is a finite gap inventory, not a conformance score or a parity claim.
Expectations come from each probe's written behavior and the cited clauses.
Newly passing probes do not modify existing test-suite expectations.
"""

import argparse
import json
import re
import subprocess
import sys
import tempfile
from pathlib import Path

import bounded_run

# name, clause, source, expected output, extra Icarus compile flags
PROBES = [
    ("wide_integral", "11.4", "module top; logic [128:0] a = 129'h10000000000000000; initial $display(\"%0d\", a+1'b1); endmodule", "18446744073709551617\n", []),
    ("fixed_arrays", "7.4.1", "module top; int a[2:4]='{12,34,56}; initial $display(\"%0d %0d\",a[2],a[4]); endmodule", "12 56\n", []),
    ("dynamic_arrays", "7.5", "module top; int a[]; initial begin a=new[3]; a[1]=7; $display(\"%0d %0d\",a.size(),a[1]); end endmodule", "3 7\n", []),
    ("queues", "7.10", "module top; int q[$]; int x; initial begin q.push_back(4); q.push_back(7); x=q.pop_front(); $display(\"%0d %0d %0d\",x,q.size(),q[0]); end endmodule", "4 1 7\n", []),
    ("associative_arrays", "7.8", "module top; int a[int]; initial begin a[7]=19; $display(\"%0d %0d\",a.num(),a[7]); end endmodule", "1 19\n", []),
    ("class_objects", "8", "class item; int value; function int read_value(); return value; endfunction endclass module top; item a; initial begin a=new; a.value=9; $display(\"%0d\",a.read_value()); end endmodule", "9\n", []),
    ("program_blocks", "24", "program automatic worker; initial $display(\"7\"); endprogram module top; worker u(); endmodule", "7\n", []),
    ("mos_switch", "28.7", "module top; wire y; nmos n(y,1'b1,1'b1); initial begin #1; $display(\"%b\",y); end endmodule", "1\n", []),
    ("bidirectional_switch", "28.8", "module top; wire a,b; assign a=1'b1; tran t(a,b); initial begin #1; $display(\"%b\",b); end endmodule", "1\n", []),
    ("specify_path", "30", "`timescale 1ns/1ns\nmodule delay_cell(input a,output b); assign b=a; specify (a=>b)=3; endspecify endmodule module top; reg a=0; wire b; delay_cell u(a,b); initial begin #5 a=1; #1 $display(\"%b\",b); #3 $display(\"%b\",b); end endmodule", "0\n1\n", ["-gspecify"]),
    ("real_net", "6.7.1", "module top; wire real r; assign r=1.5; initial begin #1; $display(\"%0.1f\",r); end endmodule", "1.5\n", []),
    ("ref_memory_files", "13.5.2, 21.4, 21.5", "module top; bit [7:0] a[2:4]; task automatic load(ref bit [7:0] m[2:4]); $readmemh(\"probe.mem\",m); endtask initial begin load(a); $display(\"%h %h %h\",a[2],a[3],a[4]); end endmodule", "12 34 56\n", []),
    ("scan_binary_u", "21.3.4.3", "module top; logic [31:0] v; int fd,n; initial begin fd=$fopen(\"probe_u.bin\",\"rb\"); n=$fscanf(fd,\"%u\",v); $display(\"%0d %h\",n,v); $fclose(fd); end endmodule", "1 00420041\n", []),
    ("scan_binary_z", "21.3.4.3", "module top; logic [3:0] v; int fd,n; initial begin fd=$fopen(\"probe_z.bin\",\"rb\"); n=$fscanf(fd,\"%z\",v); $display(\"%0d %b\",n,v); $fclose(fd); end endmodule", "1 1xz0\n", []),
    ("binary_output", "21.2.1.1", "module top; initial $write(\"%u%z\",4'b1xz0,4'b1xz0); endmodule", b"".join(n.to_bytes(4, sys.byteorder) for n in (8, 12, 6)), []),
    ("vcd", "21.7.1", "module top; bit a; initial begin $dumpfile(\"probe.vcd\"); $dumpvars(0,top); #1 a=1; #1; end endmodule", "", []),
]


def normalized(data, directory):
    text = data.decode("utf-8", errors="replace").replace(str(directory), "<probe>")
    return re.sub(r"^kirimasim: info: .*\n?", "", text, flags=re.M)[:4000]


def main():
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--kirimasim", type=Path, required=True)
    parser.add_argument("--iverilog", default="iverilog")
    parser.add_argument("--vvp", default="vvp")
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--require-parity", action="store_true",
                        help="fail when an Icarus-passing probe fails in kirimasim; still not a complete parity proof")
    args = parser.parse_args()
    simulator = str(args.kirimasim.resolve())
    versions = {}
    for name, cmd in (("kirimasim", [simulator, "--version"]),
                      ("icarus", [args.iverilog, "-V"]), ("vvp", [args.vvp, "-V"])):
        r = bounded_run.run(cmd, text=False, timeout=10)
        versions[name] = (r.stdout or r.stderr).decode(errors="replace").splitlines()[0]
    results = []
    for name, clause, source, expected, flags in PROBES:
        want = expected.encode() if isinstance(expected, str) else expected
        record = {"name": name, "clause": clause,
                  "category": "icarus_extension" if name == "real_net" else "standard"}
        if name == "real_net":
            record["clause"] = "Icarus extension: wire real"
            record["reference"] = "https://steveicarus.github.io/iverilog/usage/icarus_verilog_extensions.html"
        for engine in ("icarus", "kirimasim"):
            with tempfile.TemporaryDirectory(prefix="kirimasim-feature-") as tmp:
                directory = Path(tmp)
                path = directory / "probe.sv"
                path.write_text(source + "\n")
                (directory / "probe.mem").write_text("12 34 56\n")
                (directory / "probe_u.bin").write_bytes((0x00420041).to_bytes(4, sys.byteorder))
                (directory / "probe_z.bin").write_bytes(b"".join(n.to_bytes(4, sys.byteorder) for n in (12, 6)))
                try:
                    if engine == "icarus":
                        compiled = bounded_run.run([args.iverilog, "-g2012", *flags, "-s", "top",
                                                    "-o", str(directory / "test.vvp"), str(path)],
                                                   cwd=tmp, text=False, timeout=15)
                        if compiled.returncode:
                            record[engine] = {"status": "compile_failed", "exit": compiled.returncode,
                                              "diagnostic": normalized(compiled.stderr, directory)}
                            continue
                        cmd = [args.vvp, str(directory / "test.vvp")]
                    else:
                        cmd = [simulator, "--top=top", str(path)]
                    ran = bounded_run.run(cmd, cwd=tmp, text=False, timeout=15)
                    actual = ran.stdout
                    if name == "vcd" and engine == "icarus":
                        actual = re.sub(rb"^VCD info: .*\n", b"", actual, flags=re.M)
                    passed = ran.returncode == 0 and actual == want
                    if name == "vcd":
                        wave = directory / "probe.vcd"
                        passed = passed and wave.exists() and b"#1" in wave.read_bytes()
                    record[engine] = {"status": "passed" if passed else "run_failed",
                                      "exit": ran.returncode, "stdout_hex": ran.stdout.hex(),
                                      "diagnostic": normalized(ran.stderr, directory)}
                except (bounded_run.OutputLimit, subprocess.TimeoutExpired) as error:
                    record[engine] = {"status": "limit", "diagnostic": str(error).replace(tmp, "<probe>")}
        results.append(record)
    gaps = [r["name"] for r in results
            if r["icarus"]["status"] == "passed" and r["kirimasim"]["status"] != "passed"]
    report = {"scope": "original finite probes; complete feature parity not confirmed",
              "versions": versions, "gaps": gaps, "probes": results}
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(report, indent=2, ensure_ascii=False) + "\n")
    for result in results:
        print(f"{result['name']}: Icarus={result['icarus']['status']}, kirimasim={result['kirimasim']['status']}")
    return int(args.require_parity and bool(gaps))


if __name__ == "__main__":
    sys.exit(main())
