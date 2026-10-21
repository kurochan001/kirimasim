#!/usr/bin/env python3
# Copyright (c) 2026 The kirimasim Authors
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

"""Measures the simulation speed of kirimasim on the designs under bench/,
and of Icarus Verilog and Verilator for comparison (PERFORMANCE.md 5).

Each design takes +n=<count>, the amount of work, and prints one result line
that starts with its name. Its header gives the count to measure with
(// bench-n:) and the result line for that count (// bench-expect:), on which
kirimasim, Icarus Verilog and Verilator agreed when the design was added.

The simulation time is the difference between a run with +n=<count> and a
run of the same design with +n=1. Starting the program, reading the design
and, for kirimasim, the front end take the same time in both runs and drop
out; what is left is the work that grows with the count. Each tool first runs
each design once with +n=1, so that a newly built program has started once
before it is timed, then --runs times with each count, alternating; the
medians are subtracted.

Icarus is iverilog -g2012, then vvp. Verilator is verilator --binary --timing
with its default C++ options, then the built program. Their build steps are
timed once and reported apart; kirimasim has no build step.

Writes results.json and summary.md to --out and prints the summary. They
record the tools' versions and the machine's CPU, cores, memory, OS and load
average, and no host name or user name (PERFORMANCE.md 5.3). The exit code is
1 when a run fails or a result line differs from the expected one.

  tools/bench.py --kirimasim build/release/src/driver/kirimasim \\
      --iverilog <icarus>/bin/iverilog --verilator <verilator>/bin/verilator \\
      --out bench-out
  tools/bench.py --kirimasim <kirimasim> --n 100 --runs 1 --out <dir>   # quick check

With --n, every design runs with that count instead of its own, and only the
presence of the result line is checked.
"""

from __future__ import annotations

import argparse
import json
import os
import platform
import re
import statistics
import subprocess
import sys
import tempfile
import threading
import time
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
BENCH = ROOT / "bench"
TIMEOUT = 600
NAMES = {"kirimasim": "kirimasim", "icarus": "Icarus Verilog", "verilator": "Verilator"}


def header(path: Path) -> tuple[int, str]:
    n = expect = None
    for line in path.read_text(encoding="utf-8").splitlines():
        if m := re.fullmatch(r"// bench-n: (\d+)", line):
            n = int(m.group(1))
        if m := re.fullmatch(r"// bench-expect: (.+)", line):
            expect = m.group(1)
    if n is None or expect is None:
        sys.exit(f"{path.name}: the header needs // bench-n: and // bench-expect:")
    return n, expect


def run(cmd: list[str], cwd: Path) -> tuple[float, float, int, str]:
    """Runs cmd; returns its seconds, its maximum RSS in MiB, its exit code
    and its standard output."""
    start = time.monotonic()
    p = subprocess.Popen(cmd, cwd=cwd, stdout=subprocess.PIPE,
                         stderr=subprocess.DEVNULL, text=True)
    timer = threading.Timer(TIMEOUT, p.kill)
    timer.start()
    out = p.stdout.read()
    _, status, usage = os.wait4(p.pid, 0)
    seconds = time.monotonic() - start
    timer.cancel()
    p.returncode = 0  # reaped by wait4 above
    p.stdout.close()
    rc = os.WEXITSTATUS(status) if os.WIFEXITED(status) else 128 + os.WTERMSIG(status)
    # ru_maxrss is in bytes on macOS and in KiB on Linux.
    rss = usage.ru_maxrss / (1 << 20 if sys.platform == "darwin" else 1 << 10)
    return seconds, rss, rc, out


