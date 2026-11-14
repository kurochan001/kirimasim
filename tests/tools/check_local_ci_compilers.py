#!/usr/bin/env python3
# Copyright (c) 2026 The kirimasim Authors
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

"""local_ci.py's summary names the compilers the build trees record.

The compiler lines come from each preset's CMakeCache.txt, so a tree
configured with one compiler is reported with it even when CC and CXX
name another one later.
"""

import os
import stat
import sys
import tempfile
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / "tools"))
import local_ci  # noqa: E402


def fake_compiler(folder: Path, name: str, banner: str) -> Path:
    path = folder / name
    path.write_text(f"#!/bin/sh\necho '{banner}'\necho 'second line'\n", encoding="utf-8")
    path.chmod(path.stat().st_mode | stat.S_IXUSR)
    return path


def cache(build: Path, preset: str, cxx: Path, cc: Path) -> None:
    (build / preset).mkdir(parents=True)
    (build / preset / "CMakeCache.txt").write_text(
        "# This is the CMakeCache file.\n"
        "CMAKE_BUILD_TYPE:STRING=Debug\n"
        f"CMAKE_CXX_COMPILER:FILEPATH={cxx}\n"
        f"CMAKE_C_COMPILER:FILEPATH={cc}\n", encoding="utf-8")


def main():
    with tempfile.TemporaryDirectory() as tmp:
        tools, build = Path(tmp) / "bin", Path(tmp) / "build"
        tools.mkdir()
        new_cxx = fake_compiler(tools, "new-c++", "NewCXX version 23")
        new_cc = fake_compiler(tools, "new-cc", "NewCC version 23")
        old_cxx = fake_compiler(tools, "old-c++", "OldCXX version 20")
        other = fake_compiler(tools, "other", "Other version 1")
        cache(build, "dev", new_cxx, new_cc)
        cache(build, "release", new_cxx, new_cc)
        cache(build, "asan", old_cxx, new_cc)
        (build / "ubsan").mkdir()

        # CC and CXX of the environment do not change what is reported.
        os.environ["CXX"] = str(other)
        os.environ["CC"] = str(other)
        lines = local_ci.configured_compilers(build, ["dev", "release", "asan", "ubsan"])
        assert lines == [
            f"  compiler: c++ NewCXX version 23 ({new_cxx}, dev release)",
            f"  compiler: c++ OldCXX version 20 ({old_cxx}, asan)",
            "  compiler: c++ unknown (ubsan)",
            f"  compiler: cc  NewCC version 23 ({new_cc}, dev release asan)",
            "  compiler: cc  unknown (ubsan)",
        ], lines
        assert all("Other" not in line for line in lines), lines

        lines = local_ci.configured_compilers(build, ["dev"])
        assert lines == [f"  compiler: c++ NewCXX version 23 ({new_cxx}, dev)",
                         f"  compiler: cc  NewCC version 23 ({new_cc}, dev)"], lines

        assert local_ci.configured_compilers(build, []) == ["  compiler: none (no preset was run)"]
    print("local_ci compiler lines: recorded compilers, mixed trees, missing caches and no presets checked")


if __name__ == "__main__":
    main()
