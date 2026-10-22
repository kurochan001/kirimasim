#!/usr/bin/env python3
# Copyright (c) 2026 The kirimasim Authors
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

"""Check runtime plusarg formats, diagnostics and copyback (21.6, SIR_SPEC.md 8)."""

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

        def run(text, options=(), plusargs=("+n=42",)):
            source = root / "plusargs.sv"
            source.write_text(text, encoding="utf-8")
            return bounded_run.run([exe, *options, str(source), *plusargs], cwd=root, timeout=30)

        for fmt, declaration, display, value, diagnostic in [
            ("n=", "int a = -9;", "%0d", "-9", "format without a conversion"),
            ("n=%", "int a = -9;", "%0d", "-9", "format specifier %"),
            ("n=%q", "int a = -9;", "%0d", "-9", "format specifier %q"),
            ("n=%t", "int a = -9;", "%0d", "-9", "format specifier %t"),
            ("n=%d%d", "int a = -9;", "%0d", "-9", "format must have one assigning conversion"),
            ("n=%*d", "int a = -9;", "%0d", "-9", "format must have one assigning conversion"),
            ("n=%%", "int a = -9;", "%0d", "-9", "format must have one assigning conversion"),
            ("n=%%=%d", "int a = -9;", "%0d", "-9", "format must have one assigning conversion"),
            ("n=%f", "int a = -9;", "%0d", "-9", "%f into a target of this type"),
            ("n=%d", "real a = 2.5;", "%0.1f", "2.5", "%d into a target of this type"),
            ("n=%h", 'string a = "seed";', "%s", "seed", "%h into a target of this type"),
        ]:
            text = f'''module top;
  {declaration}
  string fmt;
  initial begin
    fmt = "{fmt}";
    void'($value$plusargs(fmt, a));
    $display("unexpected continuation");
  end
  final $display("final {display}", a);
endmodule
'''
            for options in (("parse",), ("--dump-sir",)):
                p = run(text, options)
                if p.returncode != 0:
                    failures.append(f"{fmt} {options}: {p.returncode}\n{p.stderr}")
            # Invalid formats are diagnosed even without a matching argument.
            for plusargs in (("+n=42",), ()):
                p = run(text, plusargs=plusargs)
                if (p.returncode != 2 or p.stdout != f"final {value}\n" or
                        f"$value$plusargs: unsupported: {diagnostic} (not implemented yet)" not in p.stderr or
                        not re.search(r":\d+: error:", p.stderr) or "internal error" in p.stderr):
                    failures.append(f"{fmt} {plusargs}: {p.returncode}\n{p.stdout}\n{p.stderr}")
                cases += 1
        for unknown in ("x", "z"):
            p = run(f'''module top;
  logic [255:0] fmt;
  int a = -9, calls;
  function automatic int choose(); calls++; return 0; endfunction
  int values[1] = '{{-9}};
  initial begin
    fmt = '{unknown};
    void'($value$plusargs(fmt, values[choose()]));
  end
  final $display("final %0d %0d", values[0], calls);
endmodule
''')
            if (p.returncode != 2 or p.stdout != "final -9 0\n" or
                    "$value$plusargs: unsupported: format with X/Z bits" not in p.stderr):
                failures.append(f"unknown {unknown}: {p.returncode}\n{p.stdout}\n{p.stderr}")
            cases += 1
        text = '''module top;
  int a[2] = '{77, -9};
  int index, trace, n;
  string fmt;
  function automatic string format();
    trace = trace * 10 + 1; index = 1; return fmt;
  endfunction
  function automatic int choose(); trace = trace * 10 + 2; return index; endfunction
  initial begin
    fmt = "n=%d";
    n = $value$plusargs(format(), a[choose()]);
    $display("%0d %0d %0d %0d", n, a[0], a[1], trace);
  end
endmodule
'''
        for plusargs, expected in [(("+n=42", "+n=99"), "1 77 42 12\n"),
                                   ((), "0 77 -9 1\n")]:
            p = run(text, plusargs=plusargs)
            if p.returncode != 0 or p.stdout != expected:
                failures.append(f"copyback {plusargs}: {p.returncode}\n{p.stdout}\n{p.stderr}")
            cases += 1
        p = run('''module top;
  string text = "n=%s";
  int n;
  initial begin
    n = $value$plusargs(text, text);
    $display("%0d %s", n, text);
  end
endmodule
''', plusargs=("+n=hello",))
        if p.returncode != 0 or p.stdout != "1 hello\n":
            failures.append(f"aliased format/output: {p.returncode}\n{p.stdout}\n{p.stderr}")
        cases += 1
    if failures:
        print("\n".join(failures), file=sys.stderr)
        return 1
    print(f"runtime plusargs: {cases} diagnostic/copyback cases passed")
    return 0


if __name__ == "__main__":
    sys.exit(main())
