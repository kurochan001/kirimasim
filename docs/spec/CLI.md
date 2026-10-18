# CLI: the command line

[日本語](CLI.ja.md) · This English text is the main version; the Japanese version follows it, and where they differ, this text holds.

Priority: P1. Status: **approved (2026-10-15)**. On 2026-10-16 the form of `--max-time` and how it stops a run (2.5) and how times are written in the line of the reason for ending (4) were added (approved). The same day, the defaults of the limits on runs that do not advance time (2.6) and slang's language version (2.1) were added (approved). On 2026-10-18, code 2 was made to cover run-time errors of the design (5; approved). The design specification's draft of the "external interface" was arranged by implementation stage, and the points it left open (where output goes, `--dump-sir`, plusargs) were added.

This document sets the form of kirimasim's command line, its options, its exit codes, and the form of the messages kirimasim itself gives. The reasons for ending and the meaning of the codes follow SCHEDULER_SPEC.md 16, and the output of system tasks follows SYSTEM_TASKS.md.

## 1. Form of the command

```text
kirimasim [options] <file>... [+plusarg]...     compile and run (the default)
kirimasim parse [options] <file>...            only parse and elaborate
kirimasim --version | --help
```

- Without a subcommand, kirimasim compiles and runs.
- `parse` stays as it has been since P0 (the test suite's P0 checks use it).
- `build` and `run <model>`, which reuse a compiled model, come in P2, and `lib-create`, which makes a DPI library, in P4 (the design specification's "external interface").

## 2. Options

"Stage" is the order of implementation within P1 (M3a, M4, M7 and so on). Before its stage an option is not accepted (it is an error, as an unknown option).

### 2.1 Input (passed to slang as it is)

- **Language version (approved 2026-10-16):** kirimasim always runs slang with the IEEE 1800-2023 version (slang's `--std=1800-2023`). slang's default is 1800-2017, which in places differs from the 2023 edition of the standard, for example by truncating an unsized decimal literal wider than 32 bits to 32 bits (5.7.1; checked with slang v12.0). There is no option to choose the version (CLAUDE.md: "no hidden knobs").

| Option | Meaning | Stage |
| --- | --- | --- |
| `-f <file>` | Read a list of files | M3a |
| `+incdir+<dir>`, `+define+<name>[=<value>]` | Include directories and macro definitions | M3a |
| `--top=<name>` | The top module. May be given more than once. Without it, slang chooses (the modules no other module refers to) | M3a |
| `-y <dir>`, `+libext+<ext>`, `--libdir=<dir>` | Library search | M7 |
| `-W<name>`, `--compat=<name>` | slang's warnings and compatibility modes | M7 |
| `--timescale=<unit>/<precision>` | The default for files with no timescale (SCHEDULER_SPEC.md 1) | M4 |

### 2.2 Running

| Option | Meaning | Stage |
| --- | --- | --- |
| `--max-time=<time>` | Stop when this time is reached (reason time_limit, code 4). The time has a unit (for example `10us`). Its form and how it stops: 2.5 | M4 |
| `--max-time-ok` | Treat time_limit as code 0 | M4 |
| `--max-scc-iter=<n>`, `--max-zero-delay=<n>`, `--max-steps=<n>` | Limits on runs that do not advance time (reason zero_time_limit, code 5). Their meaning and defaults: 2.6 | M4 |
| `--seed=<n>` | The random seed (the random-number section of SYSTEM_TASKS.md; to be written in P1) | M7 |
| `--wave=<file>.fst\|.vcd`, `--wave-scope=<pattern>` | Waveform output and its scope (the waveform document; to be written in P1) | M7 |

### 2.3 Output and development

| Option | Meaning | Stage |
| --- | --- | --- |
| `--dump-sir` | Make SIR, pass it through the verifier, write it in the text form (SIR_SPEC.md 9) to standard output and end, without running. If the verifier finds an error, write it to standard error and end with code 3 | M3a |
| `--stats=json` | Write the event counters as JSON at the end of standard output when the run ends (PERFORMANCE.md). Not used when the test suite compares standard output | M3a |
| `--log=<file>` | Also write a copy of standard output to a file | M7 |
| `--diag-format=json` | Write errors and warnings to standard error as JSON with an ID and the clause of the standard | M7 |

The options of P2 and later (`--tier`, `--tier-check`, `--state`, `--report-state-opt`, `--race-check`, `--race-seed`, `--public`, `--preserve`, `--disable-pass`, `--cxx`, `--cache-dir`) are added to this document in their stage.

### 2.4 Plusargs

- An argument that starts with `+` and is not `+incdir+`, `+define+` or `+libext+` is a run-time plusarg. They are given, in the order written, to `$test$plusargs` and `$value$plusargs` (SYSTEM_TASKS.md 9).
- A plusarg is not taken as a file name.

### 2.5 The form of `--max-time` and how it stops a run (approved 2026-10-16)

- Form: a decimal number of 0 or more (a fraction is allowed) followed by one of the units `s`, `ms`, `us`, `ns`, `ps`, `fs`, with no space between (for example `10us`, `1.5ns`). Anything else is an error with code 2.
- The value is converted to steps of the design's precision (SCHEDULER_SPEC.md 1). If it is not a whole number of steps, it is rounded away from zero, as delays are (SCHEDULER_SPEC.md 1; code 2 if it does not fit in 64 bits).
- How it stops: every event at that time **or before** runs. When the next event is after that time, the time is set to the value of `--max-time` and the run ends with reason time_limit. `final` runs (SCHEDULER_SPEC.md 16.2).

### 2.6 Limits on runs that do not advance time (approved 2026-10-16)

| Option | What it counts | Default |
| --- | --- | --- |
| `--max-zero-delay=<n>` | Moves of events into the Active or the Reactive region within one time slot (the "moves" of SCHEDULER_SPEC.md 3; they grow with resumptions after `#0`, evaluations after NBA updates and the like) | 1000000 |
| `--max-steps=<n>` | Tier 1 instructions one process runs in a row without waiting (they grow in a loop with no wait, such as `forever begin end`) | 1000000000 |
| `--max-scc-iter=<n>` | Iterations of a static island of tier 2 (P2) | Decided in P2 |

- `0` means no limit.
- Grounds for the defaults (measured 2026-10-16):
  - Tier 1 in the release configuration ran about 350 million instructions a second in a loop with no wait. The dev configuration was about as fast, and the asan configuration about half as fast. Stopping at the default of `--max-steps` takes about 3 seconds in release and about 6 in asan.
  - A million resumptions after `#0` took about 0.04 seconds.
  - `--max-steps` is large so as not to stop, by mistake, a design that initializes a large memory in one loop in an `initial` (16 million words take about 100 million instructions).
- These defaults are reviewed in M6, when sv-tests and real designs are run. Changing them means changing this document, with a human's approval.
- When a limit is reached, one line says which, on standard error, before the line of the reason for ending (4). Forms:
  - `kirimasim: error: time did not advance: <n> moves into the Active region in one time slot (--max-zero-delay=<n>)` (`Reactive` for the reactive set)
  - `kirimasim: error: time did not advance: process "<process name>" ran <n> instructions without waiting (--max-steps=<n>)`
  - The process name is the name of the SIR process (`top.initial0` and the like).

## 3. Where output goes

- **Standard output:** only what the design prints (`$display`, `$write`, `$strobe`, `$monitor` and so on). The test suite compares standard output only (TEST_SUITE.md). The exceptions are the SIR of `--dump-sir` and the JSON of `--stats=json` (written last).
- **Standard error:** every message of kirimasim itself (diagnostics, the version line, the line of the reason for ending and so on; SYSTEM_TASKS.md 2). Standard output is flushed before anything is written to standard error.

## 4. Messages of kirimasim itself

- The form is `kirimasim: <kind>: <text>`, the kind one of `error`, `warning`, `note`, `info`. A message with a source position has the form of slang's diagnostics, `<file>:<line>:<column>: <kind>: <text>`.
- **Version line:** at the start of a run, one line on standard error gives the version, the commit, the options and the seed (so that the same conditions can be reproduced later). Example: `kirimasim: info: kirimasim 0.1.0 (commit 0123456789ab) seed=1 args: --top=tb a.sv`. It is not written to standard output, so the test suite has no need to remove it.
- **Line of the reason for ending:** at the end, one line on standard error gives the reason and the time. Example: `kirimasim: info: finish at 120 ns`. It is not written for `$finish(0)` and `$stop(0)` (SCHEDULER_SPEC.md 16.2).
- **How times are written (approved 2026-10-16):** an integer and a unit separated by one space. The unit is the largest of `s`, `ms`, `us`, `ns`, `ps`, `fs` that gives the time exactly as an integer (for example, with a precision of 1ps, 120000 steps are `120 ns` and 1500 steps are `1500 ps`). Time 0 is written in the unit of the design's precision (`0 ps` for a precision of 1ps, `0 ps` for 10ps). Nothing is rounded, so the number of steps can be read back exactly.

## 5. Exit codes

The same as the table of SCHEDULER_SPEC.md 16.2. The names of the reasons go into `--stats=json` under the same names.

| Code | Reason | Meaning |
| --- | --- | --- |
| 0 | finish, quiescent, stop | `$finish`, no events left, `$stop`. time_limit with `--max-time-ok` is also 0 |
| 1 | fatal | `$fatal`. With the option that counts `$error`, one or more `$error` |
| 2 | error | An error in the command line or the input (syntax errors and unsupported constructs included), or a run-time error of the design (SCHEDULER_SPEC.md 16.2) |
| 3 | internal | An internal error of kirimasim (a failure of the SIR verifier and the like); never an error of the design |
| 4 | time_limit | `--max-time` reached |
| 5 | zero_time_limit | A limit reached without time advancing |
| 130 | interrupted | SIGINT |

- **Unsupported constructs:** during P1 many constructs are not implemented yet. When making SIR meets one, kirimasim gives a diagnostic at its position and ends with code 2 (as row 2 of the design specification's table of exit codes says). It is never ignored silently.
- **Run-time errors of the design (added 2026-10-18, approved):** an error the design makes while it runs ends the run with code 2, with one line on standard error at the statement's position (SCHEDULER_SPEC.md 16.2). Code 3 is only for faults of kirimasim itself.

## 6. Not confirmed, to be written later

- The name and default of the option that counts `$error`.
- The fields of `--diag-format=json` and the scheme of diagnostic IDs (M7; together with RUNTIME_ABI.md's `kirima_status`).
