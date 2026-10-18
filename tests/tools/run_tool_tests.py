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


def expect(name: str, cmd: list[str], want_ok: bool, cwd: Path | None = None) -> None:
    p = subprocess.run(cmd, capture_output=True, text=True, check=False, cwd=cwd)
    if (p.returncode == 0) != want_ok:
        failures.append(f"{name}: exit code {p.returncode}, expected "
                        f"{'0' if want_ok else 'nonzero'}\n{p.stdout}{p.stderr}")


def suite_test(kirimasim: str, tmp: Path) -> None:
    """run_suite_test.py on a copy of values/unique_case_violation."""
    src = ROOT / "tests/suite/values/unique_case_violation"
    run = [sys.executable, str(TOOLS / "run_suite_test.py"), "--kirimasim", kirimasim]

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

    def both(*edits):
        def edit(base: Path) -> None:
            for e in edits:
                e(base)
        return edit

    # A header that is not read as written (tools/suite_header.py); the
    # lines stay where they were, so that nothing else fails.
    case("header_not_first", sub(".sv", "/*\n", "/* note */ /*\n"), False)
    case("type_misspelt", sub(".sv", ":type: simulation", ":type: simulaton"), False)
    case("item_misspelt", sub(".sv", ":expected_stderr:", ":expected_sterr:"), False)
    case("required_missing", sub(".sv", ":lrm: 1800-2023", "lrm 1800-2023"), False)
    case("yes_no", sub(".sv", ":fourstate: no", ":fourstate: maybe"), False)
    # Allowed outputs of a nondeterministic test: an empty one is a mistake.
    nondeterministic = sub(".sv", ":nondeterministic: no", ":nondeterministic: yes")

    def before(text: str):
        def edit(base: Path) -> None:
            f = base.with_suffix(".expected")
            f.write_text(text + f.read_text(encoding="utf-8"), encoding="utf-8")
        return edit

    case("other_allowed_output", both(nondeterministic, before("x\n---\n")), True)
    case("empty_allowed_output", both(nondeterministic, before("---\n")), False)


def severity_test(kirimasim: str, tmp: Path) -> None:
    """run_suite_test.py on a copy of proc/severity_tasks: the fatal and info
    lines are compared too (TEST_SUITE.md, :expected_stderr:)."""
    src = ROOT / "tests/suite/proc/severity_tasks"
    run = [sys.executable, str(TOOLS / "run_suite_test.py"), "--kirimasim", kirimasim]

    def case(name: str, old: str, new: str, want_ok: bool) -> None:
        d = tmp / ("severity_" + name)
        d.mkdir()
        for ext in (".sv", ".expected", ".stderr"):
            shutil.copy(src.with_suffix(ext), d / ("severity_tasks" + ext))
        f = d / "severity_tasks.stderr"
        text = f.read_text(encoding="utf-8")
        assert old in text, old
        f.write_text(text.replace(old, new, 1), encoding="utf-8")
        expect("run_suite_test severity " + name, run + [str(d / "severity_tasks.sv")], want_ok)

    case("unchanged", "", "", True)
    case("info_line_missing", "severity_tasks.sv:32: info: info 4 [top.main @ 0ps]\n", "", False)
    case("fatal_line_changed", "fatal: fatal 4", "fatal: fatal 5", False)


def banned(tmp: Path) -> None:
    for name, line, want_ok in [
        ("pass_string", 'puts("TEST PASSED");', False),
        ("file_name", 'if (name.ends_with("-fail.sv")) {}', False),
        ("uvm_name", 'if (cls == "uvm_test") {}', False),
        ("metadata", "bool should_fail = true;", False),
        ("waived", '// kirimasim: banned-ok needed for DPI-C\nreg("uvm_hdl_deposit");', True),
        ("clean", "int x = 1;", True),
    ]:
        root = tmp / ("banned_" + name)
        (root / "src/front").mkdir(parents=True)
        (root / "src/front/x.cpp").write_text(line + "\n", encoding="utf-8")
        expect("check_banned " + name,
               [sys.executable, str(TOOLS / "check_banned.py"), "--root", str(root)], want_ok)


