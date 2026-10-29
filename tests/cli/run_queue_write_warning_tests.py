#!/usr/bin/env python3
# Copyright (c) 2026 The kirimasim Authors
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

"""Check required queue write warnings independently of optional bounds warnings (7.10.1)."""

import argparse
import json
from pathlib import Path
import re
import subprocess
import sys
import tempfile

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE.parents[1] / "tools"))
import bounded_run  # noqa: E402

DIAGNOSTIC = "write to an invalid queue index is ignored"


def warning_positions(source):
    positions = []
    for line_number, line in enumerate(source.read_text().splitlines(), 1):
        if "// WARN:" in line:
            positions.append((line_number, len(line) - len(line.lstrip()) + 1))
    return positions


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--kirimasim", type=Path, required=True)
    exe = str(parser.parse_args().kirimasim.resolve())
    failures = []
    cases = 0
    for name in ("direct", "references", "nested"):
        source = HERE / "queue_write_warnings" / f"{name}.sv"
        expected = source.with_suffix(".expected").read_text()
        positions = warning_positions(source)
        reference_stats = None
        with tempfile.TemporaryDirectory(prefix="kirimasim-queue-warning-") as work:
            for mode in ((), ("--bounds-check=off",), ("--bounds-check=warn",)):
                try:
                    result = bounded_run.run([exe, "--stats=json", *mode, str(source)],
                                             cwd=work, timeout=30)
                except subprocess.TimeoutExpired:
                    failures.append(f"{name} {mode}: simulator timed out")
                    continue
                cases += 1
                lines = result.stdout.splitlines()
                stats_lines = [line for line in lines if line.startswith("{")]
                output = "\n".join(line for line in lines if not line.startswith("{")) + "\n"
                try:
                    stats = [json.loads(line) for line in stats_lines]
                except json.JSONDecodeError:
                    stats = []
                warnings = re.findall(r"(?m)^.*?:(\d+):(\d+): warning: ([^\n]+)$", result.stderr)
                actual = [(int(line), int(column)) for line, column, _ in warnings]
                if (result.returncode != 0 or output != expected or
                        sorted(actual) != sorted(positions) or
                        any(text != DIAGNOSTIC for _, _, text in warnings) or
                        ": error:" in result.stderr or ": fatal:" in result.stderr or
                        len(stats) != 1 or
                        (reference_stats is not None and stats != reference_stats)):
                    failures.append(f"{name} {mode}: code={result.returncode}\n"
                                    f"{result.stdout}\n{result.stderr}\n"
                                    f"expected warning positions: {positions}")
                reference_stats = stats if reference_stats is None else reference_stats
    print(f"queue write warnings: {cases} checks, {len(failures)} failures")
    for failure in failures:
        print(failure)
    return bool(failures)


if __name__ == "__main__":
    raise SystemExit(main())
