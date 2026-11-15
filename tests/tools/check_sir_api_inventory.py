#!/usr/bin/env python3
# Copyright (c) 2026 The kirimasim Authors
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
"""tools/sir_api_inventory.py finds the function that holds a line and the references of the
P2 inventory (P2 review L1): on small inputs written here, on the repository, and on the
counts of the rows that the proposal states."""
import importlib.util
import os
import re
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent.parent
TOOL = ROOT / "tools/sir_api_inventory.py"
PROPOSAL = ROOT / "docs/notes/sir-v1-proposal.md"
PROPOSAL_JA = ROOT / "docs/notes/sir-v1-proposal.ja.md"
failures = []

SOURCE = """#include <vector>
// A comment with braces { and a call(
namespace kirimasim::vm {
namespace {
int helper(int x) {
    if (x) { return 1; }
    return 0;
}
}  // namespace

static const char* text = "} { (";
class Interpreter {
public:
    Interpreter(int a) {
        auto lambda = [](int b) { return b; };
        (void)lambda;
    }
    void run() {
        ctx.store.read(1);
        wide::add(a, b); fold(a);
    }
};

void Interpreter::later(int c) {
    sched_.trigger(c);
}
}  // namespace kirimasim::vm
"""

BY_KIND = """## Appendix B. Inventory

| Reference | Functions | Group | Entry |
| --- | --- | --- | --- |
| `a.one` | `Interpreter::run` | none | A: `one` |
| `a.two` | `Interpreter::run` | none; X | H: `two` |
| `a.three` | `Interpreter::run` | yes: `three` | existing (a note) |
| `a.four` | `Interpreter::run` | none | I: inline helper |
| `a.five` | `Interpreter::run` | yes: `five` | existing |
"""


def load():
    spec = importlib.util.spec_from_file_location("inventory_tool", TOOL)
    tool = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(tool)
    return tool


def main() -> int:
    tool = load()
    table = tool.functions(SOURCE)
    names = [name for name, _, _ in table]
    if names != ["helper", "Interpreter::Interpreter", "Interpreter::run", "Interpreter::later"]:
        failures.append(f"functions found: {names}")
    for line, want in ((6, "helper"), (15, "Interpreter::Interpreter"), (19, "Interpreter::run"),
                       (25, "Interpreter::later"), (11, "(between functions)"), (2, "(between functions)")):
        got = tool.holder(table, line)
        if got != want:
            failures.append(f"line {line}: {got}, expected {want}")
    if tool.names_of(table, "5–7, 18–19, 25") != ["helper", "Interpreter::run", "Interpreter::later"]:
        failures.append(f"names of a line list: {tool.names_of(table, '5–7, 18–19, 25')}")
    code = tool.strip_code(SOURCE)
    # kernel::wide::fold is called as fold, from code inside its namespace.
    for reference, want in (("ctx.store", True), ("kernel::wide::add", True), ("kernel::wide::fold", True),
                            ("sched_.trigger", True), ("sched_.triggered", False), ("ctx.heap", False),
                            ("call", False)):
        if tool.occurs(code, reference) != want:
            failures.append(f"occurs({reference}) is not {want}")
    for reference, want in (("ctx.store", True), ("kernel::wide::add", True), ("kernel::SlotId::index", False),
                            ("kernel::HeapStatus::operator bool", False), ("kernel::HeapResult<int>::value", False)):
        if tool.lexical(reference) != want:
            failures.append(f"lexical({reference}) is not {want}")
    if tool.category_counts(BY_KIND) != {"A": 1, "H": 1, "I": 1, "existing": 2}:
        failures.append(f"counts by kind: {tool.category_counts(BY_KIND)}")

    real = subprocess.run([sys.executable, str(TOOL)], capture_output=True, text=True, stdin=subprocess.DEVNULL)
    if real.returncode not in (0, 1) or "references" not in real.stdout:
        failures.append(f"the check of the repository does not run: {real.stdout[-300:]}{real.stderr[-300:]}")
    # Without the history, as in CI's shallow checkout: the table names functions, so the
    # check runs and gives the same result; asking for line numbers says what is missing.
    with tempfile.TemporaryDirectory() as directory:
        copy = Path(directory)
        (copy / "docs/notes").mkdir(parents=True)
        shutil.copy(PROPOSAL, copy / "docs/notes")
        shutil.copytree(ROOT / "src/vm", copy / "src/vm")
        bare = subprocess.run([sys.executable, str(TOOL), "--root", str(copy)], capture_output=True, text=True,
                              stdin=subprocess.DEVNULL, env={**os.environ, "GIT_CEILING_DIRECTORIES": directory})
        if bare.returncode != real.returncode or bare.stdout != real.stdout:
            failures.append(f"without the history the check differs: {bare.stdout[-300:]}{bare.stderr[-300:]}")
        lines = subprocess.run([sys.executable, str(TOOL), "--root", str(copy), "--names", "5-7"],
                               capture_output=True, text=True, stdin=subprocess.DEVNULL,
                               env={**os.environ, "GIT_CEILING_DIRECTORIES": directory})
        if lines.returncode != 2 or "git fetch --unshallow" not in lines.stderr:
            failures.append(f"line numbers without the history: exit {lines.returncode}, {lines.stderr[-300:]}")

    # The proposal states the counts in its introduction to Appendix B, in both languages.
    # Compare them with the rows, so that editing a row cannot leave the prose behind.
    proposal = PROPOSAL.read_text(encoding="utf-8")
    rows = tool.appendix_rows(proposal)
    counts = tool.category_counts(proposal)
    got = (counts["A"], counts["H"], counts["I"], counts["existing"])
    total = re.search(r"(\d+) distinct API references", proposal)
    if not total or int(total.group(1)) != len(rows):
        failures.append(f"the proposed number of references is not {len(rows)}: {total.group(1) if total else 'none'}")
    stated = re.search(r"There are (\d+) A rows.*?(\d+) H rows, (\d+) I rows and (\d+) fully covered rows", proposal)
    if not stated:
        failures.append("the introduction to Appendix B does not state the counts of its rows")
    elif tuple(int(stated.group(i)) for i in range(1, 5)) != got:
        failures.append(f"the stated counts are {stated.groups()}, the rows are {got}")
    elif sum(got) != len(rows):
        failures.append(f"the counts {got} do not add up to the {len(rows)} rows")
    japanese = PROPOSAL_JA.read_text(encoding="utf-8")
    stated_ja = re.search(r"Aは(\d+)行.*?Hは(\d+)行、Iは(\d+)行、完全対応は(\d+)行", japanese)
    if not stated_ja:
        failures.append("the Japanese introduction to Appendix B does not state the counts of its rows")
    elif tuple(int(stated_ja.group(i)) for i in range(1, 5)) != got:
        failures.append(f"the Japanese counts are {stated_ja.groups()}, the rows are {got}")

    for failure in failures:
        print("FAIL: " + failure, file=sys.stderr)
    if failures:
        return 1
    print("sir api inventory: functions, lines, references and counts are found as written")
    return 0


if __name__ == "__main__":
    sys.exit(main())
