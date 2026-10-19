# UNSPECIFIED_BEHAVIOR: what kirimasim does where the standard leaves the result open

[日本語](UNSPECIFIED_BEHAVIOR.ja.md) · This English text is the main version; the Japanese version follows it, and where they differ, this text holds.

Priority: P1. Status: **approved (2026-10-19)**. On 2026-10-19, this document was written (approved). The same day, a person chose the approved rules for the four differences of 9: for J3 and A13 as they are, for A2 with a lint warning, and for P4 with the memory's bounds where the arguments give none. The same day, the options of slang's limits for A11 and A12 were set (CLI.md 2.1; approved). The same day, N9 (the top from configs in the sources) was added (CLI.md 2.1; approved). The same day, N10 (variable input ports under `` `unconnected_drive ``) was added (VALUE_SEMANTICS.md 5.3; approved).

This document lists the places where IEEE 1800-2023 does not set an order or a result, leaves it to the implementation, or where following its wording literally would break practical code, and says what kirimasim does at each. The text of the standard is not copied; clause numbers are given. Most rules this document relies on are written in other specifications. This document gives them as references, and writes here only what has no other home: the principles (1), the kinds of diagnostics (4), the status of each item (6), and the lists of 7 and 8.

The item IDs (A1 to R5, with A14 and A15 added) follow a private survey made in October 2026.

**Status of a row.** Every row of the tables has one of these values:

- **in force**: already set by an approved specification. The section is given, and the rule is not repeated here.
- **approved 2026-10-19**: proposed by this document and approved by a person on 2026-10-19. A row approved later carries its own date. Only approved rows are implemented.
- **later (P2)**, **later (P3)**, **later (P4)**: written here now, decided in detail and implemented in that phase.

## 1. Principles

Every item follows these eight principles.

1. What the standard sets is implemented as the standard says. The 4-state default, the rules of x and the way the regions proceed are not changed for speed.
2. Where the standard does not set an order, one deterministic default order is chosen, written down, and fixed by tests. For the same version, input, options and seed, the result is the same whatever the tier, the OS and the run (NFR-2).
3. Where following the wording literally would break practical code (switching processes in the middle of a statement), kirimasim takes a choice the standard allows that matches common practice, and writes why.
4. There is a race exploration mode that perturbs the order, to find code that relies on the default order. Passing with the default order does not prove that there is no race.
5. There is no compatibility mode that copies another simulator's results, for now; a run has one meaning. The exceptions are `--state=2` (an approximation in 2 states; P2) and `--init-values` (E9). Neither is the default.
6. Speed-ups (tier 2, the 2-state version, static ordering, parallelism) do not change observable results. What could change them is a separate mode, watched by `--tier-check` and differential tests.
7. A run where time does not advance, and a run that uses too much of a resource, always stop at a limit and say where. Nothing loops silently.
8. Code that looks as if it relies on unspecified behaviour is reported in two steps: checks at elaboration (lint) and detection at run time.

## 2. Order at time 0

The steps at time 0 are written in SCHEDULER_SPEC.md 6.1 (approved 2026-10-19). Items B8, B9, B10, D2 and J3 rest on them. Static initializers (step 1) keep declaration order within a scope (9).

## 3. Default order within a region

- The order in which events are taken from a region, and the cooperative running of processes: SCHEDULER_SPEC.md 4.3 (in force). The order in which a change wakes the processes waiting on it: SCHEDULER_SPEC.md 4.3 (approved 2026-10-19).
- The order of NBA updates, and where an NBA with a delay goes when its time comes: SCHEDULER_SPEC.md 13 (the first in force, the second approved 2026-10-19).
- The order of evaluation within a static island, shared by tiers 1 and 2: CODEGEN_SPEC.md "Static islands" (later (P2)).

Items B1, B2, C1, C2 and A8 rest on these.

## 4. Kinds of diagnostics

| Kind | When | By default | Examples | Status |
| --- | --- | --- | --- | --- |
| Lint warning | Elaboration | Printed, and the run goes on. Each can be turned off or made an error with `-W` | A path with no timing control, `casex` | approved 2026-10-19 |
| Elaboration error | Elaboration | Exit code 2 | Several drivers on a `uwire` (slang's) | in force (CLI.md 5) |
| Run-time warning | Run | Printed, and the run goes on. Printed once per place, with a count at the end | An x in a condition, an out-of-range index | approved 2026-10-19 |
| Run-time error | Run | One line, then the run ends with `final` (reason error, code 2) | A write in the Postponed region, a time past the largest time | in force (SCHEDULER_SPEC.md 16.2) |
| Run-time fatal | Run | `final` runs, and the run ends with the exit code of its reason | The limits on runs where time does not advance, and on resources | in force for zero_time_limit (SCHEDULER_SPEC.md 16.2); approved 2026-10-19 for resource_limit and wall_time_limit |

- **Names:** each diagnostic of this document has a name of the form `kirima-<words>`, in lower case with hyphens (for example `kirima-zero-time-loop`), so that `-W` takes it as it takes slang's warning names and it cannot be confused with slang's. How the names are given, and the switches `-W<name>`, `-Wno-<name>` and `-Werror=<name>`, are in CLI.md 6 (approved 2026-10-19). The names are fixed in M7. Run-time errors and limits cannot be turned off; a limit can only be raised.
- **Content:** the file and line, the hierarchical names of the process and the block, and at run time the chain of SystemVerilog calls. `--diag-format=json` (CLI.md 2.3, M7) gives the same.
- Where slang gives a diagnostic of the same content (B4, E8 and others), kirimasim uses slang's and does not give a second one. Which of these slang has is checked in M7 against the diagnostics of the pinned slang (not confirmed).

## 5. Race exploration mode

What the mode perturbs and what it keeps are written in SCHEDULER_SPEC.md 4.3 and 11 (approved 2026-10-19; the mode itself is P2). The order of NBAs of different processes is perturbed only with `--race-nba` (P2). A static check reports, as the lint warning `kirima-race-blocking`, a variable that one process writes by blocking assignment and another reads on the same edge (B3, B6; approved 2026-10-19).

## 6. The items

Columns: ID; what kirimasim does; the diagnostics and options it gives; the clauses of the standard to check; the phase; the status. A row with "in force" names the section that holds the rule. An item whose parts differ in status has one row per part.

### 6.1 A: runs where time does not advance

Found at elaboration where possible, stopped at a limit at run time, and reported with where it happened.

| ID | Handling | Diagnostics and options | Clause | Phase | Status |
| --- | --- | --- | --- | --- | --- |
| A1 | `#0` resumes in Inactive (Re-Inactive in a program). Moves into the Active or Reactive region in one time slot are counted, and the run stops at the limit | Reason zero_time_limit, code 5. `--max-zero-delay` | 4.4.2.3, 9.4.1 | P1 | in force (SCHEDULER_SPEC.md 8, 16.2; CLI.md 2.6) |
| A1 | The report of a limit gains the last process and the latest events | CLI.md 2.6 | — | P1 | approved 2026-10-19 |
| A1 | Lint for a loop whose only timing control is `#0` | `kirima-zero-delay-loop` | 4.4.2.3 | P1 | approved 2026-10-19 |
| A2 | An `always` with no timing control on any path that does not reach `$finish`, `$fatal` or a disable gets a lint warning, and runs as the next row says; one where only some paths have none gets a lint warning too | `kirima-always-no-wait`, `kirima-always-no-wait-path` | 9.2.2.1 | P1 | approved 2026-10-19 (SCHEDULER_SPEC.md 6.2) |
| A2 | At run time it stops at the limit of A14 | `--max-steps` | 9.2.2.1 | P1 | in force (SCHEDULER_SPEC.md 6.2, 16.2) |
| A3 | `always @*` with an empty sensitivity never starts | — | 9.4.2.2 | P1 | in force (SCHEDULER_SPEC.md 6.2, 9.2) |
| A3 | Warn, suggesting `always_comb` | `kirima-empty-sensitivity` | 9.4.2.2 | P1 | approved 2026-10-19 |
| A4 | A `wait` that is always true or always false behaves as the standard says. A `wait` that goes on at once is not a wait for the limit of A14 | `--max-steps` | 9.4.3 | P1 | in force (SCHEDULER_SPEC.md 9.3, 16.2) |
| A4 | Warn about a `wait` in a loop that is always true after constant folding; `wait(0)` only as a note | `kirima-const-wait` | 9.4.3 | P1 | approved 2026-10-19 |
| A5 | A limit on the number of live processes made at run time | Reason resource_limit, code 6. `--max-procs` | 9.3.2 | P1 | approved 2026-10-19 (SCHEDULER_SPEC.md 16.2; CLI.md 2.7) |
| A6 | A strongly connected component of a static island is iterated until it settles, up to a limit | Reason zero_time_limit, code 5. `--max-scc-iter` | 4.5, 4.7 | P2 | in force (SCHEDULER_SPEC.md 16.2; CLI.md 2.6) |
| A6 | The report names the signals that changed in the last two iterations; lint for a loop that is a real loop bit by bit | `kirima-comb-not-converged`, `kirima-comb-loop` | 4.5, 4.7 | P2 | later (P2) |
| A7 | The dependency graph is built on bit ranges, so that a loop between different bits of a vector is cut. An index known only at run time is evaluated by iteration | `kirima-comb-loop-unsplit` (also in `--report-state-opt`) | 9.4.2, 11.5.3 | P2 | later (P2) |
| A8 | Inside an island, each process is evaluated once in level order, and tier 1 uses the same order | — | 4.7 | P2 | later (P2; CODEGEN_SPEC.md "Static islands") |
| A9 | The count of A1 includes moving from the NBA region back to Active | `--max-zero-delay` | 4.5 | P1 | in force (CLI.md 2.6) |
| A9 | Lint for an `always` that waits on a variable it writes itself (`always @(x) x <= ~x;`) | `kirima-self-trigger` | 4.5 | P1 | approved 2026-10-19 |
| A10 | Loop variables are in the sensitivity as the standard says. There is no option to leave them out | — | 9.4.2.2 | P1 | in force (SCHEDULER_SPEC.md 9.2) |
| A10 | Warn about a module variable used as the loop variable of two or more `always @*` | `kirima-shared-loop-var` | 9.4.2.2 | P1 | approved 2026-10-19 |
| A11 | Endless recursion of a constant function is left to slang, and slang's limits on constant evaluation are passed through | slang's `--max-constexpr-depth`, `--max-constexpr-steps` | 13.4.3 | P1 | approved 2026-10-19 (CLI.md 2.1) |
| A12 | Endless recursion of instances, and generate blocks that do not end, are left to slang, and slang's limits are passed through (as A11) | slang's `--max-hierarchy-depth`, `--max-generate-steps` | 27 | P1 | approved 2026-10-19 (CLI.md 2.1) |
| A13 | Nothing is written in the Postponed region; a forbidden write is a run-time error | Run-time error `kirima-readonly-write` | 4.4.2.9 | P1 | in force (SCHEDULER_SPEC.md 2.2, 16.2) |
| A13 | A function called from the arguments of `$monitor` or `$strobe` may write its own variables (its arguments, its automatic and static variables); any other write is a run-time error | — | 4.4.2.9, 21.2.2, 21.2.3 | P1 | in force (SCHEDULER_SPEC.md 2.2) |
| A13 | `vpi_put_value` in a read-only callback fails, and `vpi_chk_error` gives the reason | — | 38 | P4 | later (P4) |
| A13 | Lint for a function with side effects in the arguments of `$monitor` or `$strobe` | `kirima-monitor-side-effect` | 21.2.3 | P1 | approved 2026-10-19 |
| A14 | The instructions one process runs without waiting are counted (tier 1), and the run stops at the limit | Reason zero_time_limit, code 5. `--max-steps` | 9.4 | P1 | in force (SCHEDULER_SPEC.md 16.2; CLI.md 2.6) |
| A15 | The depth of nested calls has a limit, so that the run stops before the C++ stack overflows | Reason resource_limit, code 6. `--max-call-depth` | 13.4.2 | P1 | approved 2026-10-19 (SCHEDULER_SPEC.md 16.2; CLI.md 2.7; RUNTIME_ABI.md 5) |

### 6.2 B: races and the order of execution

The order is set by sections 2 and 3. Code that relies on it is reported by lint and by race exploration. Results may differ from other simulators within what the standard allows; there is no mode to make them agree.

| ID | Handling | Diagnostics and options | Clause | Phase | Status |
| --- | --- | --- | --- | --- | --- |
| B1 | Each region is first in, first out | — | 4.7 | P1 | in force (SCHEDULER_SPEC.md 4.3) |
| B1 | A change wakes the processes waiting on it in the order they started to wait | — | 4.7 | P1 | approved 2026-10-19 (SCHEDULER_SPEC.md 4.3) |
| B1 | Race exploration perturbs these orders | `--race-check`, `--race-seed` | 4.7 | P2 | later (P2; SCHEDULER_SPEC.md 11) |
| B2 | Cooperative running: a process is not switched until it waits or ends. A continuous assignment a write wakes is put in Active, not run inside the writing process | — | 4.7 | P1 | in force (SCHEDULER_SPEC.md 4.3, 9.5) |
| B3 | A blocking assignment and a clock edge run in the default order | — | 4.7, 10.4.1 | P1 | in force (SCHEDULER_SPEC.md 4.3) |
| B3 | Lint for a variable written by blocking assignment in one process and read in another on the same edge | `kirima-race-blocking` | 4.7, 10.4.1 | P1 | approved 2026-10-19 |
| B4 | A variable that `always_comb`, `always_latch` or `always_ff` writes must not be written by another process (bit ranges); slang's diagnostic is used | slang's error | 9.2.2.2, 9.2.2.4 | P1 | in force (SCHEDULER_SPEC.md 6.4) |
| B4 | Warn when two ordinary `always` write the same bits | `kirima-multi-write` | 9.2.2.1 | P1 | approved 2026-10-19 |
| B5 | Blocking and nonblocking assignments to the same bits of one variable run as written, with a warning | `kirima-blk-nblk-mix` | 10.4 | P1 | approved 2026-10-19 |
| B6 | A test bench and a design driving at the same time run in the default order; `kirima-race-blocking` covers module `initial`s too | `kirima-race-blocking` | 14, 24 | P1 | approved 2026-10-19 |
| B6 | `program` and clocking blocks as the standard says | — | 14, 24 | P3 | later (P3) |
| B7 | The regions of clocking block skews | — | 14.4, 14.13, 14.16 | P3 | later (P3; SCHEDULER_SPEC.md part 4) |
| B8 | Initial values in declarations make no events and no edges | — | 6.8 | P1 | in force (SCHEDULER_SPEC.md 6.1) |
| B9 | `always_comb` and `always_latch` start last at time 0 | — | 9.2.2.2.2 | P1 | in force (SCHEDULER_SPEC.md 6.1) |
| B10 | Initial values of nets: bits with no driver are z (or the value of the net type), bits with a driver are x until it first writes | — | 6.6, 10.3 | P1 | in force (VALUE_SEMANTICS.md 5.3, 5.4) |
| B11 | After `$finish`, no further event of the same time runs; `final` runs | — | 20.2 | P1 | in force (SCHEDULER_SPEC.md 6.2, 16.2) |
| B11 | Race exploration reports results that depend on the order | `--race-check` | 20.2 | P2 | later (P2) |
| B12 | Reading in `always_comb` before writing sees the previous value, as the standard says | — | 9.2.2.2.1 | P1 | in force (SCHEDULER_SPEC.md 6.3) |
| B12 | Warn about it | `kirima-comb-read-before-write` | 9.2.2.2.1 | P1 | approved 2026-10-19 |

The cooperative running of B2 is also what tier 2 and future parallelism rest on: no switch happens inside a statement, so generated code can optimize a process as an ordinary C++ function.

### 6.3 C: details of NBAs

| ID | Handling | Diagnostics and options | Clause | Phase | Status |
| --- | --- | --- | --- | --- | --- |
| C1 | The NBA region is one first-in, first-out queue: updates take effect in the order issued, within a process and across processes | — | 4.6, 10.4.2 | P1 | in force (SCHEDULER_SPEC.md 4.1, 4.3, 13) |
| C2 | An NBA with a delay, when its time comes, goes ahead of the NBAs issued in that time slot, in the order (time it falls due, issue number) | — | 4.5 | P1 | approved 2026-10-19 (SCHEDULER_SPEC.md 13) |
| C3 | The right side is evaluated when the statement runs. The target of an NBA is decided then too; the target of a blocking assignment with an intra-assignment delay when it resumes | — | 9.4.5, 10.4 | P1 | in force (SCHEDULER_SPEC.md 5, 8) |
| C4 | Intra-assignment delays of NBAs are not cancelled. Delays of continuous assignments and gates are inertial | — | 10.3.3, 10.4.2 | P1 | in force (SCHEDULER_SPEC.md 12, 13) |
| C5 | Inertial delays remove pulses shorter than the delay; rise, fall and turn-off delays as the standard says | — | 10.3.3, 28.16 | P1 | in force (SCHEDULER_SPEC.md 12) |
| C5 | `min:typ:max` takes typ by default; an option chooses; another reports removed pulses as notes | `--mintypmax`, `--report-pulses` | 30.7 | P1 | approved 2026-10-19 (CLI.md 2.2) |
| C6 | `#0` resumes in Inactive; `a <= #0 b` is an NBA of the same time; `a = #0 b` evaluates the right side, resumes in Inactive, then writes | — | 4.4.2.3, 9.4.1, 9.4.5 | P1 | in force (SCHEDULER_SPEC.md 5, 8) |
| C6 | A table of the orders of the forms of `#0`, fixed by tests | — | 4.4.2.3 | P1 | approved 2026-10-19 (8: region_order) |
| C7 | An NBA in a combinational block stays an NBA | — | 9.2.2.2 | P1 | in force (as written; no rule changes it) |
| C7 | Warn about it | `kirima-comb-nba` | 9.2.2.2 | P1 | approved 2026-10-19 |
| C8 | An NBA in `final` takes no effect, as no region runs after `final` | — | 9.2.3 | P1 | in force (SCHEDULER_SPEC.md 6.2) |
| C8 | Warn about it | `kirima-final-nba` | 9.2.3 | P1 | approved 2026-10-19 |

### 6.4 D: event controls and sensitivity

| ID | Handling | Diagnostics and options | Clause | Phase | Status |
| --- | --- | --- | --- | --- | --- |
| D1 | Edges follow the table of the standard, decided by one function | — | 9.4.2 | P1 | in force (VALUE_SEMANTICS.md 4.3) |
| D2 | In 4 states, a change from the initial x to 0 or 1 is an edge, and processes that start with an edge or event wait are waiting before anything runs at time 0, so they see it | — | 9.4.2 | P1 | approved 2026-10-19 (SCHEDULER_SPEC.md 6.1) |
| D2 | With `--state=2` there is no x, so these edges do not happen; written as a difference | — | 6.11 | P2 | later (P2) |
| D3 | A change in the middle of a time is seen only by the processes waiting then; island optimization keeps it | — | 9.4.2, 4.7 | P1 | in force (SCHEDULER_SPEC.md 9.1) |
| D3 | Waveforms record the value at the end of each time slot; an option records every change | `--wave-glitches` | 4.7 | P1 | approved 2026-10-19 (CLI.md 2.2) |
| D4 | The sensitivity of `always_comb` follows called functions, package variables and hierarchical references | — | 9.2.2.2.1 | P1 | in force (SCHEDULER_SPEC.md 6.3) |
| D4 | Reads not known statically (through handles or virtual interfaces) are left out of the sensitivity, with a warning | `kirima-comb-dynamic-read` | 9.2.2.2.1 | P3 | later (P3) |
| D5 | Longest static prefixes: `a[i]` with a variable `i` subscribes to all of `a` and to `i`; a constant index to its bit range | — | 11.5.3 | P1 | in force (SCHEDULER_SPEC.md 6.3, 9.5) |
| D6 | The differences of `always @*` and `always_comb` | — | 9.2.2.2.2 | P1 | in force (SCHEDULER_SPEC.md 6.2, 6.3, 9.2) |
| D7 | `iff` is evaluated when the edge is found; compound expressions use monitor records | — | 9.4.2.1, 9.4.2.3 | P1 | in force (SCHEDULER_SPEC.md 9.1, 9.5) |
| D8 | `triggered` stays true to the end of the time slot | — | 15.5.1, 15.5.3 | P1 | in force (SCHEDULER_SPEC.md 9.4) |
| D8 | A run-time warning when `@e` starts to wait on an event already triggered in the same time slot (suggesting `wait (e.triggered)`); not given when the process woken by that trigger waits again | `kirima-event-missed` | 15.5.1 | P1 | approved 2026-10-19 |
| D9 | `->>` triggers in the NBA region | — | 15.5.2 | P1 | in force (SCHEDULER_SPEC.md 9.4) |
| D9 | `wait_order` with several events at one time follows the order they were triggered | — | 15.5.4 | P3 | later (P3) |
| D10 | A virtual interface in a sensitivity list, where the standard forbids it, is an error (not confirmed) | `kirima-vif-in-sensitivity` | 25.9 | P3 | later (P3) |

`kirima-event-missed` is given for the typical race where `-> e` and `@e` happen at the same time and the default order put `-> e` first. Whether correct code ever gets it is to be checked on real designs (not confirmed).

### 6.5 E: 4 states and x

| ID | Handling | Diagnostics and options | Clause | Phase | Status |
| --- | --- | --- | --- | --- | --- |
| E1 | An unknown condition of `if` or `case` is not true | — | 12.4, 12.5 | P1 | in force (VALUE_SEMANTICS.md 4.1, 4.2) |
| E1 | An option reports conditions and case expressions that had x or z, once per place, with a count at the end | `--cond-x` | 12.4, 12.5 | P1 | approved 2026-10-19 (CLI.md 2.2; SIR_SPEC.md 5.2 `cond.x`) |
| E2 | The conditional operator merges both sides on an unknown condition | — | 11.4.11 | P1 | in force (VALUE_SEMANTICS.md 3.9) |
| E2 | `--cond-x` covers its condition too | `--cond-x` | 11.4.11 | P1 | approved 2026-10-19 (VALUE_SEMANTICS.md 3.9) |
| E3 | The x rules of each operator are set by `valsem` and tested in full | — | 11.4 | P1 | in force (VALUE_SEMANTICS.md 3) |
| E4 | Sources of x as the standard says; an out-of-range read of a 2-state type is 0 | — | 7.4.6, 11.4.3 | P1 | in force (VALUE_SEMANTICS.md 1.2, 3.11) |
| E5 | `--state=2` is an approximation and not a reference for expected values; `--tier-check` compares runs of the same `--state` | `--state`, `--tier-check` | 6.11 | P2 | later (P2) |
| E6 | `casex` as the standard says | — | 12.5.1 | P1 | in force (VALUE_SEMANTICS.md 4.2) |
| E6 | Warn, suggesting `casez` or `case ... inside` | `kirima-casex` | 12.5.1 | P1 | approved 2026-10-19 |
| E7 | Strengths and resolution; `trireg` is unsupported | — | 6.6, 28 | P1 | in force (VALUE_SEMANTICS.md 5) |
| E8 | Several drivers on a `uwire` are an error (slang's) | — | 6.6.2 | P1 | in force (VALUE_SEMANTICS.md 5.5) |
| E9 | Initial values as the standard says by default; an option sets the initial value of 4-state variables and memories that have none, and a run with a non-default value says so at its start | `--init-values` | 6.8 | P1 | approved 2026-10-19 (CLI.md 2.2, 4; VALUE_SEMANTICS.md 1.4) |

Used together, `--cond-x=warn` and `--init-values=one` make a reset that x optimism hides easier to find. Neither is the default, so a default run does not change in result or speed.

### 6.6 F: widths and signs of expressions

| ID | Handling | Diagnostics and options | Clause | Phase | Status |
| --- | --- | --- | --- | --- | --- |
| F1 | Context-determined widths follow slang's typing | slang's warnings | 11.6, 11.8 | P1 | in force (VALUE_SEMANTICS.md 2) |
| F1 | Tier 2 computes widths over 64 bits exactly | — | 11.6 | P2 | later (P2) |
| F2 | Mixed signedness follows slang's typing | slang's warnings | 11.7, 11.8.1 | P1 | in force (VALUE_SEMANTICS.md 2) |
| F3 | Shift amounts as the standard says; generated code handles amounts at or over the width with a test | — | 11.4.10 | P1 | in force (VALUE_SEMANTICS.md 3.8); generated code later (P2) |
| F4 | Division and modulus by 0 give x; generated code tests the division of INT64_MIN by -1 too | — | 11.4.3 | P1 | in force (VALUE_SEMANTICS.md 3.2); generated code later (P2) |
| F5 | The special cases of the power operator | — | 11.4.3 | P1 | in force (VALUE_SEMANTICS.md 3.2) |
| F6 | Real to integer rounds to the nearest, half away from zero | — | 6.12, 6.24.1 | P1 | in force (VALUE_SEMANTICS.md 1.5) |
| F7 | Out-of-range reads are x (0 for 2-state types) and writes do nothing; a part partly out of range writes its bits in range | — | 7.4.6, 11.5.1 | P1 | in force (VALUE_SEMANTICS.md 3.11) |
| F7 | Warn about a constant index out of range; an option reports run-time indices out of range, once per place | `kirima-const-out-of-range`, `--bounds-check` | 11.5.1 | P1 | approved 2026-10-19 (CLI.md 2.2; VALUE_SEMANTICS.md 3.11) |
| F8 | `+:`, `-:` and ascending ranges follow slang | slang's warnings | 11.5.1 | P1 | in force (VALUE_SEMANTICS.md 3.11) |
| F9 | A replication count of 0 follows slang | slang's diagnostics | 11.4.12.1 | P1 | in force (VALUE_SEMANTICS.md 7.2) |

### 6.7 G: short-circuiting and side effects

The presence, number and order of evaluations of expressions with side effects are not changed by optimization. Where the standard does not set the order, it is left to right.

| ID | Handling | Diagnostics and options | Clause | Phase | Status |
| --- | --- | --- | --- | --- | --- |
| G1 | The right side is not evaluated when the left decides the result | — | 11.3.5, 11.4.7 | P1 | in force (VALUE_SEMANTICS.md 3.1; SIR_SPEC.md 5.2) |
| G1 | Turning a logical operation into a bitwise one only when the right side is proved free of side effects and of null references | — | 11.3.5 | P2 | later (P2; CODEGEN_SPEC.md "Rules of C++ generation") |
| G2 | Optimization does not change how many times an expression with side effects is evaluated | — | 11.3.5 | P1 | in force (VALUE_SEMANTICS.md 3.1) |
| G2 | Warn about `++` or a function with side effects inside an index or part-select | `kirima-side-effect-in-index` | 11.3.5 | P1 | approved 2026-10-19 |
| G3 | Arguments are evaluated left to right | — | 11.3.5 | P1 | approved 2026-10-19 (VALUE_SEMANTICS.md 3.1) |
| G3 | Generated code fixes the order with temporaries, as C++ does not set the order of arguments | — | 11.3.5 | P2 | later (P2; CODEGEN_SPEC.md "Rules of C++ generation") |
| G4 | Operands are evaluated left to right | — | 11.4.2 | P1 | approved 2026-10-19 (VALUE_SEMANTICS.md 3.1) |
| G4 | Warn when one expression writes the same variable twice or more | `kirima-multi-side-effect` | 11.4.2 | P1 | approved 2026-10-19 |
| G4 | A blocking assignment with `=` evaluates its right side before its left side; an assignment operator such as `+=` evaluates its left side first | — | 10.4.1, 11.4.1 | P1 | approved 2026-10-19 (VALUE_SEMANTICS.md 3.1) |

### 6.8 H: lifetimes of tasks and functions

| ID | Handling | Diagnostics and options | Clause | Phase | Status |
| --- | --- | --- | --- | --- | --- |
| H1 | Static and automatic as the standard says (slang decides) | — | 6.21, 13.3.2 | P1 | in force (SIR_SPEC.md 3.3) |
| H1 | A run-time warning, once per task, when a static task is called again while an earlier call waits | `kirima-static-task-reentry` | 13.3.2 | P1 | approved 2026-10-19 |
| H2 | Loop variables inside `fork` as the standard says | — | 9.3.2 | P1 | in force (SCHEDULER_SPEC.md 10.2) |
| H2 | Warn when the body of `join_none` or `join_any` reads a loop variable its parent changes, not through an automatic copy | `kirima-fork-loop-var` | 9.3.2 | P1 | approved 2026-10-19 |
| H3 | A parent's frame that a child refers to is kept until the child ends (frames are GC roots) | — | 6.21, 9.3.2 | P3 | later (P3; RUNTIME_ABI.md 8) |
| H4 | Reading or writing through a `ref` whose element of a queue or dynamic array was removed or moved during a wait is a run-time error (not confirmed) | `kirima-ref-stale` | 13.5.2 | P3 | later (P3) |
| H5 | A timing control inside a function is an error, with no compatibility exception | slang's diagnostic | 13.4 | P1 | in force (slang) |
| H6 | A disabled task does not copy back its output and inout arguments | — | 9.6.2 | P1 | in force (SCHEDULER_SPEC.md 10.4) |

### 6.9 I: process control and ending

| ID | Handling | Diagnostics and options | Clause | Phase | Status |
| --- | --- | --- | --- | --- | --- |
| I1 | Disabling a task by name ends every running call of it | — | 9.6.2 | P1 | in force (SCHEDULER_SPEC.md 10.4) |
| I2 | A killed process is removed from subscriptions, monitor records and event waits | — | 9.7 | P1 | in force (SCHEDULER_SPEC.md 10.7) |
| I2 | And from the waits of semaphores and mailboxes | — | 15.3, 15.4 | P3 | later (P3; SCHEDULER_SPEC.md 10.7) |
| I3 | `wait fork` waits for direct children; `disable fork` ends every descendant | — | 9.6.1, 9.6.3 | P1 | in force (SCHEDULER_SPEC.md 10.3, 10.5) |
| I4 | `suspend` and `resume` | — | 9.7 | P3 | in force (SCHEDULER_SPEC.md 10.6); implemented in P3 |
| I5 | `final` runs on every ending but the second SIGINT and an internal error | — | 9.2.3 | P1 | in force (SCHEDULER_SPEC.md 16.2) |
| I6 | Exit codes gain 6 for the resource limits | Reason resource_limit | 20.2 | P1 | approved 2026-10-19 (SCHEDULER_SPEC.md 16.2; CLI.md 5) |
| I6 | The argument of `$finish` changes only what is printed; `$stop` prints its line and ends with code 0 | — | 20.2 | P1 | in force (SCHEDULER_SPEC.md 16.2; SYSTEM_TASKS.md 10.2) |
| I6 | An option chooses what `$stop` does | `--stop` | 20.2 | P1 | approved 2026-10-19 (SCHEDULER_SPEC.md 16.2; CLI.md 2.2) |
| I7 | No interactive mode | — | — | P1 | in force (SCHEDULER_SPEC.md 16.2) |
| I8 | A limit on wall-clock time, besides the limit on simulated time | Reason wall_time_limit, code 4. `--max-wall-time` | — | P1 | approved 2026-10-19 (SCHEDULER_SPEC.md 16.2; CLI.md 2.7) |

### 6.10 J: classes and memory

| ID | Handling | Diagnostics and options | Clause | Phase | Status |
| --- | --- | --- | --- | --- | --- |
| J1 | Precise mark-and-sweep GC | GC counts and pause times in `--stats=json` | 8.29 | P3 | later (P3; RUNTIME_ABI.md 6) |
| J2 | A null reference is a run-time error (reason error, code 2), with the chain of SystemVerilog calls | `kirima-null-deref` | 8.4 | P3 | in force for the reason and code (SCHEDULER_SPEC.md 16.2); later (P3) for the rest |
| J3 | Static initializers are evaluated in declaration order within a scope, packages and compilation units first in source order; an initial value that may read a static variable before it is set gets a warning | `kirima-static-init-order` | 6.8, 8.9, 26.3 | P1 | in force (SCHEDULER_SPEC.md 3, 6.1; SIR_SPEC.md 7.1); the warning's details approved 2026-10-19 |
| J4 | Which method a virtual call inside a constructor reaches is checked in the standard and fixed (not confirmed) | slang's diagnostics | 8.15 | P3 | later (P3) |
| J5 | Specializations of parameterized classes follow slang; static members are per specialization | — | 8.25 | P3 | later (P3) |
| J6 | `weak_reference` becomes null after marking if its object did not survive | — | the clause is not confirmed | P3 | later (P3; RUNTIME_ABI.md 6) |
| J7 | Conversion between strings and integers as the standard says | — | 6.16 | P1 | in force (VALUE_SEMANTICS.md 7.4, 7.6) |
| J7 | Writing a string through VPI honours the delay mode | — | 6.16, 38 | P4 | later (P4) |

### 6.11 K: random numbers and constraints

The same seed gives the same result, across versions too. Only `$random` and `$dist_*`, whose computation the standard sets, give the same sequences as other simulators.

| ID | Handling | Diagnostics and options | Clause | Phase | Status |
| --- | --- | --- | --- | --- | --- |
| K1 | How seeds are given out (the global seed, the order processes and objects are made, hierarchical names) is fixed in RANDOMIZE_SPEC.md and kept across versions; a change raises the major version | — | 18.14, 18.14.1 | P3 | later (P3) |
| K1 | The seed is written at the start of a run | — | 18.14 | P1 | in force (CLI.md 4) |
| K2 | Each process and object has its own random state | — | 18.14.2, 18.14.3 | P3 | later (P3) |
| K3 | `$random` and `$dist_*` give the results of the algorithms of Annex N | — | 20.15, Annex N | P1 | in force (VALUE_SEMANTICS.md 8; written in M7 with the random-number section of SYSTEM_TASKS.md) |
| K3 | `$urandom` uses kirimasim's own algorithm, kept across versions | — | 18.13 | P1 | approved 2026-10-19 |
| K4 | The same seed gives the same solution; `randc` repeats nothing within its cycle; unsupported constraints are errors | `kirima-constraint-unsupported` | 18.4.2, 18.5 | P3 | later (P3) |
| K5 | A contradiction makes `randomize()` return 0 and leave the variables (not confirmed), with the constraints involved | `kirima-randomize-failed`, `--randomize-fail` | 18.6.3 | P3 | later (P3) |
| K6 | A seed function for DPI-C, as an extension of svdpi.h (name not decided) | `kirima_dpi_seed()` | — | P3 | later (P3) |

### 6.12 L: assertions

| ID | Handling | Diagnostics and options | Clause | Phase | Status |
| --- | --- | --- | --- | --- | --- |
| L1 | Preponed samples are the values at the start of the time slot | — | 16.5.1, 4.4.2.1 | P3 | later (P3) |
| L2 | When `disable iff` is evaluated is checked in the standard (not confirmed) | — | 16.12 | P3 | later (P3) |
| L3 | Deferred immediate assertions and unique and priority use pending queues and flush points | — | 16.4, 12.4.2.1, 12.5.3 | P1 | in force (SCHEDULER_SPEC.md 15) |
| L4 | Attempts not finished at the end are not failures by default, and their number is printed | `--assert-incomplete` | 16 | P3 | later (P3) |
| L5 | Action blocks run in Reactive; the value of `$past` before the start is checked in the standard (not confirmed) | — | 16.9.3, 16.14 | P3 | later (P3) |
| L6 | Vacuous successes are counted apart from successes | `kirima-never-match` | 16.13, 16.14 | P3 | later (P3) |

### 6.13 M: VPI and DPI

| ID | Handling | Diagnostics and options | Clause | Phase | Status |
| --- | --- | --- | --- | --- | --- |
| M1 | `cbReadWriteSynch` is called in Post-NBA, and writes there make the active side run again; `cbReadOnlySynch` allows no writes | — | 4.10, 38 | P4 | in force (SCHEDULER_SPEC.md 17.2, 2.2); implemented in P4 |
| M2 | Delay modes of `vpi_put_value` | — | 38 | P4 | later (P4) |
| M3 | A callback made by a write inside a callback is queued and called after the present one returns | — | 38 | P4 | later (P4) |
| M4 | Calling an export or `svGetScope` from an import that is not `context` is a run-time error; `pure` calls are not left out | `kirima-dpi-not-context` | 35.5.3, Annex H | P1 | approved 2026-10-19 (with DPI-C import, M7) |
| M5 | A time wait inside a DPI-C task is a run-time error until fibers come | `kirima-dpi-task-wait` | 35.9 | P1 | approved 2026-10-19; fibers later (P3) |
| M6 | `$finish` inside an export, and how long 4-state arguments and open arrays stay valid | — | Annex H, 20.2 | P3 | later (P3; RUNTIME_ABI.md 8) |

### 6.14 N: hierarchy and connections

| ID | Handling | Diagnostics and options | Clause | Phase | Status |
| --- | --- | --- | --- | --- | --- |
| N1 | Ports are collapsed only where no difference can be observed | — | 23.3.3 | P1 | in force (SCHEDULER_SPEC.md 5; SIR_SPEC.md 3.5) |
| N1 | `--report-state-opt` gives how many were collapsed and why others were not | `--report-state-opt` | 23.3.3 | P2 | later (P2) |
| N2 | `force` and `release` as the standard says | — | 10.6.2 | P1 | in force (SCHEDULER_SPEC.md 14) |
| N3 | Implicit nets follow `` `default_nettype `` | slang's warnings | 6.10, 22.8 | P1 | in force (slang) |
| N4 | Hierarchical and upward names are resolved by slang | — | 23.6, 23.8 | P1 | in force (slang; SIR_SPEC.md 11) |
| N5 | `defparam` is left to slang | slang's diagnostics | 23.10 | P1 | in force (slang) |
| N6 | Unnamed generate blocks take the names slang gives (`genblk`); VPI and waveforms use them too | — | 27.6, 23.11 | P1 | in force (SIR_SPEC.md 1) |
| N7 | `inout` connections are resolution points | — | 23.3.3, 28 | P1 | in force (VALUE_SEMANTICS.md 5.3); switches later (P4) |
| N8 | A modport direction broken through a virtual interface and not found at elaboration is a run-time warning (not confirmed) | `kirima-modport-violation` | 25.5, 25.9 | P3 | later (P3) |
| N9 | Without `--top`, one config in the sources gives the top; with more than one, the standard does not say which is meant, so it is an error that asks for `--top` | `--top` | 33.5.4 | P1 | approved 2026-10-19 (CLI.md 2.1) |
| N10 | An unconnected variable input port under `` `unconnected_drive `` is not pulled: it keeps the initial value of its type (23.3.3.2), with a lint warning. 22.9 speaks of all unconnected input ports, but 23.3.3.2 sets the value of an unconnected variable input port, and a pull is a driver with a strength, which a variable does not have | `kirima-unconnected-drive-var` | 22.9, 23.3.3.2 | P1 | approved 2026-10-19 (VALUE_SEMANTICS.md 5.3) |

### 6.15 O: timescales and time

| ID | Handling | Diagnostics and options | Clause | Phase | Status |
| --- | --- | --- | --- | --- | --- |
| O1 | Mixed and missing timescales; `--timescale` sets the default | slang's diagnostics | 3.14, 22.7 | P1 | in force (SCHEDULER_SPEC.md 1; CLI.md 2.1) |
| O2 | Delays are rounded as the standard says | — | 3.14.3 | P1 | in force (SCHEDULER_SPEC.md 1) |
| O2 | Warn about a constant delay that rounds to 0; a run-time warning, once per place, for a delay expression that does | `kirima-delay-rounded-zero` | 3.14.3 | P1 | approved 2026-10-19 |
| O3 | A time past the largest time is a run-time error (reason error, code 2) | `kirima-time-overflow` | 20.3 | P1 | in force (SCHEDULER_SPEC.md 8, 16.2) |
| O4 | x, z and negative delays | — | 9.4.1 | P1 | in force (SCHEDULER_SPEC.md 8) |
| O4 | A run-time warning, once per place, for a delay with x or z, and for a negative one | `kirima-delay-unknown`, `kirima-delay-negative` | 9.4.1 | P1 | approved 2026-10-19 |
| O5 | `#1step` is one step of the smallest precision | — | 14.4 | P1 | in force (SCHEDULER_SPEC.md 1) |

### 6.16 P: files and display

| ID | Handling | Diagnostics and options | Clause | Phase | Status |
| --- | --- | --- | --- | --- | --- |
| P1 | Formats of `$display` as the tables of the standard | — | 21.2.1 | P1 | in force (SYSTEM_TASKS.md 3) |
| P2 | `$monitor` prints in Postponed when a value changed; only the last one called is active | — | 21.2.3 | P1 | in force (SYSTEM_TASKS.md 4) |
| P3 | SystemVerilog's output and DPI-C's `printf` share one standard output and keep their order | — | 21.3 | P1 | in force (SYSTEM_TASKS.md 2) |
| P3 | When output is written out, and an option that writes it out after every display task | `--autoflush` | 21.3 | P1 | approved 2026-10-19 (SYSTEM_TASKS.md 2; CLI.md 2.2) |
| P4 | `$readmemh` leaves words the file does not give unchanged | — | 21.4 | P1 | in force (SYSTEM_TASKS.md 7.1) |
| P4 | An address in the file outside the range is an error and reading stops; without a finish address the range ends at the highest address of the memory, and without a start address it begins at the lowest | — | 21.4 | P1 | in force (SYSTEM_TASKS.md 7.1); the range without a finish address approved 2026-10-19 |
| P4 | Run-time warnings for a range or a word count that does not match | `kirima-readmem-range`, `kirima-readmem-short` | 21.4 | P1 | approved 2026-10-19 |
| P5 | `$dumpvars` can only dump slots kept by the build; asking for others lists them | `kirima-dump-not-preserved` | 21.7 | P2 | later (P2) |

### 6.17 Q: determinism and parallelism

| ID | Handling | Diagnostics and options | Clause | Phase | Status |
| --- | --- | --- | --- | --- | --- |
| Q1 | Only static islands with no side effects run in parallel; processes with `$display`, DPI-C, random numbers or `$finish` run on the main thread in the default order | — | 4.5 | P4 | later (P4; RUNTIME_ABI.md 8) |
| Q2 | No container with an unset order is walked where it affects results | — | — | P1 | in force (CLAUDE.md; SIR_SPEC.md 1) |
| Q2 | The same input gives the same C++ (NFR-6), checked in CI by generating twice | — | — | P2 | later (P2; CODEGEN_SPEC.md "Rules of C++ generation") |
| Q3 | The kernel and generated code are built without `-ffast-math`, and with `-ffp-contract=off` so that no multiply and add are fused; math functions are left to libm (an exception to NFR-2) | `tools/check_release_flags.py` | 6.12 | P1 | approved 2026-10-19 (CLAUDE.md; CODEGEN_SPEC.md "Floating point") |

### 6.18 R: other items and UVM

| ID | Handling | Diagnostics and options | Clause | Phase | Status |
| --- | --- | --- | --- | --- | --- |
| R1 | `always_latch` as the standard says | — | 9.2.2.3 | P1 | in force (SCHEDULER_SPEC.md 6.2) |
| R1 | Warn when `always_comb` makes a latch, and when `always_latch` makes none | `kirima-latch-inferred`, `kirima-no-latch` | 9.2.2.3 | P1 | approved 2026-10-19 |
| R2 | `assign` and `deassign` are a layer below `force` | — | 10.6.1 | P1 | in force (SCHEDULER_SPEC.md 14) |
| R3 | Preprocessor expressions are left to slang | slang's diagnostics | 22.6 | P1 | in force (slang) |
| R4 | UVM's non-standard dependencies: DPI-C functions of kirimasim's own, `uvm_hdl_*` through the name table and `force`; `$psprintf` as `$sformatf`, with a warning | `kirima-nonstandard` | — | P3 | later (P3) |
| R5 | Mixed language versions are left to slang (`` `begin_keywords ``) | — | 22.14 | P1 | in force (CLI.md 2.1) |

A value written by `uvm_hdl_deposit` wakes processes as a write by `vpiNoDelay` does (M2; later (P4)).

## 7. Options, reasons for ending and diagnostics

### 7.1 Options

The meaning, default and stage of each option are in CLI.md. None changes the result of a default run.

| Option | Items | Where | Status |
| --- | --- | --- | --- |
| `--max-scc-iter`, `--max-zero-delay`, `--max-slot-events`, `--max-steps` | A1, A6, A9, A14 | CLI.md 2.6 | in force |
| `--max-procs`, `--max-call-depth`, `--max-wall-time` | A5, A15, I8 | CLI.md 2.7 | approved 2026-10-19 |
| `--stop`, `--init-values`, `--cond-x`, `--bounds-check`, `--autoflush`, `--mintypmax`, `--report-pulses`, `--wave-glitches` | I6, E9, E1, E2, F7, P3, C5, D3 | CLI.md 2.2 | approved 2026-10-19 |
| `--max-hierarchy-depth`, `--max-generate-steps`, `--max-constexpr-depth`, `--max-constexpr-steps` (slang's) | A11, A12 | CLI.md 2.1 | approved 2026-10-19 |
| `--race-nba`, `--randomize-fail`, `--assert-incomplete` | B1, K5, L4 | CLI.md 2.3 (options of P2 and later) | approved 2026-10-19 (P2 and later) |

### 7.2 Reasons for ending

The reasons and exit codes are in SCHEDULER_SPEC.md 16.2 and CLI.md 5.

| Reason | Code | Items | Status |
| --- | --- | --- | --- |
| resource_limit | 6 | A5, A15 | approved 2026-10-19 |
| wall_time_limit | 4 | I8 | approved 2026-10-19 |
| fatal, error, zero_time_limit | 1, 2, 5 | A1, A6, A9, A13, A14, J2, O3 | in force |

### 7.3 Diagnostics

Names as in 4. The status of each follows its row in 6; the names themselves are fixed in M7.

| Kind | Names (items) |
| --- | --- |
| Elaboration error | `kirima-vif-in-sensitivity` (D10), `kirima-constraint-unsupported` (K4). Several writers of `always_comb` and the like, and several drivers of a `uwire`, are slang's (B4, E8) |
| Lint warning | `kirima-zero-delay-loop` (A1), `kirima-always-no-wait`, `kirima-always-no-wait-path` (A2), `kirima-empty-sensitivity` (A3), `kirima-const-wait` (A4), `kirima-comb-loop` (A6), `kirima-self-trigger` (A9), `kirima-shared-loop-var` (A10), `kirima-monitor-side-effect` (A13), `kirima-race-blocking` (B3, B6), `kirima-multi-write` (B4), `kirima-blk-nblk-mix` (B5), `kirima-comb-read-before-write` (B12), `kirima-comb-nba` (C7), `kirima-final-nba` (C8), `kirima-comb-dynamic-read` (D4), `kirima-casex` (E6), `kirima-const-out-of-range` (F7), `kirima-side-effect-in-index` (G2), `kirima-multi-side-effect` (G4), `kirima-fork-loop-var` (H2), `kirima-static-init-order` (J3), `kirima-never-match` (L6), `kirima-unconnected-drive-var` (N10), `kirima-delay-rounded-zero` (O2), `kirima-latch-inferred`, `kirima-no-latch` (R1), `kirima-nonstandard` (R4) |
| Note | `kirima-comb-loop-unsplit` (A7), `kirima-const-wait` for `wait(0)` (A4), removed pulses (C5) |
| Run-time warning | `kirima-event-missed` (D8), `kirima-cond-x` (E1, E2), `kirima-out-of-range` (F7), `kirima-static-task-reentry` (H1), `kirima-randomize-failed` (K5), `kirima-modport-violation` (N8), `kirima-delay-unknown`, `kirima-delay-negative` (O4), `kirima-readmem-range`, `kirima-readmem-short` (P4), `kirima-dump-not-preserved` (P5) |
| Run-time error | `kirima-readonly-write` (A13), `kirima-ref-stale` (H4), `kirima-dpi-not-context` (M4), `kirima-dpi-task-wait` (M5), `kirima-null-deref` (J2), `kirima-time-overflow` (O3) |
| Run-time fatal | `kirima-zero-time-loop` (A1, A9, A14), `kirima-comb-not-converged` (A6), `kirima-process-limit` (A5), `kirima-call-depth-limit` (A15) |

## 8. Test groups

Every choice of this document is fixed by tests of the suite. Where the standard does not set the result, the oracle `choice` marks kirimasim's choice (TEST_SUITE.md, approved 2026-10-19). Limits and diagnostics are checked by `:expected_exit:` and `:expected_stderr:`; once the IDs are fixed (M7), by the output of `--diag-format=json`. The generators that make these groups are named in TEST_SUITE.md, "What is added during P1".

| Group | Items | Source of expected values | Content | Phase |
| --- | --- | --- | --- | --- |
| zero_time | A1, A2, A4, A5, A6, A9, A14, A15 | spec (the reason for ending and the diagnostic) | `forever #0;`, `forever begin end`, an inverting combinational loop, an NBA that wakes itself, endless `join_none`, endless recursion | P1 |
| time0_order | B8, B9, B10, D2, J3 | choice | The falling edge of a reset with `initial` and `always` in either order, the initial value of a continuous assignment with a delay, static initializers that depend on each other | P1 |
| region_order | B1, B2, B11, C1, C2, C6 | choice, spec | Several NBAs at one time, the orders of the forms of `#0`, `$finish` at one time, no switch inside a statement | P1 |
| race_detect | B3, B6, D7, D8 | spec (that `--race-check` reports the race) | Races of blocking assignments, `->` and `@` at one time | P2 |
| sensitivity | A10, B12, D4, D5, D6 | spec | Reads inside functions, package variables, indices, read before write, the differences from `always @*` | P1 |
| delay | C3, C4, C5, O2, O4 | spec | Indices of intra-assignment delays, inertial delays and no cancelling, rounding to 0, delays with x | P1 |
| x_semantics | E1 to E4, E6, F3 to F7 | spec, iverilog | x in conditions, x of each operator, out of range, the power operator, rounding of reals | P1 |
| side_effect | G1 to G4 | spec, choice | Short-circuiting and null, `++` in an index, the order of arguments | P1 |
| lifetime | H1 to H6, I1 to I4 | spec | Calls of a static task at once, loop variables in `fork`, disable, cleaning up after kill, `wait fork` | P1, P3 |
| random | K1 to K5 | spec, choice | The same sequence for the same seed, random stability, `$random` against Annex N, contradictory constraints | P1, P3 |
| assertion | L1 to L6 | spec | When samples are taken, `disable iff`, unfinished attempts, vacuous successes | P3 |
| vpi_dpi | M1 to M6, J7 | cocotb's tests, spec | When callbacks are called, delay modes, re-entry, `context` | P1, P4 |
| output | P1 to P5 | iverilog, spec | Formats, `$monitor`, the order of output, `$readmemh` | P1 |
| determinism | Q1 to Q3 | checks in CI | Runs with different thread counts, two generations compared, floating-point flags | P0, P4 |

## 9. Differences from the first proposal

This document's first proposal differed from the approved specifications in these four points. On 2026-10-19 a person chose for each what is written below.

1. **The order of evaluating static initializers (J3; SCHEDULER_SPEC.md 6.1 step 1).** The first proposal was the order of dependency (declaration order where there is none), with a warning and declaration order for a cycle. The approved rule is declaration order within a scope, packages and compilation units first in source order (SCHEDULER_SPEC.md 3, 6.1; SIR_SPEC.md 7.1). The standard sets no order among them (6.8, 10.5), and asks that a package be compiled before the scopes that read it (26.3). **Chosen on 2026-10-19: the approved rule.** Other simulators differ here too (Icarus Verilog 14.0 (devel) follows declaration order, Verilator 5.052 the order of dependency), so what matters is the warning the approved rule gives when an initial value may read a static variable before it is set (`kirima-static-init-order`; SCHEDULER_SPEC.md 3).
2. **An `always` with no timing control on any path (A2).** The first proposal made it an elaboration error. The approved rule runs it and stops it at `--max-steps` (SCHEDULER_SPEC.md 6.2, 16.2). The standard describes such an `always` as looping forever at one time, without making it illegal (9.2.2.1). **Chosen on 2026-10-19: the approved rule, with a lint warning.** The standard allows it, so it is not rejected (principle 1). It runs and stops at `--max-steps` as before (principle 7), and the lint warning `kirima-always-no-wait` tells of it at elaboration (principle 8).
3. **Writes by a function called from the arguments of `$monitor` or `$strobe` (A13).** The first proposal made every such write an error. The approved interpretation lets the function write its own arguments and its automatic and static variables (SCHEDULER_SPEC.md 2.2). The standard forbids writing nets and variables in the Postponed region (4.4.2.9). **Chosen on 2026-10-19: the approved interpretation.** Making every write an error would forbid calling a function that keeps its own work in its variables, which `$monitor` and `$strobe` allow (21.2.2, 21.2.3). The lint warning `kirima-monitor-side-effect` tells of a function with side effects there.
4. **An address out of range in `$readmemb` and `$readmemh` (P4).** The first proposal ignored it. The approved rule makes an address in the file outside the range given by the arguments an error that stops the reading (SYSTEM_TASKS.md 7.1). An address outside the memory's declared range when no range is given was set by neither. **Chosen on 2026-10-19: the approved rule, with the memory's bounds where the arguments give none.** Without a finish address the range ends at the highest address of the memory, and without a start address it begins at the lowest, so an address outside the memory is the same error (SYSTEM_TASKS.md 7.1). Icarus Verilog 14.0 (devel) does the same; Verilator 5.052 ends the run at an address or a word past the memory.

## 10. Not confirmed

- Which diagnostics of 7.3 the pinned slang already gives (4). To check: list slang's diagnostics in M7 and compare.
- Whether correct code ever gets `kirima-event-missed` (D8). To check: real designs and UVM test benches in P3.
- The clauses marked "not confirmed" in 6 (D10, H4, J4, J6, K5, L2, L5, N8). To check: read the clauses named in each row.
- The defaults of the limits (`--max-procs` and the others of CLI.md 2.6 and 2.7) on real designs. To check: Ibex, CVA6 and UVM examples in M6 and later (CLI.md 2.6).
- Whether putting `always` before `initial` at time 0 troubles real designs and UVM test benches. To check: the same runs.
- How a zero-delay `vpiInertialDelay` is queued (M2). To check: chapter 38 and cocotb's tests in P4.
- Whether a compatibility mode is ever wanted (principle 5). To check: users' requests in P4.

## 11. References

Public pages only. The standard's text is not copied; only clause numbers are given.

- Accellera, [IEEE 1800-2023 available through the IEEE GET Program](https://accellera.org/news/press-releases/394-accellera-announces-ieee-1800-2023-standard-available-through-ieee-get-program)
- Verilator: [Warnings](https://verilator.org/guide/latest/warnings.html), [Revision History](https://verilator.org/guide/latest/changes.html), [Input Languages](https://verilator.org/guide/latest/languages.html), [issue #1538 (UVM support)](https://github.com/verilator/verilator/issues/1538)
- Icarus Verilog: [iverilog manual page](https://manpages.ubuntu.com/manpages/noble/man1/iverilog.1.html), [vvp flags](https://steveicarus.github.io/iverilog/usage/vvp_flags.html)
- cocotb: [Timing Model](https://docs.cocotb.org/en/stable/timing_model.html), [Simulator Support](https://docs.cocotb.org/en/stable/simulator_support.html), issues [#448](https://github.com/cocotb/cocotb/issues/448), [#2178](https://github.com/cocotb/cocotb/issues/2178), [#4082](https://github.com/cocotb/cocotb/issues/4082)
- slang: [Command Line Reference](https://sv-lang.com/command-line-ref.html)
- A. Lööw, [The Simulation Semantics of Synthesisable Verilog](https://arxiv.org/abs/2502.19348) (arXiv 2502.19348)
