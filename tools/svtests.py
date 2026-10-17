#!/usr/bin/env python3
"""Runs the sv-tests simulation tests with kirimasim, Icarus and Verilator and compares.

This is the first P1 gate of TEST_SUITE.md ("sv-tests で Icarus 以上"):

* The population is the hand-written tests under tests/ of the pinned
  sv-tests commit whose :type: includes "simulation". Generated tests are
  not included.
* A test passes by the rules of sv-tests' tools/runner: an exit code of 126
  or more is a crash and fails; with :should_fail_because: the tool must
  fail (nonzero exit), otherwise it must succeed (0); a simulation test
  must also print no :assert: line whose expression is false. The time
  limit is :timeout: seconds (30 by default).
* For kirimasim only, a test that should fail does not pass when kirimasim
  fails because of an unsupported construct.
* Tests tagged with a library of conf/runners/libs.json (uvm) get its files
  from third_party, as sv-tests does.

Icarus is run like sv-tests' Icarus runner: iverilog -g2012, then the
compiled program. Use the build of tools/build_icarus.sh. Verilator, which
is optional and only for comparison, is run like sv-tests' Verilator
runner: verilator --timing --binary with that runner's warning and build
options, then the built program.

Writes results.json (every test, every tool, with the seconds each run
took) and summary.md (totals and counts per chapter, and each tool's
seconds summed over the tests) to --out, and prints the summary. With --gate, the exit
code is 1 when kirimasim passes fewer tests than Icarus.
"""

import argparse
import concurrent.futures
import json
import os
import re
import subprocess
import sys
import tempfile
import time
from pathlib import Path

PINNED = "15490e95fd978ada1c1dc59b7c4b53a7edb23b78"
CRASH = 126
TIMEOUT_RC = 71  # sv-tests' code for a timeout


def read_params(path: Path) -> dict[str, str]:
    """The test's metadata, read like sv-tests' tools/runner (last value wins)."""
    params: dict[str, str] = {}
    with open(path, encoding="utf-8", errors="ignore") as f:
        for line in f:
            m = re.search(r"^:([a-zA-Z_-]+):\s*(.+)", line)
            if m:
                params[m.group(1).lower()] = m.group(2)
    params.setdefault("timeout", "30")
    params.setdefault("type", "parsing elaboration")
    params.setdefault("top_module", "")
    params.setdefault("defines", "")
    params.setdefault("tags", "")
    return params


def assert_ok(log: str) -> bool:
    """sv-tests' tools/logparser.py: every :assert: expression is true."""
    for line in log.split("\n"):
        m = re.search(r":([a-z]+):(.*)", line.strip())
        if m and m.group(1) == "assert":
            try:
                if not eval(m.group(2)):  # noqa: S307 (the rule of sv-tests)
                    return False
            except Exception:  # noqa: BLE001
                return False
    return True


def run(cmd: list[str], cwd: str, timeout: int) -> tuple[int, str]:
    try:
        p = subprocess.run(cmd, cwd=cwd, stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                           timeout=timeout, check=False)
        return p.returncode, p.stdout.decode("utf-8", "ignore")
    except subprocess.TimeoutExpired:
        return TIMEOUT_RC, f"Timeout: > {timeout}s\n"


def run_kirimasim(exe: str, files, incdirs, defines, top, cwd, timeout):
    cmd = [exe] + [f"+incdir+{d}" for d in incdirs] + [f"+define+{d}" for d in defines]
    if top:
        cmd.append(f"--top={top}")
    return run(cmd + files, cwd, timeout)


def run_icarus(exe: str, files, incdirs, defines, top, cwd, timeout):
    cmd = [exe, "-g2012", "-o", "iverilog.out"]
    if top:
        cmd += ["-s", top]
    cmd += [f"-I{d}" for d in incdirs] + [f"-D{d}" for d in defines] + files
    rc, log = run(cmd, cwd, timeout)
    if rc != 0:
        return rc, log
    rc2, log2 = run(["./iverilog.out"], cwd, timeout)
    return rc2, log + log2


def run_verilator(exe: str, files, incdirs, defines, top, cwd, timeout, extra=()):
    name = f"V{top or 'top'}"
    cmd = [exe, "--timing", "--binary", "--build-jobs", "0", "-CFLAGS", "-O0", "-Wno-fatal",
           "-Wno-UNOPTFLAT", "-Wno-BLKANDNBLK", "-Wpedantic", "-Wno-context"]
    if top:
        cmd += ["--top-module", top]
    cmd += [f"-I{d}" for d in incdirs] + ["-DUVM_NO_DPI"]
    cmd += ["--Mdir", "vbuild", "--prefix", name, "-o", name] + list(extra)
    cmd += [f"-D{d}" for d in defines] + files
    rc, log = run(cmd, cwd, timeout)
    if rc != 0:
        return rc, log
    rc2, log2 = run([f"./vbuild/{name}"], cwd, timeout)
    return rc2, log + log2


