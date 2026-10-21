#!/usr/bin/env python3
# Copyright (c) 2026 The kirimasim Authors
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

"""Runs one test-suite file through kirimasim and checks the result.

The test's metadata header (sv-tests style, inside the first /* ... */
comment) decides what is expected:

* With :should_fail_because:, kirimasim must reject the design with exit
  code 2 at the intended place: stderr must contain "<file name>:<value of
  :expected_error:>", and a line of it that says "error" must match
  :expected_stderr_regex: (each that the header has), so that the test fails
  if the error comes from some unrelated place. `kirimasim parse` runs
  first; when it accepts the design, the design is compiled and run as
  well, so that errors kirimasim finds itself after slang count too, and an
  "unsupported" error there makes the test a skip as below.
* Otherwise `kirimasim parse` must succeed (exit code 0). Then, for a test
  whose :type: includes "simulation", the design is run and its standard
  output is compared with :expected_stdout:, the exit code with
  :expected_exit: (0 when absent; "nonzero" for any code above 0), with
  :expected_stderr:, kirimasim's own warning and error lines with the given
  file, and with :expected_stderr_regex:, its standard error with the
  regular expression (TEST_SUITE.md). A design that
  uses a construct kirimasim does not support yet (exit code 2 with an
  "unsupported" error, CLI.md 5) is reported as skipped, not as passed,
  when tests/suite/skips.txt lists it; otherwise it fails. With :timeout:,
  kirimasim must finish within that many seconds, or the test fails (CTest
  stops any test after 300 seconds, CMakePresets.json). A run that writes
  more than 16 MiB to standard output or standard error is stopped, and the
  test fails (tools/bounded_run.py). A listed test
  that runs and passes fails too, until it is taken off the list. The list
  covers the tests under its own directory; any other test (the own test
  suite's, which tools/p1_population.py counts) is skipped as before. A
  test with :dpi_c: or :vpi_c: needs C code, which kirimasim cannot load
  yet, so it is not run and is taken as unsupported in the same way.
* Both runs are made in an empty temporary directory made for the test,
  with the test's file given by its absolute path, so that files the
  design writes do not meet those of other tests (TEST_SUITE.md).
* :top_module: is passed to both runs as --top=<name>, one for each name.
  :files: gives its sources (.sv, .v) before the test's file, in the order
  written, and the directories of its other files as +incdir+<dir>, to
  both. :args: is given after the test's file, to the run only. :incdirs:
  and :defines: are not passed yet, so a test with one of them fails rather
  than run in a form other than written.

Standard output is compared line by line without empty lines (lines of
only white space included), as the test-suite repository does
(TEST_SUITE.md). For :nondeterministic: yes, the .expected file holds
the allowed outputs separated by lines of "---"; any one of them passes.
"""

import argparse
import re
import subprocess
import sys
import tempfile
from pathlib import Path

import bounded_run
from suite_header import read_header
from suite_skips import SKIPS, read_skips, skip_name

EXIT_INPUT_ERROR = 2
SKIP = 77  # CTest's SKIP_RETURN_CODE for this test
NOT_PASSED = ("incdirs", "defines")  # header items not given to kirimasim yet


def normalize(text: str) -> list[str]:
    return [line for line in text.splitlines() if line.strip()]


def expected_outputs(test: Path, meta: dict[str, str]) -> list[list[str]] | None:
    value = meta.get("expected_stdout")
    if value is None:
        return None
    if value.startswith("@"):
        text = (test.parent / value[1:]).read_text(encoding="utf-8")
    else:
        text = value + "\n"
    if meta.get("nondeterministic") == "yes":
        # An empty candidate, as from a stray --- at the end, would pass a
        # run that prints nothing.
        parts = [normalize(part) for part in re.split(r"(?m)^---\s*$", text)]
        if any(not part for part in parts):
            raise ValueError(f"{value[1:]} has an empty allowed output (a stray ---?)")
        return parts
    return [normalize(text)]


