#!/usr/bin/env python3
# Copyright (c) 2026 The kirimasim Authors
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
"""The fixed probe list and the one allowed time limit of the P1 exit (P1_SCOPE)."""
import contextlib
import hashlib
import importlib.util
import io
import json
import subprocess
import sys
import tempfile
from pathlib import Path
from unittest import mock

ROOT = Path(__file__).resolve().parent.parent.parent
TOOLS = ROOT / "tools"
failures = []


def digest(source: str) -> str:
    return hashlib.sha256((source + "\n").encode()).hexdigest()


def load_probe():
    spec = importlib.util.spec_from_file_location("feature_exit", TOOLS / "icarus_feature_probe.py")
    probe = importlib.util.module_from_spec(spec)
    with mock.patch.object(sys, "path", [*sys.path, str(TOOLS)]):
        spec.loader.exec_module(probe)
    return probe


def run_tool(probe, tmp: Path, row, *extra, run=None, parse=None):
    """The exit code of the tool, and its report. Nothing here needs Icarus. run and parse
    say how kirimasim's normal run and `kirimasim parse` end: "time" or "output" for a
    limit, an exit code, or None for a run that gives the expected output."""

    def ended(command, outcome):
        if outcome == "time":
            raise subprocess.TimeoutExpired(command, 1)
        if outcome == "output":
            raise probe.bounded_run.OutputLimit(command, "standard output")
        if outcome is not None:
            return subprocess.CompletedProcess(command, outcome, b"", b"")
        return subprocess.CompletedProcess(command, 0, b"7\n", b"")

    def engine(command, **kwargs):
        if "-V" in command or "--version" in command:
            return subprocess.CompletedProcess(command, 0, b"mock version\n", b"")
        if command[0] == "iverilog":
            return subprocess.CompletedProcess(command, 0, b"", b"")
        if command[0] == "vvp":
            return ended(command, None)
        return ended(command, parse if "parse" in command else run)

    report = tmp / "report.json"
    report.unlink(missing_ok=True)
    arguments = ["probe", "--kirimasim", str(tmp / "sim"), "--output", str(report), *extra]
    with mock.patch.object(probe, "PROBES", [row]), \
         mock.patch.object(probe.bounded_run, "run", side_effect=engine), \
         mock.patch.object(sys, "argv", arguments), \
         contextlib.redirect_stdout(io.StringIO()), contextlib.redirect_stderr(io.StringIO()):
        try:
            code = probe.main()
        except SystemExit as stop:
            code = stop.code
    return code, json.loads(report.read_text()) if report.exists() else None


def fixed_list(probe) -> None:
    listed = dict(line.split() for line in (ROOT / "tests/gates/p1_probes.txt").read_text().splitlines()
                  if line and not line.startswith("#"))
    if listed != {row[0]: digest(row[2]) for row in probe.PROBES + probe.EXTENDED}:
        failures.append("tests/gates/p1_probes.txt differs from the probes in the tool")
    for name, allowed in probe.P1_FRONTEND_LIMITS.items():
        if listed.get(name) != allowed["source_sha256"]:
            failures.append("the allowed time limit is not for the source on the list: " + name)


def gate(probe, tmp: Path) -> None:
    row = ("listed", "test", "listed", "7\n", [])
    source = digest("listed")
    listing = tmp / "list.txt"
    for name, text, want in (("the same list", f"# a comment\nlisted {source}\n", 0),
                             ("another source", f"listed {'0' * 64}\n", 2),
                             ("a name too many", f"listed {source}\nother {source}\n", 2),
                             ("a name too few", "", 2)):
        listing.write_text(text)
        code, _ = run_tool(probe, tmp, row, "--gate", str(listing))
        if code != want:
            failures.append(f"--gate with {name}: exit code {code}, expected {want}")


