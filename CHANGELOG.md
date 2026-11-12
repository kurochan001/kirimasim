# Changelog

[日本語](CHANGELOG.ja.md)

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

No release yet. The Added section below sums up what works as of
2026-10-18; from then on, each change a user can see has an entry of its
own. CONTRIBUTING.md says what goes in this file.
Entries about intermediate implementation checkpoints retain the scope of that
revision. The [P1 inventory](docs/P1_SUPPORT.md) describes current support and
remaining boundaries.

### Added

- The items of `case inside` may now have side effects, such as function calls, and are evaluated in the order their case sets (12.5.4). Members of the `inside` operator with side effects remain unsupported.

- `inside` and `case inside` now take dynamic arrays and queues in the set, also inside a fixed array, each standing for its elements at the time of the operator (11.4.13; VALUE_SEMANTICS.md 3.10).

- `inside` and `case inside` now take the tolerance ranges `[A +/- B]` and `[A +%- B]`, in the type of A, with a real tolerance truncated (11.4.13; VALUE_SEMANTICS.md 3.10).

- Blocking writes to a bit, part or packed member of a class property now run, including runtime positions, compound assignments and elements of property arrays. An invalid position writes nothing, as for any variable (11.5.1; VALUE_SEMANTICS.md 3.11). Nonblocking writes of properties stay unsupported, and one legacy unit expectation has its separately approved migration applied.

- Supported class operations now execute in `always_comb` and `always_latch` with argument-only implicit sensitivity ([scope](docs/notes/class-implicit-execution.md)); the two legacy unit expectations have their separately approved migration applied.

- Explicitly clocked `always` and `always_ff` bodies now execute supported class operations while preserving ordinary trigger evaluation and object-observer rejection ([scope and checks](docs/notes/class-clocked-execution.md)).

- VPI whole static integral four-state nets now support immediate point-net deposits and registered-net force/release, sharing HDL ownership and complete callback snapshots ([scope and checks](docs/notes/vpi-net-write-execution.md)).

- Simple scalar state-dependent specify `if`/`ifnone` paths now select delays from conditions at functional arrival, retaining four-state enables, force behavior and public observations ([scope and checks](docs/notes/specify-state-execution.md)).

- Disable of a resolved deferred assertion label now cancels only that statement's pending reports, retaining siblings, evaluated arguments and settled history. Includes Observed/final and completed-owner checks ([contract and evidence](docs/notes/deferred-label-cancellation.md)).


- VPI `vpiObjTypeVal` now uses the reflected declaration type for reads,
  value-change callbacks and variable release, preserving the distinction between
  a scalar and a one-bit packed vector ([checks](docs/notes/vpi-object-value-formats.md)).

- VPI variable release now returns the post-release value through the caller's requested output format ([checks](docs/notes/vpi-release-results.md)).

- Class task-form `$cast` now uses the shared compatibility check and reports a design runtime error without changing the destination on failure ([scope](docs/notes/class-task-casts.md)).

- Direct `--compat=default|vcs|all` now selects the pinned frontend profiles ([scope](docs/notes/compatibility-execution.md)).

- The four documented frontend elaboration limits can now be selected directly
  for compilation, parse and SIR output ([scope and checks](docs/notes/elaboration-limits-execution.md)).

- Source `%p` and `%0p` now format supported typed scalar, fixed aggregate and
  dynamic-array/queue values in literal/runtime formats and eligible postponed
  output ([scope](docs/notes/source-pattern-formatting.md)).

- Class objects now coexist with ordinary signal/event sensitivity, waits,
  monitors and eligible event-controlled NBAs. A shared preflight check keeps
  object-dependent observers explicitly unsupported, including indirect calls
  and callbacks ([scope](docs/notes/class-independent-observers.md)).

- Eligible static partial inout net ports now use physical identity without
  directed value carriers or extra roots, including fixed-array elements,
  packed parts/concatenations and effective wired/pull/supply kinds
  ([scope](docs/notes/partial-inout-port-execution.md)).

