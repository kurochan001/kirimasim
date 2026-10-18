# PERFORMANCE: performance

[日本語](PERFORMANCE.ja.md) · This English text is the main version; the Japanese version follows it, and where they differ, this text holds.

Priority: P1. Status: **approved (2026-10-15)**. The design specification's "performance goals and measurement" was summarized and rewritten, and the points it left open (the names of the counters and how they are written, the form in which PerfCount measures runs, the thresholds of the nightly measurement) were added. On 2026-10-17 the counter `proc.kept_peak` was added to 3.2 (approved). On 2026-10-18, comparing the exit code was added to 4.2 (approved). The same day, the heading of 4.1 was corrected, and 5.1 was marked as not implemented yet (approved). The same day, the premise in 5.1 that macOS runners cannot be had was corrected (approved).

This document sets kirimasim's performance goals, its event counters, the regression test by counts (PerfCount), and the conditions of wall-clock benchmarks. The reasons for ending follow SCHEDULER_SPEC.md 16, and the form of `--stats=json` follows CLI.md.

## 1. Approach

- **CI passes or fails by counts.** Wall-clock time swings widely with the machine's load, the level of optimization and the CI runner. The number of evaluations and assignments is the same every time for the same input and the same options. A fix of correctness can multiply the number of evaluations while the tests still pass, and measuring wall-clock time now and then hardly notices it (a rule of the design specification).
- **Wall-clock time is recorded and reported, but not used to pass or fail.** Counts do not show the cost of one operation or how memory grows. So wall-clock time, the peak RSS and the build time are measured nightly, and a worsening is reported (5).
- **People set the performance budget.** Changes to PerfCount's expected values and to thresholds need a human's approval (AI_POLICY.md; TEST_SUITE.md's rules on expected values).

## 2. Goals (draft)

The numbers are rough aims. They are reviewed after the baseline is measured in M6 of P1, and fixed at the end of P2.

| Measure | Goal (draft) | How measured | Stage measurement starts |
| --- | --- | --- | --- |
| sv-tests | On the simulation tags, as many passes as Verilator or more | The sv-tests harness. The tags are defined by an amendment to TEST_SUITE.md before M6 | P1 (M6) |
| 4-state correctness | The 4-state tests of our own test suite match their expected values; any difference can be explained | The test suite | P1 |
| Speed (2-state RTL) | 0.5 to 1 times Verilator single-threaded | CoreMark on Ibex, CVA6 | P2 |
| Speed (4-state RTL) | 10 times Icarus or more | The same | Tier 1 baseline in P1, compared in P2 |
| Speed (UVM) | 100 times or more another simulator based on CIRCT (Normal Computing's fork) | The same UVM AVIPs | P3 |
| Time to start running | Tier 1 starts right after elaboration. Tier 2 too is faster than Verilator | The same design, from starting to read the files to the first evaluation at time 0 | P1 (tier 1), P2 (tier 2) |

- Speed is compared not only by run time but also by total time. The total is the time of elaboration, plus the build time, plus the number of tests times the run time of one. Tier 2 starts later by its build, but overtakes in regressions with many tests.
- The versions compared against are the ones the test suite uses for cross-checking (Icarus Verilog 13.0, Verilator 5.052; TEST_SUITE.md).
- When comparing with Verilator, kirimasim's 2-state approximation (`--state=2`; P2) and exact 4-state (`--state=4`, the default) are treated as separate conditions.
- Results of commercial simulators are not published unless their terms of use can be checked.

## 3. Event counters

### 3.1 Form and output

- A counter is a pair of a name and an unsigned 64-bit value. The name has the form `<area>.<content>` and uses only lower-case letters, digits, `_` and `.` (for example `sched.time_advances`).
- The table of counters belongs to the Context. There are no global counters (CLAUDE.md).
- With `--stats=json`, when the run ends, all counters are written as one JSON object on **the last line of standard output** (CLI.md 3). The items are in the order the counters were added, the same every time.
- The only item that is not a number is the reason for ending, `sim.end_reason` (a string; the names of SCHEDULER_SPEC.md 16.2). PerfCount compares string items for exact equality (4).
- Counters are always counted, in every build including release. Counting is one addition only, inside kernel, VM or front.
- **Determinism:** for the same version of kirimasim, the same input and the same options, the values are the same whatever the OS, the compiler or the kind of build (dev, release, asan, ubsan). So nothing is counted that depends on the workings of the standard library (how many times a `std::vector` allocates, the size of a hash table and so on). Of memory allocation, only the objects the kernel allocates itself are counted (P3).

### 3.2 List of counters

"Stage" is the stage that adds the counter. A counter not in the table is added to this document when it is added (in the same PR is fine).

| Name | What it counts | Stage |
| --- | --- | --- |
| `front.files`, `front.syntax_trees`, `front.errors`, `front.warnings` | Files read, syntax trees, errors and warnings (those there now) | P0 |
| `sir.slots`, `sir.processes`, `sir.subroutines`, `sir.blocks`, `sir.instrs` | The size of the SIR made, to notice when `front`'s lowering adds needless instructions | M4 |
| `sched.time_slots` | Time slots run | M4 |
| `sched.time_advances` | Times the time advanced | M4 |
| `sched.active_loops`, `sched.reactive_loops` | Iterations of the active and reactive sides of the reference algorithm (SCHEDULER_SPEC.md 3) | M4 (reactive in P3) |
| `sched.events.<region>` | Events run in each region. `<region>` is a name of SCHEDULER_SPEC.md 2.1 in lower case with `_`: `active`, `inactive`, `nba`, `observed`, `reactive`, `re_inactive`, `re_nba`, `postponed` and so on. A region with no events is written as 0 | M4 |
| `proc.wakeups` | Times a process was woken to be evaluated (including the start at time 0) | M4 |
| `proc.created`, `proc.finished` | Processes made (children of fork included) and processes that ended | M5 |
| `proc.kept_peak` | The most process records kept at once (live ones, and ended ones not yet freed; SCHEDULER_SPEC.md 10.8) | P1 |
| `slot.writes` | Writes to slots (blocking assignments, continuous assignments, NBA updates) | M4 |
| `slot.changes` | Of those writes, the ones that actually changed the value | M4 |
| `nba.scheduled` | NBA updates scheduled | M4 |
| `wait.subscriptions`, `wait.monitor_evals` | Registrations of slot subscriptions, and re-evaluations of the expression of a monitor record (SCHEDULER_SPEC.md 9.5) | M4 |
| `vm.instrs` | Instructions tier 1 ran | M5 |
| `sim.end_time` | The time at the end (in steps of the design's precision) | M4 |
| `sim.end_reason` | The reason for ending (a string) | M4 |
| `island.evals2`, `island.evals4` | Evaluations of static islands (2-state and 4-state versions) | P2 |
| `mem.objects`, `gc.runs` | Objects the kernel allocated, and runs of the GC | P3 |

- `vm.instrs` changes when the way tier 1's bytecode is made changes. A change that lowers it is welcome; a change that raises it states the reason in its commit (4.3).
- Only counts that do not depend on the tier (`sched.*`, `proc.*`, `slot.*`, `nba.*`, `wait.*`, `sim.*`) may be compared across tiers. `vm.instrs` and `island.*` are counts of one tier.

## 4. PerfCount (regression test by counts)

### 4.1 The form set in P0

- `tests/perf_counts/expected.json` holds, for each test file, the expected values of the counters. `tools/perfcount_check.py` runs `kirimasim --stats=json parse <file>` and compares (CTest label `PerfCount`).
- What fails: an **increase of 10% or more** over the expected value (any increase from an expected 0), a counter in the expected values that is missing, a counter not in the expected values.
- A decrease does not fail. When a value decreases, the expected value is lowered and fixed in a separate change.

### 4.2 What changes in P1 (M4 to M6)

- **Measuring runs:** a test whose `:type:` contains `simulation` is run (`kirimasim --stats=json <file>`) instead of `parse`, and its counts compared. The JSON is read from the last line of standard output.
- **Tests covered:** the required tests (TEST_SUITE.md) that can run, and the small designs for benchmarks (5.2). Not the whole test suite (every new runnable test would need a change of expected values, and CI would grow heavy).
- **String items:** `sim.end_reason` is compared for exact equality. A difference fails.
- **Exit code (added 2026-10-18, approved):** the exit code of the run is compared too: that of `:expected_exit:` (0 when absent) for a `simulation` test, and for a test that is only parsed 0, or 2 with `:should_fail_because:`. A test whose header cannot be read as written (TEST_SUITE.md, "What is judged now") fails.
- **Threshold:** stays at 10%, with no exceptions per counter (draft). If an exception is needed, it is proposed to a human as an amendment to this document.
- Tests that cannot run because of unsupported constructs (exit code 2 and `error: unsupported:`) are not registered in the expected values.

### 4.3 How expected values change

- Registering new tests and new counters may be in the same PR as the implementation (a human reviews the grounds).
- A change that raises, removes or loosens an existing expected value goes in a PR separate from the implementation, and says why the count grew and why it is accepted (`tools/check_expected_change.py` checks this).
- When a count decreases, the expected value is lowered in a separate PR. The procedure is in `tests/perf_counts/README.md`.

## 5. Wall-clock benchmarks

### 5.1 Nightly measurement (from M6 of P1)

Not implemented yet (as of 2026-10-18): no workflow measures the benchmarks; `tests/tools` only checks that the benchmark designs run.

- A scheduled GitHub Actions workflow measures once every night on a Linux (x86-64) runner. Whether it also measures on GitHub's macOS runner, which this public repository can use (checked 2026-10-18), is decided when this is implemented; until then macOS values are measured on the developer's Mac and attached to reports.
- What is measured: the wall-clock run time of each benchmark, the peak RSS, and the build time of kirimasim itself (release). The peak RSS is taken with Python's `resource.getrusage(RUSAGE_CHILDREN)`; no new tool is installed.
- Each benchmark runs 5 times and the median is used. The results are kept as JSON among the workflow's artifacts.
- **When to report (draft):** taking the median of the last 7 nightly results as the baseline, a warning is put in the workflow's summary when wall-clock time or the peak RSS worsens by 15% or more, or the build time by 25% or more. It does not pass or fail anything (the workflow does not fail). Shared runners swing widely, so the width of the swings is checked with the values during P1, and the thresholds reviewed.

### 5.2 Benchmark designs

- P1: small synthetic designs kept in the repository (`bench/`; written by us, as category A). At least these four:
  - A combinational circuit with much 4-state arithmetic and comparison (a chain of `always_comb`)
  - Shift registers and counters with many NBAs (`always_ff`)
  - Many small processes, with waits for events and for time (`initial` with `#` and `@`)
  - fork and join, named events
- P2 and later: Ibex (CoreMark), CVA6, OpenTitan IPs, UVM AVIPs, and small synthetic tests with races and combinational loops. Third-party designs are not bundled; scripts that fetch them refer to them (category C of the design specification).

### 5.3 What is recorded

- Recorded: the CPU model, the number of cores, the amount of memory, the OS, the compiler version, kirimasim's commit and options, the versions and options of the simulators compared against, and the load average before measuring.
- Nothing that identifies the machine (host name, serial number, user name, home directory path) is recorded (CLAUDE.md).
- The median of 5 runs is used each time. Comparisons are split into three conditions: total time without a cache, rebuild time with a cache, and run time alone (the cache is P2).

## 6. Not confirmed, to be written later

- Fixing the numbers of the goals in 2 (reviewed after the baseline is measured in M6, fixed at the end of P2).
- The definition of the "simulation tags" of sv-tests (put forward as an amendment to TEST_SUITE.md before M6).
- Reviewing the reporting conditions of 5.1 (after seeing the width of the swings during P1).
- Whether a scheduled workflow can run every night on GitHub's Linux runner (not confirmed; tried in M6; if it cannot, a proposal to measure locally instead).
- How much of the counters the generated code of tier 2 counts (P2; decided after measuring whether counting slows it too much).
