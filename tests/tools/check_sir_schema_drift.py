#!/usr/bin/env python3
# Copyright (c) 2026 The kirimasim Authors
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
"""tools/sir_schema_drift.py reports every difference between SIR's code and the drafted schema
(P2 review H1a): on small inputs written here, and on the repository, where it only reports, except
that records.def has to name every member of the structs it lists (H1b step 2)."""
import subprocess
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent.parent
TOOL = ROOT / "tools/sir_schema_drift.py"
INVENTORY = ROOT / "tools/sir_op_inventory.py"
failures = []

DESIGN = """namespace kirimasim::sir {
enum class Op : std::uint16_t {
    Load,
    Store,  // a comment
    Extra,
};
struct Slot {
    std::string name;
    Type type{};
    std::uint64_t offset = 0;  ///< derived
    std::vector<ArrayDim> dims;
    [[nodiscard]] bool empty() const { return dims.empty(); }
    bool operator==(const Slot&) const = default;  // comparison, not a member named operator
    // A brace { in a comment does not start a nested struct.
    struct Inner {
        int hidden;
    };
    SlotId viewOf{kNone};
};
struct BasicBlock {
    std::vector<BlockParam> params;
    std::vector<Instr> instrs;
};
}
"""
TYPES = """const char* opName(Op op) {
    switch (op) {
        case Op::Load: return "load";
        case Op::Store: return "store";
        case Op::Gone: return "gone";
    }
}
"""
PROPOSAL = """# P2

### D1. Text grammar

```ebnf
letter     = "A" | "Z" ;
```

| Record | Fields, in order |
| --- | --- |
| slot / block | name,type,dims,missing / params,instrs |

### D2. Next

## Appendix A. Complete Op inventory

| Op / text name | Operands |
| --- | --- |
| `Load` / `load` | slot range |
| `Old` / `old` | none |

## Appendix B. Other
"""


def run(root: Path, *extra: str) -> subprocess.CompletedProcess:
    return subprocess.run([sys.executable, str(TOOL), "--root", str(root), *extra], capture_output=True, text=True,
                          stdin=subprocess.DEVNULL)


def write(root: Path, design: str, types: str, proposal: str) -> None:
    for rel, text in (("src/sir/include/kirimasim/sir/design.h", design), ("src/sir/types.cpp", types),
                      ("docs/notes/sir-v1-proposal.md", proposal)):
        path = root / rel
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(text, encoding="utf-8")


def check_variant_records(directory: Path) -> None:
    """Exercise mistakes that previously gave zero differences even in strict mode."""
    root = directory / "variants"
    header = Path("src/sir/include/kirimasim/sir")
    for rel in (header / "design.h", header / "types.h", header / "ops.def",
                Path("src/sir/types.cpp"), Path("docs/notes/sir-v1-proposal.md")):
        dest = root / rel
        dest.parent.mkdir(parents=True, exist_ok=True)
        dest.write_bytes((ROOT / rel).read_bytes())
    path = root / header / "records.def"
    source = (ROOT / header / "records.def").read_text(encoding="utf-8")
    path.write_text(source, encoding="utf-8")
    result = run(root, "--strict")
    if result.returncode != 0:
        failures.append(f"valid variant schema rejected: {result.stdout}{result.stderr}")
    queue = "TypeEntryKind::Queue"
    field = f"KIRIMASIM_SIR_KIND_FIELD(TypeEntry, {queue}, maxElements)"
    enum_row = f'KIRIMASIM_SIR_KIND(TypeEntry, {queue}, "queue")'
    slot = 'KIRIMASIM_SIR_RECORD(Slot, "slot")'
    changes = [
        (source.replace(slot, slot + "\n" + slot), "duplicate record or tag"),
        (source.replace(slot, ''), "schema row for undeclared record Slot"),
        (source.replace('Slot, offset, derived)', 'Slot, offset, unknown)'), "malformed schema row"),
        (source.replace('Slot, offset, derived)', 'Slot, name, derived)'), "duplicate field: Slot.name"),
        (source.replace('Operand::Kind::Frame, "frame"', 'Operand::Kind::Frame, "slot"'), "duplicate kind or tag"),
        (source.replace(enum_row, ''), f"undeclared kind {queue}"),
        (source.replace(field, field + "\n" + field), f"duplicate kind field: {queue}.maxElements"),
        (source.replace(field, field.replace("maxElements", "absent")), f"kind field: {queue}.absent"),
        (source.replace(field, ""), "TypeEntry.maxElements: written by no kind"),
        (source.replace(queue, "TypeEntryKind::Typo"), "unknown kind TypeEntryKind::Typo"),
        ("\n".join(line for line in source.splitlines() if queue not in line), f"missing kind {queue}"),
        ("\n".join(line for line in source.splitlines() if "(Operand," not in line),
         "missing variant record: Operand"),
        ("// empty schema\n", "no records"),
    ]
    for broken, diagnostic in changes:
        path.write_text(broken, encoding="utf-8")
        result = run(root, "--strict")
        if result.returncode != 1 or diagnostic not in result.stdout:
            failures.append(f"missing {diagnostic}: {result.stdout}{result.stderr}")


