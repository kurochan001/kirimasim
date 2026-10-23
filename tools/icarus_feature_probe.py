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

# Original additional mechanisms. Keep the 16-case seed inventory stable;
# --extended selects these too, without claiming an exhaustive language list.
EXTENDED = [
    ("mixed_named_event_wait", "9.4.2, 15.5", "module top; event a,b; bit clk=0; initial begin @(a or b or posedge clk); $display(\"first %0t\",$time); @(a or b or posedge clk); $display(\"second %0t\",$time); end initial begin #1 ->a; ->b; #1 clk=1; end endmodule", "first 1\nsecond 2\n", []),
    ("named_event_iff", "9.4.2.3", "module top; event a; logic en=0; initial begin @(a iff en); $display(\"%0t\",$time); end initial begin #1 ->a; #1 en=1; #1 ->a; end endmodule", "3\n", []),
    ("nested_packed_select_bounds", "7.4.5, 11.5.1", "module top; logic [1:0][7:0] a=16'h5500; int i=7; initial begin a[0][i+:3]=3'b111; $display(\"%h\",a); end endmodule", "5580\n", []),
    ("delayed_nba_streaming", "10.4.2, 11.4.14.3", "module top; logic [7:0] a[2]; logic [15:0] d; initial begin a='{0,0}; d=16'h1234; {<<8{a}} <= #2 d; d=0; #3 $display(\"%h %h\",a[0],a[1]); end endmodule", "34 12\n", []),
    ("delayed_nba_fixed_array", "7.6, 10.4.2", "module top; int a[2:1],b[0:1]; initial begin a='{0,0}; b='{12,34}; a <= #2 b; b='{90,91}; #3 $display(\"%0d %0d\",a[2],a[1]); end endmodule", "12 34\n", []),
    ("delayed_nba_unpacked_struct", "10.4.2, 10.9.2", "module top; typedef struct {int v; int words[2];} row_t; row_t a[2],b; int i=1; initial begin b='{v:7,words:'{12,34}}; a[i] <= #2 b; i=0; b='{v:0,words:'{0,0}}; #3 $display(\"%0d %0d %0d\",a[1].v,a[1].words[0],a[1].words[1]); end endmodule", "7 12 34\n", []),
    ("dynamic_resize_copy", "7.5", "module top; int a[]; initial begin a=new[2]; a[0]=12; a[1]=34; a=new[3](a); $display(\"%0d %0d %0d %0d\",a.size(),a[0],a[1],a[2]); a.delete(); $display(\"%0d\",a.size()); end endmodule", "3 12 34 0\n0\n", []),
    ("queue_front_back", "7.10.2", "module top; int q[$]; int x,y; initial begin q.push_back(7); q.push_front(3); x=q.pop_back(); y=q.pop_front(); $display(\"%0d %0d %0d\",x,y,q.size()); end endmodule", "7 3 0\n", []),
    ("class_constructor", "8.7", "class node; int v; function new(int n); v=n; endfunction endclass module top; node a,b; initial begin a=new(7); b=new(9); $display(\"%0d %0d\",a.v,b.v); end endmodule", "7 9\n", []),
    ("class_virtual_dispatch", "8.20", "class base; virtual function int value(); return 3; endfunction endclass class derived extends base; function int value(); return 9; endfunction endclass module top; base b; derived d; initial begin d=new; b=d; $display(\"%0d\",b.value()); end endmodule", "9\n", []),
    ("delayed_net_assignment", "6.7, 4.9.6", "`timescale 1ns/1ns\nmodule top; logic a=0; wire #2 y=a; initial begin #5 a=1; #1 $display(\"%b\",y); #2 $display(\"%b\",y); end endmodule", "0\n1\n", []),
    ("bare_delayed_net", "6.7, 4.9.6", "`timescale 1ns/1ns\nmodule top; logic a=0; wire #2 y; assign y=a; initial begin #5 a=1; #1 $display(\"%b\",y); #2 $display(\"%b\",y); end endmodule", "0\n1\n", []),
    ("event_controlled_nba", "10.4.2", "module top; bit clk=0; int d=7,q=1; initial begin q <= @(posedge clk) d; d=9; #2 clk=1; #1 $display(\"%0d\",q); end endmodule", "7\n", []),
    ("delayed_nba_array_index", "10.4.2", "module top; int a[2]='{0,0}; int i=1; initial begin a[i] <= #2 7; i=0; #3 $display(\"%0d %0d\",a[0],a[1]); end endmodule", "0 7\n", []),
    ("nested_packed_select", "7.4.1, 11.5.1", "module top; logic [1:0][7:0] a; int i=2; initial begin a=0; a[1][i+:3]=3'b101; $display(\"%h\",a); end endmodule", "1400\n", []),
    ("array_slice_assignment", "7.4.6", "module top; int a[0:3]='{1,2,3,4}; int b[0:1]; initial begin b=a[1:2]; $display(\"%0d %0d\",b[0],b[1]); end endmodule", "2 3\n", []),
    ("mixed_strength_port_bit", "23.3.3.7, 28.12", "module leaf(output wire y); assign (weak1,weak0) y=1'b0; endmodule module top; wire [1:0] y; leaf u(y[1]); assign (weak1,weak0) y[1]=1'b1; initial begin #1 $display(\"%b\",y[1]); end endmodule", "x\n", []),
]
# Original container mechanisms include both implemented and remaining forms.
# Keep the original seed and earlier extended probes for revision comparisons.
EXTENDED.extend([
    ("dynamic_value_copy", "7.5, 7.6",
     "module top; int a[],b[]; initial begin a=new[2]; a[0]=7; b=a; b[0]=9; $display(\"%0d %0d\",a[0],b[0]); end endmodule",
     "7 9\n", []),
    ("queue_value_copy", "7.10.4",
     "module top; int a[$],b[$]; initial begin a.push_back(7); b=a; b[0]=9; $display(\"%0d %0d\",a[0],b[0]); end endmodule",
     "7 9\n", []),
    ("queue_append_foreach", "7.4.5, 7.10, 12.7.3",
     "module top; int q[$]; int s; initial begin q[$+1]=4; q[$+1]=7; s=0; foreach(q[i]) s+=q[i]; $display(\"%0d %0d\",q.size(),s); end endmodule",
     "2 11\n", []),
    ("queue_actual_ref_bounds", "6.22.2, 7.10.5, 13.5.2",
     "module top; int q[$:3]; task automatic put(ref int x[$:1]); x='{1,2,3,4}; endtask initial begin put(q); $display(\"%0d %0d\",q.size(),q[3]); end endmodule",
     "4 4\n", []),
    ("nested_queue_value_copy", "7.6, 7.10.4",
     "module top; typedef int inner_t[$]; inner_t a[$],b[$]; initial begin a='{'{1,2}}; b=a; b[0][0]=9; $display(\"%0d %0d\",a[0][0],b[0][0]); end endmodule",
     "1 9\n", []),
    ("container_delayed_value", "7.6, 10.4.2",
     "module top; int q[$],r[$]; initial begin q.push_back(7); r <= #2 q; q[0]=9; #3; $display(\"%0d %0d\",r.size(),r[0]); end endmodule",
     "1 7\n", []),
    ("dynamic_array_queries", "20.7",
     "module top; int a[]; initial begin a=new[3]; $display(\"%0d %0d %0d %0d\",$size(a),$left(a),$right(a),$increment(a)); end endmodule",
     "3 0 2 -1\n", []),
    ("dynamic_array_foreach", "7.5, 12.7.3",
     "module top; int a[]; int s; initial begin a=new[3]; a[0]=1; a[1]=2; a[2]=3; s=0; foreach(a[i]) s+=a[i]; $display(\"%0d\",s); end endmodule",
     "6\n", []),
    ("queue_runtime_concat", "7.10.4, 10.10",
     "module top; int a[$],q[$]; initial begin a.push_back(4); a.push_back(7); q={a,9}; $display(\"%0d %0d %0d\",q.size(),q[0],q[2]); end endmodule",
     "3 4 9\n", []),
    ("queue_slice", "7.10.1",
     "module top; int a[$],q[$]; initial begin a.push_back(4); a.push_back(7); a.push_back(9); q=a[1:$]; $display(\"%0d %0d %0d\",q.size(),q[0],q[1]); end endmodule",
     "2 7 9\n", []),
    ("queue_conditional", "11.4.11",
     "module top; bit choose=1; int a[$],b[$],q[$]; initial begin a.push_back(4); b.push_back(9); q=choose?a:b; $display(\"%0d %0d\",q.size(),q[0]); end endmodule",
     "1 4\n", []),
    ("dynamic_array_sum", "7.12.3",
     "module top; int a[]; initial begin a=new[3]; a[0]=1; a[1]=2; a[2]=3; $display(\"%0d\",a.sum()); end endmodule",
     "6\n", []),
    ("queue_sort", "7.12.2",
     "module top; int q[$]; initial begin q.push_back(7); q.push_back(4); q.sort(); $display(\"%0d %0d\",q[0],q[1]); end endmodule",
     "4 7\n", []),
    ("dynamic_query_dimension", "20.7",
     "module top; int a[]; int dim=1; initial begin a=new[3]; $display(\"%0d\",$size(a,dim)); end endmodule",
     "3\n", []),
    ("mixed_dynamic_foreach", "12.7.3",
     "module top; bit [3:0] q[$]; int count; initial begin q.push_back(4'b1010); count=0; foreach(q[i,j]) count++; $display(\"%0d\",count); end endmodule",
     "4\n", []),
])
for _primitive in ("nmos", "pmos", "rnmos", "rpmos"):
    _enabled = 0 if _primitive.endswith("pmos") else 1
    EXTENDED.append(("mos_" + _primitive, "28.7, 28.9",
        "module top; logic en=0; wire y; " + _primitive + " u(y,1'b1,en); "
        "initial begin #1 $display(\"%b\",y); en=1; #1 $display(\"%b\",y); "
        "en=1'bx; #1 $display(\"%b\",y); en=1'bz; #1 $display(\"%b\",y); end endmodule",
        ("1\nz\nx\nx\n" if _enabled == 0 else "z\n1\nx\nx\n"), []))
