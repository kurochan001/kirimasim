#!/usr/bin/env python3
# Copyright (c) 2026 The kirimasim Authors
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

"""Check bounds warnings and unchanged values/counters (CLI.md 2.2)."""

import argparse
import json
import re
import sys
import tempfile
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE.parent.parent / "tools"))
import bounded_run  # noqa: E402


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--kirimasim", type=Path, required=True)
    args = parser.parse_args()
    exe = str(args.kirimasim.resolve())
    files = sorted((HERE / "bounds_matrix").glob("*.sv"))
    failures = []

    def check(source):
        errors = []
        expected = source.with_suffix(".expected").read_text()
        locations = source.with_suffix(".warnings").read_text().splitlines()
        reference = None
        with tempfile.TemporaryDirectory() as td:
            for mode in ((), ("--bounds-check=off",), ("--bounds-check=warn",)):
                p = bounded_run.run([exe, "--stats=json", *mode, str(source)], cwd=td, timeout=30)
                warns = re.findall(r"[^\n]*:(\d+:\d+): warning: ([^\n]+)", p.stderr)
                want = locations if mode == ("--bounds-check=warn",) else []
                counts = [json.loads(line) for line in p.stdout.splitlines() if line.startswith("{")]
                design = "\n".join(line for line in p.stdout.splitlines()
                                   if not line.startswith("{")) + "\n"
                summary = re.findall(r"bounds check: (\d+) invalid accesses at (\d+) source positions",
                                     p.stderr)
                summary_want = ([(str(28 if source.stem.endswith("_nba") else 25),
                                  str(len(locations)))] if want else [])
                if (p.returncode != 0 or design != expected or
                        [where for where, _ in warns] != want or
                        summary != summary_want or
                        any("--bounds-check=warn" not in text for _, text in warns) or
                        len(counts) != 1 or (reference is not None and counts != reference)):
                    errors.append(f"{source.name} {mode}: {p.returncode}\n{p.stdout}\n{p.stderr}"
                                  f"\nexpected warning positions: {want}")
                reference = counts if reference is None else reference
        return errors

    with ThreadPoolExecutor(max_workers=4) as pool:
        for errors in pool.map(check, files):
            failures.extend(errors)
    source = str(files[0]) if files else "missing.sv"
    for value in ("", "error", "yes", "WARN"):
        p = bounded_run.run([exe, "--bounds-check=" + value, source], timeout=30)
        if p.returncode != 2 or p.stdout or "needs off or warn" not in p.stderr:
            failures.append(f"bad option {value}: {p.returncode}\n{p.stdout}\n{p.stderr}")
    if not files:
        failures.append("no bounds matrix fixtures")
    if failures:
        print("\n".join(failures), file=sys.stderr)
        return 1
    print(f"bounds check: {len(files)} designs x default/off/warn, unchanged values/counters; "
          "4 invalid-option cases passed")
    return 0


if __name__ == "__main__":
    sys.exit(main())
