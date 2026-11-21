#!/usr/bin/env python3
# Copyright (c) 2026 The kirimasim Authors
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

"""Structure-key order, value preservation and invalid indices (7.8, 12.7.3)."""
import argparse
from pathlib import Path
import subprocess
import tempfile


def cases():
    for width in [5, 65, 129]:
        for signed in [False, True]:
            yield f"packed_{width}_{signed}", f"""
module top;
  typedef struct packed {"signed" if signed else ""} {{ logic [{width-1}:0] v; }} key_t;
  key_t k;
  string a[key_t];
  int n;
  initial begin
    k='0; a[k]="zero";
    k='1; a[k]="ones"; a[k]="updated";
    k='x; a[k]="ignored"; if (a[k]!="") $fatal;
    k='z; a[k]="ignored"; if (a[k]!="") $fatal;
    k=3; if (a[k]!="") $fatal;
    foreach (a[i]) begin
      if (i !== ((n=={0 if signed else 1}) ? key_t'('1) : key_t'('0))) $fatal;
      if (a[i] != ((n=={0 if signed else 1}) ? "updated" : "zero")) $fatal;
      n++;
    end
    if (n!=2) $fatal; $display("ok");
  end
endmodule""", ["index contains X or Z", "nonexistent associative array entry"]
    yield "unpacked", """
module top;
  typedef struct {string a; string b; logic signed [69:0] rank;} key_t;
  task automatic check();
    key_t key,copy;
    logic [88:0] values[key_t];
    int n;
    key.a="ab"; key.b=""; key.rank=0; values[key]=7;
    key.a="a"; key.b="b"; key.rank=0; values[key]=2;
    copy=key; copy.rank=-1; values[copy]=3;
    copy.rank='z; values[copy]=0;
    if (values[copy]!=='x) $fatal;
    foreach (values[i]) begin
      case(n)
        0: if(i.rank!=-1 || values[i]!=3) $fatal;
        1: if(i.a!="a" || i.b!="b" || values[i]!=2) $fatal;
        2: if(i.a!="ab" || i.b!="" || values[i]!=7) $fatal;
      endcase
      n++; if (n==1) continue; if(n==3) break;
    end
    if (n!=3) $fatal;
  endtask
  initial begin check(); check(); $display("ok"); end
endmodule""", ["index contains X or Z"]


def rejected_cases():
    decl = "typedef struct packed {bit [7:0] id;} key_t; int a[key_t]; key_t k;"
    decl += " int b[key_t]; task automatic pass_ref(ref int x); x=1; endtask"
    for name, statement, diagnostic in [
        ("element_ref", "pass_ref(a[k]);", "reference to an associative array element"),
        ("copy", "b=a;", "whole associative array assignment"),
        ("increment", "a[k]++;", "read-modify-write"),
        ("compound", "a[k]+=2;", "read-modify-write"),
        ("default", "a='{default:7};", "user-defined default"),
    ]:
        yield name, f"module top; {decl} initial begin {statement} end endmodule", diagnostic


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--kirimasim", required=True)
    parser.add_argument("--roundtrip", required=True)
    args = parser.parse_args()
    with tempfile.TemporaryDirectory() as tmp:
        for name, source, warnings in cases():
            path = Path(tmp) / f"{name}.sv"
            path.write_text(source)
            proc = subprocess.run(
                [args.kirimasim, str(path)], capture_output=True, text=True, timeout=15
            )
            if (proc.returncode or proc.stdout != "ok\n"
                    or any(w not in proc.stderr for w in warnings)):
                raise SystemExit(f"{name}: {proc.returncode}\n{proc.stdout}{proc.stderr}")
            subprocess.run([args.roundtrip, str(path)], check=True, timeout=15)
        for name, source, diagnostic in rejected_cases():
            path = Path(tmp) / f"{name}.sv"
            path.write_text(source)
            proc = subprocess.run(
                [args.kirimasim, str(path)], capture_output=True, text=True, timeout=15
            )
            if proc.returncode != 2 or diagnostic not in proc.stderr:
                raise SystemExit(f"{name}: {proc.returncode}\n{proc.stdout}{proc.stderr}")


if __name__ == "__main__":
    main()
