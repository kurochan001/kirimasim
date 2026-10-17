#!/usr/bin/env python3
"""Checks kirimasim's command line: options, exit codes and messages (CLI.md).

Each case runs kirimasim with some arguments on a small design and checks
the exit code, and text that stdout or stderr must contain.
"""

import argparse
import json
import subprocess
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
DESIGN = str(HERE / "display_finish.sv")
DELAYS = str(HERE / "delays.sv")
FINAL = str(HERE / "final.sv")
SPIN = str(HERE / "spin.sv")
ZERO_DELAY = str(HERE / "zero_delay.sv")
VIOLATIONS = str(HERE / "violations.sv")
WRITERS = str(HERE / "writers.sv")
STRING_REPL = str(HERE / "string_repl.sv")
PLUSARGS = str(HERE / "plusargs.sv")
VALUE_PLUSARGS = str(HERE / "value_plusargs.sv")
PROGRAM_MATRIX = HERE / "program_matrix.sv"
UNSUPPORTED_MATRIX = HERE / "unsupported_matrix"
LIFETIME = HERE.parent / "suite" / "sched" / "process_lifetime.sv"

# The violation reports of violations.sv, exactly these and in this order
# (SYSTEM_TASKS.md 10.4): the glitch at 1ns is flushed, unique0 with no
# match and the if with an else report nothing.
VIOLATION_LINES = [
    "violations.sv:9: warning: unique case violation: no item matches [top @ 2ns]",
    "violations.sv:22: warning: unique if violation: more than one condition is true [top @ 5ns]",
    "violations.sv:24: warning: priority if violation: no condition is true [top @ 5ns]",
    "violations.sv:28: warning: priority case violation: no item matches [top @ 5ns]",
    "violations.sv:32: warning: unique case violation: more than one item matches [top @ 5ns]",
]