def prepare(tool: str, exe: str, design: Path, work: Path):
    """Builds the design for the tool. Returns the build seconds (None for
    kirimasim), an error text or None, and a function from a count to the
    command that runs it."""
    if tool == "kirimasim":
        return None, None, lambda n: [exe, str(design), f"+n={n}"]
    if tool == "icarus":
        compiled = work / "bench.vvp"
        seconds, _, rc, _ = run([exe, "-g2012", "-o", str(compiled), str(design)], work)
        vvp = str(Path(exe).with_name("vvp"))
        return seconds, (f"iverilog exit code {rc}" if rc else None), \
            lambda n: [vvp, "-n", str(compiled), f"+n={n}"]
    seconds, _, rc, _ = run([exe, "--binary", "--timing", "-Wno-fatal", "-Wno-lint",
                             "-Wno-style", "--build-jobs", "0", "-Mdir", str(work / "obj"),
                             "--prefix", "Vbench", str(design)], work)
    program = str(work / "obj" / "Vbench")
    return seconds, (f"verilator exit code {rc}" if rc else None), lambda n: [program, f"+n={n}"]


def result_line(name: str, out: str) -> str | None:
    return next((line for line in out.splitlines() if line.startswith(name + " ")), None)


def measure(tool: str, exe: str, design: Path, n: int, expect: str | None,
            runs: int) -> dict:
    name = design.stem
    with tempfile.TemporaryDirectory() as tmp:
        work = Path(tmp)
        build, error, command = prepare(tool, exe, design, work)
        if error:
            return {"error": error, "build_seconds": build}
        run(command(1), work)  # the first start of a newly built program
        small, large = [], []
        for _ in range(runs):
            small.append(run(command(1), work))
            large.append(run(command(n), work))
    for seconds, rss, rc, out in small + large:
        if rc != 0:
            return {"error": f"exit code {rc}", "build_seconds": build}
    for seconds, rss, rc, out in large:
        line = result_line(name, out)
        if line is None or (expect is not None and line != expect):
            return {"error": f"result line {line!r}, expected {expect!r}",
                    "build_seconds": build}
    base = statistics.median(r[0] for r in small)
    whole = statistics.median(r[0] for r in large)
    return {"build_seconds": build, "base_seconds": base, "whole_seconds": whole,
            "simulation_seconds": whole - base,
            "max_rss_mib": statistics.median(r[1] for r in large),
            "all_seconds": [[r[0] for r in small], [r[0] for r in large]]}


def text_of(cmd: list[str]) -> str:
    try:
        p = subprocess.run(cmd, capture_output=True, text=True, timeout=30, check=False)
    except OSError:
        return "unknown"
    lines = (p.stdout or p.stderr).strip().splitlines()
    return lines[0] if lines else "unknown"


def machine() -> dict:
    info = {"os": platform.platform(), "cores": os.cpu_count(),
            "load_average": [round(x, 2) for x in os.getloadavg()]}
    if sys.platform == "darwin":
        info["cpu"] = text_of(["sysctl", "-n", "machdep.cpu.brand_string"])
        mem = text_of(["sysctl", "-n", "hw.memsize"])
        info["memory_gib"] = round(int(mem) / (1 << 30)) if mem.isdigit() else None
    else:
        cpuinfo = Path("/proc/cpuinfo").read_text(errors="ignore")
        meminfo = Path("/proc/meminfo").read_text(errors="ignore")
        cpu = re.search(r"^model name\s*:\s*(.+)$", cpuinfo, re.M)
        mem = re.search(r"^MemTotal:\s*(\d+) kB", meminfo, re.M)
        info["cpu"] = cpu.group(1) if cpu else "unknown"
        info["memory_gib"] = round(int(mem.group(1)) / (1 << 20)) if mem else None
    return info


def seconds_cell(r: dict | None, key: str) -> str:
    if r is None:
        return "—"
    if "error" in r:
        return "fails"
    value = r.get(key)
    return "—" if value is None else f"{value:.3f} s"


