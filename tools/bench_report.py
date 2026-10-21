#!/usr/bin/env python3
# Copyright (c) 2026 The kirimasim Authors
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

"""Compares a nightly benchmark result with the nights before it
(PERFORMANCE.md 5.1).

  tools/bench_report.py --results <results.json> --build-seconds <s>
                        --commit <sha> --out <nightly.json> [--history <dir>]
                        [--summary <file>]

<results.json> is what tools/bench.py writes. The script writes this
night's record to <nightly.json>: the date, the commit, the build time of
kirimasim and the results. The baseline of each value is the median of
the last 7 records found under <dir> (any nightly.json in it, ordered by
their dates). A value worse by 15% or more (the simulation time or the
peak RSS of a design), or by 25% or more (the build time), gets a GitHub
Actions warning. The table goes to standard output and is added to
<file>, the job summary. The exit code is 0 whatever the values: the
report passes or fails nothing, as shared runners swing widely.
"""

import argparse
import datetime
import json
import statistics
import sys
from pathlib import Path

NIGHTS = 7
RUN_LIMIT = 0.15    # simulation time and peak RSS
BUILD_LIMIT = 0.25  # build time


def history(directory: Path | None) -> list[dict]:
    if directory is None or not directory.is_dir():
        return []
    records = []
    for path in directory.rglob("nightly.json"):
        try:
            records.append(json.loads(path.read_text(encoding="utf-8")))
        except (OSError, ValueError):
            print(f"::warning::bench_report: cannot read {path}, left out")
    records.sort(key=lambda r: r.get("date", ""))
    return records[-NIGHTS:]


def values(record: dict) -> dict[str, float]:
    """The compared values of one record, by name."""
    out = {}
    if record.get("build_seconds") is not None:
        out["build seconds"] = float(record["build_seconds"])
    for design, r in record.get("results", {}).get("results", {}).get("kirimasim", {}).items():
        if r.get("simulation_seconds") is not None:
            out[f"{design} seconds"] = float(r["simulation_seconds"])
        if r.get("max_rss_mib") is not None:
            out[f"{design} MiB"] = float(r["max_rss_mib"])
    return out


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("--results", type=Path, required=True)
    ap.add_argument("--build-seconds", type=float, required=True)
    ap.add_argument("--commit", required=True)
    ap.add_argument("--out", type=Path, required=True)
    ap.add_argument("--history", type=Path, default=None)
    ap.add_argument("--summary", type=Path, default=None)
    args = ap.parse_args()

    record = {
        "date": datetime.datetime.now(datetime.timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ"),
        "commit": args.commit,
        "build_seconds": args.build_seconds,
        "results": json.loads(args.results.read_text(encoding="utf-8")),
    }
    args.out.parent.mkdir(parents=True, exist_ok=True)
    args.out.write_text(json.dumps(record, indent=1) + "\n", encoding="utf-8")

    past = history(args.history)
    now = values(record)
    lines = [f"Benchmarks at {args.commit[:12]}, against the median of the last {len(past)} "
             f"night(s) (PERFORMANCE.md 5.1; a warning at +{RUN_LIMIT:.0%} for times and "
             f"RSS, +{BUILD_LIMIT:.0%} for the build):", "",
             "| Value | This night | Baseline | Change |", "| --- | --- | --- | --- |"]
    warnings = []
    for name, v in now.items():
        before = [x for x in (values(r).get(name) for r in past) if x is not None]
        if not before:
            lines.append(f"| {name} | {v:.3f} | — | no baseline |")
            continue
        base = statistics.median(before)
        change = v / base - 1 if base > 0 else 0.0
        limit = BUILD_LIMIT if name == "build seconds" else RUN_LIMIT
        mark = " (worse)" if v >= base * (1 + limit) else ""
        lines.append(f"| {name} | {v:.3f} | {base:.3f} | {change:+.1%}{mark} |")
        if mark:
            warnings.append(f"{name}: {v:.3f} against a baseline of {base:.3f} ({change:+.1%})")
    lines += ["", f"{len(warnings)} value(s) worse than their limit."]
    text = "\n".join(lines) + "\n"
    print(text, end="")
    if args.summary is not None:
        with args.summary.open("a", encoding="utf-8") as f:
            f.write(text)
    for w in warnings:
        print(f"::warning::benchmark {w}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