def judge(rc: int, log: str, should_fail: bool, kirimasim: bool) -> bool:
    if rc >= CRASH:
        return False
    if should_fail != (rc != 0):
        return False
    if should_fail and kirimasim and "error: unsupported:" in log:
        return False
    return assert_ok(log)


def chapter(tags: str) -> str:
    first = tags.split()[0] if tags.split() else "?"
    return first.split(".")[0]


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("--sv-tests", required=True, type=Path, help="sv-tests checkout")
    ap.add_argument("--kirimasim", help="kirimasim executable")
    ap.add_argument("--iverilog", help="iverilog of tools/build_icarus.sh")
    ap.add_argument("--verilator", help="verilator, for comparison only")
    ap.add_argument("--out", type=Path, default=Path("build/svtests"))
    ap.add_argument("--jobs", type=int, default=os.cpu_count() or 4)
    ap.add_argument("--filter", default="", help="only tests whose path contains this")
    ap.add_argument("--gate", action="store_true", help="exit 1 if kirimasim < Icarus")
    args = ap.parse_args()

    root = args.sv_tests.resolve()
    head = subprocess.run(["git", "-C", str(root), "rev-parse", "HEAD"], capture_output=True,
                          text=True, check=False).stdout.strip()
    if head != PINNED:
        print(f"warning: sv-tests is at {head or '?'}, not the pinned {PINNED}", file=sys.stderr)
    libs = json.loads((root / "conf/runners/libs.json").read_text(encoding="utf-8"))

    tests = []
    for path in sorted((root / "tests").rglob("*.sv")):
        params = read_params(path)
        if "name" not in params or "simulation" not in params["type"].split():
            continue
        if args.filter and args.filter not in str(path):
            continue
        tests.append((path, params))

    tools = {}
    if args.kirimasim:
        tools["kirimasim"] = (run_kirimasim, str(Path(args.kirimasim).resolve()))
    if args.iverilog:
        tools["icarus"] = (run_icarus, str(Path(args.iverilog).resolve()))
    if args.verilator:
        tools["verilator"] = (run_verilator, args.verilator)

    def one(item):
        path, params = item
        files = [str(path)]
        incdirs = [str(path.parent)]
        for key, lib in libs.items():
            if key in params["tags"].split():
                files = [str(root / "third_party" / p) for p in lib["files"]] + files
                incdirs = [str(root / "third_party" / p) for p in lib["incdirs"]] + incdirs
        defines = params["defines"].split()
        should_fail = "should_fail_because" in params
        result = {"test": str(path.relative_to(root)), "chapter": chapter(params["tags"]),
                  "should_fail": should_fail}
        for name, (fn, exe) in tools.items():
            extra = {}
            if name == "verilator" and "runner_verilator_flags" in params:
                extra["extra"] = params["runner_verilator_flags"].split()
            with tempfile.TemporaryDirectory() as tmp:
                start = time.monotonic()
                rc, log = fn(exe, files, incdirs, defines, params["top_module"], tmp,
                             int(params["timeout"]), **extra)
                seconds = time.monotonic() - start
            result[name] = {"rc": rc, "pass": judge(rc, log, should_fail, name == "kirimasim"),
                            "unsupported": "error: unsupported:" in log,
                            "seconds": round(seconds, 3)}
        return result

    with concurrent.futures.ThreadPoolExecutor(max_workers=args.jobs) as pool:
        results = list(pool.map(one, tests))

    args.out.mkdir(parents=True, exist_ok=True)
    (args.out / "results.json").write_text(json.dumps(results, indent=1) + "\n",
                                           encoding="utf-8")
    names = list(tools)
    chapters = sorted({r["chapter"] for r in results},
                      key=lambda c: (not c.isdigit(), int(c) if c.isdigit() else 0, c))
    lines = [f"sv-tests {head[:12]}: {len(results)} simulation tests", "",
             "| chapter | tests | " + " | ".join(names) + " |",
             "| --- | --- | " + " | ".join("---" for _ in names) + " |"]
    for c in chapters + ["total"]:
        rows = [r for r in results if c == "total" or r["chapter"] == c]
        counts = [str(sum(1 for r in rows if r[n]["pass"])) for n in names]
        label = "**total**" if c == "total" else c
        lines.append(f"| {label} | {len(rows)} | " + " | ".join(counts) + " |")
    times = [f"{sum(r[n]['seconds'] for r in results):.1f}" for n in names]
    lines.append("| seconds, summed | | " + " | ".join(times) + " |")
    if "kirimasim" in tools:
        unsupported = sum(1 for r in results if r["kirimasim"]["unsupported"])
        lines += ["", f"kirimasim stopped at an unsupported construct in {unsupported} tests."]
    summary = "\n".join(lines) + "\n"
    (args.out / "summary.md").write_text(summary, encoding="utf-8")
    print(summary, end="")

    if args.gate and "kirimasim" in tools and "icarus" in tools:
        h = sum(1 for r in results if r["kirimasim"]["pass"])
        i = sum(1 for r in results if r["icarus"]["pass"])
        return 0 if h >= i else 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
