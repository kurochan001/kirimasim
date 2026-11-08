#!/usr/bin/env python3
# Copyright (c) 2026 The kirimasim Authors
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
"""Reports where SIR's structures and opcodes have moved away from the drafted v1 schema.

docs/notes/sir-v1-proposal.md (D1) drafts a record per structure of design.h with its fields
in order, and lists every opcode in Appendix A. P1 keeps adding fields and opcodes, so the
draft falls behind (P2 review H1a). This tool compares, without changing anything:

* each record of the D1 table with the fields of the struct of the same name in design.h;
* the Op enum of design.h with the names opName gives in types.cpp;
* the Op enum with the opcodes of Appendix A.

It prints every difference. It exits 0 unless --strict is given, so that it can run on every
CI run before the schema is made machine-readable (H1b) and the differences are settled. The
struct fields are read lexically: a member is a line `type name;` or `type name = value;` at
the top level of the struct body. Fields the schema leaves out on purpose are listed in
DERIVED with the reason, and reported apart.
"""
import argparse
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
DESIGN = "src/sir/include/kirimasim/sir/design.h"
TYPES = "src/sir/types.cpp"
OPS = "src/sir/include/kirimasim/sir/ops.def"
RECORDS = "src/sir/include/kirimasim/sir/records.def"
PROPOSAL = "docs/notes/sir-v1-proposal.md"
# Records whose struct has another name in design.h.
STRUCTS = {"block": "BasicBlock"}
# Fields the schema leaves out on purpose: struct.field -> reason.
DERIVED = {
    "Slot.offset": "derived when the slots are laid out, not part of the text",
}


def struct_fields(source: str, name: str):
    """The members of `struct name { ... };`, in order, or None when there is no such struct."""
    match = re.search(r"\nstruct " + re.escape(name) + r"\s*(?:final\s*)?\{(.*?)\n\};", source, re.S)
    if not match:
        return None
    fields, depth = [], 0
    for line in match.group(1).splitlines():
        text = re.sub(r"//.*", "", line).strip()
        skip = ("struct", "enum", "using", "static", "[[", "friend", "}", "template", "public:", "private:")
        if depth == 0 and text and not text.startswith(skip) and "(" not in text.split("=")[0]:
            member = re.match(r".*?([A-Za-z_]\w*)\s*(=[^;]*|\{[^;]*\})?;$", text)
            if member:
                fields.append(member.group(1))
        depth += line.count("{") - line.count("}")
    return fields


def schema_records(proposal: str) -> dict:
    """The D1 table: record name -> fields. A row may hold several records split by ' / '."""
    section = proposal[proposal.index("### D1."):proposal.index("### D2.")]
    records, fenced = {}, False
    for line in section.splitlines():
        if line.lstrip().startswith("```"):
            fenced = not fenced
            continue
        if fenced:
            continue
        cells = [c.strip() for c in line.strip().strip("|").split("|")]
        if len(cells) != 2 or cells[0] in ("Record", "---"):
            continue
        names = [n.strip() for n in cells[0].split(" / ")]
        groups = [g.strip() for g in cells[1].split(" / ")]
        if len(names) != len(groups):
            continue
        for name, group in zip(names, groups):
            records[name] = [f.strip() for f in group.split(",") if f.strip()]
    return records


def op_enum(source: str) -> list:
    match = re.search(r"enum class Op\b[^{]*\{(.*?)\};", source, re.S)
    body = re.sub(r"//[^\n]*", "", match.group(1)) if match else ""
    return [e.strip().split("=")[0].strip() for e in body.split(",") if e.strip()]


def op_names(types: str) -> dict:
    return dict(re.findall(r'case Op::(\w+):\s*return\s*"([^"]+)"', types))


def appendix_ops(proposal: str) -> list:
    at = proposal.index("## Appendix A.")
    end = proposal.find("\n## ", at + 1)
    section = proposal[at:end if end >= 0 else len(proposal)]
    return re.findall(r"^\| `(\w+)` / `[^`]+` \|", section, re.M)


def file_records(text: str) -> tuple:
    """The records of records.def by struct, its fields in order, and its derived fields."""
    records, derived = {}, {}
    for struct, member, kind in re.findall(r"^KIRIMASIM_SIR_FIELD\((\w+), (\w+), (\w+)\)", text, re.M):
        if kind == "derived":
            derived[f"{struct}.{member}"] = "derived (records.def), not written"
        else:
            records.setdefault(struct, []).append(member)
    for struct in re.findall(r"^KIRIMASIM_SIR_RECORD\((\w+), ", text, re.M):
        records.setdefault(struct, [])
    return records, derived


def report(root: Path) -> list:
    design = (root / DESIGN).read_text(encoding="utf-8")
    types = (root / TYPES).read_text(encoding="utf-8")
    proposal = (root / PROPOSAL).read_text(encoding="utf-8")
    lines = []
    records_file = root / RECORDS
    if records_file.exists():
        # One line per field, checked against design.h at compile time (H1b step 2).
        records, derived_fields = file_records(records_file.read_text(encoding="utf-8"))
        by_struct = {struct: (struct, fields) for struct, fields in records.items()}
    else:
        derived_fields = DERIVED
        by_struct = {record: (STRUCTS.get(record, record[0].upper() + record[1:]), fields)
                     for record, fields in schema_records(proposal).items()}
    for record, (struct, fields) in by_struct.items():
        have = struct_fields(design, struct)
        if have is None:
            lines.append(f"record {record}: no struct {struct} in design.h")
            continue
        missing = [f for f in have if f not in fields and f"{struct}.{f}" not in derived_fields]
        derived = [f for f in have if f not in fields and f"{struct}.{f}" in derived_fields]
        extra = [f for f in fields if f not in have]
        if missing:
            lines.append(f"{struct}: fields not in the schema: {', '.join(missing)}")
        if extra:
            lines.append(f"{struct}: schema fields not in the struct: {', '.join(extra)}")
        for field in derived:
            lines.append(f"{struct}: {field} left out on purpose: {derived_fields[f'{struct}.{field}']}")
    ops = root / OPS
    if ops.exists():
        # One line per opcode makes both the enum and opName (H1b).
        listed_ops = re.findall(r'^KIRIMASIM_SIR_OP\((\w+), "([^"]+)"\)', ops.read_text(encoding="utf-8"), re.M)
        enum, names = [n for n, _ in listed_ops], dict(listed_ops)
    else:
        enum, names = op_enum(design), op_names(types)
    listed = appendix_ops(proposal)
    lines.append(f"Op: {len(enum)} in the enum, {len(names)} named in opName, {len(listed)} in Appendix A")
    for label, items in (("Op in the enum without an opName case", [o for o in enum if o not in names]),
                         ("opName case not in the enum", [o for o in names if o not in enum]),
                         ("Op in the enum not in Appendix A", [o for o in enum if o not in listed]),
                         ("Appendix A opcode not in the enum", [o for o in listed if o not in enum])):
        if items:
            lines.append(f"{label} ({len(items)}): {', '.join(items)}")
    return lines


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--root", type=Path, default=ROOT)
    parser.add_argument("--strict", action="store_true", help="exit 1 when anything differs")
    args = parser.parse_args()
    lines = report(args.root)
    for line in lines:
        print(line)
    differs = [l for l in lines if not l.startswith("Op: ") and "left out on purpose" not in l]
    print(f"sir schema drift: {len(differs)} difference(s)")
    return int(args.strict and bool(differs))


if __name__ == "__main__":
    sys.exit(main())
