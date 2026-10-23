#!/usr/bin/env python3
# Copyright (c) 2026 The kirimasim Authors
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

"""Wall-clock stopping at VM branches and kernel event boundaries (CLI.md 2.7)."""

import argparse
import json
from pathlib import Path
import sys
import tempfile
import time

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / "tools"))
import bounded_run  # noqa: E402


def sources():
    loop = "forever n = n + 1;"
    wrappers = {
        "initial": f"initial begin {loop} end",
        "automatic_task": f"task automatic t; {loop} endtask initial t();",
        "function": f"function void f; {loop} endfunction initial f();",
        "static_init": f"function int f; {loop} return 0; endfunction int v = f();",
        "fork": f"initial fork begin {loop} end join",
        "generate": f"if (1) begin : g initial begin {loop} end end",
        "zero_delay": "always #0;",
        "timed": "initial forever #1;",
        "nba": "bit a; initial a = 1; always @(a) a <= ~a;",
    }
    for name, body in wrappers.items():
        yield name, f'''module top;
  timeunit 1ns; timeprecision 1ns;
  int n = 0;
  {body}
  final begin
    n = 0;
    for (int i = 0; i < 1000; i++) n++;
    $display("final %0d", n);
  end
endmodule
'''


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--kirimasim", type=Path, required=True)
    exe = str(ap.parse_args().kirimasim.resolve())
    failures = []
    cases = 0
    with tempfile.TemporaryDirectory() as tmp:
        root = Path(tmp)
        source = root / "wall.sv"
        for name, text in sources():
            source.write_text(text)
            start = time.monotonic()
            p = bounded_run.run([exe, "--max-wall-time=1", "--max-time-ok", "--max-steps=0",
                                 "--max-zero-delay=0", "--max-slot-events=0", "--stats=json",
                                 str(source)], cwd=root, timeout=30)
            elapsed = time.monotonic() - start
            lines = p.stdout.splitlines()
            try:
                counts = json.loads(lines[-1])
            except (ValueError, IndexError):
                counts = {}
            diagnostic = "kirimasim: error: wall-clock limit reached: 1 seconds (--max-wall-time=1)"
            if (p.returncode != 4 or lines[:-1] != ["final 1000"] or
                    counts.get("sim.end_reason") != "wall_time_limit" or
                    p.stderr.count(diagnostic) != 1 or elapsed < 0.8 or
                    "time did not advance" in p.stderr or "internal error" in p.stderr):
                failures.append(f"{name}: {p.returncode}, {elapsed:.3f}s\n{p.stdout}\n{p.stderr}")
            cases += 1
        source.write_text('module top; initial $display("finite"); endmodule')
        for limit in ("0", "18446744073709551615"):
            p = bounded_run.run([exe, f"--max-wall-time={limit}", str(source)], cwd=root, timeout=30)
            if p.returncode != 0 or p.stdout != "finite\n" or "wall-clock limit" in p.stderr:
                failures.append(f"finite {limit}: {p.returncode}\n{p.stderr}")
            cases += 1
        for limit in ("", "-1", "1.5", "1s", "nan", "inf", "18446744073709551616"):
            p = bounded_run.run([exe, f"--max-wall-time={limit}", str(source)], cwd=root, timeout=30)
            if p.returncode != 2 or p.stdout or "needs a whole number" not in p.stderr:
                failures.append(f"invalid {limit}: {p.returncode}\n{p.stderr}")
            cases += 1
    print(f"wall time: {cases} checks, {len(failures)} failures")
    for failure in failures:
        print(failure)
    return bool(failures)


if __name__ == "__main__":
    raise SystemExit(main())
