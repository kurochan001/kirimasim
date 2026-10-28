#!/usr/bin/env python3
# Copyright (c) 2026 The kirimasim Authors
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

"""Keeps changes to existing test expectations out of implementation PRs.

Looks at the diff from merge-base(base, head) to head. If that diff touches
the implementation (anything under src/), it must not also:

* change or remove an existing entry of tests/perf_counts/expected.json,
* change or remove an existing line of any other tests/**/expected* file,
  tests/**/*.expected file, tests/**/*.stderr file or tests/**/*.choice
  file, of a tests/**/*.warnings, tests/**/*.stdout or tests/**/*.accepted
  file (expected warning positions, expected output, the constructs a
  matrix accepts), or of a gate list tests/gates/*.txt,
* add a line of only "---" to an existing tests/**/*.expected file: it adds
  an allowed result, which loosens what the test expects,
* change, remove or add a line of an existing test-suite file
  (tests/**/*.sv). This covers ":expected_stdout:" lines and also
  rewriting a test's body to get the same effect,
* delete or rename an existing file under tests/ (other than CMakeLists.txt
  and README.md),
* add a test to tests/suite/skips.txt, the list of tests allowed to be
  skipped: allowing a skip loosens what the test expects (taking one off
  the list does not).

Unit test sources (tests/unit/) often have to change together with the
code they call, for example when a function is renamed, so editing them
does not fail the check. Removed lines that contain an assertion
(EXPECT_*, ASSERT_*) are listed as warnings for the reviewers instead.

Adding new tests, new expected values and new PerfCount entries in the same
PR as the implementation is fine; a human checks their basis in review.
Changes to existing expectations go in a separate PR that states the reason
and the IEEE 1800 section.
"""

import argparse
import fnmatch
import json
import re
import subprocess
import sys

TEST_SOURCE_PATTERNS = ("tests/*.sv",)
UNIT_TEST_PATTERNS = ("tests/unit/*.cpp", "tests/unit/*.h")
ASSERTION = re.compile(r"\b(?:EXPECT|ASSERT)_[A-Z_]+\s*\(")
NOT_PROTECTED = {"CMakeLists.txt", "README.md"}
PERF_EXPECTED = "tests/perf_counts/expected.json"
SKIPS = "tests/suite/skips.txt"


def git(*args: str) -> str:
    return subprocess.run(["git", *args], check=True, capture_output=True, text=True).stdout


def is_expected_file(path: str) -> bool:
    # expected* files, the <test name>.expected, <test name>.stderr and
    # <test name>.choice files that hold a test's expected output, messages
    # and chosen result, the .warnings, .stdout and .accepted files of the
    # command-line matrices, and the gate lists (see docs/spec/TEST_SUITE.md).
    name = path.rsplit("/", 1)[-1]
    if fnmatch.fnmatch(path, "tests/gates/*.txt"):
        return True
    return path.startswith("tests/") and (name.startswith("expected") or name.endswith(
        (".expected", ".stderr", ".choice", ".warnings", ".stdout", ".accepted")))


def is_test_source(path: str) -> bool:
    # fnmatch's * also matches "/", so these patterns cover subdirectories.
    return any(fnmatch.fnmatch(path, p) for p in TEST_SOURCE_PATTERNS)


def is_unit_test(path: str) -> bool:
    return any(fnmatch.fnmatch(path, p) for p in UNIT_TEST_PATTERNS)


def is_protected(path: str) -> bool:
    return (path.startswith("tests/") and path.rsplit("/", 1)[-1] not in NOT_PROTECTED)


def changed_lines(base: str, head: str, path: str) -> tuple[list[str], list[str]]:
    """The removed and the added lines of a file. The file headers come
    before the first hunk, so a removed line that itself starts with "--"
    is still a removed line."""
    out = git("diff", "--no-color", "-U0", base, head, "--", path)
    removed, added = [], []
    in_hunk = False
    for line in out.splitlines():
        if line.startswith("@@"):
            in_hunk = True
        elif in_hunk and line.startswith("-"):
            removed.append(line[1:])
        elif in_hunk and line.startswith("+"):
            added.append(line[1:])
    return removed, added


