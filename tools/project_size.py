#!/usr/bin/env python3
# Copyright (c) 2026 The kirimasim Authors
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

"""Counts the size of the repository for docs/PROJECT_SIZE.md: the lines of
the C++ sources by library, of the tests, the tools and the documents, and
the commits.

  tools/project_size.py    # prints the tables of docs/PROJECT_SIZE.md

Only files that git tracks are counted, as they are in the working tree, so
run it on a clean checkout of the commit to record. A line is a newline
character, as `wc -l` counts; a blank line is one with only white space.
"""

import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
CXX = (".cpp", ".h", ".hpp", ".inc", ".c")
# The libraries of CLAUDE.md, in the order of the table.
LIBRARIES = ["kernel", "front", "vm", "sir", "valsem", "driver", "analysis", "codegen"]


def git(*args):
    return subprocess.run(["git", *args], cwd=ROOT, capture_output=True, text=True,
                          check=True).stdout


def tracked():
    return [p for p in git("ls-files", "-z").split("\0") if p]


def measure(paths):
    """(files, lines, lines that are not blank) of paths."""
    lines = nonblank = 0
    for p in paths:
        data = (ROOT / p).read_bytes()
        lines += data.count(b"\n")
        nonblank += sum(1 for line in data.splitlines() if line.strip())
    return len(paths), lines, nonblank


def main():
    files = tracked()
    out = [f"Commit `{git('rev-parse', '--short=12', 'HEAD').strip()}`.", ""]
    out += ["| Library | Files | Lines | Not blank |", "| --- | ---: | ---: | ---: |"]
    total = []
    for lib in LIBRARIES:
        paths = [p for p in files if p.startswith(f"src/{lib}/") and p.endswith(CXX)]
        total += paths
        n, lines, nonblank = measure(paths)
        out.append(f"| {lib} | {n:,} | {lines:,} | {nonblank:,} |")
    others = [p for p in files if p.startswith("src/") and p.endswith(CXX) and p not in total]
    if others:
        sys.exit(f"project_size: C++ files outside the listed libraries: {others[:3]}")
    n, lines, nonblank = measure(total)
    out += [f"| **Total** | **{n:,}** | **{lines:,}** | **{nonblank:,}** |", ""]
    groups = [
        ("Unit tests (C++, `tests/unit/`)",
         [p for p in files if p.startswith("tests/unit/") and p.endswith((".cpp", ".h"))]),
        ("Suite tests (`tests/suite/**/*.sv`)",
         [p for p in files if p.startswith("tests/suite/") and p.endswith(".sv")]),
        ("Expected outputs of the suite tests",
         [p for p in files if p.startswith("tests/suite/")
          and p.endswith((".expected", ".stderr", ".choice"))]),
        ("Test scripts (Python, `tests/`)",
         [p for p in files if p.startswith("tests/") and p.endswith(".py")]),
        ("Tools (Python, `tools/`)",
         [p for p in files if p.startswith("tools/") and p.endswith(".py")]),
        ("Documents in English (`docs/`)",
         [p for p in files if p.startswith("docs/") and p.endswith(".md")
          and not p.endswith(".ja.md")]),
        ("Documents in Japanese (`docs/`)",
         [p for p in files if p.startswith("docs/") and p.endswith(".ja.md")]),
        ("Specifications, both languages (`docs/spec/`)",
         [p for p in files if p.startswith("docs/spec/") and p.endswith(".md")]),
    ]
    out += ["| Kind | Files | Lines |", "| --- | ---: | ---: |"]
    for label, paths in groups:
        n, lines, _ = measure(paths)
        out.append(f"| {label} | {n:,} | {lines:,} |")
    generators = [p for p in files if p.startswith("tools/gen_") and p.endswith(".py")]
    first = git("log", "--reverse", "--format=%ad", "--date=short").split("\n", 1)[0]
    commits = int(git("rev-list", "--count", "HEAD"))
    out += ["", f"Test generators (`tools/gen_*.py`): {len(generators):,}. "
            f"Files tracked: {len(files):,}. Commits: {commits:,}, the first on {first}."]
    print("\n".join(out))
    return 0


if __name__ == "__main__":
    sys.exit(main())
