#!/usr/bin/env python3
# Copyright (c) 2026 The kirimasim Authors
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
"""Generate Appendix A's names and row order from ops.def, preserving reviewed prose.

The English and Japanese tables hold the operand/type/effect descriptions; ops.def
holds the opcode names, text spellings and order. Missing, duplicate or obsolete
descriptions are errors, never filled with guessed semantics. This checks the
inventory's coverage and presentation, not the correctness of its prose.

The default prints a diff without writing. --check rejects a stale table; --write
updates both tables after validating both. No build-time generation is needed.
"""
import argparse
import difflib
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
OPS = "src/sir/include/kirimasim/sir/ops.def"
PROPOSALS = (
    ("docs/notes/sir-v1-proposal.md", "## Appendix A."),
    ("docs/notes/sir-v1-proposal.ja.md", "## 付録A."),
)


def read_ops(text: str) -> list[tuple[str, str]]:
    """Read names from the current schema or a complete historical two-column file."""
    ops, names, spellings = [], set(), set()
    has_traits = None
    for line in text.splitlines():
        code = line.split("//", 1)[0].strip()
        if not code:
            continue
        match = re.fullmatch(r'KIRIMASIM_SIR_OP\((\w+), "([^"\s]+)"'
                             r'(?:, (None|Required|Optional), (Next|End|Wait|Fork))?\)', code)
        if not match:
            raise ValueError(f"malformed ops.def record: {code}")
        name, spelling, result, flow = match.groups()
        if has_traits is not None and has_traits != (result is not None):
            raise ValueError(f"mixed opcode schema versions at {name}")
        has_traits = result is not None
        if has_traits and flow != "Next" and result != "None":
            raise ValueError(f"terminator {name} cannot produce an SSA result")
        if name in names or spelling in spellings:
            raise ValueError(f"duplicate opcode name or text spelling: {name} / {spelling}")
        names.add(name)
        spellings.add(spelling)
        ops.append((name, spelling))
    if not ops:
        raise ValueError("ops.def has no opcodes")
    return ops


def table_rows(text: str, heading: str) -> tuple[int, int, list[tuple[str, str, str]]]:
    """Return the row interval and (opcode, spelling, six prose cells) in that interval."""
    headings = list(re.finditer(r"^" + re.escape(heading) + r"[^\n]*$", text, re.M))
    if len(headings) != 1:
        raise ValueError(f"expected exactly one {heading} heading")
    start = headings[0].end()
    following = re.search(r"^## ", text[start:], re.M)
    end = start + following.start() if following else len(text)
    rows, first, last = [], None, None
    for match in re.finditer(r"^\| `[^\n]*$", text[start:end], re.M):
        row = re.fullmatch(r"\| `(\w+)` / `([^`]+)` \| (.*) \|", match.group())
        if not row:
            raise ValueError(f"malformed Appendix A row: {match.group()}")
        cells = re.split(r"(?<!\\)\|", row[3])
        if len(cells) != 6 or any(not cell.strip() for cell in cells):
            raise ValueError(f"{row[1]} needs six nonempty description cells")
        offset = start + match.start()
        if last is not None and text[last:offset] != "\n":
            raise ValueError("Appendix A rows must be contiguous")
        if first is None:
            first = offset
        last = start + match.end()
        rows.append((row[1], row[2], row[3]))
    if first is None or last is None:
        raise ValueError("Appendix A has no description rows")
    return first, last, rows


def render(text: str, heading: str, ops: list[tuple[str, str]]) -> str:
    first, last, rows = table_rows(text, heading)
    descriptions = {}
    for name, _, cells in rows:
        if name in descriptions:
            raise ValueError(f"duplicate Appendix A description: {name}")
        descriptions[name] = cells
    expected = {name for name, _ in ops}
    missing = [name for name, _ in ops if name not in descriptions]
    obsolete = [name for name in descriptions if name not in expected]
    if missing or obsolete:
        raise ValueError(f"missing descriptions: {', '.join(missing) or 'none'}; "
                         f"obsolete descriptions: {', '.join(obsolete) or 'none'}")
    lines = [f"| `{name}` / `{spelling}` | {descriptions[name]} |" for name, spelling in ops]
    return text[:first] + "\n".join(lines) + text[last:]


def updates(root: Path) -> list[tuple[str, str, str]]:
    ops = read_ops((root / OPS).read_text(encoding="utf-8"))
    result = []
    for rel, heading in PROPOSALS:
        text = (root / rel).read_text(encoding="utf-8")
        try:
            result.append((rel, text, render(text, heading, ops)))
        except ValueError as error:
            raise ValueError(f"{rel}: {error}") from error
    return result


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--root", type=Path, default=ROOT)
    mode = parser.add_mutually_exclusive_group()
    mode.add_argument("--check", action="store_true", help="fail if either table needs regeneration")
    mode.add_argument("--write", action="store_true", help="regenerate both tables after validation")
    args = parser.parse_args()
    try:
        changes = updates(args.root)
    except (OSError, ValueError) as error:
        print(f"sir op inventory: {error}", file=sys.stderr)
        return 2
    changed = False
    for rel, before, after in changes:
        if before == after:
            continue
        changed = True
        if args.write:
            (args.root / rel).write_text(after, encoding="utf-8")
        elif args.check:
            print(f"sir op inventory: regenerate {rel}", file=sys.stderr)
        else:
            sys.stdout.writelines(difflib.unified_diff(before.splitlines(keepends=True),
                                                     after.splitlines(keepends=True),
                                                     fromfile=f"a/{rel}", tofile=f"b/{rel}"))
    return int(args.check and changed)


if __name__ == "__main__":
    sys.exit(main())