- Integral input/output net ports requiring width/sign conversion or explicit
  casts now retain strong directed value carriers and use ordinary net
  resolution, including array elements and concatenations
  ([scope](docs/notes/converting-port-execution.md)).

- Static partial input/output net ports now merge effective wired, pull and
  supply kinds per physical bit and materialize one implicit root per identity
  component, preserving untouched neighbours and explicit physical origins
  ([scope](docs/notes/port-kind-execution.md)).

- Eligible input/output net ports now preserve physical strengths through
  fixed-array elements and their static packed selections/concatenations.
  Source alias bounds map to physical declaration-order ordinals; untouched
  elements and bits retain their values ([scope](docs/notes/array-element-port-execution.md)).

- Expression event controls in `always_ff`, including runtime bit/array
  selections, now share typed watches with ordinary event waits while keeping
  ff writer and timing restrictions. Mixed fixed/expression items, iff and
  startup diagnostics use the existing kernel monitor path
  ([scope](docs/notes/always-ff-expression-execution.md)).
- Named fork disable now ends child activity across join/join_any/join_none,
  returned calls and retained descendants, with ordinary scope IDs. It preserves
  unrelated children, scheduled NBAs and confirmed reports while canceling waits
  and unconfirmed assertions ([scope](docs/notes/named-fork-disable-execution.md)).
- Source class handles, constructors, typed properties, inheritance,
  virtual/direct functions and waiting tasks, interface views, covariant
  returns, shallow copy, object seeds and function-form class casts now
  lower to class SIR. Fixed property elements, ordinary call transport and
  class-valued queues use the same execution path. Dependency observation,
  property references/selected writes and managed event fields remain pending
  ([scope](docs/notes/class-source-execution.md)).
- Class SIR execution in the VM: staged constructors, field reads/writes,
  virtual/direct functions and waiting tasks, copies, casts, identity,
  object seeds, weak handles and typed roots through calls, captures,
  containers and pending assignments. Source class lowering, dependency
  observation, managed event fields and object formatting remain pending
  ([scope](docs/notes/class-vm-execution.md)).
- Shared VM heap-schema registration and typed class-value/property transport,
  including retained targets, transfer pins and class-aware automatic storage.
  This is a storage-adapter checkpoint; class instruction execution and source
  lowering remain unconnected ([scope](docs/notes/class-vm-storage.md)).
- Typed class SIR descriptors, virtual signatures, constructor phases,
  property/value operations and class-variable references, with verification
  of ownership, permissions, control flow and reference shapes. Class SIR is
  rejected before runtime allocation until its execution adapter is connected;
  source class execution remains unsupported
  ([scope](docs/notes/class-sir-runtime.md)).
- A shared-kernel class-object store provides typed identity, staged
  construction, property and bit-selection access, shallow copy, virtual
  dispatch, class-cast checks, object RNG state, joint container tracing and
  weak references. Retained automatic frames trace typed object fields.
  This is an implementation checkpoint; source class execution still needs
  SIR/front/VM integration ([scope](docs/notes/class-runtime-kernel.md)).
- Source warning options `-W<name>`, `-Wno-<name>` and `-Werror[=<name>]`
  reach slang in both `parse` and simulation, preserving its warning names,
  severity rules and option order (CLI.md 2.1).
- The VCD dump includes interface and program instances, as scopes of type
  `module` that `$dumpvars` counts as levels of instances, as Icarus 13.0
  does (WAVEFORM.md 2). Their variables were left out.
- Streaming targets that contain streams (`{<<4{{>>{t}}, g}} = v`): each
  inner stream reorders its own share of the source, level by level, as
  the inverse of packing (11.4.14.3). They were reported as unsupported.
- Writes to a bit or element chosen at run time inside a packed element
  chosen at run time (`t[i][j] = v`, also three levels deep), by blocking
  and compound assignments and by NBAs with and without a delay or an
  event; an index outside its parent writes nothing (7.4.5, 11.5.1). They
  were reported as unsupported; a runtime part-select (`t[i][k+:4]`)
  there still is.