def message_lines(stderr: str, test: Path) -> list[str]:
    """kirimasim's message lines that :expected_stderr: compares (TEST_SUITE.md):
    warning and error lines, "kirimasim: <kind>:" or "<file>:<line>...: <kind>:",
    and the fatal and info lines of $fatal and $info, which start with a
    source position (SYSTEM_TASKS.md 10.1); not kirimasim's own info lines
    (the version line changes with every commit). Paths are cut to start at
    the test's file name. Lines of other programs (an allocator's warning)
    are left out."""
    out = []
    for line in stderr.splitlines():
        m = re.match(r"^(.*?): (warning|error|fatal|info): ", line)
        if not m:
            continue
        where = m.group(1)
        at_source = re.search(r":\d+(:\d+)?$", where) is not None
        if not at_source and (where != "kirimasim" or m.group(2) in ("fatal", "info")):
            continue
        i = line.find(test.name)
        out.append(line[i:] if 0 <= i < len(where) else line)
    return out


def expected_stderr(test: Path, meta: dict[str, str]) -> list[str] | None:
    value = meta.get("expected_stderr")
    if value is None:
        return None
    if value.startswith("@"):
        return normalize((test.parent / value[1:]).read_text(encoding="utf-8"))
    return [value]


def other_files(test: Path, meta: dict[str, str]) -> list[str]:
    """The arguments that :files: gives before the test's file: the
    directories of the files the design includes, as +incdir+<dir>, and
    the sources (.sv, .v) in the order written."""
    sources, incdirs = [], []
    for name in meta.get("files", "").split():
        path = (test.parent / name).resolve()
        if path.suffix in (".sv", ".v"):
            sources.append(str(path))
        elif f"+incdir+{path.parent}" not in incdirs:
            incdirs.append(f"+incdir+{path.parent}")
    return incdirs + sources


def check_failure(args, meta, proc) -> int:
    expected_error = meta.get("expected_error")
    regex = meta.get("expected_stderr_regex")
    ok = True
    if proc.returncode != EXIT_INPUT_ERROR:
        print(f"FAIL: expected exit code {EXIT_INPUT_ERROR}, got {proc.returncode}",
              file=sys.stderr)
        ok = False
    if expected_error is not None:
        needle = f"{args.test.name}:{expected_error}"
        if needle not in proc.stderr:
            print(f"FAIL: stderr does not contain {needle!r}", file=sys.stderr)
            ok = False
    if regex is not None and not any(re.search(regex, line) and "error" in line.lower()
                                     for line in proc.stderr.splitlines()):
        print(f"FAIL: no error line of stderr matches {regex!r}", file=sys.stderr)
        ok = False
    if not ok:
        print("--- stderr ---", file=sys.stderr)
        print(proc.stderr, file=sys.stderr)
        return 1
    return 0


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("--kirimasim", required=True, help="path to the kirimasim executable")
    ap.add_argument("test", type=Path, help="test-suite .sv file")
    ap.add_argument("--skips", type=Path, default=SKIPS,
                    help="the list of allowed skips (default: tests/suite/skips.txt)")
    args = ap.parse_args()
    # tools/check_suite_files.py checks the list itself.
    skips, _ = read_skips(args.skips)
    name = skip_name(args.test, args.skips)
    covered = name is not None
    listed = name in skips

    meta, problems = read_header(args.test)
    if problems:
        for p in problems:
            print(f"FAIL: header: {p}", file=sys.stderr)
        return 1
    for key in NOT_PASSED:
        if key in meta:
            print(f"FAIL: header: :{key}: is not passed to kirimasim yet", file=sys.stderr)
            return 1
    if ("should_fail_because" in meta and "expected_error" not in meta
            and "expected_stderr_regex" not in meta):
        print("FAIL: :should_fail_because: needs :expected_error: or :expected_stderr_regex:",
              file=sys.stderr)
        return 1
    if "dpi_c" in meta or "vpi_c" in meta:
        return unsupported(args, listed, covered,
                           "needs C code (:dpi_c: or :vpi_c:), which kirimasim cannot load yet")
    timeout = int(meta["timeout"]) if "timeout" in meta else None
    try:
        with tempfile.TemporaryDirectory(prefix="kirimasim-test-") as work:
            return judge(args, meta, listed, covered, timeout, work)
    except subprocess.TimeoutExpired:
        print(f"FAIL: kirimasim did not finish within {timeout} s (:timeout:)", file=sys.stderr)
        return 1
    except bounded_run.OutputLimit as e:
        print(f"FAIL: {e}", file=sys.stderr)
        return 1


