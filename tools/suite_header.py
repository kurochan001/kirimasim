"""Reads the metadata header of a test-suite file (TEST_SUITE.md, "Metadata
header"), for the tools that judge tests.

The header is the comment at the top of the file: the file starts with
/* (blank lines before it are allowed), and each line inside of the form
:item: value gives one item. A header that is not at the top, an item
whose name is not in TEST_SUITE.md, a :type: word that is not a kind of
test, a missing required item, a yes/no item with another value, a
:timeout: that is not a number of seconds, an :expected_exit: that is
neither a number nor "nonzero" and an :expected_stderr_regex: that is not a
regular expression are problems: a misspelled item would otherwise be
dropped without a word, and the test judged by less than it says.
"""

from __future__ import annotations

import re
from pathlib import Path

# The items of TEST_SUITE.md: sv-tests' and this suite's.
ITEMS = {
    "name", "description", "tags", "type", "should_fail_because", "files", "incdirs",
    "defines", "top_module", "timeout", "lrm", "oracle", "fourstate", "nondeterministic",
    "expected_stdout", "expected_error", "spec_check", "issue", "expected_exit", "generator",
    "expected_stderr",
    # The test suite repository's items (TEST_SUITE.md, added 2026-10-21).
    "expected_stderr_regex", "args", "dpi_c", "vpi_c",
}
REQUIRED = ("name", "description", "tags", "type", "lrm", "oracle", "fourstate",
            "nondeterministic")
TYPES = {"preprocessing", "parsing", "elaboration", "simulation"}
YES_NO = ("fourstate", "nondeterministic")


def read_header(path: Path) -> tuple[dict[str, str], list[str]]:
    """The items of the header, and the problems found in it."""
    text = path.read_text(encoding="utf-8")
    meta: dict[str, str] = {}
    body = text.lstrip()
    if not body.startswith("/*") or "*/" not in body:
        return meta, ["the file does not start with its /* ... */ header"]
    problems = []
    for line in body[2:body.index("*/")].splitlines():
        fm = re.match(r"^:([A-Za-z_-]+):\s?(.*)$", line.strip())
        if not fm:
            continue
        item, value = fm.group(1), fm.group(2).strip()
        if item not in ITEMS:
            problems.append(f"unknown header item :{item}:")
        meta[item] = value
    for item in REQUIRED:
        if item not in meta:
            problems.append(f"the header has no :{item}:")
    for word in meta.get("type", "").split():
        if word not in TYPES:
            problems.append(f":type: has an unknown kind of test {word!r}")
    for item in YES_NO:
        if item in meta and meta[item] not in ("yes", "no"):
            problems.append(f":{item}: is {meta[item]!r}, not yes or no")
    if "timeout" in meta and not (meta["timeout"].isdigit() and int(meta["timeout"]) > 0):
        problems.append(f":timeout: is {meta['timeout']!r}, not a number of seconds")
    if "expected_exit" in meta and not (meta["expected_exit"].isdigit()
                                        or meta["expected_exit"] == "nonzero"):
        problems.append(f":expected_exit: is {meta['expected_exit']!r}, not a number or nonzero")
    if "expected_stderr_regex" in meta:
        try:
            re.compile(meta["expected_stderr_regex"])
        except re.error as e:
            problems.append(f":expected_stderr_regex: is not a regular expression ({e})")
    if "spec" in meta.get("oracle", "").split() and "spec_check" not in meta:
        problems.append("the header has :oracle: spec but no :spec_check:")
    return meta, problems