- Slices of fixed-size unpacked arrays (`a[l:r]`, `a[b+:w]`, `a[b-:w]`,
  the position also at run time) as values, as targets of blocking,
  delayed and nonblocking assignments, in comparisons and as input, output
  and inout actuals, including slices of arrays of structures
  (VALUE_SEMANTICS.md 3.13, SIR_SPEC.md 8). They were reported as
  unsupported; elements outside the array read the default and are not
  written. A slice is still not taken as a `ref` actual.
- Automatic arrays of unpacked structures, arguments that are arrays of
  structures (`input`, `output`, `inout`, `ref`, `const ref`, matched from
  the left bound) and functions that return one (6.21, 13.5, 7.6;
  SIR_SPEC.md 4.2, 8). They were reported as unsupported; events inside
  arrays of structures still are.
- `ref`, `const ref` and `ref static` arguments of an unpacked structure
  type: the callee reads and writes the actual's members at once, members
  that are arrays, strings or wider than 64 bits included, and a `ref
  static` one takes NBAs (13.5.2, SIR_SPEC.md 8). They were reported as
  unsupported, as was a `ref` to a member of an element of an array of
  structures chosen at run time (`t(sa[i].n)`).
- Nonblocking assignments to a character of a string with a delay
  (`s[i] <= #1 c`), through string-array elements and `ref static`
  strings; they were reported as unsupported (SCHEDULER_SPEC.md 13).
- Nonblocking assignments without a delay to an element or packed bits of
  the static variable a `ref static` argument names (`r[i] <= v`,
  `r[i][p+:8] <= v`), applied in the NBA region of the slot that issues
  them, or the Re-NBA region from a program (10.4.2, 13.5.2). They were
  reported as unsupported; only the delayed form and the whole value ran.
- `--init-values=x|zero|one|random` selects defaults for uninitialized four-state integral variables and fresh memory elements, while preserving explicit initializers and standard invalid-read values (CLI.md 2.2).
- `--report-pulses` reports pending transitions cancelled by supported inertial-delay paths, with source positions and cancellation times (CLI.md 2.2).
- `--max-procs=<n>` limits unfinished runtime-created child processes; exceeding the limit runs final procedures and exits with code 6 (CLI.md 2.7).
- Mixed fixed/dynamic/packed `foreach` dimensions and supported omissions execute in declaration order, retaining loop state across waits; runtime array-query dimensions work for the documented fixed, dynamic-array, queue and string types (SIR_SPEC.md 5; SYSTEM_TASKS.md 8).
- Fork children share supported captured automatic variables and retain their storage across parent completion, following the approved activation and lifetime contract (SIR_SPEC.md 7.5; RUNTIME_ABI.md 4).
- Source event-controlled NBAs retain captured values, targets and controls independently of the issuing process, for the documented typed subset (SCHEDULER_SPEC.md 8; SIR_SPEC.md 7.3).
- Automatic scalar/fixed-array events get fresh declaration defaults, preserve identity through supported input arguments and declaration copies, and support runtime fixed-array event selection (SIR_SPEC.md 7.4/8; RUNTIME_ABI.md 6).
- Non-parameterized class static properties use shared typed storage and pre-time-zero initialization; object construction and parameterized static storage remain unsupported (SIR_SPEC.md 7.1; VALUE_SEMANTICS.md 1.4).
- Whole fixed-unpacked input/output ports and compatible net inout connections execute in declaration order, with element-wise net resolution and supported constant-element force/release and delays (SIR_SPEC.md 3.5; VALUE_SEMANTICS.md 5).
- All six directed MOS and six passive switch kinds execute on eligible static terminals with complete strength transport and shared-kernel timing; registered-net source force and broader partial-port identities remain unsupported (VALUE_SEMANTICS.md 5; SCHEDULER_SPEC.md 14).

- Common simulator input spellings: attached or separate `-I` include paths,
  `-D` macro definitions and `-y` library directories, plus `--libdir=<dir>`.
  Existing plus forms remain available. Omitted macro values mean `1`;
  explicit empty values and punctuation are preserved. Include and library
  search remain distinct. Both execution modes support these options.

