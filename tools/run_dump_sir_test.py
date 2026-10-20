#!/usr/bin/env python3
"""Checks `kirimasim --dump-sir` against an expected SIR text.

The test runs in the directory of the .sv file and passes only the file
name, so that the source paths in the SIR text do not depend on where the
repository is checked out. The output must equal <name>.expected exactly.
A run that writes more than 16 MiB to stdout or stderr is stopped and
fails (tools/bounded_run.py).
"""

import argparse
import difflib
import sys
from pathlib import Path

import bounded_run


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("--kirimasim", required=True, help="path to the kirimasim executable")
    ap.add_argument("test", type=Path, help="the .sv file")
    args = ap.parse_args()

    expected = args.test.with_suffix(".expected").read_text(encoding="utf-8")
    try:
        proc = bounded_run.run([args.kirimasim, "--dump-sir", args.test.name],
                               cwd=args.test.parent)
    except bounded_run.OutputLimit as e:
        print(f"FAIL: {e}", file=sys.stderr)
        return 1
    if proc.returncode != 0:
        print(f"FAIL: exit code {proc.returncode}", file=sys.stderr)
        print(proc.stderr, file=sys.stderr)
        return 1
    if proc.stdout != expected:
        print("FAIL: SIR text differs", file=sys.stderr)
        sys.stderr.writelines(difflib.unified_diff(
            expected.splitlines(keepends=True), proc.stdout.splitlines(keepends=True),
            "expected", "got"))
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
