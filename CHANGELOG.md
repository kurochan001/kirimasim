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

- The format flags `-` (the value at the left of its field, the written
  width or the automatic one) and `0` (`%d` padded with 0 after the sign)
  in `%d`, `%h`, `%o`, `%b`, `%c`, `%s` and `%t`, with one warning for each
  format that uses them, as the standard does not define them; `+`, space,
  `#` and a precision outside `%e`, `%f` and `%g` are reported as
  unsupported (SYSTEM_TASKS.md 3.12). Before, `-` stopped the run with an
  internal error (code 3), and `0` was read as part of the width.

### Changed

- Building and testing need Python 3.10 or later (the test tools already
  used its syntax; CMake asked for 3.8).
- Run-time errors of the design end the run with exit code 2 and one line
  `<file>:<line>: error: <text> [<instance> @ <time>]` at the statement,
  and `final` still runs (CLI.md 5, SCHEDULER_SPEC.md 16.2): a write or an
  event from the Postponed region, which went unreported or stopped with an
  internal error (code 3); a disable from a function of a block outside
  it, an internal error before; and a delay that takes the time past the
  largest time, whose event was lost without a word.

### Fixed

- Tier 1 is fast again on designs without wide values: since values wider
  than 64 bits ran, every instruction looked at the widths of its operands
  to choose the wide path, which made the bench designs 8 to 22% slower;
  the choice is now made once, when the design is loaded (#28).
- Tier 1 runs 4 to 9% faster on three of the bench designs: the check that
  nothing is written from the Postponed region, made at each instruction,
  is marked as the rare case it is, so that the compiler lays the common
  path out first (#28).
- The action of a deferred assertion is no part of the `always_comb`,
  `always_latch` or `always_ff` that runs it: several such blocks, and
  other processes, may write what their actions write, where kirimasim
  stopped with an internal error of the SIR verifier. A write through
  the action of another process's deferred assertion to what such a
  block writes is an error at the write with a note at the assertion,
  which slang does not report (SCHEDULER_SPEC.md 6.4; #22).
- An immediate assertion waits only where its action blocks do, and a
  deferred one not at all: `always_ff` with an assertion in its body was
  reported as unsupported, and a task with one that `always_comb`,
  `always_latch` or `always_ff` calls stopped kirimasim with an internal
  error of the SIR verifier (16.3, 16.4).
- A task that `always_comb`, `always_latch` or `always_ff` calls may
  write a variable that another process writes, as 9.2.2 allows, where
  kirimasim stopped with an internal error of the SIR verifier; and
  another process that writes such a block's variable through a task is
  an error at the write with a note at the call, which slang does not
  report (SCHEDULER_SPEC.md 6.4; #21).
- `always_comb` and `always_latch` no longer run again when a variable
  changes that only the body of a task they call reads, or only an action
  block of an assertion in them or in a function they call; the arguments
  of the task and the expression of the assertion still count
  (SCHEDULER_SPEC.md 6.3; #21).
- A `fork` or a named block with one statement is placed at its first
  token, not at that statement: a run-time error at such a `fork` named
  the statement's line, and `--dump-sir` placed the `fork`, the processes
  of its statements and the block's scope instructions there (#20).
- A function that writes a variable can be called from several continuous
  assignments: its writes are procedural (6.5), so the variable takes the
  value written last, where kirimasim stopped with an internal error of
  the SIR verifier. Such a variable also keeps its value when released
  after `force` (10.6.2), where it lost the forced value (#19).
- A real constant delay is rounded to the precision as the same value in a
  variable is, from the double times the power of ten, half away from
  zero (SCHEDULER_SPEC.md 1). The constant was rounded as its decimal
  text reads, so `#1.005` at 1ns/10ps waited 101 steps and the same value
  in a `real` variable 100 (#18).
- A function that schedules work to run after it returns (a nonblocking
  assignment, `->>` or `fork ... join_none`, in it or in a function it
  calls) is an error at compile time when the initial value of a static
  variable, a continuous assignment, the right side of a procedural
  `assign` or `force`, or a `final` procedure calls it, as 13.4.4 asks
  (SCHEDULER_SPEC.md 6.5). Such calls ran, or in `final` did nothing,
  without a word (#13).
- The initial value of a static variable that may read another static
  variable before that one is set gets a warning, as SCHEDULER_SPEC.md 3
  says: a variable of another instance, or one of its own instance,
  package or compilation unit set after it in kirimasim's order. The
  value read depended on that order without a word (#16).
- A negative real constant delay, such as `#(-2.0)`, waits as the same
  value held in a real variable does: rounded to the precision, then read
  as an unsigned 64-bit count of its steps (9.4.1, SCHEDULER_SPEC.md 8).
  It was reported as unsupported, as were NaN and infinite constants,
  which now give a delay of 0 (#12).
- An input port left unconnected where `` `unconnected_drive pull1`` or
  `pull0` is in effect is pulled up or down, as 22.9 says: a `pullup` or
  `pulldown` on every bit of its net (VALUE_SEMANTICS.md 5.3). It stayed
  `z` without a word. A variable input port keeps the initial value of
  its type, with a warning (UNSPECIFIED_BEHAVIOR.md N10; #17).
- Without `--top`, a config in the sources gives the top, as 33.5.4
  says: the cells its design statement names are the tops, and its rules
  bind the instances. The config was ignored without a word, and every
  module that nothing instantiated ran. With more than one config, an
  error at the second asks for `--top` (CLI.md 2.1).
- A continuous assignment, gate or UDP instance that reads what it
  drives, and a procedural `assign` or `force` that reads what it holds,
  are evaluated again when their own change alters what they read, as
  for any other change. They changed their target once and stopped, so
  `assign a = en ? ~a : 1'b0;` settled instead of looping until
  `--max-slot-events` (SCHEDULER_SPEC.md 5, 12; #15).
- Compiling a design with many processes no longer takes time that grows
  with the square of their number: the SIR verifier's check of what
  processes write in common (SIR_SPEC.md 10, rule 6) sorts the writes of
  each slot instead of comparing every pair of processes. 40000 `always`
  blocks that each write one bit of a vector took 1.8 s to compile and now
  take 0.12 s; the errors are the same, in the same order (#14).
- Calls nested more than 10000 deep in one process are a run-time error of
  the design at the call, with exit code 2 and `final`; they were an
  internal error (code 3), the calls of tasks that wait had no limit, and
  builds whose stack ran out first, such as asan, crashed. The simulation
  runs on a thread with a stack of 512 MiB (RUNTIME_ABI.md 5; #6).
- SIGINT (Ctrl-C) stops a run before the next event or at the next branch
  of a loop, runs `final`, keeps the output written so far and ends with
  exit code 130; a second SIGINT ends at once. kirimasim was killed at
  once, losing `final` and the buffered output (SCHEDULER_SPEC.md 16.2;
  #9).
- Processes that wake each other inside one region without time advancing
  are stopped by the new limit `--max-slot-events`, the events run in one
  time slot (100000000 by default), with exit code 5 and `final`; neither
  `--max-zero-delay` nor `--max-steps` counted them, and the run never
  ended (CLI.md 2.6; #8).
- A delay that takes the time past the largest time is a run-time error of
  the design whatever its form: one computed at run time wrapped around
  silently when its conversion to steps did not fit in 64 bits, and a
  constant one was reported as an unsupported delay value (#7).
- A unique or priority violation in a function that the initializer of a
  static variable calls is reported at time 0, like any other; kirimasim
  crashed (#5).
- A wait on several items, such as `@(posedge clk or negedge rst_n)`, no
  longer keeps the items that did not wake it: memory grew with every
  clock for as long as the other signal stayed still (#4).
- A `case` on a real compares the expression and the items as reals; they
  were rounded to integers first, so 1.6 matched 2.0. A `case` on a string
  compares strings; it was reported as an unsupported value of no bits (#10).
- A DPI-C import (`import "DPI-C" ...`) is reported as not supported yet at
  its declaration, whether it is called or not; it ran as an empty
  subroutine that returned at once (#11).
- `$finish` and `$stop` with 1 (the default) or 2 report the call on standard error,
  `<file>:<line>: $finish at <time>`, and with 2 write the statistics at the
  end of the run (SYSTEM_TASKS.md 10.2); they reported nothing.
- A range select of a packed array whose elements are wider than one bit,
  such as `a[2:1]` or `a[i +: 2]` of `logic [3:0][7:0] a`, reads and
  writes whole elements; it took the indices as bit positions (#3).
- A wait in a task on a `ref` argument that comes after an argument or
  variable wider than 64 bits wakes when the variable it names changes;
  it waited on another variable, or could crash (#1).
- A conditional operator (`?:`) wider than 64 bits with a function call
  or an assignment in an operand keeps its whole value; the bits above
  the lowest 64 were lost (#2).
- A system task or function whose first argument is a string, such as
  `$fopen`, `$dumpfile` or `$readmemh`, is reported as an unsupported
  system task or function; it was reported as a string method.
- A static variable of a function named by a hierarchical name before the
  function is called stays the function's own: an event expression or a
  `$strobe` argument that calls the function may write it. It was
  rejected as an event expression that writes a variable, or stopped as a
  write in the Postponed region, depending on which came first in the
  source.
- A wait on an expression or on a condition, and `$monitor`, that read a
  function's static variable by a hierarchical name wake when it changes;
  they never woke. A function in a wait's expression that triggers an
  event no longer makes the wait depend on that event, which could make
  the run loop without end.
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
