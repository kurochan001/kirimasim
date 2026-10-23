#!/usr/bin/env python3
# Copyright (c) 2026 The kirimasim Authors
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

"""Check program metadata and strict illegal-input diagnostics (24.3, 24.6)."""

import argparse
import re
import sys
import tempfile
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / "tools"))
import bounded_run  # noqa: E402


INVALID = [
    "program worker; always #1; endprogram module top; worker p(); endmodule",
    "program worker; int value; endprogram module top; worker p(); initial $display(\"%0d\",p.value); endmodule",
    "package pkg; program; function int value(); return 7; endfunction endprogram endpackage module top; initial $display(\"%0d\",pkg::value()); endmodule",
    "module leaf; endmodule program worker; leaf u(); endprogram module top; worker p(); endmodule",
    "interface leaf; endinterface program worker; leaf u(); endprogram module top; worker p(); endmodule",
]


def main():
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--kirimasim", required=True, type=Path)
    args = parser.parse_args()
    simulator = str(args.kirimasim.resolve())
    with tempfile.TemporaryDirectory(prefix="kirimasim-program-cli-") as tmp:
        path = Path(tmp) / "program.sv"
        for source in INVALID:
            path.write_text(source)
            for options in (("parse",), ("--dump-sir",), ()):
                ran = bounded_run.run([simulator, *options, str(path)], cwd=tmp, timeout=10)
                if ran.returncode != 2 or ran.stdout or "error:" not in ran.stderr or "internal error" in ran.stderr:
                    raise AssertionError(f"program illegal input {options}: {ran.returncode}\n{ran.stdout}\n{ran.stderr}")
        path.write_text("program worker; initial #1; endprogram module top; worker p[2](); endmodule\n")
        ran = bounded_run.run([simulator, "--dump-sir", str(path)], cwd=tmp, timeout=10)
        rows = re.findall(r"^instance .* program$", ran.stdout, re.M)
        if ran.returncode or len(rows) != 2:
            raise AssertionError(f"program instance metadata: {ran.returncode}\n{ran.stdout}\n{ran.stderr}")
    print("program CLI: 15 illegal-input checks and 2 instance identities checked")
    return 0


if __name__ == "__main__":
    sys.exit(main())
