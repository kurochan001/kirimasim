#!/usr/bin/env python3
"""Checks that the judging tools fail when they should (TEST_SUITE.md,
negative controls): each case changes one thing in a copy of a test or of an
expectation and expects the tool to reject it, next to the unchanged copy,
which must pass.
"""

import argparse
import json
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent.parent
TOOLS = ROOT / "tools"
failures = []


def expect(name: str, cmd: list[str], want_ok: bool) -> None:
    p = subprocess.run(cmd, capture_output=True, text=True, check=False)
    if (p.returncode == 0) != want_ok:
        failures.append(f"{name}: exit code {p.returncode}, expected "
                        f"{'0' if want_ok else 'nonzero'}\n{p.stdout}{p.stderr}")


def suite_test(haremasim: str, tmp: Path) -> None:
    """run_suite_test.py on a copy of values/unique_case_violation."""
    src = ROOT / "tests/suite/values/unique_case_violation"
    run = [sys.executable, str(TOOLS / "run_suite_test.py"), "--haremasim", haremasim]

    def case(name: str, edit, want_ok: bool) -> None:
        d = tmp / name
        d.mkdir()
        for ext in (".sv", ".expected", ".stderr"):
            shutil.copy(src.with_suffix(ext), d / ("unique_case_violation" + ext))
        edit(d / "unique_case_violation")
        expect("run_suite_test " + name, run + [str(d / "unique_case_violation.sv")], want_ok)

    def sub(ext: str, old: str, new: str):
        def edit(base: Path) -> None:
            f = base.with_suffix(ext)
            text = f.read_text(encoding="utf-8")
            assert old in text, (ext, old)
            f.write_text(text.replace(old, new, 1), encoding="utf-8")
        return edit

    case("unchanged", lambda base: None, True)
    case("blank_lines_ignored", sub(".expected", "\n", "\n\n  \n"), True)
    case("stdout_changed", sub(".expected", "hit=1", "hit=2"), False)
    case("exit_changed", sub(".sv", ":expected_stderr:", ":expected_exit: 1\n:expected_stderr:"),
         False)
    case("stderr_changed", sub(".stderr", "@ 2ns", "@ 1ns"), False)
    case("stderr_line_added", sub(".stderr", "\n", "\nunique_case_violation.sv:1: warning: x\n"),
         False)


def banned(tmp: Path) -> None:
    for name, line, want_ok in [
        ("pass_string", 'puts("TEST PASSED");', False),
        ("file_name", 'if (name.ends_with("-fail.sv")) {}', False),
        ("uvm_name", 'if (cls == "uvm_test") {}', False),
        ("metadata", "bool should_fail = true;", False),
        ("waived", '// haremasim: banned-ok needed for DPI-C\nreg("uvm_hdl_deposit");', True),
        ("clean", "int x = 1;", True),
    ]:
        root = tmp / ("banned_" + name)
        (root / "src/front").mkdir(parents=True)
        (root / "src/front/x.cpp").write_text(line + "\n", encoding="utf-8")
        expect("check_banned " + name,
               [sys.executable, str(TOOLS / "check_banned.py"), "--root", str(root)], want_ok)


def suite_files(tmp: Path) -> None:
    for name, files, want_ok in [("paired", ["a.sv", "a.expected", "a.stderr"], True),
                                 ("orphan", ["a.sv", "b.expected"], False)]:
        root = tmp / ("files_" + name)
        (root / "tests/suite/x").mkdir(parents=True)
        (root / "tests/dump_sir").mkdir(parents=True)
        for f in files:
            (root / "tests/suite/x" / f).write_text("", encoding="utf-8")
        expect("check_suite_files " + name,
               [sys.executable, str(TOOLS / "check_suite_files.py"), "--root", str(root)],
               want_ok)


def perfcount(haremasim: str, tmp: Path) -> None:
    test = "tests/suite/sched/nba_order_same_proc.sv"
    real = json.loads((ROOT / "tests/perf_counts/expected.json").read_text(encoding="utf-8"))
    base = real[test]
    for name, change, want_ok in [
        ("unchanged", {}, True),
        ("budget_exceeded", {"proc.wakeups": base["proc.wakeups"] // 2}, False),
        ("end_reason", {"sim.end_reason": "finish"}, False),
    ]:
        exp = tmp / f"perf_{name}.json"
        exp.write_text(json.dumps({test: {**base, **change}}), encoding="utf-8")
        expect("perfcount_check " + name,
               [sys.executable, str(TOOLS / "perfcount_check.py"), "--haremasim", haremasim,
                "--expected", str(exp), "--root", str(ROOT)], want_ok)


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("--haremasim", required=True)
    args = ap.parse_args()
    with tempfile.TemporaryDirectory() as t:
        tmp = Path(t)
        suite_test(args.haremasim, tmp)
        banned(tmp)
        suite_files(tmp)
        perfcount(args.haremasim, tmp)
    for f in failures:
        print("FAIL: " + f, file=sys.stderr)
    if failures:
        return 1
    print("tool tests: every judging tool rejects the changed copies")
    return 0


if __name__ == "__main__":
    sys.exit(main())
