#!/usr/bin/env python3
# Copyright (c) 2026 The kirimasim Authors
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
"""IEEE 1800-2023 25.9, scalar virtual interface identity and member access."""
import argparse
from pathlib import Path
import subprocess
import tempfile

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--kirimasim", required=True)
    parser.add_argument("--roundtrip", required=True)
    args = parser.parse_args()
    with tempfile.TemporaryDirectory() as tmp:
        for width in [1, 17, 129]:
            bits = f"logic [{width-1}:0]"
            prefix = f"interface link #(parameter W={width}); logic [W-1:0] d; endinterface\n"
            for name, body, rc, message in [
                ("copy", f"""module top; link a(), b(); link #(3) different();
                  virtual link v; int events;
                  always @(b.d) events++;
                  task automatic check(virtual link x);
                    virtual link prev; prev=x;
                    x=a; x.d='0; prev.d='1;
                    if (prev==x || prev!=v || prev.d!=='1 || x.d!=='0) $fatal;
                  endtask
                  initial begin if (v!=null) $fatal; v=b; check(v); #1;
                    if (v.d!=='1 || events!=1) $fatal; v=null;
                    if (v!=null) $fatal; $display("ok"); end endmodule""", 0, ""),
                ("null", f"""module top; link a(); virtual link v;
                  initial begin {bits} tmp; tmp=v.d; $display("unreachable"); end endmodule""",
                 1, "null virtual interface"),
                ("nba", """module top; link a(); virtual link v;
                  initial begin v=a; v.d<='1; end endmodule""", 2, "deferred update"),
                ("property", """module top; link a(); class holder; virtual link v; endclass
                  holder h; initial h=new; endmodule""", 2, "virtual interface class property"),
                ("delayed_target", """module top; link a(); virtual link v;
                  initial begin v=a; v.d = #1 '1; end endmodule""", 2, "deferred update"),
                ("delayed_concat", """module top; link a(); virtual link v; bit b;
                  initial begin v=a; {b, v.d[0]} = #1 3; end endmodule""", 2, "deferred update"),
                ("delayed_read", f"""module top; link a(); virtual link v; {bits} result;
                  initial begin v=a; #1 v.d='1; result = #1 v.d;
                    if(result!=='1) $fatal; $display("ok"); end endmodule""", 0, ""),
            ]:
                path = Path(tmp) / f"{name}.sv"
                path.write_text(prefix + body)
                proc = subprocess.run(
                    [args.kirimasim, str(path)], capture_output=True, text=True, timeout=15
                )
                if (proc.returncode != rc or (rc == 0 and proc.stdout != "ok\n")
                        or message not in proc.stderr + proc.stdout):
                    raise SystemExit(f"{width} {name}: {proc.returncode}\n{proc.stdout}{proc.stderr}")
                if rc != 2:
                    subprocess.run([args.roundtrip, str(path)], check=True, timeout=15)

if __name__ == "__main__":
    main()
