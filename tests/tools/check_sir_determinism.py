#!/usr/bin/env python3
# Copyright (c) 2026 The kirimasim Authors
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
"""The front makes the same SIR from the same design (P2 review L4).

The cache of generated code and the separate analysis tables are keyed by the content of
SIR (RUNTIME_ABI.md 2; docs/notes/sir-v1-proposal.md D6), while SIR_SPEC.md 9 promises only
the same text from the same SIR. Each design here is turned into SIR twice, in two separate
processes, and the two format-0 texts must be byte for byte the same. The designs are those
of tests/dump_sir and every <stride>-th design of the own test suite, in sorted order. The
tests of tests/dump_sir also compare the text with an expected file on every system that runs
them, which covers the same design across systems.

A stand-in that writes a different text each time checks the comparison first.
"""
import argparse
import concurrent.futures as cf
import re
import subprocess
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent.parent


def dump(binary: str, design: Path) -> subprocess.CompletedProcess:
    header = design.read_text(encoding="utf-8", errors="replace")
    top = re.search(r"^:top_module:\s*(\S+)", header, re.M)
    command = [binary, "--dump-sir", *([f"--top={top.group(1)}"] if top else []), design.name]
    return subprocess.run(command, cwd=design.parent, capture_output=True, stdin=subprocess.DEVNULL, timeout=120)


def compare(binary: str, design: Path):
    """None when the design does not make SIR, else whether both texts are the same."""
    first = dump(binary, design)
    if first.returncode != 0:
        return None
    second = dump(binary, design)
    return second.returncode == 0 and first.stdout == second.stdout


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--kirimasim", required=True)
    parser.add_argument("--stride", type=int, default=10)
    args = parser.parse_args()
    failures = []

    with tempfile.TemporaryDirectory() as directory:
        stand_in = Path(directory) / "stand_in"
        stand_in.write_text(f"#!{sys.executable}\nimport os\nprint(os.urandom(8).hex())\n", encoding="utf-8")
        stand_in.chmod(0o755)
        design = Path(directory) / "design.sv"
        design.write_text("module top;\nendmodule\n", encoding="utf-8")
        if compare(str(stand_in), design) is not False:
            failures.append("the comparison does not see two different texts")

    designs = sorted((ROOT / "tests/dump_sir").glob("*.sv"))
    suite = sorted(p for p in (ROOT / "tests/suite").rglob("*.sv"))
    designs += suite[::args.stride]
    with cf.ThreadPoolExecutor(4) as pool:
        results = list(pool.map(lambda d: (d, compare(args.kirimasim, d)), designs))
    compared = [(d, same) for d, same in results if same is not None]
    for design, same in compared:
        if not same:
            failures.append(f"two different SIR texts: {design.relative_to(ROOT)}")
    if len(compared) < len(designs) // 2:
        failures.append(f"only {len(compared)} of {len(designs)} designs made SIR")
    for failure in failures:
        print("FAIL: " + failure, file=sys.stderr)
    if failures:
        return 1
    print(f"SIR determinism: {len(compared)} designs give the same text twice ({len(designs) - len(compared)} make no SIR)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
