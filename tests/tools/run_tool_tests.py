#!/usr/bin/env python3
# Copyright (c) 2026 The kirimasim Authors
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

"""Checks that the judging tools fail when they should (TEST_SUITE.md,
negative controls): each case changes one thing in a copy of a test or of an
expectation and expects the tool to reject it, next to the unchanged copy,
which must pass.
"""

import argparse
import contextlib
import importlib.util
import io
import json
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path
from unittest import mock

ROOT = Path(__file__).resolve().parent.parent.parent
TOOLS = ROOT / "tools"
failures = []


def expect(name: str, cmd: list[str], want_ok: bool, cwd: Path | None = None) -> None:
    p = subprocess.run(cmd, capture_output=True, text=True, check=False, cwd=cwd)
    if (p.returncode == 0) != want_ok:
        failures.append(f"{name}: exit code {p.returncode}, expected "
                        f"{'0' if want_ok else 'nonzero'}\n{p.stdout}{p.stderr}")


def expect_code(name: str, cmd: list[str], code: int) -> None:
    p = subprocess.run(cmd, capture_output=True, text=True, check=False)
    if p.returncode != code:
        failures.append(f"{name}: exit code {p.returncode}, expected {code}\n{p.stdout}{p.stderr}")


def skips_test(kirimasim: str, tmp: Path) -> None:
    """run_suite_test.py with a list of allowed skips (TEST_SUITE.md, "What
    is judged now"): an unsupported test skips only when listed, and a
    listed test that passes fails."""
    run = [sys.executable, str(TOOLS / "run_suite_test.py"), "--kirimasim", kirimasim]
    for name, test, listed, code in [
        ("unsupported_listed", "cb/input_sample_1step", True, 77),
        ("unsupported_not_listed", "cb/input_sample_1step", False, 1),
        ("listed_but_passes", "values/unique_case_violation", True, 1),
        ("passes_not_listed", "values/unique_case_violation", False, 0),
    ]:
        d = tmp / ("skips_" + name)
        d.mkdir()
        src = ROOT / "tests/suite" / test
        for f in src.parent.glob(src.name + ".*"):
            shutil.copy(f, d / f.name)
        skips = d / "skips.txt"
        skips.write_text(f"{src.name}  # a reason\n" if listed else "", encoding="utf-8")
        expect_code("run_suite_test skips " + name,
                    run + ["--skips", str(skips), str(d / (src.name + ".sv"))], code)
    # A test outside the list's directory is skipped as before.
    outside = tmp / "skips_unsupported_listed" / "skips.txt"
    expect_code("run_suite_test skips outside_the_list",
                run + ["--skips", str(tmp / "skips_passes_not_listed" / "skips.txt"),
                       str(outside.parent / "input_sample_1step.sv")], 77)


def timeout_test(kirimasim: str, tmp: Path) -> None:
    """run_suite_test.py with :timeout: on a copy of sched/empty_forever,
    which runs to the zero-time limit (seconds)."""
    run = [sys.executable, str(TOOLS / "run_suite_test.py"), "--kirimasim", kirimasim]
    src = ROOT / "tests/suite/sched/empty_forever"
    for name, value, code in [("too_short", "1", 1), ("not_a_number", "soon", 1),
                              ("long_enough", "600", 0)]:
        d = tmp / ("timeout_" + name)
        d.mkdir()
        for f in src.parent.glob(src.name + ".*"):
            shutil.copy(f, d / f.name)
        f = d / "empty_forever.sv"
        text = f.read_text(encoding="utf-8")
        header = f":nondeterministic: no\n:timeout: {value}"
        f.write_text(text.replace(":nondeterministic: no", header, 1), encoding="utf-8")
        expect_code("run_suite_test timeout " + name, run + [str(f)], code)


