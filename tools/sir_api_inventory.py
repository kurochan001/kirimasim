#!/usr/bin/env python3
# Copyright (c) 2026 The kirimasim Authors
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
"""Names the functions of vm.cpp that the inventory of docs/notes/sir-v1-proposal.md points at.

The proposal's table 2-d and Appendix B were written with line numbers of src/vm/vm.cpp at
the commit of the proposal (SNAPSHOT), and line numbers drift (P2 review L1). This tool reads
vm.cpp at that commit and at the working tree, finds the function that holds each line, and:

* `report` (the default) prints, for each row of Appendix B, the functions of the snapshot
  lines, and whether the reference still occurs in the working tree's vm.cpp; it exits 1 when
  a reference no longer occurs anywhere, so that the inventory is checked again before the
  freeze;
* `--names <lines>` prints the functions of a list such as "660–680, 735–785".

A function is found lexically: at the top level of the file, outside namespaces, a header
that holds "(" and opens a body with "{" starts a function; the body ends where the braces
close. clang-format keeps vm.cpp in this shape. A reference is matched as written in the
table, as whole identifiers. The proposal's own cross-check with the compiler's AST is not
repeated here (not confirmed by this tool).
"""
import argparse
import re
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
SNAPSHOT = "dfd5f7c2"
VM = "src/vm/vm.cpp"
PROPOSAL = "docs/notes/sir-v1-proposal.md"


def strip_code(text: str) -> str:
    """Comments, strings and character literals blanked, lines kept."""
    out, i, n = [], 0, len(text)
    while i < n:
        c = text[i]
        if text.startswith("//", i):
            j = text.find("\n", i)
            j = n if j < 0 else j
            out.append(" " * (j - i))
            i = j
        elif text.startswith("/*", i):
            j = text.find("*/", i + 2)
            j = n if j < 0 else j + 2
            out.append(re.sub(r"[^\n]", " ", text[i:j]))
            i = j
        elif c == '"' and text.startswith('R"', i - 1) is False and text[i - 1:i] != "'":
            j = i + 1
            while j < n and text[j] != '"':
                j += 2 if text[j] == "\\" else 1
            out.append('"' + " " * (j - i - 1) + '"')
            i = j + 1
        elif c == "'" and i + 2 < n and not text[i - 1].isalnum():
            j = i + 1
            while j < n and text[j] != "'":
                j += 2 if text[j] == "\\" else 1
            out.append("'" + " " * (j - i - 1) + "'")
            i = j + 1
        else:
            out.append(c)
            i += 1
    return "".join(out)


def functions(source: str) -> list:
    """(name, first line, last line) of each function defined in a namespace or a class body.
    A member function defined in its class is named Class::name."""
    # Preprocessor lines end with no ";", so they would join the next header.
    code = re.sub(r"(?m)^[ \t]*#.*$", lambda m: " " * len(m.group(0)), strip_code(source))
    found, header, header_line, line = [], "", 1, 1
    # Each frame: (kind, name, first line); kinds namespace and class take declarations.
    stack = [("namespace", "", 1)]
    for c in code:
        if c == "\n":
            line += 1
        declarations = stack[-1][0] in ("namespace", "class")
        if c == "{":
            if not declarations:
                stack.append(("other", "", line))
                continue
            text = " ".join(header.split())
            text = re.sub(r"^(public|private|protected)\s*:\s*", "", text)
            prefix = stack[-1][1]
            if re.match(r"^(inline\s+)?namespace\b|^extern\s+\"C\"", text):
                stack.append(("namespace", prefix, line))
            elif re.match(r"^(template\s*<.*>\s*)?(struct|class|union)\s+(\w+)", text) and "(" not in text.split("{")[0].split(":")[0]:
                name = re.match(r"^(template\s*<.*>\s*)?(struct|class|union)\s+(\w+)", text).group(3)
                stack.append(("class", f"{prefix}{name}::", header_line))
            elif "(" in text and not re.match(r"^enum\b", text):
                before = text[:text.index("(")]
                if "=" in before:
                    before = before[:before.index("=")]
                words = re.findall(r"[~\w:]+|operator\S*", before)
                name = words[-1] if words else "(lambda)"
                stack.append(("function", prefix + name, header_line))
            else:
                stack.append(("other", "", line))
            header = ""
            continue
        if c == "}":
            kind, name, first = stack.pop()
            if kind == "function":
                found.append((name, first, line))
            header = ""
            continue
        if declarations:
            if c == ";":
                header = ""
                continue
            if not header.strip() and not c.isspace():
                header_line = line
            header += c
    return found


