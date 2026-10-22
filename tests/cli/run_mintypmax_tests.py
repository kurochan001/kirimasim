#!/usr/bin/env python3
# Copyright (c) 2026 The kirimasim Authors
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

"""Check --mintypmax in parsing, elaboration and simulation (CLI.md 2.2)."""

import argparse
import sys
import tempfile
from pathlib import Path

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE.parent.parent / "tools"))
import bounded_run  # noqa: E402


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--kirimasim", type=Path, required=True)
    args = parser.parse_args()
    exe = str(args.kirimasim.resolve())
    design = str(HERE / "mintypmax_matrix.sv")
    failures = []
    with tempfile.TemporaryDirectory() as tmp:
        def run(options):
            return bounded_run.run([exe, *options, design], cwd=tmp, timeout=30)

        for mode in (None, "min", "typ", "max"):
            options = [] if mode is None else [f"--mintypmax={mode}"]
            expected = (HERE / f"mintypmax_matrix.{mode or 'typ'}.expected").read_text()
            result = run(options)
            if result.returncode != 0 or result.stdout != expected:
                failures.append(f"{mode}: code {result.returncode}\n{result.stdout}\n{result.stderr}")
            parsed = run(["parse", *options])
            if parsed.returncode != 0:
                failures.append(f"parse {mode}: {parsed.stderr}")
            dumped = run(["--dump-sir", *options])
            if dumped.returncode != 0:
                failures.append(f"SIR {mode}: {dumped.stderr}")
        for invalid in ("", "minimum", "TYP", "0", "typ,max"):
            result = run([f"--mintypmax={invalid}"])
            if result.returncode != 2 or "needs min, typ or max" not in result.stderr:
                failures.append(f"invalid {invalid!r}: {result.returncode}\n{result.stderr}")
        result = run(["--mintypmax=min", "--mintypmax=max"])
        if result.returncode != 0 or result.stdout != (HERE / "mintypmax_matrix.max.expected").read_text():
            failures.append(f"repeated option: {result.returncode}\n{result.stdout}\n{result.stderr}")
    if failures:
        print("\n".join(failures), file=sys.stderr)
        return 1
    print("mintypmax: default, all modes, parse, SIR and invalid arguments passed")
    return 0


if __name__ == "__main__":
    sys.exit(main())