- Runtime array reduction, stable ordering and min/max for fixed arrays,
  dynamic arrays and queues, including typed `with`, nested-container keys,
  retained element refs and snapshotting before effectful extrema keys.
  Four direct unit tests and 270 original source cases cover the approved
  contract. Effectful/unverified-pure ordering keys and unsupported aggregate
  representations remain explicit limitations.
- Binary input into multidimensional fixed integral memories, using numerical
  low-to-high traversal with the rightmost dimension varying fastest and
  linear start/count positions. Selected prefix expressions are captured
  once before later arguments; one-dimensional indexing stays unchanged.
  Add 156 original byte-model cases.

- `$countbits`, `$countones`, `$onehot`, `$onehot0` and `$isunknown` accept
  supported fixed unpacked arrays, records and arrays of records with integral
  leaves. They reuse the existing streaming conversion and kernel counter;
  136 original cases check wide/two-/four-state values and argument evaluation.
  Unsupported storage forms and dynamic/string bit streams remain separate.

- Typed pattern layouts, verification, text and `pattern.format` execution
  connect to the shared renderer. Eleven independent SIR tests cover nested
  values, packed fields, containers, guards, monitors and resource limits.
  Source `%p` still requires front's literal/runtime format bridge.

- A shared-kernel assignment-pattern renderer handles borrowed typed values,
  nested aggregates, enums, quoted SV strings and supplied handle identities.
  Iterative traversal, explicit shape/cycle/resource errors and C++20 header
  checks are covered by new tests. Source `%p` remains unsupported pending
  typed SIR/front integration.

- UDP lowering, verification and shared-kernel/VM transport accept more than
  32 inputs. Original sources through 257 inputs and independent value/SIR
  checks cover matching, state order, X/Z, delay and strength. Dense edge
  tables retain a pinned-slang compilation-scaling limitation, recorded with
  bounded original reproducers; no warning or dependency was relaxed.

- Fork-local declarations execute through the parent's existing lifetime and
  initialization rules; static storage is shared, declarations are not child
  processes, and declaration-only forks need no wait. Seventy-two original
  cases cover eight contexts, wide values, all join kinds and typed storage.
  Automatic capture still requires its shared-lifetime contract. The old
  rejection expectations are reviewed separately (6.21, 9.3.2).

- Three-state gates retain full-width L/H side masks across word boundaries,
  including 33–64-bit nets whose side operands occupy multiple words and
  short-circuit control expressions. Another 73 original cases match both
  measured Icarus versions. Delayed three-state outputs on resolved
  nets, unsupported at this step, are under the entry on their propagation
  below (28.6, 28.12; VALUE_SEMANTICS 5.8 and SIR_SPEC 3.5).

- Whole fixed-array, unpacked-structure and structure-array blocking
  intra-assignment delay/event/repeat controls retain typed private snapshots
  across waits and evaluate the actual target afterwards. Another 74 original
  cases cover eight contexts, wide four-state values, automatic ref calls,
  container roots and differing declared bounds. Structure-array copies now
  match each side by position for ordinary assignments and NBAs too. The six
  historical rejection expectations are reviewed separately from implementation
  (7.6, 9.4.5; existing SCHEDULER_SPEC 8).

- Whole fixed subarray assignment targets selected at run time, including
  structure elements' array members, and function return storage for structures
  containing fixed-array members. Explicit automatic aggregate array
  declarations retain their existing restriction.

- Blocking intra-assignment `repeat(count) @(...)` controls retain the full
  count once, preserve the earlier RHS and select the target after waiting.
  Nonpositive/X/Z counts skip the wait. Seventy-five original source cases
  cover typed counts, eight execution contexts, mixed/guarded events and
  scalar/container snapshots. Event-controlled NBAs remain unsupported
  (9.4.5; existing SCHEDULER_SPEC 8).