def removed_lines(base: str, head: str, path: str) -> list[str]:
    return changed_lines(base, head, path)[0]


def added_skips(base: str, head: str) -> list[str]:
    out = git("diff", "--no-color", "-U0", base, head, "--", SKIPS)
    added = [line[1:].strip() for line in out.splitlines()
             if line.startswith("+") and not line.startswith("+++")]
    return [f"{SKIPS}: test allowed to be skipped: {line.split('#')[0].strip()}"
            for line in added if line and not line.startswith("#")]


def perf_violations(base: str, head: str) -> list[str]:
    try:
        old = json.loads(git("show", f"{base}:{PERF_EXPECTED}"))
    except subprocess.CalledProcessError:
        return []  # the file is new: everything in it is an addition
    try:
        new = json.loads(git("show", f"{head}:{PERF_EXPECTED}"))
    except subprocess.CalledProcessError:
        return [f"{PERF_EXPECTED}: deleted"]
    errors = []
    for test, counters in sorted(old.items()):
        for name, value in sorted(counters.items()):
            if test not in new or name not in new[test]:
                errors.append(f"{PERF_EXPECTED}: {test} {name} removed")
            elif new[test][name] != value:
                errors.append(f"{PERF_EXPECTED}: {test} {name} changed "
                              f"{value} -> {new[test][name]}")
    return errors


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("--base", required=True, help="base commit of the PR")
    ap.add_argument("--head", required=True, help="head commit of the PR")
    args = ap.parse_args()

    base = git("merge-base", args.base, args.head).strip()
    status = git("diff", "--no-color", "--name-status", "-M", base, args.head).splitlines()

    impl_changed = []
    errors = []
    warnings = []
    for row in status:
        fields = row.split("\t")
        code, paths = fields[0], fields[1:]
        old_path = paths[0]
        if any(p.startswith("src/") for p in paths):
            impl_changed.append(paths[-1])
        if paths[-1] == SKIPS and code in ("A", "M"):
            errors += added_skips(base, args.head)
        if not is_protected(old_path):
            continue
        if code == "D":
            errors.append(f"{old_path}: existing test file deleted")
        elif code.startswith("R"):
            errors.append(f"{old_path}: existing test file renamed to {paths[1]}")
        elif code == "M":
            if old_path == PERF_EXPECTED:
                errors += perf_violations(base, args.head)
            elif is_expected_file(old_path) or is_test_source(old_path):
                removed, added = changed_lines(base, args.head, old_path)
                for line in removed:
                    errors.append(f"{old_path}: existing line changed or removed: {line!r}")
                for line in added:
                    if is_test_source(old_path):
                        errors.append(f"{old_path}: line added to an existing test: {line!r}")
                    elif old_path.endswith(".expected") and line.strip() == "---":
                        errors.append(f"{old_path}: allowed result added (a '---' line)")
            elif is_unit_test(old_path):
                for line in removed_lines(base, args.head, old_path):
                    if ASSERTION.search(line):
                        warnings.append(f"{old_path}: assertion changed or removed: "
                                        f"{line.strip()!r}")

    for w in warnings:
        # GitHub Actions shows ::warning:: lines as annotations on the PR.
        print(f"::warning::check_expected_change: {w}")

    if not impl_changed:
        print("check_expected_change: no changes under src/; expectation changes are allowed")
        return 0
    if errors:
        print("check_expected_change: this PR changes the implementation "
              f"({len(impl_changed)} file(s) under src/) and also changes existing "
              "expectations or tests. Move these to a separate PR:", file=sys.stderr)
        for e in errors:
            print(f"  {e}", file=sys.stderr)
        return 1
    print("check_expected_change: OK (implementation changed, existing expectations kept)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
