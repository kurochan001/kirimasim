#!/usr/bin/env python3
"""Checks the global state of src/ against its allowlist (RUNTIME_ABI.md
9.3, 9.4).

clang-tidy finds global state: cppcoreguidelines-avoid-non-const-global-
variables, and custom-kirimasim-global-state of .clang-tidy, which runs only
with --experimental-custom-checks. This script checks the two things around
them:

1. Each suppression of those checks in src/ covers one variable listed in
   tools/global_state_allowlist.txt, and each listed variable has its
   suppression. A suppression is a NOLINTNEXTLINE (or a NOLINT on the line
   of the declaration) that names the checks. In src/, a NOLINT without a
   list of checks, one whose list names them by a pattern (custom-*, *),
   and a NOLINTBEGIN that covers them are not allowed: they would hide new
   global state.
2. With --clang-tidy, that clang-tidy with the repository's .clang-tidy
   reports each kind of global state in a probe put under src/ in a
   temporary directory, and nothing that is allowed: a negative control
   that fails when the custom check does not run (a clang-tidy without it,
   or a missing --experimental-custom-checks).

  tools/check_global_state.py [--root DIR] [--clang-tidy PATH]
"""

import argparse
import fnmatch
import platform
import re
import subprocess
import sys
import tempfile
from pathlib import Path

CHECKS = ("cppcoreguidelines-avoid-non-const-global-variables", "custom-kirimasim-global-state")
SUFFIXES = {".h", ".hpp", ".cpp", ".cc", ".cxx", ".inc", ".ipp"}
NOLINT = re.compile(r"\bNOLINT(NEXTLINE|BEGIN|END)?\b(?:\(([^)]*)\))?")
# The name declared on a line: the first identifier followed by =, ;, { or [.
DECLARED = re.compile(r"([A-Za-z_]\w*)\s*(?:=|;|\{|\[)")

# Each line ends with "// global" when the custom check must report it, or
# "// ok" when nothing may.
PROBE = """\
#include <csignal>
#include <set>
#include <string>
#include <string_view>

namespace probe {
int g1 = 0;  // global
const std::set<int> g2 = {1, 2};  // global
const int g3 = 3;  // global
thread_local int g4 = 0;  // global
volatile std::sig_atomic_t g5 = 0;  // global
constexpr int g6 = 6;  // ok
constinit const int g7 = 7;  // ok
constexpr std::string_view g8[] = {"a"};  // ok
struct S {
    static int m1;  // global
    static const std::string m2;  // global
    inline static int m3 = 0;  // global
    static constexpr int m4 = 4;  // ok
};
int S::m1 = 0;  // global
const std::string S::m2 = "m2";  // global
template <class T>
struct Tm {
    static T t;  // global
};
inline int f() {
    static int l1 = 0;  // global
    static const std::set<std::string_view> l2 = {"a"};  // global
    thread_local int l3 = 0;  // global
    static constexpr std::string_view l4[] = {"a"};  // ok
    int local = 0;  // ok
    return ++l1 + l3 + local + static_cast<int>(l2.size() + l4[0].size());
}
}  // namespace probe
"""


def covers(names: str | None) -> bool:
    """Whether a NOLINT with this list of checks covers the checks."""
    if names is None:
        return True
    items = [n.strip() for n in names.split(",") if n.strip()]
    return any(fnmatch.fnmatchcase(c, item) for c in CHECKS for item in items)


def names_exactly(names: str | None) -> bool:
    items = [n.strip() for n in (names or "").split(",")]
    return any(item in CHECKS for item in items) and not any(
        "*" in item or "?" in item for item in items)


def read_allowlist(path: Path, errors: list[str]) -> dict[tuple[str, str], int]:
    entries: dict[tuple[str, str], int] = {}
    if not path.is_file():
        errors.append(f"{path.name}: missing")
        return entries
    for i, line in enumerate(path.read_text(encoding="utf-8").splitlines(), start=1):
        if not line.strip() or line.lstrip().startswith("#"):
            continue
        fields = [f.strip() for f in line.split("|")]
        if len(fields) != 4 or not all(fields) or not fields[2].startswith("9.4 item "):
            errors.append(f"{path.name}:{i}: not '<file> | <variable> | 9.4 item <n> | <reason>'")
            continue
        key = (fields[0], fields[1])
        if key in entries:
            errors.append(f"{path.name}:{i}: {fields[0]} {fields[1]} is listed twice")
        entries[key] = i
    return entries


