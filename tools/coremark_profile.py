#!/usr/bin/env python3
# Copyright (c) 2026 The kirimasim Authors
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

"""Replay the pinned one-iteration CoreMark inputs and optionally build the reference."""

import argparse
from concurrent.futures import ThreadPoolExecutor, TimeoutError as FutureTimeout
from datetime import datetime
from hashlib import file_digest
import json
import os
from pathlib import Path
import re
import signal
from statistics import median
import subprocess
import sys
import time


WORKLOADS = {"ibex": (433198, 443198), "cva6": (1051805, 3000000)}
FIRMWARE_SHA256 = "6af83d1f86eec882a9ef5791872d7520b82dc86367bab922d60a8277b8804e98"
CRCS = ("e9f5", "e714", "1fd7", "8e3a", "e714")
FAST_OPTIONS = ("-O3", "--x-assign", "fast", "--x-initial", "fast", "--no-assert")


def digest(path):
    with path.open("rb") as stream:
        return file_digest(stream, "sha256").hexdigest()


def check_output(stdout, cycles):
    fields = re.findall(r"^(?:seedcrc|\[0\]crc\w+)\s+: 0x([0-9a-f]+)$", stdout, re.M)
    exits = re.findall(r"^firmware exit (\d+) cycles (\d+)$", stdout, re.M)
    if (tuple(fields) != CRCS or exits != [("0", str(cycles))]
            or not re.search(r"^Iterations\s+: 1$", stdout, re.M)
            or "Correct operation validated." not in stdout):
        raise ValueError(f"CoreMark output disagrees with the recorded result at {cycles} cycles")


def run(command, cwd, stem, cycles, timeout, reference=False):
    prefix = ["/usr/bin/time", "-lp"] if sys.platform == "darwin" else []
    output = cwd / f"{stem}.stdout"
    diagnostic = cwd / f"{stem}.stderr"
    load_before = os.getloadavg()
    argv = [*prefix, *command]
    with output.open("w") as stdout, diagnostic.open("w") as stderr, ThreadPoolExecutor(1) as pool:
        started = time.perf_counter()
        with subprocess.Popen(argv, cwd=cwd, stdout=stdout, stderr=stderr,
                              start_new_session=True) as proc:
            # With file output, wait(timeout) can poll in 50 ms steps on POSIX.
            # Block on child exit and apply the deadline to the notification.
            exited = pool.submit(proc.wait)
            try:
                status = exited.result(timeout=timeout)
                wall = time.perf_counter() - started
            except (FutureTimeout, KeyboardInterrupt) as error:
                try:
                    os.killpg(proc.pid, signal.SIGKILL)
                except ProcessLookupError:
                    pass
                if isinstance(error, FutureTimeout):
                    raise subprocess.TimeoutExpired(argv, timeout) from error
                raise
            if status:
                raise subprocess.CalledProcessError(status, argv)
    load_after = os.getloadavg()
    transcript = output.read_text()
    check_output(transcript, cycles)
    outcome = {"wall_s": wall, "load_before": load_before, "load_after": load_after}
    if reference:
        banner = re.search(r"^.*: (Verilator 5\.054\b[^\n]*)$", transcript, re.M)
        if not banner or not re.search(r"^- Verilator: cpu .* on 1 threads;", transcript, re.M):
            raise ValueError("Expected a Verilator 5.054 reference running one simulation thread")
        outcome["reference_version"] = banner[1]
    if prefix:
        for label in ("instructions retired", "cycles elapsed"):
            match = re.search(rf"^\s*(\d+)  {label}$", diagnostic.read_text(), re.M)
            if match:
                outcome[label.replace(" ", "_")] = int(match[1])
    return outcome


