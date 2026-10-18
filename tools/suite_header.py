"""Reads the metadata header of a test-suite file (TEST_SUITE.md, "Metadata
header"), for the tools that judge tests.

The header is the comment at the top of the file: the file starts with
/* (blank lines before it are allowed), and each line inside of the form
:item: value gives one item. A header that is not at the top, an item
whose name is not in TEST_SUITE.md, a :type: word that is not a kind of
test, a missing required item and a yes/no item with another value are
problems: a misspelled item would otherwise be dropped without a word,
and the test judged by less than it says.
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
    if "spec" in meta.get("oracle", "").split() and "spec_check" not in meta:
        problems.append("the header has :oracle: spec but no :spec_check:")
    return meta, problems
