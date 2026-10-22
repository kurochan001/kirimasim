#!/usr/bin/env python3
# Copyright (c) 2026 The kirimasim Authors
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

"""Check runtime scan diagnostics, no reads of missing fields, and EOF (21.3.4.3)."""

import argparse
import re
import sys
import tempfile
from pathlib import Path

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE.parent.parent / "tools"))
import bounded_run  # noqa: E402


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--kirimasim", type=Path, required=True)
    exe = str(parser.parse_args().kirimasim.resolve())
    failures = []
    cases = 0
    with tempfile.TemporaryDirectory() as tmp:
        root = Path(tmp)

        def run(text, options=()):
            source = root / "scan.sv"
            source.write_text(text, encoding="utf-8")
            return bounded_run.run([exe, *options, str(source)], cwd=root, timeout=30)

        for file in (False, True):
            api = "$fscanf" if file else "$sscanf"
            setup = ('fd = $fopen("input.txt", "w"); $fwrite(fd, "12,34!"); '
                     '$fclose(fd); fd = $fopen("input.txt", "r");' if file else "")
            source = "fd" if file else '"12,34!"'
            for fmt, declarations, target, show, want, diagnostic in [
                ("%q", "int a = -9;", "a", "%0d", "-9", "format specifier %q"),
                ("%", "int a = -9;", "a", "%0d", "-9", "format specifier %"),
                ("%t", "int a = -9;", "a", "%0d", "-9", "format specifier %t"),
                ("%f", "int a = -9;", "a", "%0d", "-9", "%f into a target of this type"),
                ("%d", "real a = 2.5;", "a", "%0.1f", "2.5", "%d into a target of this type"),
                ("%h", 'string a = "seed";', "a", "%s", "seed", "%h into a target of this type"),
            ]:
                text = f'''module top;
  {declarations}
  int fd, n;
  string fmt;
  initial begin
    {setup}
    fmt = "{fmt}";
    n = {api}({source}, fmt, {target});
    $display("unexpected continuation");
  end
  final $display("final {show}{' %0d' if file else ''}", a{', $ftell(fd)' if file else ''});
endmodule
'''
                for options in (("parse",), ("--dump-sir",)):
                    p = run(text, options)
                    if p.returncode != 0:
                        failures.append(f"{api} {fmt} {options}: {p.returncode}\n{p.stderr}")
                p = run(text)
                expected = f"final {want}" + (" 0" if file else "") + "\n"
                if (p.returncode != 2 or p.stdout != expected or
                        f"{api}: unsupported: {diagnostic} (not implemented yet)" not in p.stderr or
                        not re.search(r":\d+: error:", p.stderr) or "internal error" in p.stderr):
                    failures.append(f"{api} {fmt}: {p.returncode}\n{p.stdout}\n{p.stderr}")
                cases += 1
            text = f'''module top;
  int fd, n, a;
  string data = "12";
  function automatic string format(); data = "99"; fd = 0; return "%d"; endfunction
  initial begin
    {setup}
    n = {api}({'fd' if file else 'data'}, format(), a);
    $display("snapshot %0d %0d %0d %s", n, a, fd, data);
  end
endmodule
'''
            p = run(text)
            if p.returncode != 0 or p.stdout != "snapshot 1 12 0 99\n":
                failures.append(f"{api} input snapshot: {p.returncode}\n{p.stdout}\n{p.stderr}")
            cases += 1
            for declaration in ("logic [255:0] fmt = 'x;", "localparam logic [255:0] fmt = 'z;"):
                text = f'''module top;
  {declaration}
  int fd, n, a = -9;
  initial begin
    {setup}
    n = {api}({source}, fmt, a);
    $display("unknown %0d %0d{' %0d' if file else ''}", n, a{', $ftell(fd)' if file else ''});
  end
endmodule
'''
                p = run(text)
                expected = "unknown -1 -9" + (" 0" if file else "") + "\n"
                if p.returncode != 0 or p.stdout != expected:
                    failures.append(f"{api} unknown format: {p.returncode}\n{p.stdout}\n{p.stderr}")
                cases += 1
            for fmt, target, want in (("%d,%d", ", a", "1 12"), ("%*d,%d", "", "0 -9")):
                rewind = "void'($rewind(fd));" if file else ""
                text = f'''module top;
  int a = -9, fd, n;
  string fmt;
  initial begin
    {setup}
    fmt = "{fmt}";
    repeat (2) begin
      {rewind}
      a = -9;
      n = {api}({source}, fmt{target});
      $display("read %0d %0d{' %0d %0d' if file else ''}", n, a{', $ftell(fd), $fgetc(fd)' if file else ''});
    end
  end
endmodule
'''
                p = run(text)
                expected = ("read " + want + (" 3 51" if file else "") + "\n") * 2
                message = f"{api} has fewer arguments than its format has conversions"
                if p.returncode != 0 or p.stdout != expected or p.stderr.count(message) != 1:
                    failures.append(f"{api} missing {fmt}: {p.returncode}\n{p.stdout}\n{p.stderr}")
                cases += 1
        # Unknown input bits give EOF; neither the output's index nor its
        # write is evaluated. The format expression still runs once.
        p = run('''module top;
  logic [63:0] data;
  int a[2], calls, n;
  function automatic string format(); calls++; return "%d"; endfunction
  function automatic int choose(); calls += 10; return 1; endfunction
  initial begin
    data = 'z; a[0] = 7; a[1] = 9;
    n = $sscanf(data, format(), a[choose()]);
    $display("%0d %0d %0d %0d", n, a[0], a[1], calls);
  end
endmodule
''')
        if p.returncode != 0 or p.stdout != "-1 7 9 1\n":
            failures.append(f"unknown input: {p.returncode}\n{p.stdout}\n{p.stderr}")
        cases += 1
    if failures:
        print("\n".join(failures), file=sys.stderr)
        return 1
    print(f"runtime scan: {cases} diagnostic/EOF cases passed")
    return 0


if __name__ == "__main__":
    sys.exit(main())