def allowance(probe) -> None:
    source = probe.P1_FRONTEND_LIMITS["dense_udp_coverage"]["source_sha256"]
    time = {"status": "limit", "limit_kind": "time"}
    output = {"status": "limit", "limit_kind": "output"}
    unknown = {"status": "limit"}
    allowed = {"name": "dense_udp_coverage", "source_sha256": source, "kirimasim": {**time, "parse_check": time}}
    if probe.independent_failure(allowed, "kirimasim") or not probe.p1_exclusion(allowed):
        failures.append("the allowed front-end time limit is reported as a failure")
    for name, changed in (
            ("another source", {"source_sha256": "0"}),
            ("another probe", {"name": "other"}),
            ("no parse check", {"kirimasim": time}),
            ("a parse that ends", {"kirimasim": {**time, "parse_check": {"status": "finished"}}}),
            ("an output limit of the run", {"kirimasim": {**output, "parse_check": time}}),
            ("an output limit of the parse", {"kirimasim": {**time, "parse_check": output}}),
            ("output limits of both", {"kirimasim": {**output, "parse_check": output}}),
            ("a limit of the run of no known kind", {"kirimasim": {**unknown, "parse_check": time}}),
            ("a limit of the parse of no known kind", {"kirimasim": {**time, "parse_check": unknown}}),
            ("a crash", {"kirimasim": {"status": "run_failed", "exit": -11, "parse_check": time}}),
            ("an internal error", {"kirimasim": {"status": "run_failed", "exit": 3, "parse_check": time}})):
        row = {**allowed, **changed}
        if not probe.independent_failure(row, "kirimasim") or probe.p1_exclusion(row):
            failures.append("a failure outside the allowance is excused: " + name)
    if not probe.independent_failure({**allowed, "icarus": {"status": "limit"}}, "icarus"):
        failures.append("a time limit of the reference is excused")


def limit_through_the_tool(probe, tmp: Path) -> None:
    name = "dense_udp_coverage"
    row = (name, "test", "slow", "7\n", [])
    reviewed = {name: {"source_sha256": digest("slow"), "reason": "reviewed"}}

    def required(code, data) -> bool:
        return (code == 1 and data["p1_required_gaps"] == [name] and bool(data["independent_failures"]) and
                not data["p1_out_of_scope"] and not data["p1_scope"]["out_of_scope"] and
                data["probes"][0]["p1_required"] and "p1_exclusion_reason" not in data["probes"][0])

    with mock.patch.dict(probe.P1_FRONTEND_LIMITS, reviewed):
        code, data = run_tool(probe, tmp, row, "--require-parity", run="time", parse="time")
        result = data["probes"][0]
        if (code != 0 or data["p1_required_gaps"] or data["independent_failures"] or
                data["p1_scope"] != {"total": 1, "evaluated": 0, "out_of_scope": 1}):
            failures.append("the front end alone reaching the limit still fails the gate")
        if result["p1_required"] or result.get("p1_exclusion_reason") != "reviewed":
            failures.append("the report calls the allowed probe required")
        if result["kirimasim"]["status"] != "limit" or data["gaps"] != [name]:
            failures.append("the allowance fabricated a pass or hid the raw gap")
        if (result["kirimasim"].get("limit_kind") != "time" or
                result["kirimasim"].get("parse_check", {}).get("limit_kind") != "time"):
            failures.append("the report does not keep which limits were reached")

        # Every other ending of the two runs stays a failure of a required probe (P1_SCOPE).
        for run, parse in (("time", "output"), ("output", "time"), ("output", "output"),
                           ("time", 0), ("output", 0), ("time", 2), ("time", 3), ("time", -11)):
            label = f"the run reaching {run}, the parse ending with {parse}"
            code, data = run_tool(probe, tmp, row, "--require-parity", run=run, parse=parse)
            if not required(code, data):
                failures.append("a limit outside the allowance is excused: " + label)
            kept = data["probes"][0]["kirimasim"]
            check = kept.get("parse_check", {})
            ended = (check.get("limit_kind") == parse if isinstance(parse, str)
                     else check.get("status") == "finished" and check.get("exit") == parse)
            if kept["status"] != "limit" or kept.get("limit_kind") != run or not ended:
                failures.append("the report does not keep how the runs ended: " + label)

    # The same two time limits on any other source.
    changed = {name: {"source_sha256": digest("another"), "reason": "reviewed"}}
    with mock.patch.dict(probe.P1_FRONTEND_LIMITS, changed):
        code, data = run_tool(probe, tmp, row, "--require-parity", run="time", parse="time")
        if not required(code, data):
            failures.append("the allowed time limit reaches a changed source")


def main() -> int:
    probe = load_probe()
    with tempfile.TemporaryDirectory() as directory:
        tmp = Path(directory)
        fixed_list(probe)
        gate(probe, tmp)
        allowance(probe)
        limit_through_the_tool(probe, tmp)
    for failure in failures:
        print("FAIL: " + failure, file=sys.stderr)
    if failures:
        return 1
    print("P1 closed exit: the fixed list and the allowed time limit hold")
    return 0


if __name__ == "__main__":
    sys.exit(main())