def check_inventory(directory: Path) -> None:
    """Exercise real regeneration and failures that an opcode-count check misses."""
    root = directory / "inventory"
    en_rel = "docs/notes/sir-v1-proposal.md"
    ja_rel = "docs/notes/sir-v1-proposal.ja.md"
    ops_rel = "src/sir/include/kirimasim/sir/ops.def"
    en = PROPOSAL[:PROPOSAL.index("## Appendix A.")] + """## Appendix A. Test

| Op / text name | Operands | Types | Terminator | Suspension | Effects | Reference |
| --- | --- | --- | --- | --- | --- | --- |
| `Load` / `load` | source | result | no | no | read | original reference |
| `Store` / `store` | target, value | fits | no | no | write | other reference |

## Appendix B. Keep this text
"""
    ja = en.replace("## Appendix A.", "## 付録A.").replace("source", "入力").replace("target, value", "対象, 値")
    ops = 'KIRIMASIM_SIR_OP(Load, "load")\nKIRIMASIM_SIR_OP(Store, "store")\n'
    write(root, DESIGN.replace("    Extra,\n", "").replace("    SlotId viewOf{kNone};\n", ""),
          TYPES.replace('        case Op::Gone: return "gone";\n', ""), en)
    # Keep the record fixture in agreement too.
    en = en.replace("name,type,dims,missing", "name,type,dims")
    ja = ja.replace("name,type,dims,missing", "name,type,dims")

    def put(english: str = en, japanese: str = ja, source: str = ops) -> None:
        for rel, text in ((en_rel, english), (ja_rel, japanese), (ops_rel, source)):
            (root / rel).write_text(text, encoding="utf-8")

    def generate(*args: str) -> subprocess.CompletedProcess:
        return subprocess.run([sys.executable, str(INVENTORY), "--root", str(root), *args],
                              capture_output=True, text=True, stdin=subprocess.DEVNULL)

    def contents() -> tuple:
        return tuple((root / rel).read_bytes() for rel in (en_rel, ja_rel, ops_rel))

    typed_ops = ('KIRIMASIM_SIR_OP(Load, "load", Required, Next)\n'
                 'KIRIMASIM_SIR_OP(Store, "store", None, Next)\n')
    current_ops = typed_ops.replace("Next)", "Next, Custom)")
    target_ops = current_ops.replace("Next, Custom)", "Next, Custom, Other)", 1).replace(
        "None, Next, Custom)", "None, Next, Custom, FirstSlot)")
    for schema in (typed_ops, current_ops, target_ops):
        put(source=schema)
        if generate("--check").returncode:
            failures.append("inventory rejected the executable opcode schema")

    def reverse_rows(text: str) -> str:
        lines = text.splitlines(keepends=True)
        at = [i for i, line in enumerate(lines) if line.startswith("| `")]
        lines[at[0]], lines[at[1]] = lines[at[1]], lines[at[0]]
        return "".join(lines)

    put(reverse_rows(en).replace("`load`", "`old.load`"), reverse_rows(ja).replace("`store`", "`old.store`"))
    before = contents()
    diff = generate()
    if diff.returncode != 0 or not all(f"a/{rel}" in diff.stdout for rel in (en_rel, ja_rel)):
        failures.append(f"inventory does not propose both diffs: {diff.stdout}{diff.stderr}")
    if generate("--check").returncode != 1 or contents() != before:
        failures.append("inventory check/default writes or accepts stale names/order")
    if generate("--write").returncode != 0 or contents()[:2] != (en.encode(), ja.encode()):
        failures.append("inventory regeneration changes prose or fails to repair names/order")
    if generate("--check").returncode != 0 or generate().stdout:
        failures.append("inventory generation is not idempotent")
    if run(root, "--strict").returncode != 0:
        failures.append("strict drift check rejects matching bilingual inventory")

    cases = [
        (en.replace("`load`", "`wrong`"), ja, "Appendix A text name for Load"),
        (en, ja.replace("`load`", "`wrong`"), "Appendix A (Japanese) text name for Load"),
        (reverse_rows(en), ja, "Appendix A opcode order differs"),
        (en, reverse_rows(ja), "Appendix A (Japanese) opcode order differs"),
        (en.replace("| `Store`", "| `Load`"), ja, "Appendix A duplicate opcodes"),
        (en, ja.replace("| `Store`", "| `Old`"), "Appendix A (Japanese) opcode not in the enum"),
        (en, ja.replace("## 付録A.", "## Gone."), "Appendix A (Japanese): missing heading"),
    ]
    for english, japanese, diagnostic in cases:
        put(english, japanese)
        result = run(root, "--strict")
        if result.returncode != 1 or diagnostic not in result.stdout:
            failures.append(f"drift check misses {diagnostic}: {result.stdout}{result.stderr}")

    store_row = next(line for line in ja.splitlines(keepends=True) if line.startswith("| `Store`"))
    invalid = [
        (en, ja.replace(store_row, ""), ops, "missing descriptions: Store"),
        (en, ja.replace("| `Store`", "| `Old`"), ops, "obsolete descriptions: Old"),
        (en, ja.replace("| `Store`", "| `Load`"), ops, "duplicate Appendix A description"),
        (en, ja.replace("| fits |", "|  |"), ops, "six nonempty description cells"),
        (en, ja.replace("| fits |", "| fits | extra |"), ops, "six nonempty description cells"),
        (en, ja, ops + 'KIRIMASIM_SIR_OP(Load, "another")\n', "duplicate opcode"),
        (en, ja, ops + 'KIRIMASIM_SIR_OP(Other, "load")\n', "duplicate opcode"),
        (en, ja, ops + 'KIRIMASIM_SIR_OP(Broken)\n', "malformed ops.def"),
        (en, ja, target_ops.replace(", Other)", ", Unknown)"), "malformed ops.def"),
        (en, ja, target_ops.replace(", Other)", ")"), "mixed opcode schema versions"),
        (en, ja, target_ops.replace("Custom, FirstSlot", "OneValue, FirstSlot"), "plain SSA value"),
        (en, ja, current_ops.replace(", Custom)", ", Unknown)"), "malformed ops.def"),
        (en, ja, current_ops.replace(", Custom)", ")", 1), "mixed opcode schema versions"),
        (en, ja, typed_ops.replace("Required", "Unknown"), "malformed ops.def"),
        (en, ja, typed_ops.replace("None, Next", "None, Unknown"), "malformed ops.def"),
        (en, ja, typed_ops.replace(", None, Next", ""), "mixed opcode schema versions"),
        (en, ja, typed_ops.replace("Required, Next", "Required, Wait"), "cannot produce an SSA result"),
        (en, ja, "// no records\n", "no opcodes"),
    ]
    for english, japanese, source, diagnostic in invalid:
        # English also needs regeneration: invalid Japanese must not leave a partial update.
        put(reverse_rows(english), japanese, source)
        before = contents()
        result = generate("--write")
        if result.returncode != 2 or diagnostic not in result.stderr or contents() != before:
            failures.append(f"inventory misses {diagnostic} or writes on validation failure: {result.stderr}")


