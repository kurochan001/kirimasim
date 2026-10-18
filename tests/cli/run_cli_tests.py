#!/usr/bin/env python3
"""Checks kirimasim's command line: options, exit codes and messages (CLI.md).

Each case runs kirimasim with some arguments on a small design and checks
the exit code, and text that stdout or stderr must contain.
"""

import argparse
import json
import os
import queue
import signal
import subprocess
import sys
import threading
import time
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
ZERO_TIME_LOOPS = HERE / "zero_time_loop_matrix"
INTERRUPTS = HERE / "interrupt_matrix.expected"
FLAG_MATRIX = HERE / "format_flag_matrix.sv"
FLAG_UNSUPPORTED = HERE / "format_flag_unsupported.sv"
BUILTIN_CALLS = HERE / "builtin_call_matrix.sv"
LIFETIME = HERE.parent / "suite" / "sched" / "process_lifetime.sv"
END_TASKS = str(HERE / "end_tasks.sv")

# Cases run in this directory, as -f reads the paths in its file from
# the current directory (CLI.md 2.1): (arguments, exit code, text stdout
# must contain, text stderr must contain), stdout starting with "=" to be
# matched whole.
CWD_CASES = [
    (["-h"], 0, "usage: kirimasim [options]", ""),
    (["--help"], 0, "usage: kirimasim [options]", ""),
    ([], 2, "", "kirimasim: error: no input files\nusage: kirimasim"),
    (["parse"], 2, "", "kirimasim: error: parse needs at least one file"),
    (["-f", "files.f"], 0, "hello\n", "kirimasim: info: finish at 0 ns"),
    (["parse", "-f", "files.f"], 0, "=", ""),
    (["-f"], 2, "", "kirimasim: error: -f needs a value"),
    (["--top"], 2, "", "kirimasim: error: --top needs a value"),
    # Without --top, every module that nothing instantiates is a top.
    (["two_tops.sv"], 0, "=first\nsecond\n", ""),
    (["--top=second_top", "two_tops.sv"], 0, "=second\n", ""),
    (["--top", "second_top", "two_tops.sv"], 0, "=second\n", ""),
    (["--top=first_top", "--top=second_top", "two_tops.sv"], 0, "=first\nsecond\n", ""),
    (["--top=no_such_top", "two_tops.sv"], 2, "=", "'no_such_top' is not a valid top-level"),
    (["+incdir+include", "macros.sv"], 0, "=no greeting 7\n", ""),
    (["+incdir+include", '+define+GREETING="hi"', "macros.sv"], 0, "=hi 7\n", ""),
    (["parse", "+incdir+include", "macros.sv"], 0, "=", ""),
    (["macros.sv"], 2, "=", "macros.sv:4:10: error: 'macros_inc.svh'"),
]

# (the plusarg that picks the call in end_tasks.sv, the reason for ending,
# the argument, the line of the call). 1, the default, reports the call
# with its place and time and writes the line of the reason for ending; 2
# adds the statistics at the end; 0 prints nothing (SYSTEM_TASKS.md 10.2,
# SCHEDULER_SPEC.md 16.2).
END_CASES = [
    ("finish", "finish", 1, 12),
    ("finish0", "finish", 0, 9),
    ("finish1", "finish", 1, 10),
    ("finish2", "finish", 2, 11),
    ("stop", "stop", 1, 16),
    ("stop0", "stop", 0, 13),
    ("stop1", "stop", 1, 14),
    ("stop2", "stop", 2, 15),
]

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
    (["--max-slot-events=", DESIGN], 2, "", "needs a whole number"),
    (["--max-slot-events=100", DESIGN], 0, "hello\n", ""),
    (["--max-slot-events=0", DESIGN], 0, "hello\n", ""),
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
    # Each #0 runs one event and moves one, so the smaller limit stops it.
    (["--max-slot-events=10", ZERO_DELAY], 5, "=final ran\n",
     "kirimasim: error: time did not advance: 10 events ran in one time slot"
     " (--max-slot-events=10)\nkirimasim: info: zero_time_limit at 0 ns"),
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


