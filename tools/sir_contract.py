#!/usr/bin/env python3
# Copyright (c) 2026 The kirimasim Authors
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
"""Render or check the reviewed, language-neutral contract for SIR format 1."""
import argparse
import difflib
import re
import sys
from pathlib import Path

from sir_op_inventory import read_ops

ROOT = Path(__file__).resolve().parent.parent
SCHEMA = Path("src/sir/include/kirimasim/sir")
SNAPSHOT = Path("docs/spec/sir-v1-contract.txt")


def records(source: str):
    for line in source.splitlines():
        line = line.partition("//")[0].strip()
        if not line:
            continue
        match = re.fullmatch(r'KIRIMASIM_(?:SIR_)?([A-Z_]+)\(([^()]*)\)', line)
        if not match:
            raise ValueError(f"malformed schema row: {line}")
        yield match[1], tuple(arg.strip().strip('"') for arg in match[2].split(","))


def snapshot(schema: Path) -> str:
    ops = (schema / "ops.def").read_text(encoding="utf-8")
    read_ops(ops)  # Includes duplicate names, spellings, waitness and target checks.
    lines = ["# SIR format 1 contract", "# Regenerate with python3 tools/sir_contract.py"]
    lines += ["", "# op name spelling result flow operands write-target Postponed-effect"]
    for tag, args in records(ops):
        if tag != "OP" or len(args) != 7:
            raise ValueError("format 1 requires seven columns for every opcode")
        lines.append("op " + " ".join(args))

    lines += ["", "# record struct tag ordered-members (~ means derived)",
              "# variant struct kind tag ordered-members"]
    members, variants = {}, {}
    for tag, args in records((schema / "records.def").read_text(encoding="utf-8")):
        if tag == "RECORD":
            members[args[0]] = [args[1]]
        elif tag == "FIELD":
            members[args[0]].append(("~" if args[2] == "derived" else "") + args[1])
        elif tag == "KIND":
            variants[args[:2]] = [args[2]]
        elif tag == "KIND_FIELD":
            variants[args[:2]].append(args[2])
        else:
            raise ValueError(f"unknown record row {tag}")
    for name, fields in members.items():
        lines.append("record " + " ".join([name, *fields]))
    for key, fields in variants.items():
        lines.append("variant " + " ".join([*key, *fields]))

    lines += ["", "# task name handler minimum-operands format-start",
              "# signature handler result tail [positional roles]",
              "# method name handler result receiver argument-1 argument-2"]
    kinds, named, methods = set(), set(), []
    for tag, args in records((schema / "systasks.def").read_text(encoding="utf-8")):
        if tag == "SYSTASK_KIND":
            if args[0] in kinds:
                raise ValueError(f"duplicate handler {args[0]}")
            kinds.add(args[0])
        elif tag == "SYSTASK":
            named.add(args[0])
            lines.append("task " + " ".join([args[1], args[0], *args[2:]]))
        elif tag == "METHOD":
            methods.append("method " + " ".join([args[1], args[0], *args[2:]]))
        else:
            raise ValueError(f"unknown task row {tag}")
    signed = set()
    for tag, args in records((schema / "task_signatures.def").read_text(encoding="utf-8")):
        if tag != "TASK_SIGNATURE" or args[0] in signed:
            raise ValueError(f"bad signature row {args[0]}")
        signed.add(args[0])
        lines.append("signature " + " ".join(args))
    if kinds != named or kinds != signed:
        raise ValueError("task handlers, registered names and signatures disagree")
    return "\n".join([*lines, *methods, ""])


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root", type=Path, default=ROOT)
    parser.add_argument("--check", action="store_true", help="compare the contract to the approved copy")
    args = parser.parse_args()
    try:
        generated = snapshot(args.root / SCHEMA)
        if not args.check:
            sys.stdout.write(generated)
            return 0
        before = (args.root / SNAPSHOT).read_text(encoding="utf-8")
    except (OSError, KeyError, ValueError) as error:
        print(f"sir contract: {error}", file=sys.stderr)
        return 2
    if before == generated:
        return 0
    sys.stdout.writelines(difflib.unified_diff(before.splitlines(True), generated.splitlines(True),
                                              fromfile=str(SNAPSHOT), tofile="generated"))
    return 1


if __name__ == "__main__":
    sys.exit(main())