# (arguments, exit code, text stdout must contain, text stderr must contain).
# Expected stdout starting with "=" must match the whole output.
CASES = [
    # Plusargs go to $test$plusargs in order, without their + (CLI.md 2.4).
    ([PLUSARGS, "+mode=fast", "+verbose"], 0, "=1 1 0 1\n", ""),
    ([PLUSARGS], 0, "=0 0 0 0\n", ""),
    ([VALUE_PLUSARGS, "+mode=fast", "+count=42", "+mask=f0g"], 0, "=1 fast\n1 42\n1 xx\n", ""),
    ([VALUE_PLUSARGS, "+count=", "+mask=3z"], 0, "=0 none\n1 0\n1 3z\n", ""),
    ([VALUE_PLUSARGS], 0, "=0 none\n0 5\n0 11\n", ""),
    ([DESIGN], 0, "hello\n", "kirimasim: info: finish at 0 ns"),
    (["--max-steps=2", DESIGN], 5, "hello\n", "kirimasim: info: zero_time_limit"),
    (["--max-steps=x", DESIGN], 2, "", "--max-steps=x needs a whole number"),
    (["--max-zero-delay=", DESIGN], 2, "", "needs a whole number"),
    (["--max-zero-delay=100", DESIGN], 0, "hello\n", ""),
    (["--max-scc-iter=3", DESIGN], 0, "hello\n", ""),
    (["--max-time-ok", DESIGN], 0, "hello\n", ""),
    (["--timescale=1ns/1ps", DESIGN], 0, "hello\n", ""),
    (["--no-such-option", DESIGN], 2, "", "unknown option '--no-such-option'"),
    (["--max-time=10us", DESIGN], 0, "hello\n", ""),
    (["--max-time=0.5ns", DESIGN], 0, "hello\n", ""),
    (["--max-time=10", DESIGN], 2, "", "--max-time=10 needs a time such as 10us"),
    (["--max-time=1 ns", DESIGN], 2, "", "needs a time"),
    (["--max-time=100000000000s", DESIGN], 2, "", "too large for the time precision 1ns"),
    (["--timescale=1ns/1ps", "--max-time=1.5ns", DESIGN], 0, "hello\n", ""),
    # Time advances in picoseconds; events at the limit itself still run.
    ([DELAYS], 0, "a\nb\n", "kirimasim: info: finish at 3500 ps"),
    (["--max-time=2ns", DELAYS], 4, "a\n", "kirimasim: info: time_limit at 2 ns"),
    (["--max-time=1500ps", DELAYS], 4, "a\n", "kirimasim: info: time_limit at 1500 ps"),
    (["--max-time=1499ps", DELAYS], 4, "", "kirimasim: info: time_limit at 1499 ps"),
    (["--max-time=2ns", "--max-time-ok", DELAYS], 0, "a\n", "time_limit at 2 ns"),
    # The zero-time limits stop the run with code 5 and say which one; final
    # procedures still run.
    (["--max-steps=1000", SPIN], 5, "=final ran\n",
     'kirimasim: error: time did not advance: process "top.initial0" ran 1000 instructions'
     " without waiting (--max-steps=1000)\nkirimasim: info: zero_time_limit at 0 ns"),
    (["--max-zero-delay=10", ZERO_DELAY], 5, "=final ran\n",
     "kirimasim: error: time did not advance: 10 moves into the Active region in one time"
     " slot (--max-zero-delay=10)\nkirimasim: info: zero_time_limit at 0 ns"),
    ([ZERO_DELAY], 5, "=final ran\n", "(--max-zero-delay=1000000)"),
    # Another writer of an always_comb variable is an error of the design
    # (SCHEDULER_SPEC.md 6.4), not an internal error; slang's analysis
    # reports it.
    ([WRITERS], 2, "", "writers.sv:6:15: error: variable 'a[1:0]' driven by always_comb procedure"
     " cannot be written to by any other process"),
    ([STRING_REPL], 0, "=[]\n[]\n",
     "string_repl.sv:9:9: warning: string replication count is negative, x or z; the result is"
     " the empty string\nkirimasim: info: quiescent"),
    ([FINAL], 0, "=a\nfinal 1 n=5\nfinal 2\n", "kirimasim: info: quiescent at 1 ns"),
    (["--max-time=0ns", FINAL], 4, "=final 1 n=0\nfinal 2\n", "time_limit at 0 ns"),
]


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("--kirimasim", required=True, help="path to the kirimasim executable")
    args = ap.parse_args()

    failures = 0
    for argv, code, out, err in CASES:
        proc = subprocess.run([args.kirimasim, *argv], capture_output=True, text=True,
                              check=False)
        problems = []
        if proc.returncode != code:
            problems.append(f"exit code {proc.returncode}, expected {code}")
        if out.startswith("="):
            if proc.stdout != out[1:]:
                problems.append(f"stdout is {proc.stdout!r}, expected {out[1:]!r}")
        elif out not in proc.stdout:
            problems.append(f"stdout lacks {out!r}")
        if err not in proc.stderr:
            problems.append(f"stderr lacks {err!r}")
        if problems:
            failures += 1
            print(f"FAIL: kirimasim {' '.join(argv)}: {'; '.join(problems)}", file=sys.stderr)
            print(f"--- stdout ---\n{proc.stdout}--- stderr ---\n{proc.stderr}", file=sys.stderr)

    # --stats=json puts one JSON object on the last line of stdout, with the
    # end reason as text (PERFORMANCE.md 3.1).
    proc = subprocess.run([args.kirimasim, "--stats=json", DESIGN], capture_output=True,
                          text=True, check=False)
    try:
        stats = json.loads(proc.stdout.splitlines()[-1])
        if stats.get("sim.end_reason") != "finish" or stats.get("proc.wakeups") != 1:
            raise ValueError(stats)
    except (IndexError, ValueError) as e:
        failures += 1
        print(f"FAIL: --stats=json: {e}\n{proc.stdout}", file=sys.stderr)

    proc = subprocess.run([args.kirimasim, VIOLATIONS], capture_output=True, text=True,
                          check=False)
    got = [line.split("/")[-1] for line in proc.stderr.splitlines() if " violation: " in line]
    if proc.returncode != 0 or proc.stdout != "h=8\n" or got != VIOLATION_LINES:
        failures += 1
        print(f"FAIL: violation reports\n{proc.stdout}{proc.stderr}", file=sys.stderr)

    # Every program instance and anonymous program is reported as
    # unsupported, wherever it is, and nothing runs (CLI.md 5; the lines come
    # from tools/gen_program_matrix.py).
    proc = subprocess.run([args.kirimasim, PROGRAM_MATRIX.name], cwd=HERE, capture_output=True,
                          text=True, check=False)
    want = PROGRAM_MATRIX.with_suffix(".stderr").read_text(encoding="utf-8").splitlines()
    got = [line for line in proc.stderr.splitlines() if ": error: " in line]
    if proc.returncode != 2 or proc.stdout != "" or sorted(got) != sorted(want):
        failures += 1
        print(f"FAIL: program matrix: exit code {proc.returncode}\n{proc.stdout}{proc.stderr}",
              file=sys.stderr)

    # Every unsupported construct is reported with its position, in every
    # container, and nothing runs (CLI.md 5; the files and the lines come
    # from tools/gen_unsupported_matrix.py).
    got = []
    for sv in sorted(UNSUPPORTED_MATRIX.glob("*.sv")):
        rel = str(sv.relative_to(HERE))
        proc = subprocess.run([args.kirimasim, rel], cwd=HERE, capture_output=True, text=True,
                              check=False)
        got += [line for line in proc.stderr.splitlines() if ": error: " in line]
        if proc.returncode != 2 or proc.stdout != "":
            failures += 1
            print(f"FAIL: {rel}: exit code {proc.returncode}\n{proc.stdout}{proc.stderr}",
                  file=sys.stderr)
    want = UNSUPPORTED_MATRIX.with_suffix(".stderr").read_text(encoding="utf-8").splitlines()
    if sorted(got) != sorted(want):
        failures += 1
        missing = sorted(set(want) - set(got))
        extra = sorted(set(got) - set(want))
        print("FAIL: unsupported matrix:\n" + "".join(f"  missing: {x}\n" for x in missing) +
              "".join(f"  extra: {x}\n" for x in extra), file=sys.stderr)

    # The records of finished processes are released (SCHEDULER_SPEC.md
    # 10.8): with twice the rounds of the process lifetime matrix, twice as
    # many processes are made, but no more records are kept at once.
    runs = []
    for n in (4, 8):
        proc = subprocess.run([args.kirimasim, "--stats=json", str(LIFETIME), f"+n={n}"],
                              capture_output=True, text=True, check=False)
        try:
            stats = json.loads(proc.stdout.splitlines()[-1])
            runs.append((stats["proc.created"], stats["proc.kept_peak"]))
        except (IndexError, KeyError, ValueError) as e:
            runs.append((0, 0))
            print(f"FAIL: process lifetime +n={n}: {e}\n{proc.stderr}", file=sys.stderr)
    if proc.returncode != 0 or runs[1][0] <= runs[0][0] or runs[1][1] != runs[0][1]:
        failures += 1
        print(f"FAIL: process records kept: (created, kept_peak) {runs[0]} with +n=4, "
              f"{runs[1]} with +n=8", file=sys.stderr)

    return 1 if failures else 0


if __name__ == "__main__":
    sys.exit(main())
