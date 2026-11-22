#!/usr/bin/env python3
# Copyright (c) 2026 The kirimasim Authors
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

"""Compile an emitted C++20 model independently and exercise both execution paths."""
import json
from pathlib import Path
import subprocess
import sys
import tempfile


def main():
    exe, compiler, project = sys.argv[1:]
    root = Path(project)
    with tempfile.TemporaryDirectory() as tmp:
        work = Path(tmp)
        source = work / "model.cpp"
        module = work / "model.so"

        def build():
            subprocess.run([compiler, "-std=c++20", "-fno-exceptions", "-fno-rtti",
                            "-ffp-contract=off", "-O2", "-shared", "-fPIC",
                            "-I", root / "src/kernel/include", source, "-o", module], check=True)

        def evaluate(design, *args):
            p = subprocess.run([exe, design, *args], capture_output=True, text=True, timeout=30)
            if p.returncode:
                raise AssertionError((p.returncode, p.stdout, p.stderr))
            lines = p.stdout.rstrip().splitlines()
            counts = {k: v for k, v in json.loads(lines[-1]).items()
                      if k.startswith(("sched.", "proc.", "slot.", "nba.", "wait.", "sim."))}
            return lines[:-1], counts, p.stderr

        for name in ("comb_chain", "nba_shift"):
            design = root / "bench" / (name + ".sv")
            subprocess.run([exe, design, "stale"], check=True)
            generated = subprocess.check_output([exe, design, "emit"])
            assert generated == subprocess.check_output([exe, design, "emit"])
            source.write_bytes(generated)
            build()
            baseline = evaluate(design, "vm")
            assert evaluate(design, module) == baseline
            assert evaluate(design, module, "check") == baseline
            if name == "comb_chain":
                changed = generated.replace(b"add4(", b"sub4(", 1)
                assert changed != generated
                source.write_bytes(changed)
                build()
                bad = subprocess.run([exe, design, module, "check"], capture_output=True, text=True)
                assert bad.returncode == 3 and "tier-check island=" in bad.stderr and "slot=" in bad.stderr, bad


if __name__ == "__main__":
    main()
