#!/usr/bin/env python3
# Copyright (c) 2026 The kirimasim Authors
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
"""--error-limit=<n> sets how many errors are reported before "too many errors", and --single-unit
makes the files of the command line one compilation unit (CLI.md 2.1). Both apply to a run and to
parse."""
import argparse
import subprocess
import sys
import tempfile
from pathlib import Path

ERRORS = 25
MANY = "module top;\n" + "  int 1;\n" * ERRORS + "endmodule\n"
STOP = "too many errors emitted"
# A variable, a function and a macro outside every module, and a module in another file that
# uses them.
UNIT = ("int unit_shared = 42;\n"
        "function automatic int unit_add(int a, int b); return a + b; endfunction\n"
        "`define UNIT_MACRO 7\n")
USER = ("module top;\n"
        "  initial $display(\"unit=%0d add=%0d\", unit_shared, unit_add(1, 2));\n"
        "`ifdef UNIT_MACRO\n"
        "  initial #1 $display(\"macro=%0d\", `UNIT_MACRO);\n"
        "`else\n"
        "  initial #1 $display(\"no macro\");\n"
        "`endif\n"
        "endmodule\n")
# The same module with nothing from the other file: only the macro tells the units apart.
MACRO_ONLY = USER.replace("unit_shared, unit_add(1, 2)", "42, 3")


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--kirimasim", required=True)
    args = parser.parse_args()
    failures = []
    with tempfile.TemporaryDirectory() as directory:
        tmp = Path(directory)
        (tmp / "many.sv").write_text(MANY)
        (tmp / "unit.sv").write_text(UNIT)
        (tmp / "user.sv").write_text(USER)
        (tmp / "macro_only.sv").write_text(MACRO_ONLY)

        def run(*words: str) -> subprocess.CompletedProcess:
            return subprocess.run([args.kirimasim, *words], cwd=tmp, capture_output=True, text=True,
                                  stdin=subprocess.DEVNULL, timeout=60)

        def errors(result: subprocess.CompletedProcess) -> int:
            return sum(": error: " in line for line in result.stderr.splitlines())

        # (the options, the errors reported, whether the run stops for too many).
        limits = [((), 20, True), (("--error-limit=100",), ERRORS, False),
                  (("--error-limit=5",), 5, True), (("--error-limit=25",), ERRORS, False),
                  (("--error-limit=0",), ERRORS, False)]
        for command in ((), ("parse",)):
            for options, count, stops in limits:
                label = " ".join(command + options) or "default"
                result = run(*command, *options, "--top=top", "many.sv")
                if result.returncode != 2:
                    failures.append(f"{label}: exit {result.returncode}, expected 2")
                if errors(result) != count:
                    failures.append(f"{label}: {errors(result)} errors, expected {count}")
                if (STOP in result.stderr) != stops:
                    failures.append(f"{label}: the too-many-errors stop is "
                                    f"{'missing' if stops else 'there'}")
            for bad in ("--error-limit=", "--error-limit=x", "--error-limit=-3"):
                result = run(*command, bad, "--top=top", "many.sv")
                # Rejected as an option, before the file is read.
                if (result.returncode != 2 or "many.sv:" in result.stderr or
                        "error-limit" not in result.stderr):
                    failures.append(f"{' '.join(command)} {bad}: not rejected as an option "
                                    f"(exit {result.returncode})")
        # Each file a unit of its own by default.
        apart = run("unit.sv", "user.sv")
        if apart.returncode != 2 or "unit_shared" not in apart.stderr:
            failures.append(f"default: the other file's declaration is visible (exit {apart.returncode})")
        macro = run("unit.sv", "macro_only.sv")
        if macro.returncode != 0 or macro.stdout != "unit=42 add=3\nno macro\n":
            failures.append(f"default: the other file's macro is visible: {macro.stdout!r}")
        # One unit: the declarations and the macro of the file before.
        for first in ("--single-unit", "unit.sv"):
            words = ["--single-unit", "unit.sv", "user.sv"] if first == "--single-unit" else \
                    ["unit.sv", "user.sv", "--single-unit"]
            one = run(*words)
            if one.returncode != 0 or one.stdout != "unit=42 add=3\nmacro=7\n":
                failures.append(f"{' '.join(words)}: exit {one.returncode}, output {one.stdout!r}")
        macro = run("--single-unit", "unit.sv", "macro_only.sv")
        if macro.returncode != 0 or macro.stdout != "unit=42 add=3\nmacro=7\n":
            failures.append(f"--single-unit: the macro of the file before is not visible: {macro.stdout!r}")
        # The order of the files is the order of the text: a file before cannot use what follows.
        reverse = run("--single-unit", "user.sv", "unit.sv")
        if reverse.returncode != 2 or "unit_shared" not in reverse.stderr:
            failures.append(f"--single-unit with the files reversed: exit {reverse.returncode}")
        parsed = run("parse", "--single-unit", "unit.sv", "user.sv")
        if parsed.returncode != 0:
            failures.append(f"parse --single-unit: exit {parsed.returncode}")
        parsed = run("parse", "unit.sv", "user.sv")
        if parsed.returncode != 2:
            failures.append(f"parse without --single-unit: exit {parsed.returncode}")
        valued = run("--single-unit=1", "unit.sv", "user.sv")
        if valued.returncode != 2 or "unknown option" not in valued.stderr:
            failures.append(f"--single-unit=1: not rejected (exit {valued.returncode})")
    for failure in failures:
        print("FAIL: " + failure, file=sys.stderr)
    if failures:
        return 1
    print("front options: --error-limit sets the number of errors, --single-unit joins the files")
    return 0


if __name__ == "__main__":
    sys.exit(main())