- The shared strength resolver exposes complete signed intervals before
  four-state projection, for arbitrary input intervals and masked driver
  bits. Existing value APIs use the same calculation. Five new tests check
  full endpoints against the independent point enumerator, including all
  158,400 interval pairs across eleven net kinds. Four compiled mutations
  expose errors missed by the previous value-only tests. MOS/switch source
  integration and strength-aware formatting remain work (VALUE_SEMANTICS 5.2–5.4).

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
- Whole fixed-array ports: a net-array connection shares its elements with
  the port, each declaration keeping its own bounds, and a value connection
  copies them; net arrays resolve the strengths of their drivers element by
  element. Constant element force and release, delays, and unconnected
  defaults and pulls work through them (7.6, 23.3.3; SIR_SPEC.md 3.5,
  VALUE_SEMANTICS.md 5). Before, these ports were reported as unsupported.
- Delayed three-state gates that drive resolved nets propagate their whole
  state: a delayed change of the L/H side is a change even when the
  four-state value stays x, and an equal pending state keeps its deadline
  (28.6, 28.16; SCHEDULER_SPEC.md 12, VALUE_SEMANTICS.md 5.8). Before, they
  were reported as unsupported.

### Changed

- Update unmodified slang to `6fbb158f`: a program's parameters can be named from outside it, as by a `defparam` (slang #2093) ([validation](docs/notes/slang-6fbb158f-update.md)).
- The judge of the test suites now gives `:incdirs:` and `:defines:` to kirimasim, as `+incdir+` and `-D` (TEST_SUITE.md).

- slang's errors `index-oob`, `string-int-concat` and `string-in-constraint` are now warnings by default, as the standard allows the code they reject (7.4.5, 11.4.12.2, 18.3); `-Werror=<name>` makes one an error again (CLI.md 2.1).

- The judge of the test suites compares the exit code only when a test gives `:expected_exit:`; an internal error or a crash still fails a test without it (TEST_SUITE.md).

