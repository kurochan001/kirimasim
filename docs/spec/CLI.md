# CLI: the command line

[日本語](CLI.ja.md) · This English text is the main version; the Japanese version follows it, and where they differ, this text holds.

Priority: P1. Status: **approved (2026-10-15)**. On 2026-10-16 the form of `--max-time` and how it stops a run (2.5) and how times are written in the line of the reason for ending (4) were added (approved). The same day, the defaults of the limits on runs that do not advance time (2.6) and slang's language version (2.1) were added (approved). On 2026-10-18, code 2 was made to cover run-time errors of the design (5; approved). The same day, the example of the version line in 4 was made the present form, without the seed (approved). On 2026-10-19, `--max-slot-events` was added to the limits of 2.6 (approved). The same day, the limits on resources (2.2, 2.7) and exit code 6 (5) were added (approved). The same day, the limit on wall-clock time was added to 2.2, 2.7 and 5 (approved). The same day, `--stop` was added to 2.2 and 5 (approved). The same day, `--race-nba` was added to the options of P2 and later (2.3; approved). The same day, `--init-values`, `--cond-x` and `--bounds-check` were added to 2.2, and the line `--init-values` writes to 4 (approved). The same day, `--autoflush` was added to 2.2 (approved). The same day, `--mintypmax`, `--report-pulses` and `--wave-glitches` were added to 2.2 (approved). The same day, `--randomize-fail` and `--assert-incomplete` were added to the options of P2 and later (2.3; approved). The same day, the last process and the latest events in the report of a limit (2.6), and a scheme of names for diagnostics (6), were added (approved). The same day, 6 was made to point to the list of names in UNSPECIFIED_BEHAVIOR.md 7.3 (approved). The same day, slang's limits on elaboration were added to 2.1 (approved). The same day, the top taken from a config in the sources was added to 2.1 (approved). The design specification's draft of the "external interface" was arranged by implementation stage, and the points it left open (where output goes, `--dump-sir`, plusargs) were added.

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
| `--top=<name>` | The top module, or a config (`<name>:config` names a config that has the name of a module too). May be given more than once. Without it: when the sources hold one config, that config, whose design statement names the top cells (33.5.4; added 2026-10-19, approved); when they hold more than one, an error at the second asks for `--top`; otherwise slang chooses (the modules no other module refers to). Configs in files read as libraries are not counted | M3a |
| `-y <dir>`, `+libext+<ext>`, `--libdir=<dir>` | Library search | M7 |
| `-W<name>`, `--compat=<name>` | slang's warnings and compatibility modes | M7 |
| `--max-hierarchy-depth=<n>`, `--max-generate-steps=<n>`, `--max-constexpr-depth=<n>`, `--max-constexpr-steps=<n>` | slang's limits on elaboration: the depth of the instance hierarchy, the steps of generate blocks, and the depth and the steps of constant evaluation. The defaults are slang's (UNSPECIFIED_BEHAVIOR.md 6.1, A11 and A12; approved 2026-10-19) | M7 |
| `--timescale=<unit>/<precision>` | The default for files with no timescale (SCHEDULER_SPEC.md 1) | M4 |

### 2.2 Running

