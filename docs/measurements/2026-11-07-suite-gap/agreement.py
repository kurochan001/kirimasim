#!/usr/bin/env python3
# Copyright (c) 2026 The kirimasim Authors
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
"""Of the tests where both controls differ from the expected value, those where
the two controls give one output.

  python3 agreement.py --kirimasim <checkout> --both-differ <both_differ.txt>
      --iverilog-bin <folder> --verilator <program> --out <file> [-j N]

Two simulators that agree do not settle what the standard requires, and the
kirimasim-tests repository records cases where both releases are wrong. These
tests are the first to read when the expected values are reviewed, nothing more.
"""

import argparse
import concurrent.futures
import os
import subprocess
import tempfile
from pathlib import Path

from measure import SIM_TIMEOUT, VERILATOR_LINE, limits, normalized, split_test


def output_of(command: list[str], folder: str, keep=lambda line: True) -> list[str] | None:
    sink_path = Path(folder) / "stdout.txt"
    try:
        with sink_path.open("wb") as sink:
            subprocess.run(
                command, stdout=sink, stderr=subprocess.DEVNULL, stdin=subprocess.DEVNULL,
                cwd=folder, timeout=SIM_TIMEOUT, preexec_fn=limits,
            )
    except subprocess.TimeoutExpired:
        return None
    lines = sink_path.read_text(errors="replace").splitlines()
    return normalized("\n".join(line for line in lines if keep(line)))


def compare(job: tuple[str, str, str, str]) -> tuple[str, str]:
    path, root, bindir, verilator = job
    test = Path(root) / path
    meta, _ = split_test(test.read_text(encoding="utf-8", errors="ignore"))
    tops = meta.get("top_module", "").split()
    args = meta.get("args", "").split()
    env = {k: v for k, v in os.environ.items() if k != "VERILATOR_ROOT"}
    env["OBJCACHE"] = "ccache"
    with tempfile.TemporaryDirectory() as tmp:
        subprocess.run(
            [f"{bindir}/iverilog", "-g2012", "-gspecify", "-ginterconnect",
             *[f"-s{t}" for t in tops], "-o", f"{tmp}/a.vvp", str(test)],
            capture_output=True, cwd=tmp,
        )
        icarus = output_of([f"{bindir}/vvp", f"{tmp}/a.vvp", *args], tmp)
        subprocess.run(
            [verilator, "--binary", "--timing", "--coverage-user", "-Wno-fatal", "-Wno-lint",
             "-Wno-style", "--x-assign", "unique", "--x-initial", "unique",
             *(["--top-module", tops[0]] if len(tops) == 1 else []),
             "-Mdir", f"{tmp}/obj", "--prefix", "Vt", str(test)],
            capture_output=True, cwd=tmp, env=env,
        )
        other = output_of(
            [f"{tmp}/obj/Vt", "+verilator+rand+reset+2", "+verilator+seed+1",
             "+verilator+error+limit+1000000", *args],
            tmp,
            keep=lambda line: not VERILATOR_LINE.match(line),
        )
    return path, "agree" if icarus is not None and icarus == other else "disagree"


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--kirimasim", type=Path, required=True)
    ap.add_argument("--both-differ", type=Path, required=True)
    ap.add_argument("--iverilog-bin", required=True)
    ap.add_argument("--verilator", required=True)
    ap.add_argument("--out", type=Path, required=True)
    ap.add_argument("-j", "--jobs", type=int, default=16)
    args = ap.parse_args()
    root = str(args.kirimasim.resolve())
    jobs = [
        (line.strip(), root, args.iverilog_bin, args.verilator)
        for line in args.both_differ.read_text().splitlines()
        if line.strip()
    ]
    with concurrent.futures.ThreadPoolExecutor(args.jobs) as pool:
        results = list(pool.map(compare, jobs))
    args.out.write_text("".join(f"{path}\n" for path, verdict in results if verdict == "agree"))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
