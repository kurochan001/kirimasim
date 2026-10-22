#!/usr/bin/env python3
# Copyright (c) 2026 The kirimasim Authors
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

"""Runtime string format diagnostics, counts and evaluation order (21.3.3)."""

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
            source = root / "format.sv"
            source.write_text(text, encoding="utf-8")
            return bounded_run.run([exe, *options, str(source)], cwd=root, timeout=30)

        for api in ("f", "s"):
            def call(fmt, args, target="result"):
                actuals = fmt + (", " + args if args else "")
                return (f"{target} = $sformatf({actuals});" if api == "f" else
                        f"$sformat({target}, {actuals});")

            for fmt, arg, diagnostic in [
                ("%q", "7", "format specifier %q"),
                ("%", "7", "ends with a lone %"),
                ("%p", "7", "format specifier %p"),
                ("%+d", "7", "flags +, space and #"),
                ("%.2d", "7", "flags +, space and #"),
                ("%0d", 'string\'(\"text\")', "%d of a string"),
            ]:
                text = f'''module top;
  string fmt, result = "seed";
  initial begin
    fmt = "{fmt}";
    {call('fmt', arg)}
    $display("unexpected continuation");
  end
  final $display("final [%s]", result);
endmodule
'''
                for options in (("parse",), ("--dump-sir",)):
                    p = run(text, options)
                    if p.returncode != 0:
                        failures.append(f"{api} {fmt} {options}: {p.returncode}\n{p.stderr}")
                p = run(text)
                if (p.returncode != 2 or p.stdout != "final [seed]\n" or
                        diagnostic not in p.stderr or not re.search(r":\d+: error:", p.stderr) or
                        "internal error" in p.stderr):
                    failures.append(f"{api} {fmt}: {p.returncode}\n{p.stdout}\n{p.stderr}")
                cases += 1
            for unknown in ("x", "z"):
                p = run(f'''module top;
  logic [255:0] fmt;
  string result = "seed";
  initial begin fmt = '{unknown}; {call('fmt', '7')} end
  final $display("final [%s]", result);
endmodule
''')
                if (p.returncode != 2 or p.stdout != "final [seed]\n" or
                        "format with X/Z bits" not in p.stderr):
                    failures.append(f"{api} {unknown}: {p.returncode}\n{p.stdout}\n{p.stderr}")
                cases += 1
            for fmt, args, expected, trace in [("A%0dB%0dC", "value(7)", "A7BC", 2),
                                               ("%0d", "value(7), value(8)", "7", 22)]:
                p = run(f'''module top;
  string fmt, result;
  int trace;
  function automatic int value(input int x); trace = trace * 10 + 2; return x; endfunction
  initial begin
    fmt = "{fmt}";
    for (int i = 0; i < 2; i++) begin
      trace = 0; {call('fmt', args)}
      $display("[%s] %0d", result, trace);
    end
  end
endmodule
''')
                if (p.returncode != 0 or p.stdout != f"[{expected}] {trace}\n" * 2 or
                        p.stderr.count(": warning:") != 1 or
                        "different number of arguments than conversions" not in p.stderr):
                    failures.append(f"{api} counts: {p.returncode}\n{p.stdout}\n{p.stderr}")
                cases += 1
            p = run(f'''module top;
  string fmt = "%0d", result;
  function automatic int value(); fmt = "%04h"; return 10; endfunction
  initial begin {call('fmt', 'value()')} $display("[%s]", result); end
endmodule
''')
            if p.returncode != 0 or p.stdout != "[10]\n":
                failures.append(f"{api} snapshot: {p.returncode}\n{p.stdout}\n{p.stderr}")
            cases += 1
            p = run(f'''module top;
  string fmt = "A%0dB%0dC", result;
  initial begin {call('fmt', ', 7')} $display("[%s]", result); end
endmodule
''')
            # The pinned slang accepts empty values for the function,
            # while the task's binding rejects an empty argument. Keep
            # that diagnostic instead of loosening the front end.
            empty_ok = (p.returncode == 0 and p.stdout == "[A B7C]\n" and
                        ": warning:" not in p.stderr) if api == "f" else (
                            p.returncode == 2 and not p.stdout and
                            "empty argument not allowed" in p.stderr)
            if not empty_ok:
                failures.append(f"{api} empty argument: {p.returncode}\n{p.stdout}\n{p.stderr}")
            cases += 1
        p = run('''module top;
  string items[2] = '{"left", "right"};
  int index, trace;
  function automatic string format(); trace = 1; index = 1; return "%0d"; endfunction
  function automatic int value(); trace = trace * 10 + 2; return 7; endfunction
  function automatic int choose(); trace = trace * 10 + 3; return index; endfunction
  initial begin
    $sformat(items[choose()], format(), value());
    $display("%s %s %0d", items[0], items[1], trace);
  end
endmodule
''')
        if p.returncode != 0 or p.stdout != "left 7 123\n":
            failures.append(f"destination copyback: {p.returncode}\n{p.stdout}\n{p.stderr}")
        cases += 1
        p = run('''module top;
  logic [31:0] bits = 32'hffff0000;
  string fmt = "%s";
  initial begin
    $sformat(bits[15:0], fmt, "ok");
    $display("%h", bits);
  end
endmodule
''')
        if p.returncode != 0 or p.stdout != "ffff6f6b\n":
            failures.append(f"packed destination: {p.returncode}\n{p.stdout}\n{p.stderr}")
        cases += 1
    if failures:
        print("\n".join(failures), file=sys.stderr)
        return 1
    print(f"runtime string format: {cases} diagnostic/evaluation cases passed")
    return 0


if __name__ == "__main__":
    sys.exit(main())
