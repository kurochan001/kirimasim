#!/usr/bin/env python3
# Copyright (c) 2026 The kirimasim Authors
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
"""tools/sir_schema_drift.py reports every difference between SIR's code and the drafted schema
(P2 review H1a): on small inputs written here, and on the repository, where it only reports."""
import subprocess
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent.parent
TOOL = ROOT / "tools/sir_schema_drift.py"
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

    real = run(ROOT)
    if real.returncode != 0 or "sir schema drift:" not in real.stdout:
        failures.append(f"the report on the repository fails: {real.stdout[-300:]}{real.stderr[-300:]}")
    for failure in failures:
        print("FAIL: " + failure, file=sys.stderr)
    if failures:
        return 1
    print("sir schema drift: the report names every difference and fails only with --strict")
    return 0


if __name__ == "__main__":
    sys.exit(main())
