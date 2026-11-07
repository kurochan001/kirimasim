#!/usr/bin/env python3
# Copyright (c) 2026 The kirimasim Authors
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
"""The table of TEST_SUITE.md, "The present state": what the tests of each suite cover.

  tools/suite_table.py kirimasim-tests=suite/tests tests/suite=tests/suite

Each argument names a column and the folder of its tests. A test is a .sv file
whose header has a :type:; the other .sv files are sources that tests name in
:files:. Words are looked for without comments and string literals. The nightly
job writes this table to its summary, and it goes in the report at the end of P1
(M8). The table reports and judges nothing.
"""

import argparse
import re
from pathlib import Path

from p1_population import header

HEADER_ROWS = (
    ("Tests", lambda m, f: True),
    ("Simulation tests", lambda m, f: "simulation" in m.get("type", "").split()),
    ("`:spec_check: human`", lambda m, f: m.get("spec_check") == "human"),
    ("`:oracle:` `spec` alone", lambda m, f: m.get("oracle", "").split() == ["spec"]),
    ("`:fourstate: yes`", lambda m, f: m.get("fourstate") == "yes"),
    ("`:nondeterministic: yes`", lambda m, f: m.get("nondeterministic") == "yes"),
    ("First clause in chapter 4", lambda m, f: first_clause(m).split(".")[0] == "4"),
    ("`:should_fail_because:`", lambda m, f: "should_fail_because" in m),
    ("Made by a generator", lambda m, f: "generator" in m),
)
WORD_ROWS = (
    ("`$strobe`", r"\$strobe\b"),
    ("`$monitor`", r"\$monitor\b"),
    ("`$fopen`", r"\$fopen\b"),
    ("`$readmem`", r"\$readmem[bh]\b"),
    ("`$writemem`", r"\$writemem[bh]\b"),
    ("`$dumpvars`", r"\$dumpvars\b"),
    ("DPI-C import", r'\bimport\s+"DPI'),
    ("`$test$plusargs`, `$value$plusargs`", r"\$(test|value)\$plusargs\b"),
)
NOT_SOURCE = re.compile(r'/\*.*?\*/|//[^\n]*|"(?:\\.|[^"\\\n])*"', re.S)


def first_clause(meta: dict[str, str]) -> str:
    clauses = re.sub(r"\b1800-\d{4}\b", " ", meta.get("lrm", "")).split()
    return clauses[0] if clauses else ""


def source_without_strings(text: str) -> str:
    """Keeps the import "DPI..." form, the one string that a row looks for."""
    return NOT_SOURCE.sub(lambda m: m.group(0) if m.group(0).startswith('"DPI') else " ", text)


def count(folder: Path) -> dict[str, int]:
    found = dict.fromkeys([name for name, _ in HEADER_ROWS] + [name for name, _ in WORD_ROWS], 0)
    for path in sorted(folder.rglob("*.sv")):
        text = path.read_text(encoding="utf-8", errors="ignore")
        meta = header(text)
        if "type" not in meta:
            continue
        for name, test in HEADER_ROWS:
            found[name] += bool(test(meta, path))
        body = source_without_strings(text)
        for name, word in WORD_ROWS:
            found[name] += bool(re.search(word, body))
    return found


def table(columns: list[tuple[str, dict[str, int]]]) -> str:
    rows = ["| What was looked at | " + " | ".join(f"`{name}`" for name, _ in columns) + " |",
            "| --- | " + " | ".join("---" for _ in columns) + " |"]
    for name in columns[0][1]:
        rows.append(f"| {name} | " + " | ".join(f"{found[name]:,}" for _, found in columns) + " |")
    return "\n".join(rows)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("column", nargs="+", help="NAME=FOLDER")
    args = parser.parse_args()
    columns = []
    for item in args.column:
        name, _, folder = item.partition("=")
        if not folder or not Path(folder).is_dir():
            parser.error(f"{item}: expected NAME=FOLDER, with a folder that exists")
        columns.append((name, count(Path(folder))))
    print(table(columns))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
