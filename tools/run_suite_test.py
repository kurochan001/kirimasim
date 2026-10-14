#!/usr/bin/env python3
"""Runs one test-suite file through `haremasim parse` and checks the result.

The test's metadata header (sv-tests style, inside the first /* ... */
comment) decides what is expected:

* Without :should_fail_because:, parse must succeed (exit code 0).
* With :should_fail_because:, parse must exit with code 2, and stderr must
  contain "<file name>:<value of :expected_error:>", so that the test fails
  if the error comes from some unrelated place.

expected_stdout is not compared at this stage.
"""

import argparse
import re
import subprocess
import sys
from pathlib import Path

EXIT_INPUT_ERROR = 2


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

    if "should_fail_because" not in meta:
        if proc.returncode != 0:
            print(f"FAIL: expected exit code 0, got {proc.returncode}", file=sys.stderr)
            print(proc.stderr, file=sys.stderr)
            return 1
        return 0

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


if __name__ == "__main__":
    sys.exit(main())