def fake_kirimasim_test(tmp: Path) -> None:
    """run_suite_test.py on stand-ins for kirimasim (TEST_SUITE.md, "What
    is judged now"): one that writes without end is stopped past the limit
    of tools/bounded_run.py and the test fails, on either stream, in
    seconds; one that writes the expected output only when it runs in an
    empty directory and is given the test by an absolute path passes, and
    so does one that writes it only when its standard input is empty, next
    to one that writes the expected output, which passes."""
    test = tmp / "limit.sv"
    test.write_text("/*\n:name: limit\n:description: d\n:tags: t\n:type: simulation\n"
                    ":lrm: 1800-2023 21.2\n:oracle: spec\n:spec_check: ai\n:fourstate: no\n"
                    ":nondeterministic: no\n:expected_stdout: hello\n*/\nmodule top;\n"
                    "endmodule\n", encoding="utf-8")
    for name, body, code, message in [
        ("expected", "sys.stdout.write('hello\\n')", 0, ""),
        ("workdir", "sys.stdout.write('hello\\n' if not os.listdir('.') and "
                    "os.path.isabs(sys.argv[-1]) else 'not in an empty directory\\n')", 0, ""),
        ("empty_stdin", "sys.stdout.write('hello\\n' if sys.stdin.read() == '' else 'input\\n')",
         0, ""),
        ("stdout", "while True: sys.stdout.write('x' * 63 + '\\n')", 1,
         "wrote more than 16 MiB to standard output"),
        ("stderr", "while True: sys.stderr.write('x' * 63 + '\\n')", 1,
         "wrote more than 16 MiB to standard error"),
    ]:
        fake = tmp / f"fake_{name}"
        fake.write_text(f"#!{sys.executable}\nimport os, sys\nif sys.argv[1] != 'parse':\n"
                        f"    {body}\n", encoding="utf-8")
        fake.chmod(0o755)
        try:
            # Given standard input that is not empty, which the test must not see.
            p = subprocess.run([sys.executable, str(TOOLS / "run_suite_test.py"), "--kirimasim",
                                str(fake), str(test)], input="not empty\n", capture_output=True,
                               text=True, check=False, timeout=120)
        except subprocess.TimeoutExpired:
            failures.append(f"run_suite_test fake kirimasim {name}: did not end in 120 s")
            continue
        if p.returncode != code or message not in p.stderr:
            failures.append(f"run_suite_test fake kirimasim {name}: exit code {p.returncode}, "
                            f"expected {code} and {message!r}\n{p.stderr[-2000:]}")


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


