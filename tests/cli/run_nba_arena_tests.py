#!/usr/bin/env python3
# Copyright (c) 2026 The kirimasim Authors
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

"""The arena of NBA values without a delay (SCHEDULER_SPEC.md 5): it is freed
as soon as no such update waits in the NBA or Re-NBA region, so NBAs that
wake each other within one time slot hold at most the updates waiting at
once (nba.slot_peak, PERFORMANCE.md 3.2), and every update still reads its
own value, of any width or kind, after the arena is reused."""

import argparse
import json
from pathlib import Path
import sys
import tempfile

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / "tools"))
import bounded_run  # noqa: E402

N = 3000


def cases():
    """(name, source, standard output, nba.slot_peak)."""
    yield ("chain_wide", f'''module top;
  logic [128:0] c = 0;
  initial c <= 129'd1 << 100;
  always @(c) if (c[99:0] < {N}) c <= c + 1;
  final $display("%0d %0d", c[128:100], c[99:0]);
endmodule
''', f"1 {N}\n", 1)
    yield ("two_chains", f'''module top;
  int a = 0, b = 0;
  initial begin a <= 1; b <= 1; end
  always @(a) if (a < {N}) a <= a + 1;
  always @(b) if (b < {N}) b <= b + 2;
  final $display("%0d %0d", a, b);
endmodule
''', f"{N} {N + 1}\n", 2)
    yield ("burst", '''module top;
  int values[1000];
  initial begin
    for (int i = 0; i < 1000; i++) values[i] <= 3 * i;
    #1;
    for (int i = 0; i < 1000; i++) if (values[i] != 3 * i) $display("bad %0d", i);
    $display("burst %0d", values[999]);
  end
endmodule
''', "burst 2997\n", 1000)
    yield ("burst_then_chain", f'''module top;
  int values[100], c = 0;
  initial begin
    for (int i = 0; i < 100; i++) values[i] <= i + 1;
    c <= 1;
  end
  always @(c) if (c < {N}) c <= c + 1;
  final begin
    for (int i = 0; i < 100; i++) if (values[i] != i + 1) $display("bad %0d", i);
    $display("%0d %0d", c, values[99]);
  end
endmodule
''', f"{N} 100\n", 101)
    yield ("string_char_chain", '''module top;
  string s = "aaaa";
  initial s[0] <= "b";
  always @(s) if (s[0] < "z") s[0] <= s[0] + 8'd1;
  final $display("%s", s);
endmodule
''', "zaaa\n", 1)
    yield ("program_re_nba_chain", f'''module top;
  runner p();
endmodule
program runner;
  int c = 0;
  initial begin
    repeat ({N}) begin
      c <= c + 1;
      @(c);
    end
    $display("%0d", c);
  end
endprogram
''', f"{N}\n", 1)


def main():
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("--kirimasim", type=Path, required=True)
    exe = str(ap.parse_args().kirimasim.resolve())
    failures, checks = [], 0
    with tempfile.TemporaryDirectory() as tmp:
        root = Path(tmp)
        for name, text, stdout, peak in cases():
            source = root / f"{name}.sv"
            source.write_text(text)
            p = bounded_run.run([exe, "--stats=json", str(source)], cwd=root, timeout=60)
            lines = p.stdout.splitlines()
            try:
                counts = json.loads(lines[-1])
            except (ValueError, IndexError):
                counts = {}
            output = "".join(line + "\n" for line in lines[:-1])
            if (p.returncode != 0 or output != stdout or counts.get("nba.slot_peak") != peak or
                    "error" in p.stderr):
                failures.append(f"{name}: exit {p.returncode}, peak "
                                f"{counts.get('nba.slot_peak')} (want {peak})\n{p.stdout}{p.stderr}")
            checks += 1
    print(f"nba arena: {checks} checks, {len(failures)} failures")
    for failure in failures:
        print(failure)
    return bool(failures)


if __name__ == "__main__":
    sys.exit(main())