def check_suppressions(root: Path, errors: list[str]) -> int:
    allowlist = root / "tools" / "global_state_allowlist.txt"
    entries = read_allowlist(allowlist, errors)
    used: set[tuple[str, str]] = set()
    checked = 0
    src = root / "src"
    for path in sorted(src.rglob("*")) if src.is_dir() else []:
        if not (path.is_file() and path.suffix in SUFFIXES):
            continue
        checked += 1
        display = path.relative_to(root).as_posix()
        lines = path.read_text(encoding="utf-8", errors="replace").splitlines()
        for i, line in enumerate(lines):
            for m in NOLINT.finditer(line):
                kind, names = m.group(1), m.group(2)
                where = f"{display}:{i + 1}"
                if kind == "END":
                    continue
                if names is None:
                    errors.append(f"{where}: a NOLINT in src/ names the checks it suppresses")
                    continue
                if not covers(names):
                    continue
                if kind == "BEGIN" or not names_exactly(names):
                    errors.append(f"{where}: global state is suppressed one declaration at a "
                                  f"time, by the checks' names ({', '.join(CHECKS)})")
                    continue
                target = lines[i + 1] if kind == "NEXTLINE" and i + 1 < len(lines) else line
                d = DECLARED.search(target.split("//")[0])
                key = (display, d.group(1) if d else "")
                if key not in entries:
                    errors.append(f"{where}: global state '{key[1]}' is not in "
                                  f"tools/{allowlist.name} (RUNTIME_ABI.md 9.4)")
                used.add(key)
    for (file, name), i in entries.items():
        if (file, name) not in used:
            errors.append(f"{allowlist.name}:{i}: {file} {name} has no suppression in src/")
    return checked


def check_probe(root: Path, clang_tidy: str, errors: list[str]) -> None:
    want = {i for i, line in enumerate(PROBE.splitlines(), start=1) if line.endswith("// global")}
    with tempfile.TemporaryDirectory() as tmp:
        probe = Path(tmp) / "src" / "kernel" / "probe.cpp"
        probe.parent.mkdir(parents=True)
        probe.write_text(PROBE, encoding="utf-8")
        cmd = [clang_tidy, "--experimental-custom-checks", f"--config-file={root / '.clang-tidy'}",
               "--quiet", str(probe), "--", "-std=c++23"]
        if platform.system() == "Darwin":
            sdk = subprocess.run(["xcrun", "--show-sdk-path"], capture_output=True, text=True,
                                 check=False).stdout.strip()
            if sdk:
                cmd.append(f"-isysroot{sdk}")
        p = subprocess.run(cmd, capture_output=True, text=True, check=False)
    got = {int(m.group(1)) for m in re.finditer(
        r"probe\.cpp:(\d+):\d+: (?:warning|error): .*\[custom-kirimasim-global-state",
        p.stdout + p.stderr)}
    if got != want:
        errors.append(f"probe: clang-tidy reported global state on lines {sorted(got)}, "
                      f"expected {sorted(want)}\n{p.stdout}{p.stderr}")


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("--root", type=Path, default=Path(__file__).resolve().parent.parent,
                    help="repository root (default: parent of tools/)")
    ap.add_argument("--clang-tidy", default=None,
                    help="also run the probe through this clang-tidy")
    args = ap.parse_args()

    errors: list[str] = []
    checked = check_suppressions(args.root, errors)
    if args.clang_tidy:
        check_probe(args.root, args.clang_tidy, errors)
    for e in errors:
        print(e, file=sys.stderr)
    if errors:
        print(f"check_global_state: {len(errors)} problem(s)", file=sys.stderr)
        return 1
    probe = ", probe reported as expected" if args.clang_tidy else ""
    print(f"check_global_state: {checked} file(s) checked{probe}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