for _primitive in ("tranif0", "tranif1", "rtranif0", "rtranif1"):
    EXTENDED.append(("switch_" + _primitive, "28.8, 28.9",
        "module top; logic en=0; wire a,y; assign a=1'b1; " + _primitive + " u(a,y,en); "
        "initial begin #1 $display(\"%b\",y); en=1; #1 $display(\"%b\",y); "
        "en=1'bx; #1 $display(\"%b\",y); en=1'bz; #1 $display(\"%b\",y); end endmodule",
        ("1\nz\nx\nx\n" if _primitive.endswith("0") else "z\n1\nx\nx\n"), []))
for _primitive in ("cmos", "rcmos"):
    EXTENDED.append(("mos_" + _primitive, "28.7, 28.9",
        "module top; logic n=0,p=1; wire y; " + _primitive + " u(y,1'b1,n,p); "
        "initial begin #1 $display(\"%b\",y); n=1;p=0; #1 $display(\"%b\",y); "
        "n=1'bx;p=1'bx; #1 $display(\"%b\",y); end endmodule", "z\n1\nx\n", []))
EXTENDED.append(("switch_rtran", "28.8, 28.9",
    "module top; wire a,y; assign a=1'b1; rtran u(a,y); initial begin #1 $display(\"%b\",y); end endmodule",
    "1\n", []))