def items_test(kirimasim: str, tmp: Path) -> None:
    """run_suite_test.py with the items of the test suite repository
    (TEST_SUITE.md, added 2026-10-21): :expected_stderr_regex: on an illegal
    design and on a run, nonzero of :expected_exit:, :files:, and :dpi_c: or
    :vpi_c:, each next to a change that must fail."""
    run = [sys.executable, str(TOOLS / "run_suite_test.py"), "--kirimasim", kirimasim]

    def copy(name: str, test: str, old: str, new: str) -> Path:
        d = tmp / ("items_" + name)
        d.mkdir()
        src = ROOT / "tests/suite" / test
        for f in src.parent.glob(src.name + ".*"):
            shutil.copy(f, d / f.name)
        f = d / (src.name + ".sv")
        text = f.read_text(encoding="utf-8")
        assert old in text, old
        f.write_text(text.replace(old, new, 1), encoding="utf-8")
        return f

    illegal = "proc/task_write_direct_comb"
    error = (":expected_error: 21:15: error: variable 't' driven by always_comb procedure "
             "cannot be written to by any other process")
    # Each change takes the place of a header line, so that the lines of
    # the design, which the messages name, stay where they were.
    for name, old, new, want_ok in [
        ("regex_at_the_place", error, r":expected_stderr_regex: task_write_direct_comb\.sv:21:",
         True),
        ("regex_elsewhere", error, r":expected_stderr_regex: task_write_direct_comb\.sv:20:",
         False),
        ("regex_and_error", ":issue: 21", ":expected_stderr_regex: :21:", True),
        ("regex_and_error_elsewhere", ":issue: 21", ":expected_stderr_regex: :20:", False),
        ("neither", error, ":issue: 21", False),
        ("regex_not_a_regex", error, ":expected_stderr_regex: (", False),
    ]:
        expect("run_suite_test items " + name, run + [str(copy(name, illegal, old, new))],
               want_ok)

    passes = "values/unique_case_violation"  # exits 0, with a warning
    stderr = ":expected_stderr: @unique_case_violation.stderr"
    fatal = "proc/severity_tasks"            # exits 1
    for name, test, old, new, want_ok in [
        ("nonzero_of_1", fatal, ":expected_exit: 1", ":expected_exit: nonzero", True),
        ("nonzero_of_0", passes, stderr, ":expected_exit: nonzero", False),
        ("exit_a_word", fatal, ":expected_exit: 1", ":expected_exit: some", False),
        ("run_regex_matches", passes, stderr,
         r":expected_stderr_regex: :31: warning: unique case", True),
        ("run_regex_misses", passes, stderr,
         r":expected_stderr_regex: :31: error: unique case", False),
    ]:
        expect("run_suite_test items " + name, run + [str(copy(name, test, old, new))], want_ok)

    # C code: not run, and unsupported under the rule of allowed skips.
    for name, listed, code in [("c_code_outside", None, 77), ("c_code_listed", True, 77),
                               ("c_code_not_listed", False, 1)]:
        f = copy(name, passes, stderr, ":vpi_c: @app.c")
        args = []
        if listed is not None:
            skips = f.parent / "skips.txt"
            skips.write_text("unique_case_violation  # a reason\n" if listed else "",
                             encoding="utf-8")
            args = ["--skips", str(skips)]
        expect_code("run_suite_test items " + name, run + args + [str(f)], code)

    # :files: gives a source before the test and an included file's directory.
    d = tmp / "items_files"
    (d / "inc").mkdir(parents=True)
    (d / "files_part.sv").write_text("module files_part(output int v);\n  assign v = 3;\n"
                                     "endmodule\n", encoding="utf-8")
    (d / "inc" / "files_add.svh").write_text("localparam int ADD = 4;\n", encoding="utf-8")
    header = ("/*\n:name: files\n:description: d\n:tags: t\n:type: simulation\n"
              ":lrm: 1800-2023 22.4\n:oracle: spec\n:spec_check: ai\n:fourstate: no\n"
              ":nondeterministic: no\n:expected_stdout: 7\n{files}*/\n")
    body = ("`include \"files_add.svh\"\nmodule top;\n  int v;\n  files_part p(v);\n"
            "  initial #1 $display(\"%0d\", v + ADD);\nendmodule\n")
    for name, files, want_ok in [("files", ":files: files_part.sv inc/files_add.svh\n", True),
                                 ("files_without_source", ":files: inc/files_add.svh\n", False),
                                 ("files_without_include", ":files: files_part.sv\n", False)]:
        f = d / f"test_{name}.sv"
        f.write_text(header.format(files=files) + body, encoding="utf-8")
        expect("run_suite_test items " + name, run + [str(f)], want_ok)

    # With stand-ins for kirimasim whose parse accepts everything: an
    # illegal design rejected after slang, or stopped there as unsupported,
    # and :args:, given to the run only, after the test's file.
    d = tmp / "items_fake"
    d.mkdir()
    late = ("/*\n:name: late\n:description: d\n:tags: t\n:type: elaboration\n"
            ":lrm: 1800-2023 9.2.2.2\n:oracle: spec\n:spec_check: ai\n:fourstate: no\n"
            ":nondeterministic: no\n:should_fail_because: r\n{regex}*/\nmodule top;\nendmodule\n")
    plusargs = ("/*\n:name: plusargs\n:description: d\n:tags: t\n:type: simulation\n"
                ":lrm: 1800-2023 21.6\n:oracle: spec\n:spec_check: ai\n:fourstate: no\n"
                ":nondeterministic: no\n:expected_stdout: hello\n{args}*/\nmodule top;\n"
                "endmodule\n")
    for name, body, text, code in [
        ("late_error", "sys.stderr.write(sys.argv[-1] + ':3:1: error: rejected\\n'); sys.exit(2)",
         late.format(regex=":expected_stderr_regex: test_late_error\\.sv:3:\n"), 0),
        ("late_error_elsewhere",
         "sys.stderr.write(sys.argv[-1] + ':3:1: error: rejected\\n'); sys.exit(2)",
         late.format(regex=":expected_stderr_regex: test_late_error_elsewhere\\.sv:4:\n"), 1),
        ("late_unsupported", "sys.stderr.write(sys.argv[-1] + ':3:1: error: unsupported: "
         "a thing (not implemented yet)\\n'); sys.exit(2)",
         late.format(regex=":expected_stderr_regex: test_late_unsupported\\.sv:3:\n"), 77),
        ("args", "print('hello' if sys.argv[-1] == '+a=1' and sys.argv[-2].endswith('.sv') "
         "else 'no')", plusargs.format(args=":args: +a=1\n"), 0),
        ("args_missing", "print('hello' if sys.argv[-1] == '+a=1' else 'no')",
         plusargs.format(args=""), 1),
    ]:
        fake = d / f"fake_{name}"
        fake.write_text(f"#!{sys.executable}\nimport sys\nif sys.argv[1] == 'parse':\n"
                        "    sys.exit(3 if '+a=1' in sys.argv else 0)\n"
                        f"{body}\n", encoding="utf-8")
        fake.chmod(0o755)
        test = d / f"test_{name}.sv"
        test.write_text(text, encoding="utf-8")
        expect_code("run_suite_test items fake " + name,
                    [sys.executable, str(TOOLS / "run_suite_test.py"), "--kirimasim", str(fake),
                     str(test)], code)


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


