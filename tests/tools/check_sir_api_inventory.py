#!/usr/bin/env python3
# Copyright (c) 2026 The kirimasim Authors
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
"""tools/sir_api_inventory.py finds the function that holds a line and the references of the
P2 inventory (P2 review L1): on small inputs written here, and on the repository."""
import importlib.util
import os
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent.parent
TOOL = ROOT / "tools/sir_api_inventory.py"
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

    real = subprocess.run([sys.executable, str(TOOL)], capture_output=True, text=True, stdin=subprocess.DEVNULL)
    if real.returncode not in (0, 1) or "sir api inventory: 274 references" not in real.stdout:
        failures.append(f"the check of the repository does not run: {real.stdout[-300:]}{real.stderr[-300:]}")
    # Without the history, as in CI's shallow checkout: the table names functions, so the
    # check runs and gives the same result; asking for line numbers says what is missing.
    root = Path(__file__).resolve().parent.parent.parent
    with tempfile.TemporaryDirectory() as directory:
        copy = Path(directory)
        (copy / "docs/notes").mkdir(parents=True)
        shutil.copy(root / "docs/notes/sir-v1-proposal.md", copy / "docs/notes")
        shutil.copytree(root / "src/vm", copy / "src/vm")
        bare = subprocess.run([sys.executable, str(TOOL), "--root", str(copy)], capture_output=True, text=True,
                              stdin=subprocess.DEVNULL, env={**os.environ, "GIT_CEILING_DIRECTORIES": directory})
        if bare.returncode != real.returncode or bare.stdout != real.stdout:
            failures.append(f"without the history the check differs: {bare.stdout[-300:]}{bare.stderr[-300:]}")
        lines = subprocess.run([sys.executable, str(TOOL), "--root", str(copy), "--names", "5-7"],
                               capture_output=True, text=True, stdin=subprocess.DEVNULL,
                               env={**os.environ, "GIT_CEILING_DIRECTORIES": directory})
        if lines.returncode != 2 or "git fetch --unshallow" not in lines.stderr:
            failures.append(f"line numbers without the history: exit {lines.returncode}, {lines.stderr[-300:]}")
    for failure in failures:
        print("FAIL: " + failure, file=sys.stderr)
    if failures:
        return 1
    print("sir api inventory: functions, lines and references are found as written")
    return 0


if __name__ == "__main__":
    sys.exit(main())
