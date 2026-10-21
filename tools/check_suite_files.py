#!/usr/bin/env python3
# Copyright (c) 2026 The kirimasim Authors
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

"""Fails if tests/suite/ or tests/dump_sir/ has expectation files that no
test uses (TEST_SUITE.md): a .expected or .stderr file without the .sv file
of the same name. Every .sv file under tests/suite/ is registered with CTest
by a glob, so a test that exists always runs; an orphan expectation means a
test was renamed or removed and its expectation left behind.

It also checks tests/suite/skips.txt, the list of tests allowed to be
skipped: every line names a test that exists, once, with a reason.
"""

import argparse
import sys
from pathlib import Path

from suite_skips import read_skips


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("--root", type=Path, default=Path(__file__).resolve().parent.parent)
    args = ap.parse_args()
    errors = []
    for d in ("tests/suite", "tests/dump_sir"):
        for path in sorted((args.root / d).rglob("*")):
            if path.suffix in (".expected", ".stderr") and not path.with_suffix(".sv").exists():
                errors.append(f"{path.relative_to(args.root).as_posix()}: no "
                              f"{path.with_suffix('.sv').name} uses it")
    suite = args.root / "tests/suite"
    skips, problems = read_skips(suite / "skips.txt")
    errors += [f"tests/suite/{p}" for p in problems]
    for name in skips:
        if not (suite / f"{name}.sv").exists():
            errors.append(f"tests/suite/skips.txt: {name} names no test")
    for e in errors:
        print(e, file=sys.stderr)
    if errors:
        return 1
    print("check_suite_files: every expectation file has its test")
    return 0


if __name__ == "__main__":
    sys.exit(main())
