#!/usr/bin/env python3
# Copyright (c) 2026 The kirimasim Authors
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

"""Observe output before the design's blocking STDIN read (CLI.md 2.2)."""

import argparse
import os
from pathlib import Path
import select
import subprocess
import tempfile
import time


def sources():
    body = '$write("ready"); c = $fgetc(32\'h8000_0000); $write("done");'
    wrappers = {
        "initial": f"initial begin {body} end",
        "always": f"always begin #1; {body} $finish(0); end",
        "final": f"final begin {body} end",
        "task": f"task automatic t; {body} endtask initial t();",
        "function": f"function void f; {body} endfunction initial f();",
        "fork": f"initial fork begin {body} end join",
        "generate": f"if (1) begin : g initial begin {body} end end",
    }
    for name, wrapper in wrappers.items():
        yield name, f"module top; int c; {wrapper} endmodule", b"ready"
    yield "interface", f"interface bus; int c; initial begin {body} end endinterface module top; bus b(); endmodule", b"ready"
    for api in ("$display", "$fwrite", "$fdisplay", "$strobe", "$monitor", "$fstrobe", "$fmonitor"):
        args = '32\'h8000_0001, "ready"' if api.startswith("$f") else '"ready"'
        wait = "#1;" if "strobe" in api or "monitor" in api else ""
        source = f'''module top;
  int c;
  initial begin
    {api}({args}); {wait}
    c = $fgetc(32'h8000_0000);
    $monitoroff; $write("done");
  end
endmodule
'''
        yield api, source, b"ready" + (b"" if api == "$fwrite" else b"\n")


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--kirimasim", type=Path, required=True)
    exe = str(ap.parse_args().kirimasim.resolve())
    failures = []
    cases = 0
    with tempfile.TemporaryDirectory() as tmp:
        root = Path(tmp)
        path = root / "live.sv"
        for name, source, expected in sources():
            path.write_text(source)
            for enabled in (False, True):
                cmd = [exe, *(["--autoflush"] if enabled else []), str(path)]
                p = subprocess.Popen(cmd, cwd=root, stdin=subprocess.PIPE,
                                     stdout=subprocess.PIPE, stderr=subprocess.PIPE)
                observed = b""
                try:
                    deadline = time.monotonic() + (15 if enabled else 0.1)
                    while len(observed) < len(expected):
                        left = max(0, deadline - time.monotonic())
                        if not select.select([p.stdout], [], [], left)[0]:
                            break
                        chunk = os.read(p.stdout.fileno(), len(expected) - len(observed))
                        if not chunk:
                            break
                        observed += chunk
                    if observed != (expected if enabled else b""):
                        failures.append(f"{name} enabled={enabled}: live output {observed!r}")
                    try:
                        p.stdin.write(b"x")
                        p.stdin.flush()
                    except BrokenPipeError:
                        pass  # The exit status and captured diagnostic below explain it.
                    try:
                        p.stdin.close()
                    except BrokenPipeError:
                        pass
                    p.stdin = None
                    rest, err = p.communicate(timeout=15)
                    if p.returncode != 0 or observed + rest != expected + b"done":
                        failures.append(f"{name} enabled={enabled}: code {p.returncode}, "
                                        f"output {observed + rest!r}, error {err!r}")
                finally:
                    if p.poll() is None:
                        p.kill()
                        p.communicate()
                cases += 1
    print(f"autoflush: {cases} live pipe checks, {len(failures)} failures")
    for failure in failures:
        print(failure)
    return bool(failures)


if __name__ == "__main__":
    raise SystemExit(main())
