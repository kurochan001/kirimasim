#!/usr/bin/env python3
# Copyright (c) 2026 The kirimasim Authors
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

"""Measures the simulation speed of kirimasim on the designs under bench/,
and of Icarus Verilog, Verilator, nvc and Lyra for comparison (PERFORMANCE.md 5).

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
with its default C++ options, then the built program. nvc is nvc -a, then
nvc -e of the design's top, then nvc -r. Lyra is lyra build with its
default backend, into an empty cache, then the built program; --lyra-llvm
does the same with --backend llvm, as a column of its own. Their build steps are timed once and reported
apart; kirimasim has no build step.

Writes results.json and summary.md to --out and prints the summary. They
record the tools' versions and the machine's CPU, cores, memory, OS and load
average, and no host name or user name (PERFORMANCE.md 5.3). results.json
also keeps every run: its seconds, maximum RSS, exit code, whether it reached
the time limit, and its result line. A run with +n=1 has to print the line
for n=1, so that a tool that does not pass the count on is found. A design
that fails keeps the step it failed at (build, warmup, small, large or
output) and the end of that step's messages. The exit code is 1 when a step
fails or a result line differs from the expected one.

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
NAMES = {"kirimasim": "kirimasim", "icarus": "Icarus Verilog", "verilator": "Verilator",
         "nvc": "nvc", "lyra": "Lyra", "lyra_llvm": "Lyra (LLVM backend)"}
# The option that names each tool's executable.
OPTIONS = {"kirimasim": "kirimasim", "icarus": "iverilog", "verilator": "verilator",
           "nvc": "nvc", "lyra": "lyra", "lyra_llvm": "lyra_llvm"}
# How much of a failing step's messages results.json keeps.
TAIL = 4000


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


def watch_peak(pid: int, name: str, done: threading.Event, peak: list) -> None:
    """Keeps in peak[0] the last VmHWM, in KiB, of the process once it runs
    the program called name. On Linux, ru_maxrss of a child includes the
    memory of the Python process it was started from, until it ran the
    program, so a program smaller than this script shows this script's size;
    VmHWM counts the program alone. The samples before the program runs, when
    the process is still a copy of Python, are left out by its name."""
    status = Path(f"/proc/{pid}/status")
    while not done.is_set():
        try:
            text = status.read_text(errors="ignore")
        except OSError:
            return
        comm = re.search(r"^Name:\s*(.*)$", text, re.M)
        hwm = re.search(r"^VmHWM:\s*(\d+) kB", text, re.M)
        if comm and hwm and comm.group(1) == name[:15]:
            peak[0] = int(hwm.group(1))
        done.wait(0.01)


def run(cmd: list[str], cwd: Path, env: dict | None = None) -> dict:
    """Runs cmd; returns its seconds, its maximum RSS in MiB, its exit code,
    whether it was stopped at the time limit, its standard output and the
    end of its standard error. On Linux the RSS is the program's VmHWM, or
    None for a run too short to sample; on macOS it is ru_maxrss."""
    stopped, done, peak = threading.Event(), threading.Event(), [None]
    linux = sys.platform.startswith("linux")
    with tempfile.TemporaryFile() as err:
        start = time.monotonic()
        p = subprocess.Popen(cmd, cwd=cwd, stdout=subprocess.PIPE, stderr=err, text=True,
                             env=env)

        def stop() -> None:
            stopped.set()
            p.kill()

        timer = threading.Timer(TIMEOUT, stop)
        timer.start()
        watcher = None
        if linux:
            watcher = threading.Thread(target=watch_peak,
                                       args=(p.pid, Path(cmd[0]).name, done, peak))
            watcher.start()
        out = p.stdout.read()
        _, status, usage = os.wait4(p.pid, 0)
        seconds = time.monotonic() - start
        done.set()
        timer.cancel()
        if watcher is not None:
            watcher.join()
        p.returncode = 0  # reaped by wait4 above
        p.stdout.close()
        err.seek(0)
        messages = err.read().decode(errors="replace")
    rc = os.WEXITSTATUS(status) if os.WIFEXITED(status) else 128 + os.WTERMSIG(status)
    if linux:
        rss = None if peak[0] is None else peak[0] / (1 << 10)
    else:
        rss = usage.ru_maxrss / (1 << 20)  # in bytes on macOS
    return {"seconds": seconds, "max_rss_mib": rss, "rss_source": "VmHWM" if linux
            else "ru_maxrss", "exit_code": rc, "timed_out": stopped.is_set(),
            "stdout": out, "stderr": messages}


def scrub(text: str, work: Path) -> str:
    """Puts names in place of the directories of this machine, so that
    results.json holds no home directory or user name (PERFORMANCE.md 5.3)."""
    return (text.replace(str(work), "${WORK}").replace(str(ROOT), "${PROJECT_ROOT}")
            .replace(str(Path.home()), "${HOME}"))


def build_steps(steps: list[list[str]], work: Path, env: dict | None = None) -> dict:
    """Runs the build steps in order, up to the first that fails. Returns
    their total seconds and, for a failure, the step and its messages."""
    record = {"seconds": 0.0, "steps": []}
    for cmd in steps:
        r = run(cmd, work, env)
        record["seconds"] += r["seconds"]
        record["steps"].append({"command": [Path(cmd[0]).name]
                                + [scrub(a, work) for a in cmd[1:]],
                                "seconds": r["seconds"], "exit_code": r["exit_code"],
                                "timed_out": r["timed_out"]})
        if r["exit_code"] != 0 or r["timed_out"]:
            record["error"] = (f"{Path(cmd[0]).name} "
                               + ("reached the time limit" if r["timed_out"]
                                  else f"exit code {r['exit_code']}"))
            record["diagnostics"] = scrub((r["stdout"] + r["stderr"])[-TAIL:], work)
            break
    return record


def prepare(tool: str, exe: str, design: Path, work: Path):
    """Builds the design for the tool. Returns the build record (None for
    kirimasim) and a function from a count to the command that runs it."""
    top = design.stem
    if tool == "kirimasim":
        return None, lambda n: [exe, str(design), f"+n={n}"]
    if tool == "icarus":
        compiled = work / "bench.vvp"
        build = build_steps([[exe, "-g2012", "-o", str(compiled), str(design)]], work)
        vvp = str(Path(exe).with_name("vvp"))
        return build, lambda n: [vvp, "-n", str(compiled), f"+n={n}"]
    if tool == "nvc":
        build = build_steps([[exe, "-a", str(design)], [exe, "-e", top]], work)
        return build, lambda n: [exe, "-r", top, f"+n={n}"]
    if tool in ("lyra", "lyra_llvm"):
        program = work / "bench_lyra"
        # An empty cache of its own, so that the build is a whole compile and
        # nothing from another run is reused.
        backend = ["--backend", "llvm"] if tool == "lyra_llvm" else []
        build = build_steps([[exe, "build", "--top", top, *backend, "--cache-dir",
                              str(work / "cache"), "-o", str(program), str(design)]], work)
        return build, lambda n: [str(program), f"+n={n}"]
    build = build_steps([[exe, "--binary", "--timing", "-Wno-fatal", "-Wno-lint",
                          "-Wno-style", "--build-jobs", "0", "-Mdir", str(work / "obj"),
                          "--prefix", "Vbench", str(design)]], work)
    program = str(work / "obj" / "Vbench")
    return build, lambda n: [program, f"+n={n}"]


def result_line(name: str, out: str) -> str | None:
    return next((line for line in out.splitlines() if line.startswith(name + " ")), None)


def sample(name: str, r: dict) -> dict:
    """What results.json keeps of a run."""
    return {"seconds": r["seconds"], "max_rss_mib": r["max_rss_mib"],
            "rss_source": r["rss_source"],
            "exit_code": r["exit_code"], "timed_out": r["timed_out"],
            "result_line": result_line(name, r["stdout"])}


def median_or_none(values) -> float | None:
    known = [v for v in values if v is not None]
    return statistics.median(known) if known else None


def failure(record: dict, stage: str, error: str, r: dict, work: Path) -> dict:
    record.update({"error": error, "stage": stage,
                   "diagnostics": scrub((r["stdout"] + r["stderr"])[-TAIL:], work)})
    return record


def measure(tool: str, exe: str, design: Path, n: int, expect: str | None,
            runs: int) -> dict:
    name = design.stem
    with tempfile.TemporaryDirectory() as tmp:
        work = Path(tmp)
        build, command = prepare(tool, exe, design, work)
        record = {"build_seconds": None if build is None else build["seconds"],
                  "build": build}
        if build is not None and "error" in build:
            record.update({"error": build["error"], "stage": "build",
                           "diagnostics": build["diagnostics"]})
            return record
        warm = run(command(1), work)  # the first start of a newly built program
        small, large = [], []
        for _ in range(runs):
            small.append(run(command(1), work))
            large.append(run(command(n), work))
    record["runs"] = {"warmup": [sample(name, warm)],
                      "small": [sample(name, r) for r in small],
                      "large": [sample(name, r) for r in large]}
    for stage, rs in (("warmup", [warm]), ("small", small), ("large", large)):
        for r in rs:
            if r["timed_out"]:
                return failure(record, stage, "reached the time limit", r, work)
            if r["exit_code"] != 0:
                return failure(record, stage, f"exit code {r['exit_code']}", r, work)
    # A run with +n=1 has to say n=1: a tool that drops the plusarg would
    # otherwise time the same work twice.
    for r in [warm] + small:
        line = result_line(name, r["stdout"])
        if line is None or not line.startswith(f"{name} n=1 "):
            return failure(record, "output", f"result line {line!r} of a run with +n=1", r,
                           work)
    for r in large:
        line = result_line(name, r["stdout"])
        if line is None or (expect is not None and line != expect):
            return failure(record, "output", f"result line {line!r}, expected {expect!r}", r,
                           work)
    base = statistics.median(r["seconds"] for r in small)
    whole = statistics.median(r["seconds"] for r in large)
    record.update({"base_seconds": base, "whole_seconds": whole,
                   "simulation_seconds": whole - base,
                   "max_rss_mib": median_or_none(r["max_rss_mib"] for r in large),
                   "all_seconds": [[r["seconds"] for r in small],
                                   [r["seconds"] for r in large]]})
    return record


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
        release = Path("/etc/os-release")
        name = re.search(r'^PRETTY_NAME="?([^"\n]*)', release.read_text(errors="ignore"),
                         re.M) if release.exists() else None
        info["distribution"] = name.group(1) if name else "unknown"
    return info


def seconds_cell(r: dict | None, key: str) -> str:
    if r is None:
        return "—"
    if "error" in r:
        return f"fails ({r.get('stage', 'unknown')})"
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
            cells.append("—" if r is None or "error" in r or r["max_rss_mib"] is None
                         else f"{r['max_rss_mib']:.1f}")
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
    ap.add_argument("--nvc", help="the nvc executable")
    ap.add_argument("--lyra", help="the lyra executable (its default backend)")
    ap.add_argument("--lyra-llvm", help="the lyra executable, run with --backend llvm")
    ap.add_argument("--lyra-version", help="what to record as lyra's version (its commit)")
    ap.add_argument("--runs", type=int, default=5, help="runs with each count (default 5)")
    ap.add_argument("--n", type=int, help="run every design with this count instead")
    ap.add_argument("--only", nargs="*", help="names of the designs to run")
    ap.add_argument("--out", required=True, type=Path, help="directory for the results")
    args = ap.parse_args()

    tools = [t for t in NAMES if getattr(args, OPTIONS[t])]
    if not tools:
        ap.error("give at least one of " + ", ".join("--" + o for o in OPTIONS.values()))
    # The runs start in a temporary directory, so paths must be absolute.
    exes = {t: (str(Path(e).resolve()) if e and os.sep in e else e)
            for t, e in ((t, getattr(args, OPTIONS[t])) for t in NAMES)}
    designs = sorted(p.stem for p in BENCH.glob("*.sv"))
    if args.only:
        designs = [d for d in designs if d in args.only]

    # lyra has no --version, so --lyra-version says which commit was built.
    version_flag = {"icarus": "-V"}
    versions = {t: text_of([exes[t], version_flag.get(t, "--version")])
                if exes[t] and not t.startswith("lyra") else None for t in NAMES}
    for t in ("lyra", "lyra_llvm"):
        if exes[t]:
            versions[t] = args.lyra_version or "not recorded (lyra has no --version)"
    info = {"machine": machine(), "runs": args.runs, "timeout_seconds": TIMEOUT,
            "versions": versions}
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