def run_interrupt(kirimasim: str, case: dict) -> list[str]:
    """Runs one case of interrupt_matrix.expected: waits for each $info
    line on stderr and sends SIGINT after it. Returns the problems."""
    proc = subprocess.Popen([kirimasim, *case["args"], case["file"]], cwd=HERE,
                            stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
    out: list[str] = []
    err: list[str] = []
    lines: queue.Queue = queue.Queue()

    def read_err():
        for line in proc.stderr:
            err.append(line)
            lines.put(line)
        lines.put(None)

    readers = [threading.Thread(target=lambda: out.extend(proc.stdout), daemon=True),
               threading.Thread(target=read_err, daemon=True)]
    for r in readers:
        r.start()
    problems = []
    for marker in case["markers"]:
        deadline = time.monotonic() + 30
        while not problems:
            try:
                line = lines.get(timeout=max(0.0, deadline - time.monotonic()))
            except queue.Empty:
                line = None
            if line is None:
                problems.append(f"no line with info: {marker}")
            elif f": info: {marker} [" in line:
                break
        if problems:
            break
        proc.send_signal(signal.SIGINT)
    try:
        proc.wait(timeout=20)
    except subprocess.TimeoutExpired:
        proc.kill()
        proc.wait()
        problems.append("did not end")
    for r in readers:
        r.join(timeout=10)
    if proc.returncode != 130:
        problems.append(f"exit code {proc.returncode}, expected 130")
    if "".join(out) != case["stdout"]:
        problems.append(f"stdout is {''.join(out)!r}, expected {case['stdout']!r}")
    ends = [line for line in err if line.startswith("kirimasim: info: interrupted at ")]
    if case["end"] is None:
        if ends:
            problems.append(f"an end line after the second SIGINT: {ends[0]!r}")
    elif not err or not err[-1].startswith("kirimasim: info: interrupted at ") or (
            case["end"] and err[-1] != f"kirimasim: info: interrupted at {case['end']}\n"):
        problems.append(f"the last line of stderr is {err[-1:]!r}, expected the end line at "
                        f"{case['end'] or 'some time'}")
    if problems:
        problems.append("stderr:\n" + "".join(err))
    return problems


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("--kirimasim", required=True, help="path to the kirimasim executable")
    ap.add_argument("--slot-event-default", action="store_true",
                    help="only run a loop to the default of --max-slot-events, which takes "
                         "seconds (CLI.md 2.6)")
    args = ap.parse_args()

    if args.slot_event_default:
        # The default stops the loop of processes that wake each other
        # through @(a) and @(b) (tools/gen_zero_time_loop_matrix.py).
        rel = str((ZERO_TIME_LOOPS / "subscription.sv").relative_to(HERE))
        proc = subprocess.run([args.kirimasim, rel], cwd=HERE, capture_output=True, text=True,
                              check=False)
        want = ("kirimasim: error: time did not advance: 100000000 events ran in one time slot"
                " (--max-slot-events=100000000)\nkirimasim: info: zero_time_limit at 1 ns\n")
        if (proc.returncode != 5 or proc.stdout != "started\nfinal ran\n" or
                not proc.stderr.endswith(want)):
            print(f"FAIL: {rel}: exit code {proc.returncode}\n{proc.stdout}{proc.stderr}",
                  file=sys.stderr)
            return 1
        return 0

    failures = 0
    cases = [(c, None) for c in CASES] + [(c, HERE) for c in CWD_CASES]
    for (argv, code, out, err), cwd in cases:
        proc = subprocess.run([args.kirimasim, *argv], cwd=cwd, capture_output=True, text=True,
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

    # $finish and $stop end the run at 5ns with exit code 0; final runs and
    # nothing scheduled later does (SCHEDULER_SPEC.md 16.2, 9.2.3).
    for plusarg, reason, level, line_no in END_CASES:
        proc = subprocess.run([args.kirimasim, "--stats=json", END_TASKS, f"+{plusarg}"],
                              capture_output=True, text=True, check=False)
        lines = proc.stdout.splitlines()
        problems = []
        if proc.returncode != 0:
            problems.append(f"exit code {proc.returncode}")
        if lines[:-1] != ["final 5"]:
            problems.append(f"stdout {lines[:-1]!r}")
        try:
            stats = json.loads(lines[-1])
            got = (stats.get("sim.end_reason"), stats.get("sim.end_time"))
            if got != (reason, 5):
                problems.append(f"--stats=json gives {got}")
        except (IndexError, ValueError) as e:
            problems.append(f"--stats=json: {e}")
        report = f"end_tasks.sv:{line_no}: ${reason} at 5 ns"
        reason_line = f"kirimasim: info: {reason} at 5 ns"
        for text, want in ((report, level >= 1), (reason_line, level >= 1)):
            if (text in proc.stderr) != want:
                problems.append(f"stderr {'lacks' if want else 'has'} {text!r}")
        stats = [x for x in proc.stderr.splitlines()
                 if x.startswith("kirimasim: info: statistics ")]
        if len(stats) != (1 if level == 2 else 0):
            problems.append(f"{len(stats)} lines of statistics")
        elif stats:
            try:
                got = json.loads(stats[0][len("kirimasim: info: statistics "):])
                if got != json.loads(lines[-1]):
                    problems.append("the statistics differ from --stats=json")
            except (IndexError, ValueError) as e:
                problems.append(f"statistics: {e}")
        if problems:
            failures += 1
            print(f"FAIL: +{plusarg}: {'; '.join(problems)}\n{proc.stdout}{proc.stderr}",
                  file=sys.stderr)

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

    # Processes that wake each other without time advancing, in each way
    # they can (the files come from tools/gen_zero_time_loop_matrix.py):
    # --max-slot-events stops each loop at time 1 and says so on the line
    # before the reason, and final procedures still run (CLI.md 2.6).
    loops = sorted(ZERO_TIME_LOOPS.glob("*.sv"))
    if not loops:
        failures += 1
        print(f"FAIL: no files in {ZERO_TIME_LOOPS}", file=sys.stderr)
    want = ("kirimasim: error: time did not advance: 1000 events ran in one time slot"
            " (--max-slot-events=1000)\nkirimasim: info: zero_time_limit at 1 ns\n")
    for sv in loops:
        rel = str(sv.relative_to(HERE))
        proc = subprocess.run([args.kirimasim, "--max-slot-events=1000", rel], cwd=HERE,
                              capture_output=True, text=True, check=False)
        if (proc.returncode != 5 or proc.stdout != "started\nfinal ran\n" or
                not proc.stderr.endswith(want)):
            failures += 1
            print(f"FAIL: {rel}: exit code {proc.returncode}\n{proc.stdout}{proc.stderr}",
                  file=sys.stderr)

    # The format flags - and 0, which the standard does not define
    # (SYSTEM_TASKS.md 3.12; the files come from tools/gen_format_flag_matrix.py):
    # what they print, and one warning for each format that uses them, in
    # any order; the flags that are not supported yet stop before the run.
    proc = subprocess.run([args.kirimasim, FLAG_MATRIX.name], cwd=HERE, capture_output=True,
                          text=True, check=False)
    want_out = FLAG_MATRIX.with_suffix(".expected").read_text(encoding="utf-8")
    want = FLAG_MATRIX.with_suffix(".stderr").read_text(encoding="utf-8").splitlines()
    got = [line for line in proc.stderr.splitlines() if ": warning: " in line]
    if proc.returncode != 0 or proc.stdout != want_out or sorted(got) != sorted(want):
        failures += 1
        print(f"FAIL: format flag matrix: exit code {proc.returncode}\n{proc.stdout}{proc.stderr}",
              file=sys.stderr)
    proc = subprocess.run([args.kirimasim, FLAG_UNSUPPORTED.name], cwd=HERE, capture_output=True,
                          text=True, check=False)
    want = FLAG_UNSUPPORTED.with_suffix(".stderr").read_text(encoding="utf-8").splitlines()
    got = [line for line in proc.stderr.splitlines() if ": error: " in line]
    if proc.returncode != 2 or proc.stdout != "" or sorted(got) != sorted(want):
        failures += 1
        print(f"FAIL: unsupported format flags: exit code {proc.returncode}\n{proc.stdout}"
              f"{proc.stderr}", file=sys.stderr)

    # A system task or function whose first argument is a string is reported
    # as one, not as a string method (the files come from
    # tools/gen_builtin_call_matrix.py).
    proc = subprocess.run([args.kirimasim, BUILTIN_CALLS.name], cwd=HERE, capture_output=True,
                          text=True, check=False)
    want = BUILTIN_CALLS.with_suffix(".stderr").read_text(encoding="utf-8").splitlines()
    got = [line for line in proc.stderr.splitlines() if ": error: " in line]
    if proc.returncode != 2 or proc.stdout != "" or sorted(got) != sorted(want):
        failures += 1
        print(f"FAIL: builtin call matrix: exit code {proc.returncode}\n{proc.stdout}"
              f"{proc.stderr}", file=sys.stderr)

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

    # SIGINT, on POSIX systems (the files come from tools/gen_interrupt_matrix.py):
    # the first stops the run, final runs, the output is kept and the code is
    # 130; a second one ends kirimasim at once (SCHEDULER_SPEC.md 16.2).
    if os.name == "posix":
        for line in INTERRUPTS.read_text(encoding="utf-8").splitlines():
            case = json.loads(line)
            problems = run_interrupt(args.kirimasim, case)
            if problems:
                failures += 1
                print(f"FAIL: SIGINT {case['file']}: " + "; ".join(problems), file=sys.stderr)

    return 1 if failures else 0


if __name__ == "__main__":
    sys.exit(main())