def banned_state(tmp: Path) -> None:
    """RUNTIME_ABI.md 9.3: outside inputs, ending the process and the
    process's output, banned in src/ but the driver; abort and the like,
    banned in the driver too; and calls that only look like them."""
    for name, line, where, want_ok in [
        ("getenv_front", 'const char* e = std::getenv("X");', "src/front", False),
        ("getenv_sir", 'const char* e = getenv("X");', "src/sir", False),
        ("getenv_driver", 'const char* e = std::getenv("X");', "src/driver", True),
        ("setenv", 'setenv("X", "1", 1);', "src/front", False),
        ("locale", 'std::setlocale(LC_ALL, "");', "src/kernel", False),
        ("std_locale", "std::locale l;", "src/vm", False),
        ("system_clock", "auto t = std::chrono::system_clock::now();", "src/kernel", False),
        ("std_time", "auto t = std::time(nullptr);", "src/vm", False),
        ("localtime", "auto* tm = localtime(&t);", "src/front", False),
        ("random_device", "std::random_device rd;", "src/kernel", False),
        ("rand", "int r = rand();", "src/vm", False),
        ("srand", "srand(1);", "src/kernel", False),
        ("getpid", "auto seed = getpid();", "src/kernel", False),
        ("exit", "std::exit(1);", "src/kernel", False),
        ("exit_unqualified", "exit(1);", "src/front", False),
        ("quick_exit", "std::quick_exit(1);", "src/vm", False),
        ("exit_driver", "std::_Exit(130);", "src/driver", True),
        ("abort", "std::abort();", "src/kernel", False),
        ("abort_driver", "std::abort();", "src/driver", False),
        ("terminate", "std::terminate();", "src/vm", False),
        ("assert", "assert(x > 0);", "src/front", False),
        ("cassert", "#include <cassert>", "src/kernel", False),
        ("stdout", "std::fwrite(p, 1, n, stdout);", "src/kernel", False),
        ("stderr", "std::fputs(s, stderr);", "src/vm", False),
        ("cout", "std::cout << x;", "src/front", False),
        ("printf", 'std::printf("%d", x);', "src/vm", False),
        ("puts", 'puts("x");', "src/kernel", False),
        ("print", 'std::println("{}", x);', "src/kernel", False),
        ("stdout_driver", "std::fputs(s, stdout);", "src/driver", True),
        ("stdout_waived", "// kirimasim: banned-ok the defaults\nf(stdout);", "src/kernel", True),
        # Names that only look like the banned ones.
        ("member_time", "const auto now = sched_.time();", "src/vm", True),
        ("member_exit", "terminate(Op::Br, {Operand::block(*n.exit)}, sl);", "src/front", True),
        ("fprintf_given", 'std::fprintf(to_.err, "%s", s);', "src/kernel", True),
        ("snprintf", "std::snprintf(b, sizeof b, \"%d\", x);", "src/kernel", True),
        ("static_assert", "static_assert(sizeof(int) == 4);", "src/kernel", True),
        ("operand_random", "int r = op.random(seed);", "src/vm", True),
    ]:
        root = tmp / ("state_" + name)
        (root / where).mkdir(parents=True)
        (root / where / "x.cpp").write_text(line + "\n", encoding="utf-8")
        expect(f"check_banned {name} in {where}",
               [sys.executable, str(TOOLS / "check_banned.py"), "--root", str(root)], want_ok)


