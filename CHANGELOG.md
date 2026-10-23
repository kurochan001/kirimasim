# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

No release yet. The Added section below sums up what works as of
2026-10-18; from then on, each change a user can see has an entry of its
own. CONTRIBUTING.md says what goes in this file.

### Fixed

- Integral `$fread` again follows the approved SIR_SPEC 8 copy-in/copyback
  interpretation of 4.9.7. A written result finds its target at copyback;
  a zero-byte result has no copyback. The earlier capture-once target rule
  and nine mechanism oracles were incorrect. Expectation correction and
  implementation are isolated in separate review branches.

- Integral `$fread` destinations accept optional start/count arguments without
  using their values to limit input (21.3.4.4). Supplied expressions run once
  in the approved left-to-right order. Copy-in and copyback follow SIR_SPEC 8
  as corrected above. An original matrix adds 153 source cases across nine
  contexts, eight widths and both state domains; existing expectations remain.

- Diagnose a container-cell NBA as illegal under 6.21/10.4.2, with its source
  position, and correct the integration inventory: this is a standard
  restriction, not a missing P1 feature. New source tests cover dynamic arrays
  and queues with whole-element, bit and part targets; whole-value NBAs to
  eligible static container variables remain separate.

### Added

- The comparison utility adds seven original array-extrema probes and a
  `reference_mismatches` report field. Single-result expectations follow
  7.12.1 even for tied keys; reference mismatches remain separate from
  Icarus-passing execution gaps, with judging-tool regressions.

- Runtime unpacked concatenation combines scalar, fixed-array, dynamic-array
  and queue operands, including nested values and returned arrays. Queue
  rvalue slices use the same typed construction loop, with clipping, `$`,
  empty/X/Z ranges, wide unsigned bounds and source snapshots before bound
  side effects. Another 256 original source cases cover eight contexts and
  six widths (7.10.1, 7.10.4, 10.10; existing SIR_SPEC 5.8 operations).

- SystemVerilog dynamic arrays and queues execute through the shared typed
  heap: construction, deep value copies, integral/real/string/nested elements,
  queue methods, actual-bound refs, detached cells, whole-value delayed
  assignments, explicit heap waits, dynamic foreach and first-dimension
  queries. Original matrices add 240 source cases across eight execution
  contexts, plus 11 positioned CLI diagnostics. Forwarded refs retain
  empty-variable binding dependencies. Implicit heap sensitivity and remaining
  container forms are explicit unsupported cases; complete P1 parity remains
  unfinished (7.5, 7.6, 7.10, 10.4.2, 13.5; SIR_SPEC 5.8).

- Heap-dependent SIR watches/waits capture typed reads and reattach after
  index, structure or binding changes. The shared scheduler queues notices
  with monitor serials, excludes called-private containers, restores hook
  ownership and reports resource failures. Tests cover direct/called refs,
  detached cells, cancellation/reuse and bounded registrations.

- Typed heap read/change observation hooks distinguish container structure
  from cell values. Notifications follow committed changes and skip equal
  normalized writes, failed operations and invalid selections. Scheduler/VM
  monitor integration is covered by the entry above.

- Automatic typed-container collection after drained scheduling regions and
  slots, tracing typed storage, live activations, detached cell refs, deferred
  call arguments and owned scheduler values (RUNTIME_ABI 6). Root-only changes
  trigger collection; failed marking retains all values. This is tested with
  original SIR inputs.

- Tier 1 executes the 16 typed container SIR instructions through the shared
  heap, with actual-target reference metadata, stable element refs, snapshots,
  source-position method warnings and resource-error mapping (SIR_SPEC 5.8).
  Independent SIR tests cover calls, waits, packed writes and allocation
  failure. Typed-root collection and heap-change notification are covered by the
  entries above. Language integration is described above;
  complete container coverage remains unfinished.

- Nested container copies apply each destination queue's bound through the
  common heap copy engine, including element writes and queue methods, with
  truncation warnings and allocation-failure rollback. Actual-type snapshots
  preserve source bounds (6.22.2, 7.5, 7.10.5; SIR_SPEC 5.8, RUNTIME_ABI 6).
  VM integration is covered above.

- Typed container SIR descriptors, receiver references, 16 instructions,
  verification and text support, including explicit snapshot/copy/assignment
  operations and queue-bound equivalence for ref arguments (6.22.2, 7.5,
  7.10, 13.5.2; SIR_SPEC 5.8). VM and language integration are covered above;
  full container coverage remains unfinished.