| Option | Meaning | Stage |
| --- | --- | --- |
| `--max-time=<time>` | Stop when this time is reached (reason time_limit, code 4). The time has a unit (for example `10us`). Its form and how it stops: 2.5 | M4 |
| `--max-time-ok` | Treat time_limit as code 0 | M4 |
| `--max-scc-iter=<n>`, `--max-zero-delay=<n>`, `--max-slot-events=<n>`, `--max-steps=<n>` | Limits on runs that do not advance time (reason zero_time_limit, code 5). Their meaning and defaults: 2.6 | M4 |
| `--max-procs=<n>`, `--max-call-depth=<n>` | Limits on resources (reason resource_limit, code 6). Their meaning and defaults: 2.7 (approved 2026-10-19) | M7 |
| `--max-wall-time=<seconds>` | A limit on wall-clock time (reason wall_time_limit, code 4). 2.7 (approved 2026-10-19) | M7 |
| `--stop=finish\|ignore\|error` | What `$stop` does: end (the default), go on, or end with code 1 (SCHEDULER_SPEC.md 16.2; approved 2026-10-19) | M7 |
| `--init-values=x\|zero\|one\|random` | The initial value of the 4-state variables and memories that have no initial value: x (the default, as the standard says), 0, 1, or random bits from `--seed` (VALUE_SEMANTICS.md 1.4; approved 2026-10-19) | M7; `random` after `--seed` |
| `--cond-x=off\|warn\|error` | Report the conditions of `if` and `?:` and the expressions of `case` that had x or z bits: `warn` once per place, with a count at the end; `error` ends the first time, as a run-time error of the design (VALUE_SEMANTICS.md 3.9, 4.1, 4.2; approved 2026-10-19). Default `off` | M7 |
| `--bounds-check=off\|warn` | Report indices and positions out of range at run time, once per place (VALUE_SEMANTICS.md 3.11; approved 2026-10-19). Default `off` | M7 |
| `--autoflush` | Write standard output out after every display task (SYSTEM_TASKS.md 2; approved 2026-10-19). Off by default | M7 |
| `--seed=<n>` | The random seed (the random-number section of SYSTEM_TASKS.md; to be written in P1) | M7 |
| `--wave=<file>.fst\|.vcd`, `--wave-scope=<pattern>` | Waveform output and its scope (WAVEFORM.md 5; `.fst` waits for the decision on libfstapi) | M7 |
| `--wave-glitches` | Record in the waveform every change within a time slot, not only the value at its end (approved 2026-10-19) | M7, with `--wave` |
| `--mintypmax=min\|typ\|max` | Which delay of `min:typ:max` to use. Default `typ` (SCHEDULER_SPEC.md 12; approved 2026-10-19) | M7 |
| `--report-pulses` | Report, as notes, the pulses that inertial delays remove (SCHEDULER_SPEC.md 12; approved 2026-10-19). Off by default | M7 |

### 2.3 Output and development

| Option | Meaning | Stage |
| --- | --- | --- |
| `--dump-sir` | Make SIR, pass it through the verifier, write it in the text form (SIR_SPEC.md 9) to standard output and end, without running. If the verifier finds an error, write it to standard error and end with code 3 | M3a |
| `--stats=json` | Write the event counters as JSON at the end of standard output when the run ends (PERFORMANCE.md). Not used when the test suite compares standard output | M3a |
| `--log=<file>` | Also write a copy of standard output to a file | M7 |
| `--diag-format=json` | Write errors and warnings to standard error as JSON with an ID and the clause of the standard | M7 |

The options of P2 and later (`--tier`, `--tier-check`, `--state`, `--report-state-opt`, `--race-check`, `--race-seed`, `--race-nba` (approved 2026-10-19; SCHEDULER_SPEC.md 11), `--public`, `--preserve`, `--disable-pass`, `--cxx`, `--cache-dir`, and, approved 2026-10-19, `--randomize-fail=warn|error` for a failed `randomize()` and `--assert-incomplete=ignore|warn|fail` for assertion attempts not finished at the end, both P3) are added to this document in their stage.

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
| `--max-slot-events=<n>` | Events run in one time slot, in any region (the events that `--stats=json` counts as `sched.events.*`). They grow with every resumption, update and evaluation, also when processes wake each other inside one region, which `--max-zero-delay` does not count (added 2026-10-19, approved) | 100000000 |
| `--max-steps=<n>` | Tier 1 instructions one process runs in a row without waiting (they grow in a loop with no wait, such as `forever begin end`) | 1000000000 |
| `--max-scc-iter=<n>` | Iterations of a static island of tier 2 (P2) | Decided in P2 |

- `0` means no limit.
- Grounds for the defaults (measured 2026-10-16):
  - Tier 1 in the release configuration ran about 350 million instructions a second in a loop with no wait. The dev configuration was about as fast, and the asan configuration about half as fast. Stopping at the default of `--max-steps` takes about 3 seconds in release and about 6 in asan.
  - A million resumptions after `#0` took about 0.04 seconds.
  - `--max-steps` is large so as not to stop, by mistake, a design that initializes a large memory in one loop in an `initial` (16 million words take about 100 million instructions).
  - `--max-slot-events` (measured 2026-10-19): two processes that wake each other reached the default in about 7 seconds in release, 8 in dev, 14 in ubsan and 32 in asan. The default leaves a margin of 100 times over a time slot of a million events, such as a design where a million processes wake on one clock edge.