def global_state(tmp: Path) -> None:
    """check_global_state.py: each suppression of the global-state checks in
    src/ has its line in the allowlist, and each line its suppression
    (RUNTIME_ABI.md 9.4)."""
    both = "cppcoreguidelines-avoid-non-const-global-variables,custom-kirimasim-global-state"
    flag = f"// NOLINTNEXTLINE({both})\nvolatile std::sig_atomic_t flag = 0;\n"
    entry = "src/driver/main.cpp | flag | 9.4 item 3 | the flag of SIGINT\n"
    for name, source, allowlist, want_ok in [
        ("listed", flag, entry, True),
        ("same_line", f"volatile int flag = 0;  // NOLINT({both})\n", entry, True),
        ("not_listed", flag, "", False),
        ("other_name", flag.replace("flag =", "other ="), entry, False),
        ("no_suppression", "int x = 0;\n", entry, False),
        ("listed_twice", flag, entry + entry, False),
        ("no_item", flag, "src/driver/main.cpp | flag | the flag of SIGINT\n", False),
        ("bare_nolint", "int f();  // NOLINT\n", "", False),
        ("pattern", "// NOLINTNEXTLINE(custom-*)\nint x = 0;\n", "", False),
        ("everything", "// NOLINTNEXTLINE(*)\nint x = 0;\n", "", False),
        ("begin", f"// NOLINTBEGIN({both})\nint x = 0;\n// NOLINTEND({both})\n", "", False),
        ("other_check", "int f();  // NOLINT(bugprone-unused-return-value)\n", "", True),
    ]:
        root = tmp / ("global_" + name)
        (root / "src/driver").mkdir(parents=True)
        (root / "tools").mkdir()
        (root / "src/driver/main.cpp").write_text(source, encoding="utf-8")
        (root / "tools/global_state_allowlist.txt").write_text("# header\n" + allowlist,
                                                              encoding="utf-8")
        expect("check_global_state " + name,
               [sys.executable, str(TOOLS / "check_global_state.py"), "--root", str(root)],
               want_ok)


def bench_report(tmp: Path) -> None:
    """bench_report.py (PERFORMANCE.md 5.1): a warning for each value worse
    than the median of the last seven nights by its limit, none for the
    others, and exit code 0 either way."""
    def record(seconds: float, mib: float) -> dict:
        return {"results": {"kirimasim": {"d": {"simulation_seconds": seconds,
                                                "max_rss_mib": mib}}}}
    same = [(1.0, 10.0)] * 3
    for name, past, now, build, want in [
        ("same", same, (1.0, 10.0), 100.0, set()),
        ("time_below", same, (1.14, 10.0), 100.0, set()),
        ("time_at_limit", same, (1.15, 10.0), 100.0, {"d seconds"}),
        ("rss", same, (1.0, 11.5), 100.0, {"d MiB"}),
        ("build_below", same, (1.0, 10.0), 124.0, set()),
        ("build_at_limit", same, (1.0, 10.0), 125.0, {"build seconds"}),
        ("median_not_mean", [(1.0, 10.0), (9.0, 10.0), (1.0, 10.0)], (1.2, 10.0), 100.0,
         {"d seconds"}),
        # Eight nights: the oldest one is left out, or the median would be 5.5.
        ("last_seven", [(10.0, 10.0)] * 4 + [(1.0, 10.0)] * 4, (1.2, 10.0), 100.0,
         {"d seconds"}),
        ("no_history", [], (5.0, 10.0), 100.0, set()),
    ]:
        root = tmp / ("bench_" + name)
        for i, (seconds, mib) in enumerate(past):
            night = root / "history" / str(i)
            night.mkdir(parents=True)
            (night / "nightly.json").write_text(json.dumps(
                {"date": f"2026-10-{i + 1:02d}T00:00:00Z", "commit": "c", "build_seconds": 100.0,
                 "results": record(seconds, mib)}), encoding="utf-8")
        root.mkdir(parents=True, exist_ok=True)
        (root / "results.json").write_text(json.dumps(record(*now)), encoding="utf-8")
        p = subprocess.run([sys.executable, str(TOOLS / "bench_report.py"),
                            "--results", str(root / "results.json"),
                            "--build-seconds", str(build), "--commit", "0123456789ab",
                            "--out", str(root / "out" / "nightly.json"),
                            "--history", str(root / "history")],
                           capture_output=True, text=True, check=False)
        got = {line.removeprefix("::warning::benchmark ").split(":")[0]
               for line in p.stdout.splitlines() if line.startswith("::warning::benchmark ")}
        if p.returncode != 0 or got != want or not (root / "out" / "nightly.json").is_file():
            failures.append(f"bench_report {name}: exit code {p.returncode}, warnings "
                            f"{sorted(got)}, expected {sorted(want)}\n{p.stdout}{p.stderr}")