- Context-owned typed container heap foundation: stable element references,
  deep copies, bounded queue operations, allocation-failure rollback and
  exact tracing with generation-checked table reuse (7.5, 7.10, 13.5.2;
  RUNTIME_ABI 6). Typed-root publication and collection are covered above.
  Language integration is covered above; this foundation alone does not
  establish full dynamic-array/queue coverage.

- Named events can share event controls with signal subscriptions and
  expression watches, each with its own `iff`. False guards retain their
  item and the first satisfied item cancels the whole wait. C ABI 0.3
  appends `wait_mixed` without changing old entries (9.4.2.3, 15.5;
  SIR_SPEC 7.2, SCHEDULER_SPEC 9.5, RUNTIME_ABI 4.2).

- Runtime child selections inside statically bounded packed members/parts
  preserve the parent boundary for writes and compound reads. Slots,
  automatic frames, refs and static NBA/delayed-NBA targets share kernel
  clipping, preserving neighbouring bits (7.4.5, 11.5.1; SIR_SPEC 5.3).

- Streaming NBA targets accept intra-assignment delays with the same
  reorder, snapshot and target lowering as ordinary streaming and delayed
  stores, including short final blocks and runtime selectors (10.4.2,
  11.4.14.3; VALUE_SEMANTICS 8).

- Delayed NBAs snapshot whole fixed arrays and unpacked structures,
  including nested fixed-array, real and string members. Aggregate copies
  retain runtime element prefixes and preserve each side's declared order
  through the common storage traversal (7.6, 10.4.2, 10.9; VALUE_SEMANTICS 3.13).

- Delayed NBAs capture runtime fixed-array indices, packed bit/part positions
  and concatenation targets, including wide values and partial ranges. The
  shared kernel retains their captured updates across task/fork completion
  and uses NBA/Re-NBA according to the issuing thread (10.4.2, 9.4.5;
  SIR_SPEC 5.3, SCHEDULER_SPEC 5, 13).

- Program execution uses Reactive/Re-Inactive/Re-NBA through calls and
  forks, with named program identities and anonymous-program subroutines.
  The shared kernel ends descendants on program completion, implements
  `$exit` by dynamic origin and finishes after all programs with initials
  end. Empty programs and confirmed report calls keep their existing rules
  (24.3, 24.3.1, 24.6, 24.7; SIR_SPEC 7.6, SCHEDULER_SPEC 16.1).

- `$sscanf` and `$fscanf` read binary `%u`/`%z` fields at arbitrary integral
  widths, including runtime formats and automatic/ref destinations. Raw
  whitespace and NUL remain payload; incomplete fields leave targets
  unchanged. Width zero follows the approved omitted-width rule
  (SYSTEM_TASKS.md 6.3, SIR_SPEC.md 8).

- `%u` and `%z` write unformatted 2-state and 4-state words, including
  arbitrary-width integral and packed values, through console, file and
  string-format APIs. Packed destinations preserve zero bytes; string
  destinations omit them (SYSTEM_TASKS.md 3.11, VALUE_SEMANTICS.md 7.6).
- Memory-file tasks accept fixed memories through `ref` arguments, with
  `const ref` for writing files, forwarding and waiting calls, and
  references to rows of multidimensional arrays (SYSTEM_TASKS.md 7,
  SIR_SPEC.md 8).
- `--autoflush` publishes stdout after display/write/strobe/monitor output,
  including file tasks directed to stdout (CLI.md 2.2, SYSTEM_TASKS.md 2).
- `--stop=finish|ignore|error` selects normal termination, continuation,
  or exit code 1 for `$stop`; ignored `$stop(2)` still reports statistics
  at the eventual end (SCHEDULER_SPEC.md 16.2, SYSTEM_TASKS.md 10.2).
- `--max-wall-time=<seconds>` limits elapsed simulation time, excluding
  compilation. It checks VM branches and kernel event boundaries, runs
  final procedures to completion and ends with `wall_time_limit` / code 4,
  even with `--max-time-ok` (CLI.md 2.7, SCHEDULER_SPEC.md 16.2).
- `--bounds-check=off|warn` reports out-of-range or X/Z-containing indices
  in packed selections and fixed unpacked arrays, including wide values,
  automatic/ref storage and NBA targets. Warnings are given once per source
  position, with a count at the end; values and event counts stay the same
  (CLI.md 2.2, VALUE_SEMANTICS.md 3.11).
- `$sformat` and `$sformatf` accept formats evaluated at run time, including
  strings, packed values and function results. They share numeric/string
  conversion, `%t` scaling and `%m`/`%l` handling; missing or extra values
  warn and continue (SYSTEM_TASKS.md 3.13; 21.3.3).
