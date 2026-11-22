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

        known_module = work / "known.so"

        def build(output=module):
            subprocess.run([compiler, "-std=c++20", "-fno-exceptions", "-fno-rtti",
                            "-ffp-contract=off", "-O2", "-shared", "-fPIC",
                            "-I", root / "src/kernel/include", source, "-o", output], check=True)

        def evaluate(design, *args):
            p = subprocess.run([exe, design, *args], capture_output=True, text=True, timeout=30)
            if p.returncode:
                raise AssertionError((p.returncode, p.stdout, p.stderr))
            lines = p.stdout.rstrip().splitlines()
            counts = {k: v for k, v in json.loads(lines[-1]).items()
                      if k.startswith(("sched.", "proc.", "slot.", "nba.", "wait.", "sim."))}
            return lines[:-1], counts, p.stderr

        # The island starts in X, sees known inputs, returns to X and Z,
        # then sees known inputs again. The two-state path must be reversible.
        toggle = work / "switch.sv"
        toggle.write_text("""module top;
  logic [7:0] a, b;
  always_comb b = a + 8'd3;
  initial begin
    #1; $display("%h", b);
    a = 4; #1; $display("%h", b);
    a = 'x; #1; $display("%h", b);
    a = 'z; #1; $display("%h", b);
    a = 9; #1; $display("%h", b);
  end
endmodule
""")
        for design in (root / "bench/comb_chain.sv", root / "bench/nba_shift.sv", toggle):
            subprocess.run([exe, design, "stale"], check=True)
            generated = subprocess.check_output([exe, design, "emit"])
            assert generated == subprocess.check_output([exe, design, "emit"])
            source.write_bytes(generated)
            build()
            baseline = evaluate(design, "vm")
            assert evaluate(design, module) == baseline
            assert evaluate(design, module, "check") == baseline
            known = subprocess.check_output([exe, design, "emit-known"])
            assert known and known == subprocess.check_output([exe, design, "emit-known"])
            source.write_bytes(known)
            build(known_module)
            for mode in ("run", "check"):
                assert evaluate(design, module, mode, known_module) == baseline
            if design == toggle:
                assert baseline[0] == ["xx", "07", "xx", "xx", "0c"], baseline
                out = subprocess.check_output([exe, design, module, "run", known_module], text=True)
                counts = json.loads(out.rstrip().splitlines()[-1])
                assert counts["island.evals2"] == 2 and counts["island.evals4"] == 3, counts
                changed = known.replace(b".a + ", b".a - ", 1)
                assert changed != known
                source.write_bytes(changed)
                build(known_module)
                bad = subprocess.run([exe, design, module, "check", known_module],
                                     capture_output=True, text=True)
                assert bad.returncode == 3 and "tier-check island=" in bad.stderr, bad
            if design.stem == "comb_chain":
                changed = generated.replace(b"add4(", b"sub4(", 1)
                assert changed != generated
                source.write_bytes(changed)
                build()
                bad = subprocess.run([exe, design, module, "check"], capture_output=True, text=True)
                assert bad.returncode == 3 and "tier-check island=" in bad.stderr and "slot=" in bad.stderr, bad

        deep = work / "deep.sv"
        deep.write_text("module top; int i, v; always_comb begin v = i;\n"
                        + "v = v + 1;\n" * 250
                        + 'end initial begin i=3; #1; $display("%0d", v); end endmodule\n')
        generated = subprocess.check_output([exe, deep, "emit"])
        assert b"static void part" in generated
        source.write_bytes(generated)
        build()
        assert evaluate(deep, module, "check") == evaluate(deep, "vm")

        # Known inputs can still produce X from division by zero or an
        # out-of-range selector. Such islands keep normal four-state code.
        for expression in ("a / k", "a[k]", "a / 8'd0", "a + 8'hxz"):
            toggle.write_text(f"""module top;
  logic [7:0] a, k, b;
  always_comb b = {expression};
  initial begin a = 1; k = 2; #1; $display("%h", b); end
endmodule
""")
            assert subprocess.check_output([exe, toggle, "emit-known"]) == b"", expression


if __name__ == "__main__":
    main()