def suite_files(tmp: Path) -> None:
    for name, files, skips, want_ok in [
        ("paired", ["a.sv", "a.expected", "a.stderr"], "", True),
        ("orphan", ["a.sv", "b.expected"], "", False),
        # The list of allowed skips names tests that exist, once, with a reason.
        ("skip_listed", ["a.sv"], "x/a  # a reason\n", True),
        ("skip_of_no_test", ["a.sv"], "x/b  # a reason\n", False),
        ("skip_without_reason", ["a.sv"], "x/a\n", False),
        ("skip_twice", ["a.sv"], "x/a  # a reason\nx/a  # again\n", False),
    ]:
        root = tmp / ("files_" + name)
        (root / "tests/suite/x").mkdir(parents=True)
        (root / "tests/dump_sir").mkdir(parents=True)
        for f in files:
            (root / "tests/suite/x" / f).write_text("", encoding="utf-8")
        if skips:
            (root / "tests/suite/skips.txt").write_text(skips, encoding="utf-8")
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
           "tests/suite/t.expected": "out\n", "tests/suite/t.stderr": "warning\n",
           "tests/suite/skips.txt": "# allowed skips\nu  # a reason\n"})
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
        # Allowing a skip loosens an expectation; taking one off does not.
        ("src_and_new_skip",
         {**src, "tests/suite/skips.txt": "# allowed skips\nu  # a reason\nt  # new\n"}, False),
        ("src_and_skip_removed", {**src, "tests/suite/skips.txt": "# allowed skips\n"}, True),
        ("new_skip_only", {"tests/suite/skips.txt": "# allowed skips\nu  # a reason\nt  # new\n"},
         True),
    ]:
        git("checkout", "-q", "-B", name, "base")
        write(files)
        git("commit", "-qam", name)
        expect("check_expected_change " + name,
               [sys.executable, str(TOOLS / "check_expected_change.py"), "--base", "base",
                "--head", "HEAD"], want_ok, cwd=repo)


def mutants_check(tmp: Path) -> None:
    """mutants.py --check on a copy of src/: it passes, and fails once the
    text that one mistake rewrites is changed."""
    spec = importlib.util.spec_from_file_location("mutants", TOOLS / "mutants.py")
    mutants = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(mutants)
    run = [sys.executable, str(TOOLS / "mutants.py"), "--check", "--root"]
    for name, change in [("unchanged", False), ("text_changed", True)]:
        root = tmp / ("mutants_" + name)
        shutil.copytree(ROOT / "src", root / "src")
        if change:
            _, rel, old, _ = mutants.MUTANTS[0]
            f = root / rel
            half = len(old) // 2  # a character in the middle, so that the text is not there
            f.write_text(f.read_text(encoding="utf-8").replace(old, old[:half] + "@" + old[half:]),
                         encoding="utf-8")
        expect("mutants check " + name, run + [str(root)], not change)


def mutant_detection(tmp: Path) -> None:
    """Self-check failures cannot turn a surviving mutation into a catch."""
    spec = importlib.util.spec_from_file_location("mutants_runner", TOOLS / "mutants.py")
    runner = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(runner)
    root = tmp / "mutants_runner"
    (root / "src").mkdir(parents=True)
    source = root / "src/probe.cpp"
    source.write_text("correct")
    for genuine_failure in (False, True):
        calls = []

        def tests(command):
            calls.append(command)
            if len(calls) == 1:
                return 0, set()
            exclusions = command[command.index("-E") + 1] if "-E" in command else ""
            self_checks_disabled = all(name in exclusions for name in
                                       ("tools/mutants_apply", "tools/negative_controls"))
            # Without exclusions the anchor self-check fails even when the
            # simulated behaviour is unchanged. A real failure still counts.
            return int(genuine_failure or not self_checks_disabled), set()

        output = io.StringIO()
        with mock.patch.object(runner, "ROOT", root), \
             mock.patch.object(runner, "MUTANTS", [("probe", "src/probe.cpp", "correct", "wrong")]), \
             mock.patch.object(runner, "run", return_value=0), \
             mock.patch.object(runner, "run_tests", side_effect=tests), \
             mock.patch.object(sys, "argv", ["mutants.py"]), contextlib.redirect_stdout(output):
            result = runner.main()
        wanted = 0 if genuine_failure else 1
        label = "caught" if genuine_failure else "SURVIVED"
        if result != wanted or label not in output.getvalue() or len(calls) != 2:
            failures.append("mutant detection misclassified a self-check or real test failure")
        if source.read_text() != "correct":
            failures.append("mutant detection did not restore its source")