- P1 now ends at a closed list instead of aiming at execution coverage at least as
  broad as Icarus 13.0: four conditions, including 188 fixed probes compared with
  Icarus 13.0. Forms beyond the list move to P4 and P5, and no design changes its
  result ([P1_SCOPE](docs/spec/P1_SCOPE.md#closed-exit-list-approved-2026-11-07),
  [proposal](docs/notes/p1-closed-exit-proposal.md)).

- Update unmodified slang to `d02b7cda`, including #2072 recovery and static
  function return lifetimes; retain strict executable driver checks under the
  new array compatibility relaxation ([validation](docs/notes/slang-d02b7cda-update.md)).

- P1 no longer requires Icarus's `wire real` extension. The confirmed `real_net`
  probe is reported out of scope without changing its raw failure; standard
  real features and independent faults retain their existing treatment
  ([decision](docs/notes/p1-wire-real-exception.md)).

- `atoi`, `atohex`, `atooct` and `atobin` read one leading `-`, as slang's
  constant evaluation does, so `"-5"` gives -5 where it gave 0 (6.16.9;
  VALUE_SEMANTICS.md 7.6, approved 2026-10-28). Underscores before the first
  digit are still read (`"_5"` gives 5), which slang does not, so front
  computes these conversions at run time wherever the call stands; only a
  parameter keeps slang's value. With `P = "-5"`, `P.atoi()` was -5 in an
  assignment and 0 as an argument of `$display`.

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
  checker as unsupported (P4, P3 before the 2026-10-24 renumbering;
  16.14.7, 17.4; #24).
- The phases are renumbered: a new P2 makes SIR v1 and the kernel boundary
  ready for tier 2, and C++ generation moves to P3, broader testbench
  features to P4, and switch-level work, multithreading and embedding to
  P5. Tier numbers and the coverage P1 must reach are unchanged
  (docs/notes/phase-renumbering.md).
- A write that another process makes, through a task it calls or through
  the action of a deferred assertion it runs, to a variable that
  `always_comb`, `always_latch` or `always_ff` writes is no longer an
  error: kirimasim follows slang, which keeps such writes legal on purpose
  because the standard is not clear on them (slang issue #2040). The design
  runs, and each write is an ordinary write of the variable
  (SCHEDULER_SPEC.md 6.4, SIR_SPEC.md 3.4). Before, front reported them
  with exit code 2.

### Fixed

- `%t` prints a signed argument with its sign and an argument wider than 64 bits whole. A negative value used to print as a large unsigned number that differed with the time scale (`int t = -1` gave 4294967295 or 18446744073709550616), and the upper bits of a wide value were dropped (SYSTEM_TASKS.md 3.7).
- An address in the file of `$readmemb` and `$readmemh` that needs more than 64 bits is outside the range. Only its low 64 bits used to be taken, so `@10000000000000002` loaded the word at address 2 without a warning (21.4, SYSTEM_TASKS.md 7.1).
- A variable that a continuous assignment drives takes the driver's value again when it is released from VPI (`vpiReleaseFlag`), as the `release` statement does. It used to keep the forced value, also after the driver changed, unless the design forced the same variable with a statement too (10.6.2, SCHEDULER_SPEC.md 14.3).
- `$monitor` and `$fmonitor` evaluate each argument once when they run. They used to evaluate it once to compare and again to print, so a function that counts its calls printed 2, 4, 6 for 1, 2, 3, and a `$urandom` argument printed another value than the one compared (21.2.3, SYSTEM_TASKS.md 4).
- `$writememb` and `$writememh` end the run with an error when the text cannot be written whole or the file cannot be closed, as they do when the file cannot be opened. A failed write used to be ignored (SYSTEM_TASKS.md 7.2).
- A VPI `cbAfterDelay` callback with delay 0 that is registered in the middle of a time slot is called in that time slot, as an Active event (a Reactive event when registered from a program). It used to be called only after that time slot had ended, after `$strobe` and `$monitor` had printed (SCHEDULER_SPEC.md 17.2).
- A port whose variable is an unpacked structure, or an array of them, is connected. Such a port used to be left unconnected without a message: an input kept its initial value and an output never reached the variable outside (23.3.3).
- A conditional operator with an x or z condition gives x for the bits that differ when both sides have one 2-state type that is not a plain vector, such as an enumeration. `c ? e1 : e2` used to print 0 for those bits, while the same expression with `int` sides, or stored in an `integer`, gave x (11.4.11, VALUE_SEMANTICS.md 3.9).
- An input port of a real, queue or dynamic array type declared with no `var` keyword (`input real r`, `input int q [$]`) runs, as the variable that `input var` declares. It used to end with an internal error of the SIR verifier.
- `%v` prints a 2-state value (a `bit` variable, a constant, an expression of them) with the strong strength, as it prints a 4-state value that is no net. It used to end with an internal error of the SIR verifier (SYSTEM_TASKS.md 3.10).
- A format with a width or a precision over 100000 is an error at the format (`format width over 100000`), and the design does not run. A width over the limit used to end with an internal error, and a precision near the largest integer printed `?` (SYSTEM_TASKS.md 3.3).
- An output port that is a variable has the initial value written on it: `output logic [3:0] y = 4'b1010`, and `output reg [2:0] q = 3'd4` among the port declarations. The value used to be ignored without a message, and the variable started as x or 0 (23.2.2.2, 6.8).
- A port expression that selects a bit or a part of a vector (`module m (.q(v[5:2]));`, `module m (v[5:2]);`) connects those bits alone. The select used to be ignored without a message, and the port was connected to the low bits of the whole vector (23.2.2.1).
- A chain of conditional operators whose result is a string or a class handle compiles in time and memory that grow with its length. With calls or reads through handles on its sides, the size used to double with each level: 14 levels took about two minutes and over 1 GB, and 16 ran out of memory (VALUE_SEMANTICS.md 3.9).
- `$fgets` and `%s` of `$fscanf` leave the zero bytes of a file out of a string variable, which holds none. The string used to keep them, so its length and its comparisons counted bytes that a string cannot have (6.16, VALUE_SEMANTICS.md 7.6).
- A constant part-select that reaches outside its vector (`v[5:2]` of a `logic [3:0]`) is a warning by default, `range-oob`, and runs as 11.5.1 says: the bits outside read as x (0 for a 2-state vector) and are dropped when written. It used to be an error; `-Werror=range-oob` makes it one again (CLI.md 2.1).
- An input port of an unpacked structure type declared with no `var` keyword (`input st_t s`) runs, as the variable that `input var` declares. It used to be reported as unsupported (P1_SUPPORT.md, Hierarchy).

- A chain of conditional operators whose sides call functions (or, since the fix above, read through class handles) no longer takes time and memory that double with each level to compile; a chain 18 deep did not finish in ten minutes and now takes a fraction of a second (11.4.11).

- An assignment to a part or a bit of a packed element of a queue or dynamic array whose element type is signed, such as `q[0][1] += 1` for `logic signed [3:0][7:0] q[$]`, no longer ends with an internal error (7.4.5, 11.8.1).

- `%m`, `%l` and `$printtimescale` print a name that holds a `%` (an escaped identifier) as it is; such a name used to be read as part of the format and ended the run with an internal error (21.2.1.6; SYSTEM_TASKS.md 3.8).

- `&&`, `||`, `->` and `<->` with a real operand beside an integral operand wider than 64 bits now take the real as true unless it is 0.0; it was always taken as false (11.4.7; VALUE_SEMANTICS.md 3.5).

- An operand that `&&`, `||`, `->` or `?:` skips is no longer evaluated when it reads a member through a class handle, so the guard `h != null && h.v == 3` no longer ends with "null class handle access" when `h` is null (11.3.5, 11.4.11; VALUE_SEMANTICS.md 3.1).

- Reading or writing `type_option` of a covergroup no longer crashes kirimasim; it reports that this is not supported yet.

- `===` and `!==` on strings no longer stop with an internal error; they compare as `==` and `!=` do (table 6-9, 11.4.5).

- `===` and `!==` on a real or shortreal operand, and `==?` and `!=?` on a class handle, are now rejected with a diagnostic instead of an internal error (table 11-1).

- A class that extends a base whose constructor has default arguments no longer stops with an internal error when no call of `super.new` is written. The implied call takes the default values, and `function new(default)` and `super.new(default)` pass the expanded arguments on (8.7, 8.17, 13.5.3).

- A `priority case` now stops evaluating its items at the first match, as a case without a qualifier does. Items after the match used to be evaluated too, and their side effects happened (12.5, 12.5.3; SCHEDULER_SPEC.md 15.1).

- At time 0, continuous assignments now start first, then other `always` procedures, then `initial` procedures, however the source orders them (SCHEDULER_SPEC.md 6.1). An `initial` written before an `assign` used to read x instead of the assigned value.

- A run that ends because every program has ended, or through `$exit`, now writes the line of the reason for ending (`kirimasim: info: finish at <time>`) on standard error. The implicit `$finish` of 24.3 and 24.7 has no argument, so it takes level 1 (20.2); an explicit `$finish(0)` stays silent. Standard output and exit codes do not change.

- VPI `cbNextSimTime` now ignores numeric time payloads and permits registration from read-only callbacks, while retaining format checks and actual next-slot delivery ([scope and checks](docs/notes/vpi-next-time-callback.md)).

- Ordinary `always @*` function bodies may read supported containers without false implicit-observer rejection; argument-only triggers and actual-read boundaries are preserved ([scope](docs/notes/wildcard-function-dependencies.md)).

- Packed VPI bit selections are unsigned independently of their parent declarations, preserving positive integer, real, decimal and callback values for a selected 1 ([scope and checks](docs/notes/vpi-packed-bit-values.md)).

- Source driver conflicts exposed by relaxed warnings or compatibility now produce source-position compilation errors before SIR lowering, preserving the strict executable writer rules.

- Keep class SIR metadata compact so deeply nested expression lowering retains
  its existing native stack budget in sanitizer builds.
- An output or inout argument of an unpacked structure type, or an array
  of them, is copied back to the element its indices choose on return,
  as SIR_SPEC.md 8 says (4.9.7). The indices were found at the call: a
  callee that changed them wrote back into the old element, and a task
  that waited stopped with an internal verifier error.
- NBAs that keep waking each other within one time slot, which run on
  only with `--max-slot-events=0`, no longer hold memory without bound:
  the values of NBAs with no delay are freed as soon as none of them waits
  (SCHEDULER_SPEC.md 5, approved 2026-10-30). Such a loop grew by about
  840 MiB a second; `--stats=json` reports the new counter
  `nba.slot_peak`.
- An NBA to a character of a string writes its byte into the string held
  when it applies, with or without a delay or an event control
  (SCHEDULER_SPEC.md 13, approved 2026-10-30). It wrote back the whole
  string as it was when the NBA was issued, so `s[0] <= "a"; s[1] <= "b";`
  kept only the second character and undid any write in between.
- `&&`, `||`, `->` and `?:` skip a function call in the operand they do
  not evaluate also when the call stands inside `inside`, an assignment
  pattern or a streaming concatenation (11.3.5, 11.4.11; VALUE_SEMANTICS.md
  3.1, 3.9). The call ran, and its side effects happened.
- `atoi` of a constant string with an underscore before the first digit,
  and a unary plus of a constant with x or z bits, give their run-time
  value also inside `inside`, an assignment pattern or a streaming
  concatenation (SIR_SPEC.md 6). slang's constant was used there.
- `priority if` evaluates its conditions in the order written only up to
  the first true one (12.4.2; SCHEDULER_SPEC.md 15.1). It evaluated every
  condition, as `unique` and `unique0` do, so the side effects of the
  later ones happened.
- The SIR verifier rejects a value instruction with a slot operand, a
  `load` of anything but a slot, a `repl` count past 64 bits or whose
  product with the width wraps, and a `partsel` position that wraps
  (SIR_SPEC.md 5.1, 10). These passed, and the tiers then read past
  their buffers or gave different results.
- An integer converted to `shortreal` is rounded once to the nearest
  float (VALUE_SEMANTICS.md 1.5). It went through a double, so a value
  of more than 53 significant bits just past a tie, such as
  `64'h1000_0010_0000_0001`, came out one step low.
- kirimasim's own code is built with `-ffp-contract=off`, as CODEGEN_SPEC.md
  ("Floating point") says, so that a `real` result has the same bits on
  arm64 and x86-64; arm64 builds fused a multiply and an add before.
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
- Calls nested more than 10000 deep in one process stop the run at the
  call, with `final` (exit code 6, as under Changed); they were an
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
  is an error; formats known only at run time are under Added
  (SYSTEM_TASKS.md 3.13).
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
- Killing a display/file monitor callback now detaches its registration and
  heap dependencies before its frame can be released. Ended callbacks are
  not invoked again on later cell changes; other file monitors remain live.
  Twenty new source cases and scheduler lifetime/resource-failure tests cover
  queue/dynamic values, file closure, descriptor reuse and hook restoration.

- A display monitor can observe a change in an existing queue cell without
  replacing its container handle. Heap reads are captured during the actual
  Postponed callback and detached with the registration's lifetime. The new
  heap-backed file-monitor combinations are now covered by the follow-up
  matrix and callback lifetime checks described above.

- Aggregate blocking and nonblocking assignments capture all RHS values
  before runtime target selectors under the approved evaluation policy.
  Container leaves snapshot before later field effects. Seventy-two original
  source cases cover eight contexts, wide/two-/four-state payloads,
  real/string members, runtime prefixes and deep values (VALUE_SEMANTICS
  3.1/3.13, SIR_SPEC 7.3; IEEE leaves relative side order unspecified).

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

- `string.atoreal()` reads one leading `+` or `-` and then a real constant
  as 5.7.2 writes it, so a negative number written by `realtoa` reads back
  (6.16.10, 6.16.15; VALUE_SEMANTICS.md 7.6, approved 2026-10-28). Since
  5eae2ecf a leading sign gave 0, and four tests of the P1 population
  failed. Underscores are dropped, and leading white space, a leading point
  or a sign not followed by a digit still give 0 (#57).
