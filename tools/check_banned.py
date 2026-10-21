#!/usr/bin/env python3
# Copyright (c) 2026 The kirimasim Authors
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

"""Fails if src/ uses something banned (CLAUDE.md).

Banned in src/kernel/ and src/vm/: std::shared_ptr, dynamic_cast,
std::function, typeid. Shared ownership is banned in every spelling:
shared_ptr, weak_ptr, make_shared, allocate_shared,
enable_shared_from_this and the pointer casts, with or without std::
(after a using declaration); so is function<...> without std::.

Banned in all of src/ but src/driver/ (RUNTIME_ABI.md 9.3): outside inputs
(9.3.8: environment variables, the locale, the time of day, random seeds
from outside, the process ID), ending the process (9.3.9: exit, _Exit,
quick_exit, atexit), and writing to the process's standard output or
standard error (9.3.10: stdout, stderr, std::cout, printf and the like).
Banned in all of src/ (9.3.9): abort, std::terminate, assert and
__builtin_trap.

Banned in all of src/ (the simulator must know nothing of test suites):
pass/fail strings such as "TEST PASSED", the names of test metadata
(should_fail, expected_stdout and the like), and string literals that pick
out files or libraries by name: "-fail.sv", "/uvm/", "/testbenches/", or a
UVM class or package name ("uvm_...").

A line may carry
    // kirimasim: banned-ok <reason>
(or have it on the line just above) where something of the two lists above
is needed for another purpose; reviews look at every waiver.

std::unordered_map and std::unordered_set are allowed only when the same line
or the line just above carries a comment of the form
    // kirimasim: unordered-ok <reason>
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
    (re.compile(r"\b(?:shared_ptr|weak_ptr|make_shared|allocate_shared|enable_shared_from_this"
                r"|(?:static|dynamic|const|reinterpret)_pointer_cast)\b"),
     "shared ownership (std::shared_ptr and its family)"),
    (re.compile(r"\bdynamic_cast\b"), "dynamic_cast"),
    (re.compile(r"\bstd\s*::\s*function\b|(?<![\w:])function\s*<"), "std::function"),
    (re.compile(r"\btypeid\b"), "typeid"),
]
# RUNTIME_ABI.md 9.3, checked in all of src/ but src/driver/.
OUTSIDE_DRIVER = [
    (re.compile(r"\b(?:secure_)?getenv\b|\b(?:setenv|putenv|unsetenv|clearenv)\b|\benviron\b"),
     "an environment variable (RUNTIME_ABI.md 9.3.8)"),
    (re.compile(r"\bsetlocale\b|\bstd\s*::\s*locale\b|\blocaleconv\b|\bimbue\s*\("),
     "the locale (RUNTIME_ABI.md 9.3.8)"),
    (re.compile(r"\b(?:system|high_resolution|utc|tai|gps|file)_clock\b"
                r"|\b(?:gettimeofday|clock_gettime|timespec_get|localtime(?:_r)?|gmtime(?:_r)?"
                r"|asctime|ctime|strftime)\b|\bstd\s*::\s*time\s*\(|(?<![\w.>]):\s*:\s*time\s*\("),
     "the time of day (RUNTIME_ABI.md 9.3.8)"),
    (re.compile(r"\brandom_device\b|\bgetrandom\b|\barc4random\w*|\b[dlm]rand48\b|\bsrand\w*\s*\("
                r"|(?<![\w.>])(?:std\s*::\s*)?rand\s*\(|\bgetpid\b"),
     "a random seed from outside (RUNTIME_ABI.md 9.3.8, 9.5)"),
    (re.compile(r"(?<![\w.>])(?:std\s*::\s*)?(?:exit|_Exit|quick_exit|atexit|at_quick_exit)\s*\("),
     "ending the process (RUNTIME_ABI.md 9.3.9)"),
    (re.compile(r"\bstd(?:out|err)\b|\bstd\s*::\s*w?c(?:out|err|log)\b"
                r"|(?<!\w)(?:std\s*::\s*)?(?:v?printf|puts|putchar|perror)\s*\("
                r"|\bstd\s*::\s*print(?:ln)?\s*\("),
     "the process's standard output or standard error (RUNTIME_ABI.md 9.3.10)"),
]
# RUNTIME_ABI.md 9.3.9, checked in all of src/.
EVERYWHERE = [
    (re.compile(r"(?<![\w.>])(?:std\s*::\s*)?abort\s*\(|\bstd\s*::\s*terminate\b|\bassert\s*\("
                r"|<c?assert(?:\.h)?>|\b__builtin_trap\b"),
     "abort, std::terminate or assert (RUNTIME_ABI.md 9.3.9)"),
]
# Test-suite knowledge, checked in all of src/.
SUITE_KNOWLEDGE = [
    (re.compile(r"TEST[ _](?:PASS|FAIL)", re.IGNORECASE), "a pass/fail string"),
    (re.compile(r"\bshould_fail|\bexpected_(?:stdout|stderr|exit|error)\b"),
     "test metadata"),
    (re.compile(r'"[^"\n]*(?:-fail\.sv|/uvm/|/testbenches/)[^"\n]*"'),
     "a file name or path test"),
    (re.compile(r'"uvm_[A-Za-z0-9_]*'), "a UVM name"),
]
SUITE_WAIVER = re.compile(r"//\s*kirimasim:\s*banned-ok\s+\S")
UNORDERED = re.compile(r"\bstd\s*::\s*unordered_(?:map|set)\b")
WAIVER = re.compile(r"//\s*kirimasim:\s*unordered-ok\s+\S")


def check_all_src(path: Path, display: str) -> list[str]:
    """Test-suite knowledge and RUNTIME_ABI.md 9.3, which have the waiver."""
    patterns = SUITE_KNOWLEDGE + EVERYWHERE
    if not display.startswith("src/driver/"):
        patterns = patterns + OUTSIDE_DRIVER
    errors = []
    lines = path.read_text(encoding="utf-8", errors="replace").splitlines()
    for i, line in enumerate(lines):
        prev = lines[i - 1] if i > 0 else ""
        if SUITE_WAIVER.search(line) or SUITE_WAIVER.search(prev):
            continue
        for pattern, what in patterns:
            if pattern.search(line):
                errors.append(f"{display}:{i + 1}: banned in src/: {what}")
    return errors


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
                    "'// kirimasim: unordered-ok <reason>'")
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

    src = args.root / "src"
    if src.is_dir():
        for path in sorted(src.rglob("*")):
            if path.is_file() and path.suffix in SUFFIXES:
                errors += check_all_src(path, path.relative_to(args.root).as_posix())

    for e in errors:
        print(e, file=sys.stderr)
    if errors:
        print(f"check_banned: {len(errors)} violation(s)", file=sys.stderr)
        return 1
    print(f"check_banned: {checked} file(s) clean")
    return 0


if __name__ == "__main__":
    sys.exit(main())
