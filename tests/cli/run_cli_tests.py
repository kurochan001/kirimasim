#!/usr/bin/env python3
# Copyright (c) 2026 The kirimasim Authors
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

"""Checks kirimasim's command line: options, exit codes and messages (CLI.md).

Each case runs kirimasim with some arguments on a small design and checks
the exit code, and text that stdout or stderr must contain. A run that
writes more than 16 MiB to stdout or stderr is stopped and fails
(tools/bounded_run.py).
"""

import argparse
import json
import os
import queue
import re
import signal
import subprocess
import sys
import tempfile
import threading
import time
from pathlib import Path

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE.parent.parent / "tools"))
import bounded_run  # noqa: E402

DESIGN = str(HERE / "display_finish.sv")
DELAYS = str(HERE / "delays.sv")
FINAL = str(HERE / "final.sv")
SPIN = str(HERE / "spin.sv")
FOR_SPIN = str(HERE / "for_omitted_spin.sv")
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
ERROR_DESIGNS = [HERE / "later_work_matrix.sv", HERE / "later_work_override.sv",
                 HERE / "deferred_write_matrix.sv"]
TASK_WRITES = HERE / "task_write_matrix.sv"
BLOCK_POSITIONS = HERE / "block_position_matrix.sv"
LIFETIME = HERE.parent / "suite" / "sched" / "process_lifetime.sv"
END_TASKS = str(HERE / "end_tasks.sv")
FILE_MONITOR = str(HERE / "file_monitor_ends.sv")

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
    # $stacktrace, innermost first: the call's scope, then the task or
    # function, or the module instance of the procedure, that made each
    # call (SYSTEM_TASKS.md 10.3).
    (["stacktrace.sv"], 0,
     "=top (stacktrace.sv:27)\n"
     "top.u.f (stacktrace.sv:8)\ntop.u.t (stacktrace.sv:12)\ntop (stacktrace.sv:28)\n"
     "top.fb (stacktrace.sv:31)\n"
     "top.g.gt (stacktrace.sv:23)\ntop (stacktrace.sv:34)\n"
     "top.u.w (stacktrace.sv:16)\ntop (stacktrace.sv:35)\n", ""),
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
    # The seed of $urandom, named in the version line (CLI.md 4, SYSTEM_TASKS.md 13).
    ([DESIGN], 0, "hello\n", ") seed=0 args: "),
    (["--seed=12345", DESIGN], 0, "hello\n", ") seed=12345 args: --seed=12345"),
    (["--seed=x", DESIGN], 2, "", "--seed=x needs a whole number"),
    # FST dependencies are prepared; its backend remains unsupported (WAVEFORM.md 1, 5).
    (["--wave=w.fst", DESIGN], 2, "", "--wave=w.fst: FST is not supported yet; use a .vcd file"),
    (["--wave=w", DESIGN], 2, "", "--wave=w needs a file ending in .vcd"),
    (["--wave=.vcd", DESIGN], 2, "", "--wave=.vcd needs a file ending in .vcd"),
    (["--wave-scope=top,,u", DESIGN], 2, "", "--wave-scope=top,,u has an empty pattern"),
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
    (["--max-steps=1000", FOR_SPIN], 5, "=final ran\n",
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
    over: list[str] = []

    def read(stream, name: str, keep: list[str], tell: bool) -> None:
        # At most bounded_run.LIMIT characters, as bounded_run.run keeps.
        size = 0
        while line := stream.readline(bounded_run.LIMIT + 1 - size):
            size += len(line)
            if size > bounded_run.LIMIT:
                over.append(name)
                proc.kill()
                break
            keep.append(line)
            if tell:
                lines.put(line)
        if tell:
            lines.put(None)

    readers = [threading.Thread(target=read, args=(proc.stdout, "stdout", out, False),
                                daemon=True),
               threading.Thread(target=read, args=(proc.stderr, "stderr", err, True),
                                daemon=True)]
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
    if over:
        return [f"wrote more than {bounded_run.LIMIT >> 20} MiB to {over[0]}; it was stopped"]
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
        proc = bounded_run.run([args.kirimasim, rel], cwd=HERE)
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
        proc = bounded_run.run([args.kirimasim, *argv], cwd=cwd)
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
    proc = bounded_run.run([args.kirimasim, "--stats=json", DESIGN])
    try:
        stats = json.loads(proc.stdout.splitlines()[-1])
        if stats.get("sim.end_reason") != "finish" or stats.get("proc.wakeups") != 1:
            raise ValueError(stats)
    except (IndexError, ValueError) as e:
        failures += 1
        print(f"FAIL: --stats=json: {e}\n{proc.stdout}", file=sys.stderr)

    # A file monitor ends when its last file closes (SYSTEM_TASKS.md 6.2):
    # its process and frame go with it, so the processes kept do not grow
    # with the number of monitors the design makes (100 here). Without that,
    # proc.kept_peak would be about 100.
    with tempfile.TemporaryDirectory() as tmp:
        proc = bounded_run.run([args.kirimasim, "--stats=json", FILE_MONITOR], cwd=tmp)
        problems = []
        if proc.returncode != 0:
            problems.append(f"exit code {proc.returncode}")
        if "done" not in proc.stdout.splitlines():
            problems.append("stdout lacks 'done'")
        try:
            stats = json.loads(proc.stdout.splitlines()[-1])
            kept = stats.get("proc.kept_peak")
            if not isinstance(kept, int) or kept > 8:
                problems.append(f"proc.kept_peak is {kept}, not at most 8")
        except (IndexError, ValueError) as e:
            problems.append(f"--stats=json: {e}")
        if problems:
            failures += 1
            print(f"FAIL: a file monitor ends with its last file: {'; '.join(problems)}",
                  file=sys.stderr)
            print(f"--- stdout ---\n{proc.stdout}--- stderr ---\n{proc.stderr}", file=sys.stderr)

    # $finish and $stop end the run at 5ns with exit code 0; final runs and
    # nothing scheduled later does (SCHEDULER_SPEC.md 16.2, 9.2.3).
    for plusarg, reason, level, line_no in END_CASES:
        proc = bounded_run.run([args.kirimasim, "--stats=json", END_TASKS, f"+{plusarg}"])
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

    proc = bounded_run.run([args.kirimasim, VIOLATIONS])
    got = [line.split("/")[-1] for line in proc.stderr.splitlines() if " violation: " in line]
    if proc.returncode != 0 or proc.stdout != "h=8\n" or got != VIOLATION_LINES:
        failures += 1
        print(f"FAIL: violation reports\n{proc.stdout}{proc.stderr}", file=sys.stderr)

    # 24.3/24.3.1/24.6: every root initial runs in its own program;
    # ordering among Reactive processes is unspecified. Check the multiset.
    proc = bounded_run.run([args.kirimasim, PROGRAM_MATRIX.name], cwd=HERE)
    want = PROGRAM_MATRIX.with_suffix(".stderr").read_text(encoding="utf-8").splitlines()
    got = [line for line in proc.stderr.splitlines() if not line.startswith("kirimasim: info:")]
    stdout = PROGRAM_MATRIX.with_suffix(".stdout").read_text(encoding="utf-8").splitlines()
    if proc.returncode != 0 or sorted(proc.stdout.splitlines()) != stdout or got != want:
        failures += 1
        print(f"FAIL: program matrix: exit code {proc.returncode}\n{proc.stdout}{proc.stderr}",
              file=sys.stderr)

    # Every unsupported construct is reported with its position, in every
    # container, and nothing runs (CLI.md 5; the files and the lines come
    # from tools/gen_unsupported_matrix.py).
    got = []
    accepted = set(UNSUPPORTED_MATRIX.with_suffix(".accepted").read_text(encoding="utf-8").splitlines())
    for sv in sorted(UNSUPPORTED_MATRIX.glob("*.sv")):
        rel = str(sv.relative_to(HERE))
        proc = bounded_run.run([args.kirimasim, rel], cwd=HERE)
        diagnostics = [line for line in proc.stderr.splitlines() if not line.startswith("kirimasim: info:")]
        implemented = sv.stem in accepted
        if implemented:
            accepted.remove(sv.stem)
        else:
            got += [line for line in diagnostics if ": error: " in line]
        expected = sv.with_suffix(".expected")
        stdout = expected.read_text(encoding="utf-8") if implemented and expected.exists() else ""
        if proc.returncode != (0 if implemented else 2) or proc.stdout != stdout or (implemented and diagnostics):
            failures += 1
            print(f"FAIL: {rel}: exit code {proc.returncode}\n{proc.stdout}{proc.stderr}",
                  file=sys.stderr)
    if accepted:
        failures += 1
        print(f"FAIL: missing accepted historical inputs: {sorted(accepted)}", file=sys.stderr)
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
        proc = bounded_run.run([args.kirimasim, "--max-slot-events=1000", rel], cwd=HERE)
        if (proc.returncode != 5 or proc.stdout != "started\nfinal ran\n" or
                not proc.stderr.endswith(want)):
            failures += 1
            print(f"FAIL: {rel}: exit code {proc.returncode}\n{proc.stdout}{proc.stderr}",
                  file=sys.stderr)

    # The format flags - and 0, which the standard does not define
    # (SYSTEM_TASKS.md 3.12; the files come from tools/gen_format_flag_matrix.py):
    # what they print, and one warning for each format that uses them, in
    # any order; the flags that are not supported yet stop before the run.
    proc = bounded_run.run([args.kirimasim, FLAG_MATRIX.name], cwd=HERE)
    want_out = FLAG_MATRIX.with_suffix(".expected").read_text(encoding="utf-8")
    want = FLAG_MATRIX.with_suffix(".stderr").read_text(encoding="utf-8").splitlines()
    got = [line for line in proc.stderr.splitlines() if ": warning: " in line]
    if proc.returncode != 0 or proc.stdout != want_out or sorted(got) != sorted(want):
        failures += 1
        print(f"FAIL: format flag matrix: exit code {proc.returncode}\n{proc.stdout}{proc.stderr}",
              file=sys.stderr)
    proc = bounded_run.run([args.kirimasim, FLAG_UNSUPPORTED.name], cwd=HERE)
    want = FLAG_UNSUPPORTED.with_suffix(".stderr").read_text(encoding="utf-8").splitlines()
    got = [line for line in proc.stderr.splitlines() if ": error: " in line]
    if proc.returncode != 2 or proc.stdout != "" or sorted(got) != sorted(want):
        failures += 1
        print(f"FAIL: unsupported format flags: exit code {proc.returncode}\n{proc.stdout}"
              f"{proc.stderr}", file=sys.stderr)

    # A system task or function whose first argument is a string is reported
    # as one, not as a string method (the files come from
    # tools/gen_builtin_call_matrix.py).
    proc = bounded_run.run([args.kirimasim, BUILTIN_CALLS.name], cwd=HERE)
    want = BUILTIN_CALLS.with_suffix(".stderr").read_text(encoding="utf-8").splitlines()
    got = [line for line in proc.stderr.splitlines() if ": error: " in line]
    if proc.returncode != 2 or proc.stdout != "" or sorted(got) != sorted(want):
        failures += 1
        print(f"FAIL: builtin call matrix: exit code {proc.returncode}\n{proc.stdout}"
              f"{proc.stderr}", file=sys.stderr)

    # Errors with exit code 2: those front finds, each with a note, for a
    # function that schedules work after it returns, called where 13.4.4
    # does not allow it (SCHEDULER_SPEC.md 6.5; tools/gen_later_work_matrix.py
    # and tools/gen_later_work_override_matrix.py); and the run-time error
    # of the Postponed region that stops the design in which other processes
    # write, through the actions of deferred assertions, what an always_comb,
    # always_latch or always_ff writes, legal in itself (SCHEDULER_SPEC.md
    # 2.2, 6.4; tools/gen_deferred_write_matrix.py).
    for design in ERROR_DESIGNS:
        proc = bounded_run.run([args.kirimasim, design.name], cwd=HERE)
        want = design.with_suffix(".stderr").read_text(encoding="utf-8").splitlines()
        got = [line for line in proc.stderr.splitlines()
               if ": error: " in line or ": note: " in line]
        if proc.returncode != 2 or proc.stdout != "" or sorted(got) != sorted(want):
            failures += 1
            print(f"FAIL: {design.stem}: exit code {proc.returncode}\n{proc.stdout}"
                  f"{proc.stderr}", file=sys.stderr)

    # Other processes write through tasks what an always_comb, always_latch
    # or always_ff writes: legal, as slang keeps it (SCHEDULER_SPEC.md 6.4;
    # tools/gen_task_write_matrix.py). The design runs with no output.
    proc = bounded_run.run([args.kirimasim, TASK_WRITES.name], cwd=HERE)
    if proc.returncode != 0 or proc.stdout != "" or ": error: " in proc.stderr:
        failures += 1
        print(f"FAIL: {TASK_WRITES.stem}: exit code {proc.returncode}\n{proc.stdout}"
              f"{proc.stderr}", file=sys.stderr)

    # A block statement is placed at its first token, whatever its body:
    # the fork instruction, the process of each statement of a fork, and
    # the scope instructions of a named block (the files come from
    # tools/gen_block_position_matrix.py).
    proc = bounded_run.run([args.kirimasim, "--dump-sir", BLOCK_POSITIONS.name], cwd=HERE)
    got = []
    for line in proc.stdout.splitlines():
        at = line.rsplit("; ", 1)[-1]
        if line.startswith("  fork "):
            got.append(f"fork {at}")
        elif line.startswith(("  scope.enter ", "  scope.leave ")):
            got.append(f"{line.split()[0]} {at}")
        elif line.startswith("process ") and re.search(r'\.fork\d+\.\d+" ', line):
            got.append(f"branch {at}")
    want = BLOCK_POSITIONS.with_suffix(".expected").read_text(encoding="utf-8").splitlines()
    if proc.returncode != 0 or sorted(got) != want:
        failures += 1
        missing = sorted(set(want) - set(got))
        extra = sorted(set(got) - set(want))
        print(f"FAIL: block positions: exit code {proc.returncode}\n" +
              "".join(f"  missing: {x}\n" for x in missing) +
              "".join(f"  extra: {x}\n" for x in extra) + proc.stderr, file=sys.stderr)

    # The records of finished processes are released (SCHEDULER_SPEC.md
    # 10.8): with twice the rounds of the process lifetime matrix, twice as
    # many processes are made, but no more records are kept at once.
    runs = []
    for n in (4, 8):
        proc = bounded_run.run([args.kirimasim, "--stats=json", str(LIFETIME), f"+n={n}"])
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
    try:
        sys.exit(main())
    except bounded_run.OutputLimit as e:
        print(f"FAIL: {e}", file=sys.stderr)
        sys.exit(1)
