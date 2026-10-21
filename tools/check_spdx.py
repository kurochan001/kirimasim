#!/usr/bin/env python3
# Copyright (c) 2026 The kirimasim Authors
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

"""Checks that each file of kirimasim's own code starts with its copyright
line and its SPDX line (CONTRIBUTING.md, "Rules for the code").

  tools/check_spdx.py          # exit 1 if a file lacks them
  tools/check_spdx.py --add    # put them at the top of each file that lacks them

The files are those git tracks with these names: C++ (.cpp, .h, and
.h.in), CMake (CMakeLists.txt, .cmake), Python, shell, and the workflows
of .github/workflows. The two lines come first, after a #! line, as
comments of the file's language:

  Copyright (c) <year> The kirimasim Authors
  SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

Test data (.sv, expected outputs and the like) has no such lines: one
line more at the top would move the positions that expected diagnostics
name. The README says that every file is under the same licence unless
it says otherwise. --add writes the year of today, the year a new file
is made.
"""

import argparse
import datetime
import re
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
LICENSE = "SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception"
COPYRIGHT = re.compile(r"Copyright \(c\) \d{4} The kirimasim Authors")


def prefix(path: str):
    """The comment prefix of the file's language, or None for a file not checked."""
    name = path.rsplit("/", 1)[-1]
    if path.endswith((".cpp", ".h", ".h.in")):
        return "//"
    if name == "CMakeLists.txt" or path.endswith((".cmake", ".py", ".sh")):
        return "#"
    if path.startswith(".github/workflows/") and path.endswith((".yml", ".yaml")):
        return "#"
    return None


def files():
    out = subprocess.run(["git", "ls-files"], cwd=ROOT, capture_output=True, text=True,
                         check=True).stdout.split("\n")
    return [(p, prefix(p)) for p in out if p and prefix(p) is not None]


def has_lines(lines, mark):
    """Whether the copyright and SPDX lines come first, after a #! line."""
    at = 1 if lines and lines[0].startswith("#!") else 0
    head = lines[at:at + 2]
    return (len(head) == 2 and head[0].startswith(mark + " ")
            and COPYRIGHT.fullmatch(head[0][len(mark) + 1:]) is not None
            and head[1] == f"{mark} {LICENSE}")


def add(lines, mark, year):
    at = 1 if lines and lines[0].startswith("#!") else 0
    new = [f"{mark} Copyright (c) {year} The kirimasim Authors", f"{mark} {LICENSE}"]
    if at < len(lines) and lines[at].strip():
        new.append("")
    return lines[:at] + new + lines[at:]


def main():
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("--add", action="store_true")
    args = ap.parse_args()
    year = datetime.date.today().year
    missing = []
    for path, mark in files():
        full = ROOT / path
        text = full.read_text(encoding="utf-8")
        lines = text.split("\n")
        if has_lines(lines, mark):
            continue
        if args.add:
            full.write_text("\n".join(add(lines, mark, year)), encoding="utf-8")
        else:
            missing.append(path)
    for path in missing:
        print(f"check_spdx: {path}: no copyright and SPDX lines at the top", file=sys.stderr)
    if missing:
        print("check_spdx: tools/check_spdx.py --add puts them there", file=sys.stderr)
        return 1
    if not args.add:
        print(f"check_spdx: {len(files())} file(s) have them")
    return 0


if __name__ == "__main__":
    sys.exit(main())