- These defaults are reviewed in M6, when sv-tests and real designs are run. Changing them means changing this document, with a human's approval.
- When a limit is reached, one line says which, on standard error, before the line of the reason for ending (4). Forms:
  - `kirimasim: error: time did not advance: <n> moves into the Active region in one time slot (--max-zero-delay=<n>)` (`Reactive` for the reactive set)
  - `kirimasim: error: time did not advance: <n> events ran in one time slot (--max-slot-events=<n>)`
  - `kirimasim: error: time did not advance: process "<process name>" ran <n> instructions without waiting (--max-steps=<n>)`
  - The process name is the name of the SIR process (`top.initial0` and the like).
- **The last process and the latest events (approved 2026-10-19):** after that first line, whose form does not change, the report gives the last process that put an event in the time slot, and the latest events of the time slot, newest first, at most 32, each with its region, its process, its source position and what it does. For example:

  ```text
  kirimasim: error: time did not advance: 1000000 moves into the Active region in one time slot (--max-zero-delay=1000000)
    last process: top.u_ctl.always0 (always at ctl.sv:12)
    last events (newest first):
      Inactive  top.u_ctl.always0  ctl.sv:15  resume after #0
      Active    top.u_ctl.always0  ctl.sv:14  blocking assignment
  ```

  The added lines are indented and contain no `: error: `, so `:expected_stderr:` (TEST_SUITE.md) does not compare them.

### 2.7 Limits on resources and on wall-clock time (approved 2026-10-19)

| Option | What it counts | Default |
| --- | --- | --- |
| `--max-procs=<n>` | Processes made at run time (the children of `fork` and the like) that have not ended. The static processes that elaboration makes (continuous assignments, gates, `always`, `initial` and so on) are not counted: their number is fixed before the run, and a large gate-level design must not stop at the default | 1000000 |
| `--max-call-depth=<n>` | Calls one process has nested at once, the calls of tasks that wait and the others together (SCHEDULER_SPEC.md 16.2) | 10000 |
| `--max-wall-time=<seconds>` | Seconds of wall-clock time from the start of the simulation (compiling not counted), a whole number. The reason is wall_time_limit, code 4, the code of time_limit, as both cut a run short | None |

- `--max-procs=0` and `--max-wall-time=0` mean no limit, as in 2.6. `--max-call-depth=0` is not accepted (code 2): the stack of the simulation thread is sized by it (RUNTIME_ABI.md 5).
- `--max-procs` is checked when a process is made, `--max-call-depth` when a call is made, and `--max-wall-time` at the check points of SIGINT (SCHEDULER_SPEC.md 16.2). `--max-time-ok` does not apply to wall_time_limit.
- Grounds for the defaults: `--max-call-depth` keeps the depth measured with the present stack (RUNTIME_ABI.md 5). `--max-procs` is not measured yet; it is reviewed when real designs are run, as 2.6 says.
- Stage: M7, the stage of P1 left. The P1 plan gives no stage for these options; M7 is the only one left.
- When a limit is reached, one line says which, on standard error, before the line of the reason for ending (4). Forms:
  - `kirimasim: error: resource limit reached: <n> live processes made at run time (--max-procs=<n>)`
  - `kirimasim: error: wall-clock limit reached: <n> seconds (--max-wall-time=<n>)`
  - The depth of calls keeps the form of a run-time error of the design, with the statement's position and the instance's hierarchical name (SCHEDULER_SPEC.md 16.2): `<file>:<line>: error: calls nested more than <n> deep (--max-call-depth=<n>) [<hierarchical name> @ <time>]`.

## 3. Where output goes

- **Standard output:** only what the design prints (`$display`, `$write`, `$strobe`, `$monitor` and so on). The test suite compares standard output only (TEST_SUITE.md). The exceptions are the SIR of `--dump-sir` and the JSON of `--stats=json` (written last).
- **Standard error:** every message of kirimasim itself (diagnostics, the version line, the line of the reason for ending and so on; SYSTEM_TASKS.md 2). Standard output is flushed before anything is written to standard error.

