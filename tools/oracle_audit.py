#!/usr/bin/env python3
# Copyright (c) 2026 The kirimasim Authors
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

"""Looks for tests of the P1 population whose expected output may come from
a bug of the simulator that recorded it.

  tools/oracle_audit.py --tests-repo DIR --out DIR [--list FILE]
      [--iverilog-bin DIR] [--verilator BIN] [--kirimasim BIN]
      [--seeds 1,7,...] [--jobs N]

The expected outputs of the test suite were recorded with Icarus Verilog
13.0, or with Verilator 5.052 for the tests that Icarus cannot compile
(`:oracle:`). This tool runs every test of the population list
(tests/gates/p1_population.txt by default) with both, Verilator once for
each seed, and with kirimasim, and writes DIR/oracle_audit.csv with one row
per test and DIR/outputs/<test>/ with the normalized outputs. Only the
programs are run; nothing of their sources is read (CLAUDE.md).

A test is a candidate for review when one of these holds:

  a  its expected output was recorded with Verilator, a two-state
     simulator, and x or z values take part (in the expected output, in the
     code, or :fourstate: yes);
  b  both Icarus and Verilator run it and print different outputs;
  c  Verilator, built with --x-assign unique --x-initial unique and run with
     +verilator+rand+reset+2, prints different outputs for different seeds;
  d  the code uses constructs whose result can depend on the order of
     events (#0, nonblocking assignments, fork, named events, $strobe or
     $monitor, a variable written by more than one procedure) and the
     expected output was recorded with a simulator.

The detection of x and z values and of the constructs works on the text,
with comments and strings removed, so it can be wrong both ways; the CSV
says what it found, for a person to check.
"""

import argparse
import concurrent.futures
import csv
import os
import re
import subprocess
import sys
import tempfile
from pathlib import Path

HERE = Path(__file__).resolve().parent
DEFAULT_LIST = HERE.parent / "tests" / "gates" / "p1_population.txt"

# Lines that a simulator prints itself rather than the design: the $finish
# line of Icarus and the banners of Verilator, which the runners of the test
# suite removed when they recorded the expected outputs, and the messages of
# the severity tasks, whose form the standard leaves to the tool (20.10),
# with the run-time warnings of Verilator, which the runners remove now.
ICARUS_FINISH = re.compile(r"^\S+:\d+: \$finish(?:\(\d\))? called at \d+ \(\d*\w*s\)$")
VERILATOR_BANNER = re.compile(r"^- (?:\S+:\d+: Verilog \$finish|S i m u l a t i o n|Verilator: )")
SEVERITY = re.compile(r"^(?:(?:INFO|WARNING|ERROR|FATAL): \S+:\d+: |\s+Time: \d+\s+Scope: "
                      r"|%Warning-|\[\d+\] (?:-Info|%Warning|%Error|%Fatal): "
                      r"|(?:-Info|%Error): \S+:\d+: Verilog \$stop|Aborting\.\.\.$)")

SCHEDULING = {
    "#0": r"#\s*0(?![\d.'])",
    "nonblocking": r"(?:^\s*|;\s*|\bbegin\s+|\belse\s+|\)\s*)"
                   r"(?:[A-Za-z_][\w.]*(?:\s*\[[^\]\n]*\])*|\{[^}\n]*\})\s*<=(?!=)",
    "fork": r"\bfork\b",
    "named event": r"\bevent\b|(?:^|[;)]|\bbegin\b|\belse\b)\s*->>?\s*[A-Za-z_]",
    "$strobe/$monitor": r"\$(?:strobe|monitor)\w*\b",
}
PROCEDURE = re.compile(r"\b(initial|always_ff|always_comb|always_latch|always|final)\b")
WRITE = re.compile(r"(?:^|[;\s)])([A-Za-z_]\w*)(?:\s*\[[^\]\n]*\])*\s*(?:<=|=)(?![=>])")
XZ_CODE = re.compile(r"\d*'[sS]?(?:[bB][01_xXzZ?]*[xXzZ?]|[hH][0-9a-fA-F_xXzZ?]*[xXzZ?]"
                     r"|[oO][0-7_xXzZ?]*[xXzZ?]|[dD]\s*[xXzZ?])|'[xXzZ]\b|\$isunknown\b")
# A value with x or z in the expected output: a run of digits, x and z that
# has an x or a z, is not a C-style hexadecimal number (0x1f) and is not a
# name followed by "=" or ":".
XZ_VALUE = re.compile(r"(?<![\w.])(?!0[xX][0-9a-fA-F_]+(?![\w]))"
                      r"[0-9a-fA-F_]*[xXzZ][0-9a-fA-FxXzZ_]*(?!\w|\s*[=:])")


def header(text: str) -> dict[str, str]:
    m = re.search(r"/\*(.*?)\*/", text, re.DOTALL)
    meta = {}
    for line in (m.group(1) if m else "").splitlines():
        fm = re.match(r"^:([A-Za-z_-]+):\s?(.*)$", line.strip())
        if fm:
            meta[fm.group(1)] = fm.group(2).strip()
    return meta


