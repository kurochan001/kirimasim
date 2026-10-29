#!/usr/bin/env python3
# Copyright (c) 2026 The kirimasim Authors
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

"""Check the common input spellings and their actual frontend effects (CLI 2.1)."""

import argparse
from pathlib import Path
import subprocess
import sys
import tempfile

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / "tools"))
import bounded_run  # noqa: E402


MACROS = '''`include "common.svh"
module top;
  localparam int empty = 1 `EMPTY + 2;
  initial begin
    $display("dir=%0d flag=%0d value=%0d empty=%0d text=%s runtime=%0d",
             `DIRECTORY, `FLAG, `VALUE, empty, `TEXT, $test$plusargs("runtime"));
    $finish(0);
  end
endmodule
'''


def definitions(separate=False, flag="FLAG"):
    values = [flag, "VALUE=2+3", "EMPTY=", 'TEXT="a = b+c"']
    if separate:
        return [part for value in values for part in ("-D", value)]
    return ["-D" + value for value in values]


def output(directory, flag=1, runtime=1):
    return f"dir={directory} flag={flag} value=5 empty=3 text=a = b+c runtime={runtime}\n"


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--kirimasim", required=True, type=Path)
    exe = str(parser.parse_args().kirimasim.resolve())
    failures = []
    checked = 0
    with tempfile.TemporaryDirectory(prefix="kirimasim-common-input-") as directory:
        root = Path(directory)
        for name, number in (("first", 11), ("second", 22),
                             ("include path + equals=1", 33), ("local", 44)):
            path = root / name
            path.mkdir()
            (path / "common.svh").write_text(f"`define DIRECTORY {number}\n", encoding="utf-8")
        (root / "macros.sv").write_text(MACROS, encoding="utf-8")
        (root / "local" / "macros.sv").write_text(MACROS, encoding="utf-8")
        for name, number in (("lib first", 9), ("lib+second=2", 18)):
            path = root / name
            path.mkdir()
            library = (f'module library_child; initial $display("library={number}"); endmodule\n'
                       'module unused_library_top; initial $display("unexpected top"); endmodule\n')
            (path / "library_child.sv").write_text(library, encoding="utf-8")
            (path / "custom_child.custom").write_text(
                'module custom_child; initial $display("custom=27"); endmodule\n',
                encoding="utf-8")
        (root / "library_top.sv").write_text(
            "module top; library_child c(); initial begin #1; $finish(0); end endmodule\n",
            encoding="utf-8")
        (root / "custom_top.sv").write_text(
            "module top; custom_child c(); initial begin #1; $finish(0); end endmodule\n",
            encoding="utf-8")
        (root / "lists").mkdir()
        (root / "lists" / "sources.f").write_text(
            '-Ifirst -DVALUE=2+3 macros.sv\n', encoding="utf-8")
        (root / "lists" / "library.f").write_text(
            '-y "lib first" library_top.sv\n', encoding="utf-8")

        cases = []
        for separate in (False, True):
            defs = definitions(separate)
            for include in (["-Ifirst"], ["-I", "first"], ["+incdir+first"]):
                cases.append((f"macro forms {separate} {include}",
                              [*include, *defs, "macros.sv", "+runtime"], output(11)))
            for include in (["-Iinclude path + equals=1"],
                            ["-I", "include path + equals=1"]):
                cases.append((f"spaced include {separate} {include}",
                              [*include, *defs, "macros.sv", "+runtime"], output(33)))
        for include, number in ((["-Ifirst", "+incdir+second"], 11),
                                (["+incdir+first", "-Isecond"], 11),
                                (["-Isecond", "+incdir+first"], 22),
                                (["+incdir+second", "-Ifirst"], 22),
                                (["-I", "second", "-I", "first"], 22)):
            cases.append((f"include order {include}",
                          [*include, *definitions(), "macros.sv", "+runtime"], output(number)))
        cases += [
            ("local include first", ["-Ifirst", *definitions(), "local/macros.sv", "+runtime"],
             output(44)),
            ("explicit one", ["-Ifirst", *definitions(flag="FLAG=1"), "macros.sv", "+runtime"],
             output(11)),
            ("plus macro default", ["-Ifirst", "+define+FLAG", *definitions()[1:],
                                    "macros.sv", "+runtime"], output(11)),
            # Pinned slang keeps the first predefinition, also through the
            # existing -f path. Adding a spelling must preserve that rule.
            ("D after plus define", ["-Ifirst", "+define+FLAG=7", *definitions(flag="FLAG=8"),
                                     "macros.sv", "+runtime"], output(11, flag=7)),
            ("plus define after D", ["-Ifirst", *definitions(flag="FLAG=8"), "+define+FLAG=7",
                                     "macros.sv", "+runtime"], output(11, flag=8)),
            ("macros are not plusargs", ["-Ifirst", *definitions(), "-Druntime=1", "macros.sv"],
             output(11, runtime=0)),
            ("options after source", ["macros.sv", "-Ifirst", *definitions(), "+runtime"],
             output(11)),
            ("file list uses working directory", ["-DFLAG", "-DEMPTY=", '-DTEXT="a = b+c"',
                                                  "-f", "lists/sources.f", "+runtime"], output(11)),
            ("library file list", ["-f", "lists/library.f"], "library=9\n"),
        ]
        for library in (["-ylib first"], ["-y", "lib first"], ["--libdir=lib first"]):
            cases.append((f"library form {library}", [*library, "library_top.sv"], "library=9\n"))
            cases.append((f"library extension {library}",
                          [*library, "+libext+.custom", "custom_top.sv"], "custom=27\n"))
        cases += [
            ("library order first", ["-ylib first", "--libdir=lib+second=2", "library_top.sv"],
             "library=9\n"),
            ("library order second", ["--libdir=lib+second=2", "-y", "lib first", "library_top.sv"],
             "library=18\n"),
        ]

        errors = [
            ("missing I", ["-I"], "-I needs a value"),
            ("missing D", ["-D"], "-D needs a value"),
            ("missing y", ["-y"], "-y needs a value"),
            ("empty I", ["-I", "", "macros.sv"], "-I needs a nonempty directory"),
            ("empty y", ["-y", "", "library_top.sv"], "-y needs a nonempty directory"),
            ("empty libdir", ["--libdir=", "library_top.sv"], "--libdir needs a nonempty directory"),
            ("empty D", ["-D", "", "macros.sv"], "-D needs a nonempty macro name"),
            ("no D name", ["-D=1", "macros.sv"], "-D needs a nonempty macro name"),
            ("no separate D name", ["-D", "=1", "macros.sv"], "-D needs a nonempty macro name"),
            ("no name or value", ["-D=", "macros.sv"], "-D needs a nonempty macro name"),
            ("I is not y", ["-Ilib first", "library_top.sv"], "library_child"),
            ("y is not I", ["-yfirst", *definitions(), "macros.sv"], "common.svh"),
            ("extension is required", ["-ylib first", "custom_top.sv"], "custom_child"),
            ("unapproved input flag", ["-UFLAG", "macros.sv"], "unknown option '-UFLAG'"),
        ]
        for mode in ([], ["parse"]):
            for label, arguments, expected in cases:
                checked += 1
                try:
                    result = bounded_run.run([exe, *mode, *arguments], cwd=root, timeout=20)
                except (subprocess.TimeoutExpired, bounded_run.OutputLimit) as error:
                    failures.append(f"{mode} {label}: {error}")
                    continue
                wanted = "" if mode else expected
                if result.returncode != 0 or result.stdout != wanted:
                    failures.append(f"{mode} {label}: exit={result.returncode}, "
                                    f"stdout={result.stdout!r}, stderr={result.stderr!r}")
            for label, arguments, diagnostic in errors:
                checked += 1
                try:
                    result = bounded_run.run([exe, *mode, *arguments], cwd=root, timeout=20)
                except (subprocess.TimeoutExpired, bounded_run.OutputLimit) as error:
                    failures.append(f"{mode} {label}: {error}")
                    continue
                if result.returncode != 2 or result.stdout or diagnostic not in result.stderr:
                    failures.append(f"{mode} {label}: exit={result.returncode}, "
                                    f"stdout={result.stdout!r}, stderr={result.stderr!r}")
        help_result = bounded_run.run([exe, "--help"], cwd=root, timeout=20)
        checked += 1
        for spelling in ("-I<dir>", "-I <dir>", "-D<name>", "-D <name>",
                         "-y<dir>", "-y <dir>", "--libdir=<dir>"):
            if help_result.returncode != 0 or spelling not in help_result.stdout:
                failures.append(f"help missing {spelling}")

    print(f"common input options: {checked} checks, {len(failures)} failures")
    for failure in failures:
        print(failure)
    return bool(failures)


if __name__ == "__main__":
    raise SystemExit(main())