- `$value$plusargs` accepts a string or packed format evaluated at each
  call, including function results. Matching still uses the first plusarg,
  and unmatched calls leave the output and its index expressions untouched
  (SYSTEM_TASKS.md 9, SIR_SPEC.md 8; 21.6).
- `$sscanf` and `$fscanf` accept formats evaluated at run time, from strings
  or packed values. Missing output arguments warn and stop reading before
  the missing field; X/Z bits in a packed format return -1 without reading
  (SYSTEM_TASKS.md 6.3; 21.3.4.3).
- Streaming assignment targets wider than 64 bits, including arrays,
  structures, automatic variables and ref arguments, with the inverse
  block order of `<<` and with NBA value capture (VALUE_SEMANTICS.md 8;
  11.4.14.3).
- Enumeration `next(n)` and `prev(n)` accept counts evaluated at run time,
  including calls with side effects, and wrap in declaration order for
  two-state, four-state and wide base types (VALUE_SEMANTICS.md 7.7;
  6.19.5.3, 6.19.5.4).
- `--mintypmax=min|typ|max` selects delay expressions throughout the
  design, including expressions evaluated at run time (CLI.md 2.2,
  SCHEDULER_SPEC.md 12).
- `for` loops with an omitted condition, including `for (;;)`, with
  ordinary loop control and time waits (SIR_SPEC.md 7.2; 12.7.1, 12.8).
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
- File output: `$fopen`, `$fclose`, `$fflush`, `$fdisplay`, `$fwrite`,
  `$fstrobe` and `$fmonitor` in their b, o and h forms, to an mcd or an fd,
  and `$fgets` into a string. `$fclose` ends the `$fstrobe`s and
  `$fmonitor`s of the files it closes; a descriptor that names no open file
  gives one warning for each place (SYSTEM_TASKS.md 6). Before, each of them
  was reported as unsupported.
- File input: `$fgetc`, `$ungetc`, `$fscanf`, `$fread` into a variable or a
  memory, `$ftell`, `$fseek`, `$rewind`, `$feof` and `$ferror`, and
  `$fgets` into an integral variable; STDIN reads standard input. Reading
  an mcd or a closed file gives one warning for each place, and reading a
  file opened only for writing fails with an error that `$ferror` reports
  (SYSTEM_TASKS.md 6.3, 6.4). Before, each of them was reported as
  unsupported, and `$fgets` on STDIN read nothing.
- `$stime`, `$timeunit` and `$timeprecision` (with no argument, an
  instance, `$unit` or `$root`), and the `%l` format, which prints
  `work.<module>` (SYSTEM_TASKS.md 3.8, 5.1, 5.2). Before, each was
  reported as unsupported.
- `$readmemb`, `$readmemh`, `$writememb` and `$writememh`, with and without
  address arguments, on memories of one or more dimensions; an address out
  of the range and a count of words that does not match give warnings, and
  a file that cannot be read ends the run with code 2 (SYSTEM_TASKS.md 7).
  Before, each was reported as unsupported.
- `$random` and `$dist_uniform`, `$dist_normal`, `$dist_exponential`,
  `$dist_poisson`, `$dist_chi_square`, `$dist_t` and `$dist_erlang`, which
  give the values of IEEE 1800-2023 Annex N, written from a description in
  SYSTEM_TASKS.md 12; an argument of 0 or less where it is to be above 0
  gives the warning `kirima-dist-argument`. Before, each was reported as
  unsupported.
- Waveforms: `$dumpfile`, `$dumpvars`, `$dumpoff`, `$dumpon`, `$dumpall`,
  `$dumplimit` and `$dumpflush` write a 4-state VCD file with the values
  at the end of each time slot, and `--wave=<file>.vcd`, `--wave-scope`
  and `--wave-glitches` dump from the command line (WAVEFORM.md). A
  `$dumpvars` or `$dumpfile` called too late, or under `--wave`, is
  ignored with a warning. Before, the tasks were reported as unsupported
  and the options were unknown. FST is not supported yet.
- `$readmemb` and `$readmemh` into a memory of an enumerated type: a word
  that is the value of none of its items ends the run with code 2, the
  words before it loaded (SYSTEM_TASKS.md 7.1, 21.4.2). Before, it was
  reported as unsupported.
- `$fread` into a memory of unpacked structures reads each element's
  members in declaration order (SYSTEM_TASKS.md 6.3, 21.3.4.4). Before, it
  was reported as unsupported.