def code_of(text: str) -> str:
    text = re.sub(r"/\*.*?\*/", " ", text, flags=re.DOTALL)
    text = re.sub(r"//[^\n]*", "", text)
    return re.sub(r'"(?:\\.|[^"\\\n])*"', '""', text)


def expected_of(test: Path, meta: dict[str, str]) -> str | None:
    value = meta.get("expected_stdout")
    if value is None:
        return None
    if value.startswith("@"):
        path = test.parent / value[1:]
        return path.read_text(encoding="utf-8", errors="replace") if path.exists() else None
    return value + "\n"


def normalize(text: str, severity: bool = False) -> str:
    """Without empty lines and the lines the simulators print themselves,
    and, with severity, without the messages of the severity tasks."""
    out = []
    for line in text.splitlines():
        if not line.strip() or ICARUS_FINISH.match(line) or VERILATOR_BANNER.match(line):
            continue
        if severity and SEVERITY.match(line):
            continue
        out.append(line)
    return "".join(l + "\n" for l in out)


def compare(output: str, expected: str) -> str:
    if normalize(output) == normalize(expected):
        return "match"
    if normalize(output, True) == normalize(expected, True):
        return "match-without-severity"
    return "differ"


def without_for_headers(code: str) -> str:
    """The code with the parentheses of each for loop emptied, so that a
    condition such as i <= n is not taken for a nonblocking assignment."""
    out, pos = [], 0
    for m in re.finditer(r"\bfor\s*\(", code):
        if m.start() < pos:
            continue
        depth, i = 1, m.end()
        while i < len(code) and depth:
            depth += {"(": 1, ")": -1}.get(code[i], 0)
            i += 1
        out.append(code[pos:m.end()] + ")")
        pos = i
    return "".join(out) + code[pos:]


def scheduling_of(code: str) -> list[str]:
    code = without_for_headers(code)
    found = [name for name, pat in SCHEDULING.items() if re.search(pat, code, re.MULTILINE)]
    starts = [m.start() for m in PROCEDURE.finditer(code)]
    writers: dict[str, set[int]] = {}
    for i, start in enumerate(starts):
        end = starts[i + 1] if i + 1 < len(starts) else len(code)
        block = code[start:end].split("endmodule")[0]
        for name in set(WRITE.findall(block)):
            writers.setdefault(name, set()).add(i)
    shared = sorted(n for n, s in writers.items() if len(s) > 1 and n not in ("i", "j", "k"))
    if shared:
        found.append("written by several procedures: " + " ".join(shared[:4]))
    return found


def run(cmd: list[str], cwd: str, timeout: int = 60) -> tuple[int, str, str]:
    try:
        p = subprocess.run(cmd, cwd=cwd, capture_output=True, text=True, errors="replace",
                           timeout=timeout)
        return p.returncode, p.stdout, p.stderr
    except subprocess.TimeoutExpired:
        return -1, "", "timed out"


def icarus(test: Path, bindir: Path | None) -> tuple[str, str]:
    """The state (output, no-compile, timeout) and the output."""
    iverilog = str(bindir / "iverilog") if bindir else "iverilog"
    vvp = str(bindir / "vvp") if bindir else "vvp"
    with tempfile.TemporaryDirectory() as tmp:
        rc, _, err = run([iverilog, "-g2012", "-o", "a.vvp", str(test)], tmp)
        if rc != 0:
            return "no-compile", err.strip().splitlines()[0] if err.strip() else ""
        rc, out, err = run([vvp, "-n", "a.vvp"], tmp)
        if rc == -1:
            return "timeout", ""
        # vvp refuses a program that uses a function it does not have.
        if "Program not runnable" in out + err:
            return "no-compile", "Program not runnable"
        return "output", out


def verilator(test: Path, binary: str, seed: int) -> tuple[str, str]:
    with tempfile.TemporaryDirectory() as tmp:
        rc, _, err = run([binary, "--binary", "--timing", "-Wno-fatal", "-Wno-lint", "-Wno-style",
                          "--x-assign", "unique", "--x-initial", "unique", "-Mdir", "obj",
                          "--prefix", "Vt", str(test)], tmp, timeout=300)
        if rc != 0:
            first = next((l for l in err.splitlines() if l.startswith("%Error")), "")
            return "no-build", first
        rc, out, err = run(["obj/Vt", "+verilator+rand+reset+2", f"+verilator+seed+{seed}",
                            "+verilator+error+limit+1000000"], tmp)
        if rc == -1:
            return "timeout", ""
        return "output", out


def kirimasim(test: Path, binary: str) -> tuple[str, str]:
    """kirimasim's verdict by tools/run_suite_test.py, and its output."""
    rc, _, _ = run([sys.executable, str(HERE / "run_suite_test.py"), "--kirimasim", binary,
                    str(test)], str(HERE), timeout=120)
    verdict = {0: "pass", 77: "unsupported"}.get(rc, "fail")
    with tempfile.TemporaryDirectory() as tmp:
        _, out, _ = run([binary, str(test)], tmp)
    return verdict, out


