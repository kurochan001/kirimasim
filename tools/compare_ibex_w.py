#!/usr/bin/env python3
# Copyright (c) 2026 The kirimasim Authors
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
"""Replay the P4 entry, pre-W builds and fixed-store W on identical Ibex inputs."""

import argparse
import json
import os
from pathlib import Path
import platform
import re
import subprocess
import sys
import time

import coremark_profile as coremark


def legacy_timer(command, cwd, stem, method):
    """The recorded 4fa11d58 script used pipes; the older profile runner polled."""
    prefix = ["/usr/bin/time", "-lp"] if sys.platform == "darwin" else []
    argv = [*prefix, *command]
    before = os.getloadavg()
    started = time.perf_counter()
    if method == "pipes":
        proc = subprocess.run(argv, cwd=cwd, capture_output=True, text=True, check=True)
        wall = time.perf_counter() - started
        output, diagnostic = proc.stdout, proc.stderr
        (cwd / f"{stem}.stderr").write_text(diagnostic)
    else:
        with (cwd / f"{stem}.stdout").open("w") as out, (cwd / f"{stem}.stderr").open("w") as err:
            subprocess.run(argv, cwd=cwd, stdout=out, stderr=err, timeout=240, check=True)
        wall = time.perf_counter() - started
        output = (cwd / f"{stem}.stdout").read_text()
        diagnostic = (cwd / f"{stem}.stderr").read_text()
    after = os.getloadavg()
    coremark.check_output(output, coremark.WORKLOADS["ibex"][0])
    result = dict(wall_s=wall, load_before=before, load_after=after)
    retired = re.search(r"^\s*(\d+)  instructions retired", diagnostic, re.M)
    if retired:
        result["instructions_retired"] = int(retired[1])
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    names = ("entry", "historical", "before", "after", "reference")
    for key in (*names, "sir", "firmware", "out"):
        parser.add_argument("--" + key, type=Path, required=True)
    parser.add_argument("--runs", type=int, default=5)
    parser.add_argument("--cxx", default="c++")
    parser.add_argument("--resume", action="store_true",
                        help="retain qualifying samples and warm caches from this replay")
    parser.add_argument("--load-limit", type=float, default=(os.cpu_count() or 1) / 2)
    args = parser.parse_args()
    if args.load_limit <= 0:
        parser.error("--load-limit must be positive")
    if args.runs < 1:
        parser.error("--runs must be positive")
    if coremark.digest(args.firmware) != coremark.FIRMWARE_SHA256:
        parser.error("unexpected firmware")
    cwd = args.out.resolve()
    cwd.mkdir(parents=True, exist_ok=True)
    binaries = {name: getattr(args, name).resolve() for name in names}
    cycles, limit = coremark.WORKLOADS["ibex"]
    arguments = ["+image=" + str(args.firmware.resolve()), f"+cycles={limit}"]
    arms = [(name, 2, "exit") for name in names]
    arms += [(name, 4, "exit") for name in ("entry", "before", "after")]
    arms += [(name, 2, "pipes") for name in ("historical", "before")]
    arms.append(("before", 2, "poll"))
    report = {
        "runs": args.runs, "cycles": cycles, "crcs": coremark.CRCS,
        "os": platform.system() + " " + platform.release(), "architecture": platform.machine(),
        "cxx": subprocess.check_output([args.cxx, "--version"], text=True).splitlines()[0],
        "sha256": {k: coremark.digest(v) for k, v in
                   {**binaries, "sir": args.sir, "firmware": args.firmware}.items()},
        "warmup": [], "samples": [],
    }
    destination = cwd / "results.json"
    if args.resume:
        previous = json.loads(destination.read_text())
        if any(report[k] != previous[k] for k in ("sha256", "runs", "cxx")):
            parser.error("resume requires identical builds, inputs, compiler and run count")
        if any(not any((cwd / f"cache-{row['build']}-{row['state']}").glob("native-v1/*/model.so"))
               for row in previous["warmup"] if row["build"] != "reference"):
            parser.error("missing warmed module; use a fresh replay directory")
        report.update(previous)
    elif destination.exists():
        parser.error("output already contains a replay; use --resume or a fresh directory")
    def qualifies(row):
        return max(row[k][0] for k in ("load_before", "load_after")) <= args.load_limit
    rejected = report.setdefault("high_load", [])
    rejected.extend(row for row in report["samples"] if not qualifies(row))
    report["samples"] = [row for row in report["samples"] if qualifies(row)]
    report["load_limit"] = args.load_limit
    for index in range(args.runs + 1):
        first = index * 3 % len(arms)
        order = arms[first:] + arms[:first]
        for name, state, method in (reversed(order) if index % 2 else order):
            reference = name == "reference"
            cmd = [str(binaries[name])]
            if not reference:
                cmd.extend(["--tier=native", f"--state={state}", f"--cxx={args.cxx}",
                            f"--load-sir={args.sir.resolve()}", f"--cache-dir={cwd}/cache-{name}-{state}",
                            "--max-scc-iter=65536"])
            cmd.extend(arguments)
            bucket = report["samples" if index else "warmup"]
            identity = dict(round=index, build=name, state=state, timer=method)
            if any(all(row[k] == v for k, v in identity.items()) for row in bucket):
                continue
            while True:
                while os.getloadavg()[0] > args.load_limit:
                    time.sleep(5)
                stem = f"{index}-{name}-{state}-{method}-{len(rejected)}"
                if method == "exit":
                    result = coremark.run(cmd, cwd, stem, cycles, 240, reference=reference)
                else:
                    result = legacy_timer(cmd, cwd, stem, method)
                child = re.search(r"^real (\d+\.\d+)$", (cwd / f"{stem}.stderr").read_text(), re.M)
                if child:
                    result["time_real_s"] = float(child[1])
                accepted = not index or qualifies(result)
                (bucket if accepted else rejected).append({**identity, **result})
                destination.write_text(json.dumps(report, indent=2) + "\n")
                if accepted:
                    break



if __name__ == "__main__":
    main()
