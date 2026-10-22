#!/usr/bin/env python3
# Copyright (c) 2026 The kirimasim Authors
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

"""Time the frozen P1 population using kirimasim-tests' existing judges.

This is a single run for test-runner overhead, not a simulation benchmark.
Use tools/bench.py for simulation speed. --runner-repo supplies the current
kirimasim-tests runners; --tests-repo supplies the frozen test snapshot.
"""

import argparse
import concurrent.futures
import hashlib
import json
from pathlib import Path
import subprocess
import sys
import threading
import time
from types import SimpleNamespace

ROOT = Path(__file__).resolve().parent.parent


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--tests-repo", required=True, type=Path)
    ap.add_argument("--runner-repo", required=True, type=Path)
    ap.add_argument("--kirimasim", required=True, type=Path)
    ap.add_argument("--iverilog", required=True, type=Path)
    ap.add_argument("--list", type=Path, default=ROOT / "tests/gates/p1_population.txt")
    ap.add_argument("--jobs", type=int, default=6)
    ap.add_argument("--out", required=True, type=Path)
    args = ap.parse_args()
    if args.jobs < 1:
        ap.error("--jobs must be positive")
    runner_dir = args.runner_repo.resolve() / "tools"
    sys.path.insert(0, str(runner_dir))
    import run_icarus
    import run_kirimasim

    kirimasim = str(args.kirimasim.resolve())
    iverilog = args.iverilog.resolve()
    local = threading.local()

    def timed_run(command, *pos, **kw):
        exe = str(command[0])
        # Same subtraction as tools/svtests.py: reading/elaboration/SIR is
        # estimated with --dump-sir, then subtracted from the whole run.
        build = 0.0
        if exe == kirimasim:
            start = time.monotonic()
            subprocess.run([command[0], "--dump-sir", *command[1:]], *pos, **kw)
            build = time.monotonic() - start
        start = time.monotonic()
        result = subprocess.run(command, *pos, **kw)
        elapsed = time.monotonic() - start
        if exe == kirimasim:
            local.times["build_seconds"] += build
            local.times["run_seconds"] += max(0.0, elapsed - build)
        elif Path(exe).name in ("iverilog", "iverilog-vpi"):
            local.times["build_seconds"] += elapsed
        elif Path(exe).name == "vvp":
            local.times["run_seconds"] += elapsed
        return result

    # Each runner gets a proxy; the shared subprocess module stays intact.
    proxy = SimpleNamespace(run=timed_run, TimeoutExpired=subprocess.TimeoutExpired)
    run_kirimasim.subprocess = proxy
    run_icarus.subprocess = proxy
    names = [s.strip() for s in args.list.read_text().splitlines()
             if s.strip() and not s.startswith("#")]
    tests = args.tests_repo.resolve() / "tests"

    def one(name):
        test = tests / (name + ".sv")
        row = {"test": name}
        for tool in ("kirimasim", "icarus"):
            local.times = {"build_seconds": 0.0, "run_seconds": 0.0}
            if tool == "kirimasim":
                status, _ = run_kirimasim.run(test, kirimasim)
                passed = status == "pass"
            else:
                passed, _ = run_icarus.run(test, iverilog.parent)
            row[tool] = {"pass": passed, **local.times}
        return row

    with concurrent.futures.ThreadPoolExecutor(max_workers=args.jobs) as pool:
        rows = list(pool.map(one, names))
    runner_names = ("run_kirimasim.py", "run_icarus.py", "suite_results.py",
                    "normalize_output.py")
    data = {"jobs": args.jobs, "runs": 1,
            "population_header": args.list.read_text().splitlines()[0],
            "runner_sha256": {n: hashlib.sha256((runner_dir / n).read_bytes()).hexdigest()
                              for n in runner_names},
            "results": rows}
    args.out.parent.mkdir(parents=True, exist_ok=True)
    args.out.write_text(json.dumps(data, indent=1) + "\n")
    for label, selected in (("all", rows), ("both pass", [r for r in rows
                              if r["kirimasim"]["pass"] and r["icarus"]["pass"]])):
        print(f"{label}: {len(selected)} tests")
        for tool in ("kirimasim", "icarus"):
            build = sum(r[tool]["build_seconds"] for r in selected)
            run = sum(r[tool]["run_seconds"] for r in selected)
            print(f"  {tool}: build {build:.3f}s, run {run:.3f}s, total {build + run:.3f}s")
    return 0


if __name__ == "__main__":
    sys.exit(main())