def feature_probe_report(tmp: Path) -> None:
    """Reference mismatches remain visible and cannot count as passing gaps."""
    spec = importlib.util.spec_from_file_location("feature_probe", TOOLS / "icarus_feature_probe.py")
    probe = importlib.util.module_from_spec(spec)
    with mock.patch.object(sys, "path", [*sys.path, str(TOOLS)]):
        spec.loader.exec_module(probe)
    cases = [(name, "test", name, "7\n", []) for name in
             ("confirmed_gap", "reference_wrong", "both_wrong", "both_correct",
              "aggregate_nba_rhs_order", "aggregate_blocking_rhs_order")]

    def engine(command, **kwargs):
        if "-V" in command or "--version" in command:
            return subprocess.CompletedProcess(command, 0, b"mock version\n", b"")
        if command[0] == "iverilog":
            return subprocess.CompletedProcess(command, 0, b"", b"")
        name = (Path(kwargs["cwd"]) / "probe.sv").read_text().strip()
        reference = command[0] == "vvp"
        correct = name in ("both_correct", "aggregate_nba_rhs_order", "aggregate_blocking_rhs_order") or \
            (reference and name == "confirmed_gap") or \
            (not reference and name == "reference_wrong")
        return subprocess.CompletedProcess(command, 0, b"7\n" if correct else b"9\n", b"")

    for only in (None, "reference_wrong"):
        report = tmp / ("feature_all.json" if only is None else "feature_reference_wrong.json")
        arguments = ["icarus_feature_probe.py", "--kirimasim", str(tmp / "sim"),
                     "--output", str(report), "--require-parity"]
        if only:
            arguments += ["--only", only]
        with mock.patch.object(probe, "PROBES", cases), \
             mock.patch.object(probe.bounded_run, "run", side_effect=engine), \
             mock.patch.object(sys, "argv", arguments), contextlib.redirect_stdout(io.StringIO()):
            code = probe.main()
        data = json.loads(report.read_text())
        gaps = ["confirmed_gap"] if only is None else []
        mismatches = ["reference_wrong", "both_wrong"] if only is None else ["reference_wrong"]
        if data["gaps"] != gaps or data["reference_mismatches"] != mismatches:
            failures.append("feature probe confuses reference mismatches with passing gaps")
        if code != int(only is None):
            failures.append("feature probe require-parity exit ignores its passing-gap scope")
        rows = {row["name"]: row for row in data["probes"]}
        if rows["reference_wrong"]["icarus"]["status"] != "result_mismatch" or \
                rows["reference_wrong"]["kirimasim"]["status"] != "passed":
            failures.append("feature probe loses the independent per-engine result")
        if only is None:
            for name in ("aggregate_nba_rhs_order", "aggregate_blocking_rhs_order"):
                row = rows[name]
                if row["category"] != "project_policy" or "unspecified" not in row.get("policy", ""):
                    failures.append("feature probe mislabels the project evaluation order as a standard rule")


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("--kirimasim", required=True)
    args = ap.parse_args()
    with tempfile.TemporaryDirectory() as t:
        tmp = Path(t)
        suite_test(args.kirimasim, tmp)
        skips_test(args.kirimasim, tmp)
        timeout_test(args.kirimasim, tmp)
        fake_kirimasim_test(tmp)
        severity_test(args.kirimasim, tmp)
        items_test(args.kirimasim, tmp)
        banned(tmp)
        banned_kernel(tmp)
        banned_state(tmp)
        global_state(tmp)
        bench_report(tmp)
        suite_files(tmp)
        perfcount(args.kirimasim, tmp)
        expected_change(tmp)
        mutants_check(tmp)
        mutant_detection(tmp)
        feature_probe_report(tmp)
    for f in failures:
        print("FAIL: " + f, file=sys.stderr)
    if failures:
        return 1
    print("tool tests: every judging tool rejects the changed copies")
    return 0


if __name__ == "__main__":
    sys.exit(main())