## 4. Messages of kirimasim itself

- The form is `kirimasim: <kind>: <text>`, the kind one of `error`, `warning`, `note`, `info`. A message with a source position has the form of slang's diagnostics, `<file>:<line>:<column>: <kind>: <text>`.
- **Version line:** at the start of a run, one line on standard error gives the version, the commit, the options and the seed (so that the same conditions can be reproduced later). Example: `kirimasim: info: kirimasim 0.1.0 (commit 0123456789ab) args: --top=tb a.sv`. When `--seed` comes (M7), `seed=<n>` is added before `args:` (corrected 2026-10-18, approved: the example had the seed before the option existed). It is not written to standard output, so the test suite has no need to remove it.
- **Initial values that are not the standard's (approved 2026-10-19):** when `--init-values` is not `x`, one more line follows the version line: `kirimasim: note: --init-values=<value>: 4-state variables with no initial value start as <value>, not x (VALUE_SEMANTICS.md 1.4)`. It is a line of its own, not part of the version line, so that it stands out. Like the version line, it goes to standard error and the test suite does not compare it.
- **Line of the reason for ending:** at the end, one line on standard error gives the reason and the time. Example: `kirimasim: info: finish at 120 ns`. It is not written for `$finish(0)` and `$stop(0)` (SCHEDULER_SPEC.md 16.2).
- **How times are written (approved 2026-10-16):** an integer and a unit separated by one space. The unit is the largest of `s`, `ms`, `us`, `ns`, `ps`, `fs` that gives the time exactly as an integer (for example, with a precision of 1ps, 120000 steps are `120 ns` and 1500 steps are `1500 ps`). Time 0 is written in the unit of the design's precision (`0 ps` for a precision of 1ps, `0 ps` for 10ps). Nothing is rounded, so the number of steps can be read back exactly.

## 5. Exit codes

The same as the table of SCHEDULER_SPEC.md 16.2. The names of the reasons go into `--stats=json` under the same names.

| Code | Reason | Meaning |
| --- | --- | --- |
| 0 | finish, quiescent, stop | `$finish`, no events left, `$stop`. time_limit with `--max-time-ok` is also 0 |
| 1 | fatal | `$fatal`. With the option that counts `$error`, one or more `$error`. `$stop` with `--stop=error` is also 1 (the reason stays stop; approved 2026-10-19) |
| 2 | error | An error in the command line or the input (syntax errors and unsupported constructs included), or a run-time error of the design (SCHEDULER_SPEC.md 16.2) |
| 3 | internal | An internal error of kirimasim (a failure of the SIR verifier and the like); never an error of the design |
| 4 | time_limit, wall_time_limit | `--max-time` reached; `--max-wall-time` passed (2.7; approved 2026-10-19) |
| 5 | zero_time_limit | A limit reached without time advancing |
| 6 | resource_limit | A limit on resources reached (2.7; approved 2026-10-19) |
| 130 | interrupted | SIGINT |

- **Unsupported constructs:** during P1 many constructs are not implemented yet. When making SIR meets one, kirimasim gives a diagnostic at its position and ends with code 2 (as row 2 of the design specification's table of exit codes says). It is never ignored silently.
- **Run-time errors of the design (added 2026-10-18, approved):** an error the design makes while it runs ends the run with code 2, with one line on standard error at the statement's position (SCHEDULER_SPEC.md 16.2). Code 3 is only for faults of kirimasim itself.

## 6. Not confirmed, to be written later

- The name and default of the option that counts `$error`.
- The fields of `--diag-format=json` and the scheme of diagnostic IDs (M7; together with RUNTIME_ABI.md's `kirima_status`).
- **A scheme of names (approved 2026-10-19; fixed in M7):** kirimasim's own diagnostics are named `kirima-<words>`, in lower case with hyphens (for example `kirima-zero-time-loop`), in the form of slang's warning names. `-W<name>`, `-Wno-<name>` and `-Werror=<name>` (2.1) then take them as they take slang's, and the prefix keeps them apart from slang's. Run-time errors and limits cannot be turned off; a limit can only be raised. The names given so far, with the items they come from, are listed in UNSPECIFIED_BEHAVIOR.md 7.3.