def main() -> int:
    with tempfile.TemporaryDirectory() as directory:
        root = Path(directory) / "drift"
        write(root, DESIGN, TYPES, PROPOSAL)
        result = run(root)
        out = result.stdout
        for want in ("Slot: fields not in the schema: viewOf",
                     "Slot: schema fields not in the struct: missing",
                     "Slot: offset left out on purpose",
                     "Op: 3 in the enum, 3 named in opName, 2 in Appendix A",
                     "Op in the enum without an opName case (1): Extra",
                     "opName case not in the enum (1): Gone",
                     "Op in the enum not in Appendix A (2): Store, Extra",
                     "Appendix A opcode not in the enum (1): Old"):
            if want not in out:
                failures.append(f"not reported: {want}")
        for unwanted in ("hidden", "empty", "letter", "record block", "Slot: fields not in the schema: viewOf, offset"):
            if unwanted in out:
                failures.append(f"reported by mistake: {unwanted}")
        if result.returncode != 0:
            failures.append(f"exit {result.returncode} without --strict")
        if run(root, "--strict").returncode != 1:
            failures.append("--strict does not fail on a difference")

        same = Path(directory) / "same"
        write(same, DESIGN.replace("    Extra,\n", "").replace("    SlotId viewOf{kNone};\n", ""),
              TYPES.replace('        case Op::Gone: return "gone";\n', ""),
              PROPOSAL.replace("name,type,dims,missing", "name,type,dims").replace("| `Old` / `old` | none |",
                                                                                    "| `Store` / `store` | x |"))
        result = run(same, "--strict")
        if result.returncode != 0 or "0 difference(s)" not in result.stdout:
            failures.append(f"--strict fails with no difference: {result.stdout}")

        ops = Path(directory) / "ops"
        write(ops, DESIGN, TYPES, PROPOSAL)
        (ops / "src/sir/include/kirimasim/sir/ops.def").write_text(
            '// comment\nKIRIMASIM_SIR_OP(Load, "load")  ///< x\nKIRIMASIM_SIR_OP(Other, "other")\n', encoding="utf-8")
        out = run(ops).stdout
        for want in ("Op: 2 in the enum, 2 named in opName, 2 in Appendix A",
                     "Op in the enum not in Appendix A (1): Other", "Appendix A opcode not in the enum (1): Old"):
            if want not in out:
                failures.append(f"not reported from ops.def: {want}")
        if "Extra" in out or "Gone" in out:
            failures.append("the enum and opName are read although ops.def exists")

        records = Path(directory) / "records"
        write(records, DESIGN, TYPES, PROPOSAL)
        (records / "src/sir/include/kirimasim/sir/records.def").write_text(
            '// comment\nKIRIMASIM_SIR_RECORD(Slot, "slot")\nKIRIMASIM_SIR_FIELD(Slot, name, value)\n'
            'KIRIMASIM_SIR_FIELD(Slot, type, value)\nKIRIMASIM_SIR_FIELD(Slot, offset, derived)\n'
            'KIRIMASIM_SIR_FIELD(Slot, dims, value)\nKIRIMASIM_SIR_FIELD(Slot, gone, value)\n'
            'KIRIMASIM_SIR_RECORD(BasicBlock, "block")\nKIRIMASIM_SIR_FIELD(BasicBlock, params, value)\n'
            'KIRIMASIM_SIR_FIELD(BasicBlock, instrs, value)\n', encoding="utf-8")
        out = run(records).stdout
        for want in ("Slot: fields not in the schema: viewOf", "Slot: schema fields not in the struct: gone",
                     "Slot: offset left out on purpose: derived (records.def)"):
            if want not in out:
                failures.append(f"not reported from records.def: {want}")
        if "missing" in out or "BasicBlock: " in out:
            failures.append(f"the D1 table is read although records.def exists: {out}")

        check_variant_records(Path(directory))
        check_inventory(Path(directory))

    # SIR_SPEC section 9 fixes these record tags and field order. Omission of an
    # entire record or marking a required field as derived must fail here too.
    definitions = (ROOT / "src/sir/include/kirimasim/sir/records.def").read_text().splitlines()
    for struct, tag, fields in (
        ("SourceLoc", "loc", ("file", "line", "column")),
        ("SlotRange", "range", ("slot", "firstElem", "numElems", "lsb", "width")),
        ("ClockingInput", "clockingInput", ("source", "target")),
        ("ClockingBlock", "clockingBlock", ("clock", "edge", "event", "instance", "inputs", "loc")),
    ):
        actual = [line for line in definitions if
                  line.startswith((f"KIRIMASIM_SIR_RECORD({struct},", f"KIRIMASIM_SIR_FIELD({struct},"))]
        expected = [f'KIRIMASIM_SIR_RECORD({struct}, "{tag}")',
                    *(f"KIRIMASIM_SIR_FIELD({struct}, {field}, value)" for field in fields)]
        if actual != expected:
            failures.append(f"clocking interchange schema differs for {struct}: {actual}")
    real = run(ROOT)
    if real.returncode != 0 or "sir schema drift:" not in real.stdout:
        failures.append(f"the report on the repository fails: {real.stdout[-300:]}{real.stderr[-300:]}")
    # records.def names every member of every struct it lists (H1b step 2): strict for the fields.
    for line in real.stdout.splitlines():
        if "not in the schema" in line or "not in the struct" in line or line.startswith("record "):
            failures.append(f"records.def and design.h differ: {line}")
    for failure in failures:
        print("FAIL: " + failure, file=sys.stderr)
    if failures:
        return 1
    print("sir schema drift: the report names every difference and fails only with --strict")
    return 0


if __name__ == "__main__":
    sys.exit(main())