def build_references(args, output):
    source_option_file = args.verilator_args.resolve()
    source_options = json.loads(source_option_file.read_text())
    if not isinstance(source_options, list) or not all(isinstance(opt, str) for opt in source_options):
        raise ValueError("Verilator input options must be a JSON array of arguments")
    for opt in ("--Mdir", "--threads", "--top-module", "--binary", "--timing"):
        if any(arg == opt or arg.startswith(opt + "=") for arg in source_options):
            raise ValueError(f"{opt} is set by the replay script")
    version = subprocess.check_output([args.verilator, "--version"], text=True).strip()
    if not version.startswith("Verilator 5.054 "):
        raise ValueError(f"Expected Verilator 5.054, found {version}")
    binaries = {}
    for variant, tuning in (("default", ()), ("speed", FAST_OPTIONS)):
        model = output / f"model-{variant}"
        model.mkdir()
        command = [args.verilator, "--binary", "--timing", "--threads", "1", "-Wno-fatal",
                   "--top-module", "top", "--Mdir", str(model), *source_options, *tuning]
        with (output / f"build-{variant}.log").open("w") as log:
            subprocess.run(command, cwd=source_option_file.parent, stdout=log, stderr=subprocess.STDOUT,
                           timeout=args.timeout, check=True)
        binaries[f"verilator-{variant}"] = model / "Vtop"
    return binaries, version


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("workload", choices=WORKLOADS)
    p.add_argument("--kirimasim", type=Path, required=True)
    p.add_argument("--sir", type=Path, required=True)
    p.add_argument("--firmware", type=Path, required=True)
    p.add_argument("--out", type=Path, required=True)
    p.add_argument("--cxx", default="c++")
    p.add_argument("--runs", type=int, default=5)
    p.add_argument("--timeout", type=int, default=3000)
    p.add_argument("--states", type=int, choices=(2, 4), nargs="+")
    p.add_argument("--diagnostic", action="store_true")
    p.add_argument("--verilator", default="verilator")
    reference = p.add_mutually_exclusive_group(required=True)
    reference.add_argument("--verilator-args", type=Path,
                           help="JSON array of pinned input flags and source paths relative to this file")
    reference.add_argument("--reference", type=Path, help="prepared single-threaded Verilator 5.054 Vtop")
    args = p.parse_args()
    if args.runs < 1 or args.timeout < 1:
        p.error("runs and timeout must be positive")
    cycles, limit = WORKLOADS[args.workload]
    states = args.states or ([2, 4] if args.workload == "ibex" else [2])
    binary, sir, firmware = (path.resolve() for path in (args.kirimasim, args.sir, args.firmware))
    if digest(firmware) != FIRMWARE_SHA256:
        raise ValueError("Unexpected CoreMark firmware digest")
    output = args.out.resolve()
    output.mkdir(parents=True)
    plusargs = [f"+image={firmware}", f"+cycles={limit}"]
    refs, ref_version = (build_references(args, output) if args.verilator_args else
                         ({"verilator-prepared": args.reference.resolve()}, None))
    native = {f"native-{state}": [str(binary), "--tier=native", f"--state={state}", f"--cxx={args.cxx}",
                                 "--max-scc-iter=65536", f"--load-sir={sir}",
                                 f"--cache-dir={output / f'cache-{state}'}"] for state in states}
    commands = {**native, **{name: [str(path)] for name, path in refs.items()}}
    record = {
        "workload": args.workload,
        "started": datetime.now().astimezone().isoformat(),
        "cycles": cycles,
        "crcs": CRCS,
        "versions": {"kirimasim": subprocess.check_output([binary, "--version"], text=True).strip(),
                     "cxx": subprocess.check_output([args.cxx, "--version"], text=True).splitlines()[0],
                     "verilator": ref_version},
        "sha256": {"sir": digest(sir), "firmware": FIRMWARE_SHA256, "kirimasim": digest(binary),
                   **{name: digest(path) for name, path in refs.items()}},
        "options": {"native": ["--tier=native", "--max-scc-iter=65536", "--load-sir=<SIR>",
                               "--cache-dir=<cache>", "+image=<firmware>", f"+cycles={limit}"],
                    "states": states, "verilator_speed": FAST_OPTIONS if ref_version else None},
        "warmup": {},
        "samples": [],
        "diagnostic": {},
    }

    def save():
        temporary = output / "results.tmp"
        temporary.write_text(json.dumps(record, indent=2) + "\n")
        temporary.replace(output / "results.json")

    def execute(name, stem, extra=()):
        return run([*commands[name], *extra, *plusargs], output, stem, cycles, args.timeout,
                   reference=name in refs)

    def cache_signature():
        return {str(path.relative_to(output)): (digest(path), path.stat().st_mtime_ns)
                for path in output.glob("cache-*/native-v1/*/model.so")}

    for name in commands:
        record["warmup"][name] = execute(name, f"warmup-{name}")
        save()
    warmed = cache_signature()
    if not warmed:
        raise ValueError("The native warmup produced no compiled module")
    for index in range(args.runs):
        order = list(commands)[::(-1 if index % 2 else 1)]
        record["samples"].append({name: execute(name, f"run-{index+1}-{name}") for name in order})
        save()
    if args.diagnostic:
        for name in native:
            record["diagnostic"][name] = execute(name, f"diag-{name}", (
                "--stats=json", f"--report-tier={output / (name + '.jsonl')}",
                f"--report-islands={output / (name + '.islands')}",
            ))
            save()
    if cache_signature() != warmed:
        raise ValueError("The generated module changed after cache preparation")
    record["medians_s"] = {name: median(row[name]["wall_s"] for row in record["samples"])
                           for name in commands}
    record["finished"] = datetime.now().astimezone().isoformat()
    save()
    print(json.dumps(record["medians_s"], indent=2))


if __name__ == "__main__":
    main()
