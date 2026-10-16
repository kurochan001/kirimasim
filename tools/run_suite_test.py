#!/usr/bin/env python3
"""Runs one test-suite file through haremasim and checks the result.

The test's metadata header (sv-tests style, inside the first /* ... */
comment) decides what is expected:

* With :should_fail_because:, `haremasim parse` must exit with code 2, and
  stderr must contain "<file name>:<value of :expected_error:>", so that the
  test fails if the error comes from some unrelated place.
* Otherwise `haremasim parse` must succeed (exit code 0). Then, for a test
  whose :type: includes "simulation", the design is run and its standard
  output is compared with :expected_stdout:, the exit code with
  :expected_exit: (0 when absent) and, with :expected_stderr:, haremasim's
  own warning and error lines with the given file (TEST_SUITE.md). A design that
  uses a construct haremasim does not support yet (exit code 2 with an
  "unsupported" error, CLI.md 5) is reported as skipped, not as passed.

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

EXIT_INPUT_ERROR = 2
SKIP = 77  # CTest's SKIP_RETURN_CODE for this test


def read_metadata(path: Path) -> dict[str, str]:
    text = path.read_text(encoding="utf-8")
    m = re.search(r"/\*(.*?)\*/", text, re.DOTALL)
    meta: dict[str, str] = {}
    if not m:
        return meta
    for line in m.group(1).splitlines():
        fm = re.match(r"^:([A-Za-z_]+):\s?(.*)$", line.strip())
        if fm:
            meta[fm.group(1)] = fm.group(2).strip()
    return meta


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
        return [normalize(part) for part in re.split(r"(?m)^---\s*$", text)]
    return [normalize(text)]


def message_lines(stderr: str, test: Path) -> list[str]:
    """haremasim's warning and error lines (CLI.md 4): "haremasim: <kind>:" or
    "<file>:<line>...: <kind>:". Paths are cut to start at the test's file
    name. Lines of other programs (an allocator's warning) are left out."""
    out = []
    for line in stderr.splitlines():
        m = re.match(r"^(.*?): (warning|error): ", line)
        if not m:
            continue
        where = m.group(1)
        if where != "haremasim" and not re.search(r":\d+(:\d+)?$", where):
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
    ap.add_argument("--haremasim", required=True, help="path to the haremasim executable")
    ap.add_argument("test", type=Path, help="test-suite .sv file")
    args = ap.parse_args()

    meta = read_metadata(args.test)
    proc = subprocess.run(
        [args.haremasim, "parse", str(args.test)],
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
        [args.haremasim, str(args.test)],
        capture_output=True,
        text=True,
        check=False,
    )
    if run.returncode == EXIT_INPUT_ERROR and "error: unsupported:" in run.stderr:
        print("SKIP: uses a construct that is not supported yet", file=sys.stderr)
        print(run.stderr, file=sys.stderr)
        return SKIP
    expected = expected_outputs(args.test, meta)
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
        return 0
    for p in problems:
        print("FAIL: " + p, file=sys.stderr)
    print("--- stderr ---\n" + run.stderr, file=sys.stderr)
    return 1


if __name__ == "__main__":
    sys.exit(main())
