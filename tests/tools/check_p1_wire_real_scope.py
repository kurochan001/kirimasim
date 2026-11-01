#!/usr/bin/env python3
# Copyright (c) 2026 The kirimasim Authors
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
"""P1 scope, raw outcomes and fault handling for the confirmed wire-real exception."""
import contextlib
import importlib.util
import io
import json
import subprocess
import sys
import tempfile
from pathlib import Path
from unittest import mock

TOOLS = Path(__file__).resolve().parent.parent.parent / "tools"
failures = []


def feature_probe_p1_scope(tmp: Path) -> None:
    """Only confirmed extension dependency is optional; faults and reals remain visible."""
    spec = importlib.util.spec_from_file_location("feature_scope", TOOLS / "icarus_feature_probe.py")
    probe = importlib.util.module_from_spec(spec)
    with mock.patch.object(sys, "path", [*sys.path, str(TOOLS)]):
        spec.loader.exec_module(probe)
    original = next(row for row in probe.PROBES if row[0] == "real_net")
    ordinary = "probe.sv:1:23: error: unsupported real net declaration\n"
    # Native result, compile-only result, source override, expected gate result.
    scenarios = [
        ("compile_rejection", (2, "", ordinary), (2, "", ordinary), None, 0),
        ("supported", (0, "1.5\n", ""), None, None, 0),
        ("optional_output_mismatch", (0, "0.0\n", ""), None, None, 0),
        ("crash", (-11, "", "segmentation fault"), None, None, 1),
        ("shell_crash", (139, "", "segmentation fault"), None, None, 1),
        ("timeout", "timeout", None, None, 1),
        ("output_limit", "output_limit", None, None, 1),
        ("internal", (3, "", "kirimasim: internal error: invalid design"), None, None, 1),
        ("internal_exit_two", (2, "", "kirimasim: internal error: bad design"),
         (2, "", ordinary), None, 1),
        ("internal_compile", (2, "", ordinary),
         (2, "", "probe.sv:1:23: error: internal error: invalid design"), None, 1),
        ("compile_crash", (2, "", ordinary), (-11, "", "segmentation fault"), None, 1),
        ("compile_timeout", (2, "", ordinary), "timeout", None, 1),
        ("compile_output_limit", (2, "", ordinary), "output_limit", None, 1),
        ("runtime_error", (2, "", ordinary), (0, "sir\n", ""), None, 1),
        ("missing_compile_diagnostic", (2, "", ""), (2, "", ""), None, 1),
        ("resource_failure", (6, "", "resource limit"), None, None, 1),
        ("unconfirmed_changed_source", (2, "", ordinary), None,
         original[2] + "\n// changed source requires dependency review", 1),
        ("standard_variable", (2, "", ordinary), None,
         'module top; real real_net; initial begin real_net=1.5; $display("%0.1f",real_net); end endmodule', 1),
        ("standard_continuous", (2, "", ordinary), None,
         'module top; real value; assign value=1.5; initial #1 $display("%0.1f",value); endmodule', 1),
        ("standard_nettype", (2, "", ordinary), None,
         'module top; nettype real sample_t; sample_t value; assign value=1.5; initial #1 $display("%0.1f",value); endmodule', 1),
        ("mixed_standard_and_extension", (2, "", ordinary), None,
         original[2].replace('initial begin #1;', 'real standard_value; assign standard_value=2.5; initial begin #1; if(standard_value!=2.5)$fatal;'), 1),
    ]
    for name, native, compile_result, changed, want in scenarios:
        row = (*original[:2], changed or original[2], *original[3:])
        report = tmp / ("scope_" + name + ".json")

        def engine(command, **kwargs):
            if "-V" in command or "--version" in command:
                return subprocess.CompletedProcess(command, 0, b"mock version\n", b"")
            if command[0] == "iverilog":
                return subprocess.CompletedProcess(command, 0, b"", b"")
            if command[0] == "vvp":
                return subprocess.CompletedProcess(command, 0, b"1.5\n", b"")
            value = compile_result if "--dump-sir" in command else native
            if "--dump-sir" in command and changed and value is None:
                value = (2, "", ordinary)
            if value == "timeout":
                raise subprocess.TimeoutExpired(command, 15)
            if value == "output_limit":
                raise probe.bounded_run.OutputLimit(command, "stdout")
            if value is None:
                raise AssertionError("unexpected compile-only check: " + name)
            code, stdout, stderr = value
            return subprocess.CompletedProcess(command, code, stdout.encode(), stderr.encode())

        with mock.patch.object(probe, "PROBES", [row]), \
             mock.patch.object(probe.bounded_run, "run", side_effect=engine), \
             mock.patch.object(sys, "argv", ["probe", "--kirimasim", str(tmp / "sim"),
                                            "--output", str(report), "--require-parity"]), \
             contextlib.redirect_stdout(io.StringIO()):
            code = probe.main()
        data = json.loads(report.read_text())
        excluded = 0 if changed else 1
        if code != want or data["p1_scope"] != {
                "total": 1, "evaluated": 1 - excluded, "out_of_scope": excluded}:
            failures.append("wire-real scope or parity gate is incorrect: " + name)
        result = data["probes"][0]
        if (native != (0, "1.5\n", "") and result["kirimasim"]["status"] == "passed"):
            failures.append("wire-real exemption fabricated a PASS: " + name)
        if changed:
            if data["p1_out_of_scope"] or data["p1_required_gaps"] != ["real_net"]:
                failures.append("real spelling or changed/mixed source was broadly excluded: " + name)
            if result["category"] != "standard" or not result["p1_required"]:
                failures.append("standard real coverage was relabeled as an extension: " + name)
        else:
            if data["p1_required_gaps"] or not data["p1_out_of_scope"][0]["reason"]:
                failures.append("confirmed optional extension became a P1 feature blocker: " + name)
            if want and not data["independent_failures"]:
                failures.append("optional extension masked an independent failure: " + name)



def main():
    with tempfile.TemporaryDirectory() as directory:
        feature_probe_p1_scope(Path(directory))
    for failure in failures:
        print("FAIL: " + failure, file=sys.stderr)
    if failures:
        return 1
    print("P1 wire-real scope: all 21 exemption, fault and standard-real scenarios passed")
    return 0


if __name__ == "__main__":
    sys.exit(main())
