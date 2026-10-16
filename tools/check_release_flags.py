#!/usr/bin/env python3
"""Checks that the release build really optimizes kirimasim's own code.

Reads build/release/compile_commands.json and requires every entry whose
source file is under the repository's src/ or tests/ to have -O2 or -O3,
with no later -O0, -O1, -Og, -Os, -Oz or plain -O that would override it.

Entries for fetched dependencies (slang, GoogleTest, ...; they live under
build/.../_deps/) are not checked. The check fails if it finds no entry to
check.
"""

import argparse
import json
import shlex
import sys
from pathlib import Path

GOOD = {"-O2", "-O3"}


def arguments(entry: dict) -> list[str]:
    if "arguments" in entry:
        return list(entry["arguments"])
    return shlex.split(entry["command"])


def final_opt_level(args: list[str]) -> str | None:
    last = None
    for a in args:
        if a.startswith("-O"):
            last = a
    return last


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    root_default = Path(__file__).resolve().parent.parent
    ap.add_argument("--root", type=Path, default=root_default, help="repository root")
    ap.add_argument("--compile-commands", type=Path, default=None,
                    help="default: <root>/build/release/compile_commands.json")
    args = ap.parse_args()

    root = args.root.resolve()
    cc_path = args.compile_commands or root / "build" / "release" / "compile_commands.json"
    entries = json.loads(cc_path.read_text(encoding="utf-8"))
    targets = [root / "src", root / "tests"]

    checked = 0
    errors = []
    for e in entries:
        f = Path(e["file"])
        if not f.is_absolute():
            f = Path(e["directory"]) / f
        f = f.resolve()
        if not any(f.is_relative_to(t) for t in targets):
            continue
        checked += 1
        level = final_opt_level(arguments(e))
        if level not in GOOD:
            errors.append(f"{f.relative_to(root)}: last optimization flag is {level or 'missing'}")

    for err in errors:
        print(err, file=sys.stderr)
    if checked == 0:
        print("check_release_flags: no kirimasim translation units found", file=sys.stderr)
        return 1
    if errors:
        print(f"check_release_flags: {len(errors)} of {checked} unit(s) not optimized",
              file=sys.stderr)
        return 1
    print(f"check_release_flags: {checked} unit(s) built with -O2/-O3")
    return 0


if __name__ == "__main__":
    sys.exit(main())
