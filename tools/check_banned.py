#!/usr/bin/env python3
"""Fails if src/kernel/ or src/vm/ uses a banned C++ feature.

Banned everywhere in those directories: std::shared_ptr, dynamic_cast,
std::function, getenv, typeid.

std::unordered_map and std::unordered_set are allowed only when the same line
or the line just above carries a comment of the form
    // haremasim: unordered-ok <reason>
(their iteration order is unspecified, so they must not affect results).

Violations are printed as file:line. Comments are not ignored: write the
names differently in prose (for example, "shared pointer").
"""

import argparse
import re
import sys
from pathlib import Path

DIRS = ("src/kernel", "src/vm")
SUFFIXES = {".h", ".hpp", ".cpp", ".cc", ".cxx", ".inc", ".ipp"}

BANNED = [
    (re.compile(r"\bstd\s*::\s*shared_ptr\b"), "std::shared_ptr"),
    (re.compile(r"\bdynamic_cast\b"), "dynamic_cast"),
    (re.compile(r"\bstd\s*::\s*function\b"), "std::function"),
    (re.compile(r"\b(?:std\s*::\s*)?(?:secure_)?getenv\b"), "getenv"),
    (re.compile(r"\btypeid\b"), "typeid"),
]
UNORDERED = re.compile(r"\bstd\s*::\s*unordered_(?:map|set)\b")
WAIVER = re.compile(r"//\s*haremasim:\s*unordered-ok\s+\S")


def check_file(path: Path, display: str) -> list[str]:
    errors = []
    lines = path.read_text(encoding="utf-8", errors="replace").splitlines()
    for i, line in enumerate(lines):
        for pattern, what in BANNED:
            if pattern.search(line):
                errors.append(f"{display}:{i + 1}: banned: {what}")
        if UNORDERED.search(line):
            prev = lines[i - 1] if i > 0 else ""
            if not (WAIVER.search(line) or WAIVER.search(prev)):
                errors.append(
                    f"{display}:{i + 1}: unordered container without "
                    "'// haremasim: unordered-ok <reason>'")
    return errors


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("--root", type=Path, default=Path(__file__).resolve().parent.parent,
                    help="repository root (default: parent of tools/)")
    args = ap.parse_args()

    errors = []
    checked = 0
    for d in DIRS:
        base = args.root / d
        if not base.is_dir():
            continue
        for path in sorted(base.rglob("*")):
            if path.is_file() and path.suffix in SUFFIXES:
                checked += 1
                errors += check_file(path, path.relative_to(args.root).as_posix())

    for e in errors:
        print(e, file=sys.stderr)
    if errors:
        print(f"check_banned: {len(errors)} violation(s)", file=sys.stderr)
        return 1
    print(f"check_banned: {checked} file(s) clean")
    return 0


if __name__ == "__main__":
    sys.exit(main())
