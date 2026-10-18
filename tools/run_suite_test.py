#!/usr/bin/env python3
"""Runs one test-suite file through kirimasim and checks the result.

The test's metadata header (sv-tests style, inside the first /* ... */
comment) decides what is expected:

* With :should_fail_because:, `kirimasim parse` must exit with code 2, and
  stderr must contain "<file name>:<value of :expected_error:>", so that the
  test fails if the error comes from some unrelated place.
* Otherwise `kirimasim parse` must succeed (exit code 0). Then, for a test
  whose :type: includes "simulation", the design is run and its standard
  output is compared with :expected_stdout:, the exit code with
  :expected_exit: (0 when absent) and, with :expected_stderr:, kirimasim's
  own warning and error lines with the given file (TEST_SUITE.md). A design that
  uses a construct kirimasim does not support yet (exit code 2 with an
  "unsupported" error, CLI.md 5) is reported as skipped, not as passed,
  when tests/suite/skips.txt lists it; otherwise it fails. A listed test
  that runs and passes fails too, until it is taken off the list. The list
  covers the tests under its own directory; any other test (the own test
  suite's, which tools/p1_population.py counts) is skipped as before.

Standard output is compared line by line without empty lines (lines of
only white space included), as the test-suite repository does
(TEST_SUITE.md). For :nondeterministic: yes, the .expected file holds
the allowed outputs separated by lines of "---"; any one of them passes.
"""

import argparse
import re
import subprocess
import sys
from pathlib import Path

from suite_header import read_header
from suite_skips import SKIPS, read_skips, skip_name

EXIT_INPUT_ERROR = 2
SKIP = 77  # CTest's SKIP_RETURN_CODE for this test


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


def check_failure(args, meta, proc) -> int:
    expected_error = meta.get("expected_error")
    if not expected_error:
        print("FAIL: :should_fail_because: needs an :expected_error: field", file=sys.stderr)
        return 1
    ok = True
    if proc.returncode != EXIT_INPUT_ERROR:
        print(f"FAIL: expected exit code {EXIT_INPUT_ERROR}, got {proc.returncode}",
              file=sys.stderr)
        ok = False
    needle = f"{args.test.name}:{expected_error}"
    if needle not in proc.stderr:
        print(f"FAIL: stderr does not contain {needle!r}", file=sys.stderr)
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
    proc = subprocess.run(
        [args.kirimasim, "parse", str(args.test)],
        capture_output=True,
        text=True,
        check=False,
    )

    if "should_fail_because" in meta:
        return check_failure(args, meta, proc)
    if proc.returncode != 0:
        print(f"FAIL: parse: expected exit code 0, got {proc.returncode}", file=sys.stderr)
        print(proc.stderr, file=sys.stderr)
        return 1
    if "simulation" not in meta.get("type", "").split():
        return 0

    run = subprocess.run(
        [args.kirimasim, str(args.test)],
        capture_output=True,
        text=True,
        check=False,
    )
    if run.returncode == EXIT_INPUT_ERROR and "error: unsupported:" in run.stderr:
        if listed or not covered:
            print("SKIP: uses a construct that is not supported yet", file=sys.stderr)
            print(run.stderr, file=sys.stderr)
            return SKIP
        print(f"FAIL: uses a construct that is not supported yet, and {args.skips.name} does not "
              "list it as a skip", file=sys.stderr)
        print(run.stderr, file=sys.stderr)
        return 1
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
    want_exit = int(meta.get("expected_exit", "0"))
    if run.returncode != want_exit:
        problems.append(f"exit code {run.returncode}, expected {want_exit}")
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
