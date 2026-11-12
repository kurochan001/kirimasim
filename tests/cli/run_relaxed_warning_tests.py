#!/usr/bin/env python3
# Copyright (c) 2026 The kirimasim Authors
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
"""Four of slang's errors are warnings by default, and -Werror=<name> makes each an error again (CLI.md 2.1).
Other errors of slang stay errors."""
import argparse
import subprocess
import sys
import tempfile
from pathlib import Path

DESIGNS = {
    "index-oob": "module top;\n  int a[2];\n  initial begin\n    a[5] = 1;\n    $display(\"%0d\", a[5]);\n  end\nendmodule\n",
    "range-oob": "module top;\n  logic [3:0] v = 4'b1010;\n  initial $display(\"%b\", v[5:2]);\nendmodule\n",
    "string-int-concat": "module top;\n  string s = \"x\";\n  byte v = \"a\";\n  initial $display(\"%s\", {s, v});\nendmodule\n",
    "string-in-constraint": "class c;\n  rand bit [3:0] s;\n  string t = \"a\";\n  constraint k { (t == \"a\") -> s < 4'd2; }\nendclass\n"
                            "module top;\n  c h;\n  initial begin\n    h = new;\n    $display(\"ok\");\n  end\nendmodule\n",
}
STRICT = "module dup; endmodule\nmodule dup; endmodule\nmodule top; dup u(); endmodule\n"


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--kirimasim", required=True)
    args = parser.parse_args()
    failures = []
    with tempfile.TemporaryDirectory() as directory:
        tmp = Path(directory)

        def run(text: str, *options: str) -> subprocess.CompletedProcess:
            path = tmp / "design.sv"
            path.write_text(text)
            return subprocess.run([args.kirimasim, "--top=top", *options, str(path)], cwd=tmp, capture_output=True,
                                  text=True, stdin=subprocess.DEVNULL, timeout=60)

        for name, text in DESIGNS.items():
            tag = f"[-W{name}]"
            relaxed = run(text)
            if relaxed.returncode != 0 or f"warning: " not in relaxed.stderr or tag not in relaxed.stderr:
                failures.append(f"{name}: not a warning by default (exit {relaxed.returncode})")
            if "error:" in relaxed.stderr:
                failures.append(f"{name}: an error by default")
            strict = run(text, f"-Werror={name}")
            if strict.returncode != 2 or not any("error:" in line and tag in line for line in strict.stderr.splitlines()):
                failures.append(f"{name}: -Werror={name} does not make it an error (exit {strict.returncode})")
            quiet = run(text, f"-Wno-{name}")
            if quiet.returncode != 0 or tag in quiet.stderr:
                failures.append(f"{name}: -Wno-{name} does not silence it")
        kept = run(STRICT)
        if kept.returncode != 2 or "error:" not in kept.stderr or "[-Wduplicate-definition]" not in kept.stderr:
            failures.append(f"duplicate-definition: no longer an error (exit {kept.returncode})")
    for failure in failures:
        print("FAIL: " + failure, file=sys.stderr)
    if failures:
        return 1
    print("relaxed warnings: four errors are warnings by default, and -Werror brings each back")
    return 0


if __name__ == "__main__":
    sys.exit(main())
