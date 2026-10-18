# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

No release yet. The Added section below sums up what works as of
2026-10-18; from then on, each change a user can see has an entry of its
own. CONTRIBUTING.md says what goes in this file.

### Added

- The `kirimasim` command: compile and run a design (`kirimasim
  <file>...`), `kirimasim parse`, `--version`; the input options `-f`,
  `+incdir+`, `+define+`, `--top` and `--timescale`; run-time plusargs
  for `$test$plusargs` and `$value$plusargs` (CLI.md 1, 2).
- Limits on a run: `--max-time` and `--max-time-ok`, and `--max-zero-delay`
  and `--max-steps` for runs where time does not advance, each ending with
  its own exit code and message (CLI.md 2.5, 2.6, 5).
- Every construct that is not supported yet is reported at its source
  position, and kirimasim exits with code 2 before the simulation starts;
  nothing is skipped silently (CLI.md 5).
- `--dump-sir`, which prints the intermediate representation in its text
  form, and `--stats=json`, which prints the event counters at the end of a
  run (SIR_SPEC.md 9, PERFORMANCE.md 3).
- The execution kernel's scheduler: every region of the IEEE 1800-2023
  time slot with the reference algorithm, including pending reports
  settled in the Observed region and `$strobe` and `$monitor` in the
  Postponed region (SCHEDULER_SPEC.md). Tier 1 runs designs by
  interpreting SIR.
- Design structure: modules, ports (`input`, `output`, `inout`),
  parameters and `defparam`, `generate`, arrays of instances, interfaces
  and modports, packages and `$unit`, hierarchical names, specparams.
- Data types: two-state and four-state integral types of any width,
  `real`, `shortreal`, `realtime`, `string`, enumerations, packed and
  unpacked structures, packed unions, fixed-size unpacked arrays (several
  dimensions, arrays of structures), unpacked array parameters, named
  events, `chandle` (null only).
- Nets: `wire`, `tri`, `wand`, `wor`, `tri0`, `tri1`, `supply0`,
  `supply1` and `uwire`; drive strengths and the resolution of several
  drivers; continuous assignments and net delays; built-in gates and
  pulls; user-defined primitives; `alias` (VALUE_SEMANTICS.md 5).
- Procedures and timing: `initial`, `always`, `always_comb`,
  `always_latch`, `always_ff`, `final`; blocking and nonblocking
  assignments with intra-assignment delays; delay and event controls on
  signals and on expressions, with `iff`; `wait (expr)`; named events;
  `fork` with `join`, `join_any` and `join_none`; `wait fork`, `disable
  fork` and `disable`; `assign`/`deassign` and `force`/`release`.
- Functions and tasks: static and automatic, recursion, `input`,
  `output`, `inout` and `ref` arguments, default argument values, unpacked
  structures and arrays as arguments and return values.
- Statements and expressions: `if`, `case`, `casez`, `casex` and `case
  inside`; `unique`, `unique0` and `priority` with their violation
  reports; every loop with `break`, `continue` and `return`; every
  operator, including streaming, `inside`, wildcard equality, assignment
  operators and increments; assignment patterns (VALUE_SEMANTICS.md 3).
- Immediate assertions (`assert`, `assume`, `cover`) and deferred ones
  (`#0`, `final`).
- System tasks and functions: display and formatting (`$display`,
  `$write`, `$strobe`, `$monitor`, `$sformat`, `$sformatf`, `$swrite`),
  `$sscanf`, time (`$time`, `$realtime`, `$timeformat`,
  `$printtimescale`), ending a run (`$finish`, `$stop`), severity tasks
  (`$fatal`, `$error`, `$warning`, `$info`), conversions, `$bits`,
  `$typename`, the array queries, `$clog2` and the real math functions,
  the bit-vector functions, `$test$plusargs` and `$value$plusargs`
  (SYSTEM_TASKS.md).

### Fixed

- `$sformat` and `$sformatf` printed a format that is not a string
  literal, such as a string parameter, as a value to show. A constant
  format is now read as a format, and a wrong number of arguments for it
  is an error; a format known only at run time is reported as not
  supported yet (SYSTEM_TASKS.md 3.13).
- A function or task called by a hierarchical name from another instance
  (`m1.f()`) was a second copy of it, whose static variables were
  initialized once more. It is now the declaring instance's subroutine,
  with its static variables initialized once (SIR_SPEC.md 3.3).
- An element of a function's array value passed as an array argument
  (`sum(g()[1])`) called the function twice. An input argument is now
  evaluated once, at the call (SIR_SPEC.md 8).
- A pending unique or priority violation, or failed deferred assertion,
  was dropped when its process went on after a `fork`'s `join` or
  `join_any`, after `wait fork`, or after a block was disabled. Only
  resuming from an event control or `wait`, or a new start of
  `always_comb`, drops it now (SCHEDULER_SPEC.md 15.4).
