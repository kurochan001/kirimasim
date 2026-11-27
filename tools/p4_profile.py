#!/usr/bin/env python3
# Copyright (c) 2026 The kirimasim Authors
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

"""Summarize an opt-in tier report, scheduler counters and macOS sample trees."""

import argparse
from collections import Counter, defaultdict
import json
from pathlib import Path
import re


def coverage(rows):
    """Count per-process activity and the reasons for taking the VM path."""
    known = ("native_calls", "vm_entries", "vm_instructions")
    total = {key: sum(row[key] for row in rows) for key in known}
    by_reason = {key: defaultdict(Counter)
                 for key in ("generation", "exclusion", "rejection", "blocking_member")}
    island_calls = island_vm_entries = 0
    for row in rows:
        if row["island"] is not None:
            island_calls += row["native_calls"]
            island_vm_entries += row["vm_entries"]
        activity = {"processes": 1, **{key: row[key] for key in known}}
        labels = {
            "generation": (row["generation"],),
            "exclusion": row["exclusions"],
            "rejection": (row["rejection"],) if row["rejection"] else (),
            "blocking_member": (map(str, row["blocking_members"])
                                if row["generation"] == "blocked-by-peer" else ()),
        }
        for category, reasons in labels.items():
            for reason in reasons:
                by_reason[category][reason].update(activity)
    return {
        "processes": len(rows),
        **total,
        "island_native_calls": island_calls,
        "island_vm_entries": island_vm_entries,
        "by_reason": by_reason,
        "hottest_vm": sorted((row for row in rows if row["vm_instructions"]),
                             key=lambda row: -row["vm_instructions"])[:10],
        "hottest_native": sorted((row for row in rows if row["native_calls"]),
                                 key=lambda row: -row["native_calls"])[:10],
    }


def scc_shape(island_report):
    membership = Counter(re.findall(r"\bscc=(\d+)\b", island_report))
    return {"components": len(membership),
            "members": sum(membership.values()),
            "components_by_member_count": dict(sorted(Counter(membership.values()).items()))}


def leaf_category(leaf, nba_apply):
    if "(in model.so)" in leaf:
        return "generated_cpp"
    if nba_apply:
        return "nba_application"
    if "Scheduler::DirtyMember" in leaf or "Scheduler::BoundArm" in leaf:
        return "island_and_wait_scheduling"
    if "Scheduler::" in leaf:
        if any(f"Scheduler::{name}" in leaf for name in
               ("scheduleNba", "addSlotNba", "enqueueNba", "captureNba")):
            return "nba_registration"
        if any(f"Scheduler::{name}" in leaf for name in
               ("wait", "rearm", "wake", "evaluateIsland", "evaluateCombGroup",
                "resumeProcess", "executeMember", "preparedMatches", "orderBoundArms")):
            return "island_and_wait_scheduling"
        if any(f"Scheduler::{name}" in leaf for name in
               ("write", "deliverSubscriptions", "notify", "onChange")):
            return "slot_writes_and_notification"
        if any(f"Scheduler::{name}" in leaf for name in
               ("run()", "runSlot", "runRegion", "dispatchThrough", "moveFirst",
                "nextTime", "takeInterrupt")):
            return "time_slot_and_region"
    if "kirimasim::vm::" in leaf:
        if "Interpreter::waitTriggers(" in leaf:
            return "island_and_wait_scheduling"
        if any(name in leaf for name in ("resumeBody<", "evaluateComb(", "readyComb(")):
            return "vm_resume_and_dispatch"
        return "vm_interpreter"
    return "other"


def is_nba_application(stack, nba_sites):
    for frame in stack:
        if "Scheduler::execute(" in frame:
            offsets = re.search(r"\(in [^)]*\) \+ (\d+(?:,\d+)*) +\[", frame)
            if offsets and all(int(offset) in nba_sites for offset in offsets[1].split(",")):
                return True
    return False


def sample_tree(text, nba_sites):
    """Use exclusive counts on the scheduler thread and keep overlapping ancestry."""
    tree = text.split("Call graph:\n", 1)[1].split("\nTotal number in stack", 1)[0]
    pending = []
    totals = Counter()
    self_samples = Counter()
    inclusive = Counter()
    functions = Counter()

    def finish():
        _, count, frame, descendants = pending.pop()
        self_count = count - descendants
        if self_count < 0:
            raise ValueError(f"Invalid sample tree below {frame}")
        if pending:
            parent = pending[-1]
            parent[3] += count
        if not self_count:
            return
        path = [item[2] for item in pending] + [frame]
        if not any("Scheduler::run()" in f for f in path):
            return
        totals["scheduler_samples"] += self_count
        nba_apply = is_nba_application(path, nba_sites)
        category = leaf_category(frame, nba_apply)
        self_samples[category] += self_count
        functions[frame.partition("  (in ")[0]] += self_count
        island = any("Scheduler::evaluateIsland(" in f for f in path)
        if island:
            inclusive["evaluate_island"] += self_count
        if nba_apply:
            inclusive["nba_application"] += self_count
        if island or nba_apply:
            inclusive["island_or_nba_application"] += self_count
        if not island and category == "vm_interpreter":
            inclusive["vm_interpreter_outside_island"] += self_count

    for line in tree.splitlines():
        match = re.fullmatch(r"([ +!:\|\t]*)(\d+) (.+)", line)
        if match:
            indent, count, frame = match.groups()
            while pending and pending[-1][0] >= len(indent):
                finish()
            pending.append([len(indent), int(count), frame, 0])
    while pending:
        finish()
    return {**totals, "self": dict(self_samples), "inclusive": dict(inclusive),
            "top_functions": dict(functions.most_common(20))}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--stdout", type=Path, required=True, help="stdout from --stats=json")
    parser.add_argument("--tier", type=Path, required=True, help="output of --report-tier")
    parser.add_argument("--islands", type=Path, required=True, help="output of --report-islands")
    parser.add_argument("--sample", type=Path, action="append", default=[])
    parser.add_argument("--nba-sites", type=Path, help="verified Scheduler::execute offsets in NBA branches")
    args = parser.parse_args()
    counters = json.loads(args.stdout.read_text().splitlines()[-1])
    rows = [json.loads(line) for line in args.tier.read_text().splitlines()]
    nba_sites = set(json.loads(args.nba_sites.read_text())) if args.nba_sites else set()
    print(json.dumps({
        "counters": counters,
        "tier": coverage(rows),
        "scc": scc_shape(args.islands.read_text()),
        "profiles": [sample_tree(path.read_text(), nba_sites) for path in args.sample],
    }, indent=2))


if __name__ == "__main__":
    main()
