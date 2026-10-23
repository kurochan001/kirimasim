#!/usr/bin/env python3
# Copyright (c) 2026 The kirimasim Authors
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

"""Matrix of $stop policies, levels and containers (SCHEDULER_SPEC.md 16.2)."""

import argparse
import json
from pathlib import Path
import sys
import tempfile

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / "tools"))
import bounded_run  # noqa: E402


def sources(level):
    body = f'$display("before"); $stop({level}); $display("after");'
    wrappers = {
        "initial": f"initial begin {body} end",
        "static_task": f"task t; {body} endtask initial t();",
        "automatic_task": f"task automatic t; {body} endtask initial t();",
        "function": f"function void f; {body} endfunction initial f();",
        "static_init": f"function int f; {body} return 7; endfunction int v = f();",
        "fork": f"initial fork begin {body} end join",
        "generate": f"if (1) begin : g initial begin {body} end end",
        "always": f"always begin #1; {body} $finish(0); end",
        "waiting_task": f"task automatic t; #1; {body} endtask initial t();",
    }
    for name, wrapper in wrappers.items():
        yield name, f'module top; {wrapper} final $display("final"); endmodule'
    yield "interface", f'''interface bus;
  initial begin {body} end
endinterface
module top; bus b(); final $display("final"); endmodule'''


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--kirimasim", type=Path, required=True)
    exe = str(ap.parse_args().kirimasim.resolve())
    failures = []
    cases = 0
    with tempfile.TemporaryDirectory() as tmp:
        root = Path(tmp)
        source = root / "stop.sv"
        for level in (0, 1, 2):
            for container, text in sources(level):
                source.write_text(text)
                for mode in (None, "finish", "ignore", "error"):
                    flags = [] if mode is None else [f"--stop={mode}"]
                    p = bounded_run.run([exe, "--stats=json", *flags, str(source)], cwd=root, timeout=20)
                    lines = p.stdout.splitlines()
                    want = ["before", *(["after"] if mode == "ignore" else []), "final"]
                    reason = ("finish" if container == "always" else "quiescent") if mode == "ignore" else "stop"
                    try:
                        counts = json.loads(lines[-1])
                    except (ValueError, IndexError):
                        counts = {}
                    reports = [s for s in p.stderr.splitlines() if s.startswith("kirimasim: info: statistics ")]
                    if (p.returncode != (1 if mode == "error" else 0) or lines[:-1] != want or
                            counts.get("sim.end_reason") != reason or
                            ("$stop at" in p.stderr) != (level != 0) or
                            len(reports) != (1 if level == 2 else 0)):
                        failures.append(f"{container} {level} {mode}: {p.returncode}\n{p.stdout}\n{p.stderr}")
                    if reports and json.loads(reports[0].split("statistics ", 1)[1]) != counts:
                        failures.append(f"{container} {level} {mode}: statistics differ")
                    cases += 1
        for mode in ("", "pause", "IGNORE", "-1", "finish,ignore"):
            p = bounded_run.run([exe, f"--stop={mode}", str(source)], cwd=root, timeout=20)
            if p.returncode != 2 or p.stdout or "needs finish, ignore or error" not in p.stderr:
                failures.append(f"invalid policy {mode}: {p.returncode}\n{p.stderr}")
            cases += 1
    print(f"stop: {cases} checks, {len(failures)} failures")
    for failure in failures:
        print(failure)
    return bool(failures)


if __name__ == "__main__":
    raise SystemExit(main())