def holder(table: list, line: int) -> str:
    for name, first, last in table:
        if first <= line <= last:
            return name
    return "(between functions)"


def lines_of(text: str) -> list:
    out = []
    for part in re.split(r"[,;]", text):
        bounds = re.findall(r"\d+", part)
        if len(bounds) == 1:
            out.append(int(bounds[0]))
        elif len(bounds) >= 2:
            out.extend(range(int(bounds[0]), int(bounds[1]) + 1))
    return out


def names_of(table: list, text: str) -> list:
    names = []
    for line in lines_of(text):
        name = holder(table, line)
        if name not in names:
            names.append(name)
    return names


def appendix_rows(proposal: str) -> list:
    """(reference, second cell) of each row of Appendix B: snapshot lines or function names."""
    at = proposal.index("## Appendix B.")
    rows = []
    for line in proposal[at:].splitlines():
        match = re.match(r"^\| `([^`]+)` \| ([^|]+) \|", line)
        if match:
            rows.append((match.group(1), match.group(2).strip()))
    return rows


def snapshot_source(root: Path) -> str:
    return subprocess.run(["git", "-C", str(root), "show", f"{SNAPSHOT}:{VM}"], capture_output=True, text=True,
                          check=True, stdin=subprocess.DEVNULL).stdout


def lexical(reference: str) -> bool:
    """Whether a reference can be found lexically: not an operator, a template argument, or a
    member named through its type (Type::member), whose receiver only the compiler knows."""
    if "operator" in reference or "<" in reference:
        return False
    parts = re.sub(r"^(kirimasim::)?kernel::", "", reference).split("::")
    return not any(p[:1].isupper() for p in parts[:-1])


def occurs(code: str, reference: str) -> bool:
    """A namespace may be left out where the code is written inside it or imports it."""
    reference = re.sub(r"^(kirimasim::)?kernel::", "", reference)
    parts = [re.escape(p) for p in re.split(r"(?:\.|->|::)", reference)]
    for start in range(len(parts)):
        if start and "::" not in reference:
            break
        pattern = r"\b" + r"\s*(?:\.|->|::)\s*".join(parts[start:]) + r"\b"
        if re.search(pattern, code):
            return True
    return False


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--root", type=Path, default=ROOT)
    parser.add_argument("--names", help="print the snapshot functions of these lines")
    args = parser.parse_args()
    snapshot = functions(snapshot_source(args.root))
    if args.names:
        print(", ".join(names_of(snapshot, args.names)))
        return 0
    sources = sorted((args.root / "src/vm").glob("*.cpp"))
    current = "\n".join(strip_code(p.read_text(encoding="utf-8")) for p in sources)
    defined = {name.split("::")[-1] for p in sources for name, _, _ in functions(p.read_text(encoding="utf-8"))}
    rows = appendix_rows((args.root / PROPOSAL).read_text(encoding="utf-8"))
    gone, unchecked, moved = [], [], []
    for reference, cell in rows:
        names = names_of(snapshot, cell) if re.fullmatch(r"[\d, –-]+", cell) else [n.strip("` ") for n in cell.split(",")]
        notes = []
        if not lexical(reference):
            unchecked.append(reference)
            notes.append("not checked lexically")
        elif not occurs(current, reference):
            gone.append(reference)
            notes.append("no longer in src/vm")
        missing = [n for n in names if not n.startswith("(") and n.split("::")[-1] not in defined]
        if missing:
            moved.append(reference)
            notes.append("function gone: " + ", ".join(missing))
        print(f"{reference}: {', '.join(names)}{'  [' + '; '.join(notes) + ']' if notes else ''}")
    print(f"sir api inventory: {len(rows)} references; {len(gone)} no longer in src/vm, "
          f"{len(moved)} with a function gone, {len(unchecked)} not checked lexically")
    return int(bool(gone or moved))


if __name__ == "__main__":
    sys.exit(main())