- `$stacktrace`, as a task and as a function: the call's scope and place,
  then each call that led there (SYSTEM_TASKS.md 10.3). Before, it was
  reported as unsupported.
- `$urandom` and `$urandom_range`, from a generator of each procedure and
  fork branch seeded as 18.14 asks, and `--seed`, which the version line
  now shows (SYSTEM_TASKS.md 13). Before, the functions were reported as
  unsupported and `--seed` was an unknown option.
- `randcase`, which takes a branch by its weights from the process's
  generator (SYSTEM_TASKS.md 13.3, 18.16). Before, it was reported as
  unsupported.
- `--max-call-depth=<n>`: how deep the calls of one process may nest,
  10000 by default; above the default, the stack of the simulation thread
  grows in proportion (CLI.md 2.7, RUNTIME_ABI.md 5).

### Changed

- P1 now targets Icarus Verilog 13.0-or-broader execution coverage, bringing
  necessary later-phase mechanisms into P1. Binary `%u`/`%z` input semantics
  are approved, including width-zero, raw whitespace/NUL and partial-EOF
  choices (P1_SCOPE, SYSTEM_TASKS 6.3).

- Nonblocking updates without a delay use their saved values directly,
  avoiding a temporary heap allocation for each update while retaining
  the values until the time slot ends (SCHEDULER_SPEC.md 5, 13; #28).
- Less overhead in tier 1 when starting and resuming processes, by reusing
  the queue for reclaiming ended processes and skipping file-monitor
  completion checks when no file monitors remain (SCHEDULER_SPEC.md 10.8,
  SYSTEM_TASKS.md 6.2; #28).
- Building and testing need Python 3.10 or later (the test tools already
  used its syntax; CMake asked for 3.8).
- Run-time errors of the design end the run with exit code 2 and one line
  `<file>:<line>: error: <text> [<instance> @ <time>]` at the statement,
  and `final` still runs (CLI.md 5, SCHEDULER_SPEC.md 16.2): a write or an
  event from the Postponed region, which went unreported or stopped with an
  internal error (code 3); a disable from a function of a block outside
  it, an internal error before; and a delay that takes the time past the
  largest time, whose event was lost without a word.
- Calls nested deeper than the limit now end the run with the reason
  resource_limit and exit code 6, not error and code 2, and the line names
  the option: `calls nested more than <n> deep (--max-call-depth=<n>)`
  (SCHEDULER_SPEC.md 16.2, CLI.md 2.7).
- `$ungetc` on a file opened only for writing returns 0, as for any
  open file, and the reads that follow return the characters pushed back,
  the last first, before they fail with EBADF (21.3.4.3, SYSTEM_TASKS.md
  6.3). It returned -1 before, as the C library of macOS does.
- A `$fmonitor` whose last file closes lets go of the frame and the
  process it made, so a design that opens and closes monitors many times
  keeps its memory flat instead of growing with each call
  (SYSTEM_TASKS.md 6.2; #37).
- A checker instantiated under a module's default clocking no longer
  stops with "no inferred clock in this context": `kirimasim parse`
  accepts the design, and running it reports the clocking block and the
  checker as unsupported for P3 (16.14.7, 17.4; #24).

### Fixed

- A sequential UDP instance with a delay has the value of its `initial`
  statement on its output from time 0, as one without a delay does; only
  the later changes take the delay (29.7, VALUE_SEMANTICS.md 5.9). It was
  x until the delay had passed (#26).
- An array of sequential UDP instances runs: its elements got one name
  between them, so the slots of their states clashed and kirimasim stopped
  with an internal error (28.3.6). Each element is now named with its
  indices, as an element of an array of module instances is.
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
- A design whose statements or expressions nest about 1000 deep, which
  slang accepts, no longer crashes kirimasim on macOS while it compiles:
  slang now does all its work on the thread the driver sized, not on a
  thread pool with the system's default stack of 512 KiB (RUNTIME_ABI.md
  5; #35).
- kirimasim stops before the stack of the thread it runs on runs out,
  on any thread and in any build, not only on the driver's thread of 512
  MiB: a call of the design with 1 MiB or less of stack left ends the run
  with the reason resource_limit (exit code 6) and the line `<file>:<line>:
  error: the stack of this thread is nearly used up [<instance> @ <time>]`,
  and front stops with the diagnostic `nested too deeply for the stack of
  this thread` (RUNTIME_ABI.md 5; #38). slang's own recursion, while it
  binds a deeply nested expression before front lowers it, is not
  checked.