def banned_kernel(tmp: Path) -> None:
    """The constructs banned in src/kernel and src/vm, in every spelling
    (CLAUDE.md), and prose that only names them."""
    for name, line, want_ok in [
        ("shared_ptr", "std::shared_ptr<int> p;", False),
        ("shared_ptr_using", "using namespace std;\nshared_ptr<int> p;", False),
        ("make_shared", "auto p = std::make_shared<int>(1);", False),
        ("weak_ptr", "std::weak_ptr<int> w;", False),
        ("shared_from_this", "struct S : std::enable_shared_from_this<S> {};", False),
        ("pointer_cast", "auto q = std::static_pointer_cast<int>(p);", False),
        ("dynamic_cast", "auto* d = dynamic_cast<D*>(b);", False),
        ("function", "std::function<void()> f;", False),
        ("function_using", "using namespace std;\nfunction<void()> f;", False),
        ("getenv", 'const char* e = std::getenv("X");', False),
        ("typeid", "auto& t = typeid(x);", False),
        ("unordered", "std::unordered_map<int, int> m;", False),
        ("unordered_waived",
         "// kirimasim: unordered-ok lookup only\nstd::unordered_map<int, int> m;", True),
        ("prose", "// a shared pointer would do; a function takes it", True),
        ("longer_name", "auto f = make_function<int>();", True),
    ]:
        for where in ("src/kernel", "src/vm"):
            root = tmp / ("banned_" + name + "_" + where.split("/")[1])
            (root / where).mkdir(parents=True)
            (root / where / "x.cpp").write_text(line + "\n", encoding="utf-8")
            expect(f"check_banned {name} in {where}",
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


def perfcount(kirimasim: str, tmp: Path) -> None:
    test = "tests/suite/sched/nba_order_same_proc.sv"
    real = json.loads((ROOT / "tests/perf_counts/expected.json").read_text(encoding="utf-8"))
    base = real[test]
    for name, change, header, want_ok in [
        ("unchanged", {}, None, True),
        ("budget_exceeded", {"proc.wakeups": base["proc.wakeups"] // 2}, None, False),
        ("end_reason", {"sim.end_reason": "finish"}, None, False),
        # The exit code of the run, and the header, are checked too.
        ("exit_code", {}, (":nondeterministic:", ":expected_exit: 1\n:nondeterministic:"), False),
        ("header", {}, (":lrm:", ":lrn:"), False),
    ]:
        root = ROOT
        if header:
            root = tmp / f"perf_{name}_root"
            (root / test).parent.mkdir(parents=True)
            text = (ROOT / test).read_text(encoding="utf-8")
            assert header[0] in text, header
            (root / test).write_text(text.replace(header[0], header[1], 1), encoding="utf-8")
        exp = tmp / f"perf_{name}.json"
        exp.write_text(json.dumps({test: {**base, **change}}), encoding="utf-8")
        expect("perfcount_check " + name,
               [sys.executable, str(TOOLS / "perfcount_check.py"), "--kirimasim", kirimasim,
                "--expected", str(exp), "--root", str(root)], want_ok)


def expected_change(tmp: Path) -> None:
    """check_expected_change.py on a scratch repository: a change under src/
    may not come with a change to an existing line of an expectation."""
    repo = tmp / "expected_change"
    repo.mkdir()

    def git(*args: str) -> None:
        subprocess.run(["git", "-c", "user.name=t", "-c", "user.email=", *args], cwd=repo,
                       check=True, capture_output=True)

    def write(files: dict[str, str]) -> None:
        for path, text in files.items():
            (repo / path).parent.mkdir(parents=True, exist_ok=True)
            (repo / path).write_text(text, encoding="utf-8")

    git("init", "-q")
    write({"src/a.cpp": "int a;\n", "tests/suite/t.sv": "module t; endmodule\n",
           "tests/suite/t.expected": "out\n", "tests/suite/t.stderr": "warning\n"})
    git("add", ".")
    git("commit", "-qm", "base")
    git("tag", "base")
    src = {"src/a.cpp": "int b;\n"}
    for name, files, want_ok in [
        ("src_only", src, True),
        ("expected_only", {"tests/suite/t.expected": "changed\n"}, True),
        ("src_and_expected", {**src, "tests/suite/t.expected": "changed\n"}, False),
        ("src_and_stderr", {**src, "tests/suite/t.stderr": "changed\n"}, False),
        ("src_and_new_stderr_line", {**src, "tests/suite/t.stderr": "warning\nmore\n"}, True),
    ]:
        git("checkout", "-q", "-B", name, "base")
        write(files)
        git("commit", "-qam", name)
        expect("check_expected_change " + name,
               [sys.executable, str(TOOLS / "check_expected_change.py"), "--base", "base",
                "--head", "HEAD"], want_ok, cwd=repo)


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("--kirimasim", required=True)
    args = ap.parse_args()
    with tempfile.TemporaryDirectory() as t:
        tmp = Path(t)
        suite_test(args.kirimasim, tmp)
        severity_test(args.kirimasim, tmp)
        banned(tmp)
        banned_kernel(tmp)
        suite_files(tmp)
        perfcount(args.kirimasim, tmp)
        expected_change(tmp)
    for f in failures:
        print("FAIL: " + f, file=sys.stderr)
    if failures:
        return 1
    print("tool tests: every judging tool rejects the changed copies")
    return 0


if __name__ == "__main__":
    sys.exit(main())