def audit(name: str, args) -> dict:
    test = args.tests_repo / "tests" / f"{name}.sv"
    text = test.read_text(encoding="utf-8", errors="replace")
    meta = header(text)
    code = code_of(text)
    expected = expected_of(test, meta) or ""
    oracle = meta.get("oracle", "").split()
    row = {"test": name, "oracle": " ".join(oracle), "spec_check": meta.get("spec_check", ""),
           "fourstate": meta.get("fourstate", ""),
           "expected_xz": "yes" if XZ_VALUE.search(expected) else "no",
           "code_xz": "yes" if XZ_CODE.search(code) else "no",
           "scheduling": "; ".join(scheduling_of(code))}
    out_dir = args.out / "outputs" / name
    out_dir.mkdir(parents=True, exist_ok=True)
    (out_dir / "expected.txt").write_text(normalize(expected))

    i_state, i_out = icarus(test, args.iverilog_bin)
    (out_dir / "icarus.txt").write_text(normalize(i_out))
    row["icarus"] = i_state if i_state != "output" else compare(i_out, expected)

    v_outs = []
    for seed in args.seeds:
        v_state, v_out = verilator(test, args.verilator, seed)
        (out_dir / f"verilator-{seed}.txt").write_text(normalize(v_out))
        v_outs.append((v_state, v_out))
        if v_state != "output":
            break
    v_state, v_out = v_outs[0]
    row["verilator"] = v_state if v_state != "output" else compare(v_out, expected)
    if v_state == "output":
        same = all(normalize(o) == normalize(v_out) for _, o in v_outs)
        row["verilator_seeds"] = "same" if same else "vary"
    else:
        row["verilator_seeds"] = ""
    if i_state == "output" and v_state == "output":
        same = normalize(i_out, True) == normalize(v_out, True)
        row["icarus_vs_verilator"] = "same" if same else "differ"
    else:
        row["icarus_vs_verilator"] = ""

    if args.kirimasim:
        k_verdict, k_out = kirimasim(test, args.kirimasim)
        (out_dir / "kirimasim.txt").write_text(normalize(k_out))
        row["kirimasim"] = k_verdict
    else:
        row["kirimasim"] = ""

    reasons = []
    xz = row["expected_xz"] == "yes" or row["code_xz"] == "yes" or row["fourstate"] == "yes"
    if "verilator@5.052" in oracle and "iverilog@13.0" not in oracle and xz:
        reasons.append("a")
    if row["icarus_vs_verilator"] == "differ":
        reasons.append("b")
    if row["verilator_seeds"] == "vary":
        reasons.append("c")
    if row["scheduling"] and any(o in oracle for o in ("iverilog@13.0", "verilator@5.052")):
        reasons.append("d")
    row["candidate"] = "".join(reasons)
    return row


FIELDS = ["test", "oracle", "spec_check", "fourstate", "expected_xz", "code_xz", "scheduling",
          "icarus", "verilator", "verilator_seeds", "icarus_vs_verilator", "kirimasim", "candidate"]


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("--tests-repo", required=True, type=Path)
    ap.add_argument("--out", required=True, type=Path)
    ap.add_argument("--list", type=Path, default=DEFAULT_LIST)
    ap.add_argument("--iverilog-bin", type=Path, default=None,
                    help="directory of iverilog and vvp of Icarus Verilog 13.0")
    ap.add_argument("--verilator", default="verilator", help="verilator of version 5.052")
    ap.add_argument("--kirimasim", default=None)
    ap.add_argument("--seeds", default="1,7,42,2026")
    ap.add_argument("-j", "--jobs", type=int, default=os.cpu_count() or 4)
    args = ap.parse_args()
    args.tests_repo = args.tests_repo.resolve()
    args.out = args.out.resolve()
    args.seeds = [int(s) for s in args.seeds.split(",")]
    if args.kirimasim:
        args.kirimasim = str(Path(args.kirimasim).resolve())
    names = [l for l in args.list.read_text(encoding="utf-8").splitlines()
             if l and not l.startswith("#")]
    with concurrent.futures.ThreadPoolExecutor(max_workers=args.jobs) as pool:
        rows = list(pool.map(lambda n: audit(n, args), names))
    with (args.out / "oracle_audit.csv").open("w", newline="", encoding="utf-8") as f:
        w = csv.DictWriter(f, fieldnames=FIELDS)
        w.writeheader()
        w.writerows(rows)
    counts = {c: sum(c in r["candidate"] for r in rows) for c in "abcd"}
    print(f"{len(rows)} tests; candidates: " + ", ".join(f"{c} {n}" for c, n in counts.items())
          + f"; any {sum(bool(r['candidate']) for r in rows)}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