def summary(designs: list[str], counts: dict, tools: list[str], results: dict,
            info: dict) -> str:
    head = "| Design | Count | " + " | ".join(NAMES[t] for t in tools) + " |"
    rule = "| --- | --- |" + " --- |" * len(tools)
    out = ["Simulation seconds (median of the runs with the count, less the median",
           "of the runs with +n=1):", "", head, rule]
    for d in designs:
        out.append(f"| {d} | {counts[d]:,} | "
                   + " | ".join(seconds_cell(results[t].get(d), "simulation_seconds")
                                for t in tools) + " |")
    out += ["", "Seconds of a run with +n=1 (start, reading the design, front end):", "",
            head, rule]
    for d in designs:
        out.append(f"| {d} | 1 | "
                   + " | ".join(seconds_cell(results[t].get(d), "base_seconds")
                                for t in tools) + " |")
    out += ["", "Build seconds (once):", "", head, rule]
    for d in designs:
        out.append(f"| {d} | | "
                   + " | ".join(seconds_cell(results[t].get(d), "build_seconds")
                                for t in tools) + " |")
    out += ["", "Maximum RSS of a run with the count (MiB, median):", "", head, rule]
    for d in designs:
        cells = []
        for t in tools:
            r = results[t].get(d)
            cells.append("—" if r is None or "error" in r else f"{r['max_rss_mib']:.0f}")
        out.append(f"| {d} | {counts[d]:,} | " + " | ".join(cells) + " |")
    out += ["", "Versions:"] + [f"- {NAMES[t]}: {info['versions'][t]}" for t in tools]
    m = info["machine"]
    out += ["", f"Machine: {m['cpu']}, {m['cores']} cores, {m['memory_gib']} GiB, {m['os']}; "
            f"load average before the runs {m['load_average']}; {info['runs']} runs each."]
    return "\n".join(out) + "\n"


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("--kirimasim", help="the kirimasim executable (a release build)")
    ap.add_argument("--iverilog", help="Icarus Verilog's iverilog; vvp must be next to it")
    ap.add_argument("--verilator", help="the verilator executable")
    ap.add_argument("--runs", type=int, default=5, help="runs with each count (default 5)")
    ap.add_argument("--n", type=int, help="run every design with this count instead")
    ap.add_argument("--only", nargs="*", help="names of the designs to run")
    ap.add_argument("--out", required=True, type=Path, help="directory for the results")
    args = ap.parse_args()

    tools = [t for t in NAMES if getattr(args, t if t != "icarus" else "iverilog")]
    if not tools:
        ap.error("give at least one of --kirimasim, --iverilog and --verilator")
    # The runs start in a temporary directory, so paths must be absolute.
    exes = {t: (str(Path(e).resolve()) if e and os.sep in e else e)
            for t, e in (("kirimasim", args.kirimasim), ("icarus", args.iverilog),
                         ("verilator", args.verilator))}
    designs = sorted(p.stem for p in BENCH.glob("*.sv"))
    if args.only:
        designs = [d for d in designs if d in args.only]

    info = {"machine": machine(), "runs": args.runs,
            "versions": {"kirimasim": text_of([exes["kirimasim"], "--version"]) if args.kirimasim
                         else None,
                         "icarus": text_of([exes["icarus"], "-V"]) if args.iverilog else None,
                         "verilator": text_of([exes["verilator"], "--version"])
                         if args.verilator else None}}
    counts, results, failed = {}, {t: {} for t in tools}, False
    for d in designs:
        n, expect = header(BENCH / f"{d}.sv")
        if args.n is not None:
            n, expect = args.n, None
        counts[d] = n
        for t in tools:
            r = measure(t, exes[t], BENCH / f"{d}.sv", n, expect, args.runs)
            results[t][d] = r
            if "error" in r:
                failed = True
                print(f"{d}: {NAMES[t]}: {r['error']}", file=sys.stderr)

    args.out.mkdir(parents=True, exist_ok=True)
    (args.out / "results.json").write_text(json.dumps(
        {"info": info, "counts": counts, "results": results}, indent=1) + "\n")
    text = summary(designs, counts, tools, results, info)
    (args.out / "summary.md").write_text(text)
    print(text, end="")
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