def unsupported(args, listed: bool, covered: bool, why: str, stderr: str = "") -> int:
    """A test that kirimasim cannot run yet: a skip when the list of allowed
    skips names it or does not cover it, a failure otherwise."""
    if listed or not covered:
        print(f"SKIP: {why}", file=sys.stderr)
        print(stderr, file=sys.stderr)
        return SKIP
    print(f"FAIL: {why}, and {args.skips.name} does not list it as a skip", file=sys.stderr)
    print(stderr, file=sys.stderr)
    return 1


def judge(args, meta: dict[str, str], listed: bool, covered: bool, timeout: int | None,
          work: str) -> int:
    """Parses, and runs a simulation test in the directory work, and
    compares what it expects."""
    tops = [f"--top={name}" for name in meta.get("top_module", "").split()]
    test = str(args.test.resolve())
    files = other_files(args.test, meta)
    # A path to kirimasim would be taken from work; a bare name is looked
    # up on PATH.
    exe = str(Path(args.kirimasim).resolve()) if "/" in args.kirimasim else args.kirimasim
    proc = bounded_run.run([exe, "parse", *tops, *files, test], cwd=work, timeout=timeout)
    command = [exe, *tops, *files, test, *meta.get("args", "").split()]

    if "should_fail_because" in meta:
        if proc.returncode == 0:
            # slang accepted it; kirimasim may still reject it.
            proc = bounded_run.run(command, cwd=work, timeout=timeout)
            if proc.returncode == EXIT_INPUT_ERROR and "error: unsupported:" in proc.stderr:
                return unsupported(args, listed, covered,
                                   "uses a construct that is not supported yet", proc.stderr)
        return check_failure(args, meta, proc)
    if proc.returncode != 0:
        print(f"FAIL: parse: expected exit code 0, got {proc.returncode}", file=sys.stderr)
        print(proc.stderr, file=sys.stderr)
        return 1
    if "simulation" not in meta.get("type", "").split():
        return 0

    run = bounded_run.run(command, cwd=work, timeout=timeout)
    if run.returncode == EXIT_INPUT_ERROR and "error: unsupported:" in run.stderr:
        return unsupported(args, listed, covered, "uses a construct that is not supported yet",
                           run.stderr)
    try:
        expected = expected_outputs(args.test, meta)
    except ValueError as e:
        print(f"FAIL: {e}", file=sys.stderr)
        return 1
    if expected is None:
        print("FAIL: a simulation test needs :expected_stdout:", file=sys.stderr)
        return 1
    problems = []
    got = normalize(run.stdout)
    if got not in expected:
        problems.append("standard output differs\n--- expected ---\n" +
                        "\n".join(expected[0]) + "\n--- got ---\n" + "\n".join(got))
    want_exit = meta.get("expected_exit", "0")
    if not (run.returncode > 0 if want_exit == "nonzero" else run.returncode == int(want_exit)):
        problems.append(f"exit code {run.returncode}, expected {want_exit}")
    regex = meta.get("expected_stderr_regex")
    if regex is not None and not re.search(regex, run.stderr, re.MULTILINE):
        problems.append(f"no line of stderr matches {regex!r}")
    want_err = expected_stderr(args.test, meta)
    if want_err is not None:
        got_err = message_lines(run.stderr, args.test)
        if got_err != want_err:
            problems.append("warning and error lines differ\n--- expected ---\n" +
                            "\n".join(want_err) + "\n--- got ---\n" + "\n".join(got_err))
    if not problems:
        if listed:
            print(f"FAIL: runs and passes, but {args.skips.name} lists it as a skip; take it off "
                  "the list", file=sys.stderr)
            return 1
        return 0
    for p in problems:
        print("FAIL: " + p, file=sys.stderr)
    print("--- stderr ---\n" + run.stderr, file=sys.stderr)
    return 1


if __name__ == "__main__":
    sys.exit(main())