# Keep new file-input observations separate from the original 59 probes.
EXTENDED.extend([
    ("integral_fread_ignored_range", "21.3.4.4",
     'module top; bit[15:0] v; int fd,n; initial begin fd=$fopen("probe.mem","rb"); n=$fread(v,fd,19,0); $display("%0d %h",n,v); end endmodule',
     "2 3132\n", []),
    ("multidimensional_fread", "21.3.4.4",
     'module top; bit[7:0] m[2][2]; int fd,n; initial begin fd=$fopen("probe.mem","rb"); n=$fread(m,fd,0,2); $display("%0d %h %h",n,m[0][0],m[0][1]); end endmodule',
     "2 31 32\n", []),
])

# The oracle follows 7.12.1: an extremum result has at most one element,
# even for equal keys. A reference result mismatch is not a passing gap.
EXTENDED.extend([
    ("dynamic_minimum_unique", "7.12.1",
     'module top; int a[],q[$]; initial begin a=new[3]; a[0]=4;a[1]=7;a[2]=6; q=a.min(); $display("%0d %0d",q.size(),q[0]); end endmodule',
     "1 4\n", []),
    ("queue_maximum_unique", "7.12.1",
     'module top; int a[$],q[$]; initial begin a.push_back(4);a.push_back(7);a.push_back(6); q=a.max(); $display("%0d %0d",q.size(),q[0]); end endmodule',
     "1 7\n", []),
    ("dynamic_minimum_duplicate", "7.12.1",
     'module top; int a[],q[$]; initial begin a=new[3]; a[0]=4;a[1]=7;a[2]=4; q=a.min(); $display("%0d %0d",q.size(),q[0]); end endmodule',
     "1 4\n", []),
    ("queue_maximum_duplicate", "7.12.1",
     'module top; int a[$],q[$]; initial begin a.push_back(4);a.push_back(7);a.push_back(7); q=a.max(); $display("%0d %0d",q.size(),q[0]); end endmodule',
     "1 7\n", []),
    ("dynamic_minimum_with_key", "7.12.1",
     'module top; int a[],q[$]; initial begin a=new[3]; a[0]=4;a[1]=7;a[2]=6; q=a.min() with (-item); $display("%0d %0d",q.size(),q[0]); end endmodule',
     "1 7\n", []),
    ("dynamic_minimum_empty", "7.12.1",
     'module top; int a[],q[$]; initial begin a=new[0]; q=a.min(); $display("%0d",q.size()); end endmodule',
     "0\n", []),
    ("queue_maximum_empty", "7.12.1",
     'module top; int a[$],q[$]; initial begin q=a.max(); $display("%0d",q.size()); end endmodule',
     "0\n", []),
])


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
    parser.add_argument("--extended", action="store_true", help="include additional original mechanism probes")
    parser.add_argument("--only", action="append", help="select named probes (repeatable)")
    args = parser.parse_args()
    simulator = str(args.kirimasim.resolve())
    versions = {}
    for name, cmd in (("kirimasim", [simulator, "--version"]),
                      ("icarus", [args.iverilog, "-V"]), ("vvp", [args.vvp, "-V"])):
        r = bounded_run.run(cmd, text=False, timeout=10)
        versions[name] = (r.stdout or r.stderr).decode(errors="replace").splitlines()[0]
    results = []
    selected = PROBES + EXTENDED if args.extended else PROBES
    if args.only:
        unknown = set(args.only) - {row[0] for row in selected}
        if unknown:
            parser.error("unknown probe(s): " + ", ".join(sorted(unknown)))
        selected = [row for row in selected if row[0] in args.only]
    for name, clause, source, expected, flags in selected:
        want = expected.encode() if isinstance(expected, str) else expected
        record = {"name": name, "clause": clause,
                  "category": "icarus_extension" if name == "real_net" else "standard",
                  "expected_stdout_hex": want.hex()}
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
                    record[engine] = {"status": "passed" if passed else "run_failed" if ran.returncode else "result_mismatch",
                                      "exit": ran.returncode, "stdout_hex": ran.stdout.hex(),
                                      "compared_stdout_hex": actual.hex(),
                                      "diagnostic": normalized(ran.stderr, directory)}
                except (bounded_run.OutputLimit, subprocess.TimeoutExpired) as error:
                    record[engine] = {"status": "limit", "diagnostic": str(error).replace(tmp, "<probe>")}
        results.append(record)
    gaps = [r["name"] for r in results
            if r["icarus"]["status"] == "passed" and r["kirimasim"]["status"] != "passed"]
    reference_mismatches = [r["name"] for r in results
                            if r["icarus"]["status"] == "result_mismatch"]
    report = {"scope": "original finite probes; complete feature parity not confirmed",
              "versions": versions, "gaps": gaps,
              "reference_mismatches": reference_mismatches, "probes": results}
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(report, indent=2, ensure_ascii=False) + "\n")
    for result in results:
        print(f"{result['name']}: Icarus={result['icarus']['status']}, kirimasim={result['kirimasim']['status']}")
    if reference_mismatches:
        print("Icarus result mismatches against the written oracle: " + ", ".join(reference_mismatches))
    return int(args.require_parity and bool(gaps))


if __name__ == "__main__":
    sys.exit(main())
