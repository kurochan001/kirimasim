# P1 supported features

[日本語](P1_SUPPORT.ja.md) · This English text is the main version.

The [approved P1 scope](spec/P1_SCOPE.md) now targets Icarus Verilog
13.0-or-broader execution support, advancing the necessary later-phase
mechanisms. This is a target; unsupported forms below remain unimplemented.
See the [comparison progress](notes/p1-icarus-progress.md) for measured gaps.
The remaining-work section distinguishes required P1 coverage from broader
later-phase plans.

This inventory describes tier 1 as of 2026-10-24, including dynamic arrays
and queues, wide three-state masks, whole-aggregate blocking timing, value capture before target effects, repeated blocking
event controls, corrected integral `$fread`
copyback, delayed NBA snapshots,
bounded packed selections and mixed named-event waits. The strength-interval
API is documented separately as a kernel foundation; it does not enable
MOS/switch execution. Check `kirimasim --help` for a build's options and
`--version` for its revision. The [current validation and pending reviews](notes/p1-icarus-progress.md#current-implementation-and-review-status-2026-10-24)
separate the latest comparison from historical measurements.

P1 is in progress. “Supported” below means that the listed forms can be
lowered to SIR and run by the interpreter, with tests in this repository.
It does not promise every combination of those features. Acceptance by
`kirimasim parse` only establishes front-end acceptance, not execution
support. The [specifications](spec/SIR_SPEC.md) also describe planned work;
this document lists what is implemented now.

For runnable examples of recent additions, see [P1 features](P1_FEATURES.md).
For build instructions and completion criteria, see the [README](../README.md).

## Language and execution

| Area | Supported forms | Important limits |
| --- | --- | --- |
| Front end | Preprocessing, parsing, type checking and elaboration through unmodified, pinned slang | slang accepting a construct does not make it executable in kirimasim |
| Hierarchy | Modules; `input`, `output`, `inout`; parameters, `localparam`, `defparam`; generate blocks; instance arrays; hierarchical names | Module `ref` ports and part-select connections to `inout` ports are not implemented |
| Shared declarations | Packages, imports, `$unit`, interfaces and modports | Virtual interfaces and classes are not implemented |
| Configuration | Source-level configs, top selection and bindings; specparams, including in specify blocks | Library map files, specify module paths and timing checks are not supported |
| Integral values | Arbitrary-width 2-state and 4-state values, signed and unsigned arithmetic, X/Z propagation and conversions | Practical memory and execution limits still apply |
| Other scalar values | `real`, `shortreal`, `realtime`, `string`, named events, null `chandle` | Non-null handles need DPI-C, which is not implemented |
| Aggregates | Packed/unpacked structures, packed unions, fixed unpacked arrays, multidimensional arrays, static arrays of structures, unpacked array parameters | Dynamic arrays/queues have the tested subset below; associative arrays and unpacked unions are not implemented; array slices are not generally assignable |
| Enumerations | Enum values and `first`, `last`, `num`, `name`, `next`, `prev` | Runtime `next(n)` / `prev(n)` counts are converted to 32-bit unsigned values; see the example guide |
| Expressions | Arithmetic, bitwise, logical, reduction and comparison operations; shifts; ternary expressions; concatenation, replication, casts, `inside`, wildcard equality, assignment operators, increments and assignment patterns | Pattern-matching `matches` forms of `if` and the conditional operator are not implemented |
| Selections | Packed bit/part/indexed-part selects, packed dimensions, fixed unpacked-array indexing, structure members; runtime writes/compound reads inside a statically bounded packed member or part, including NBA/delayed NBA | Runtime child selects whose parent position also varies, or whose constant parent range itself lies outside storage, remain unsupported |
| Streaming | `>>` and `<<` with constant slice sizes; fixed-array and structure operands; unpacking into integral, fixed-array and structure targets, including widths above 64 bits; blocking and nonblocking assignments, including delayed streaming NBAs | No `with` clause, string target or nested streaming target; automatic targets cannot receive an NBA |
| Branches and loops | `if`, `case`, `casez`, `casex`, `case inside`, `randcase`; `unique`, `unique0`, `priority` reports; `for` (including `for (;;)`), `while`, `do`, `repeat`, `forever`, `foreach`, `break`, `continue`, `return` | `randsequence` and `wait_order` are not implemented |
| Procedures | `initial`, `always`, `always_comb`, `always_latch`, `always_ff`, `final`; named programs and anonymous-program subroutines | Programs use Reactive/Re-Inactive/Re-NBA and program completion rules; `final` cannot wait |
| Assignment and delay | Blocking and nonblocking assignment; scalar and whole-aggregate blocking intra-assignment delay/event/repeat controls, `#` controls, time units/precision and rounding, min/typ/max selection; delayed NBAs to runtime fixed-array elements, packed bit/part selects and concatenations; fixed-array/unpacked-structure delayed NBA snapshots | Event-controlled NBAs and delayed NBAs through refs/string characters are not implemented; automatic NBA targets remain illegal |
| Event controls | Signal edges, integral expressions, `iff`, `wait (expr)`, named events mixed with signals/watches, per-item named-event `iff`, `->`, `->>`, `.triggered` | Existing variable-write restrictions on event/wait conditions remain; no runtime-indexed named event, event-controlled `->>`, or `always_ff` trigger on a computed expression |
| Concurrency | `fork` / `join`, `join_any`, `join_none`, `wait fork`, `disable fork`, parent execution of fork-local declarations with shared static storage, disable of supported named blocks and tasks | Automatic variables captured by fork branches and disable of a named fork block have unsupported forms; automatic declarations retain their usual type limitations |
| Subroutines | Static/automatic functions and tasks, recursion, default arguments, `input`, `output`, `inout`, `ref`, `const ref`; fixed-array and unpacked-structure arguments/results; tasks that wait | No unpacked-structure `ref` argument, array-of-structures argument, automatic array of structures, or array-slice output/inout actual |
| Nets and resolution | `wire`, `tri`, `wand`, `wor`, `tri0`, `tri1`, `supply0`, `supply1`, `uwire`; multiple drivers and drive strengths | No `trireg`, user-defined nettypes or `interconnect` |
| Continuous drivers | Continuous assignments and their rise/fall/turn-off delays, including net declaration assignments | A bare delayed net declaration such as `wire #5 n;` is not implemented; its delay would apply to every driver |
| Primitives | Built-in logic gates and pulls; combinational and sequential UDPs | No MOS/bidirectional switches or UDP instances with unconnected ports |
| Net aliases | Aliases of compatible whole nets | No partial/concatenated/array aliases, incompatible net types, or merging groups already joined separately by aliases or ports |
| Procedural drivers | `assign` / `deassign`, `force` / `release` on supported integral targets | No real/whole-unpacked-array target or force of a net connected through a port |
| Assertions | Immediate `assert`, `assume`, `cover`; deferred immediate assertions (`#0`, `final`) | No concurrent assertions or module-item deferred assertions; a deferred assertion action cannot call a task that may wait |
| Scheduler | All time-slot regions and the reference scheduling algorithm; pending reports in Observed; strobe/monitor output in Postponed | Program initial threads inherit their group and origin through calls/forks; clocking blocks and concurrent assertions remain unsupported |

`randcase` weights wider than 32 bits are not implemented.

### Aggregate assignment capture

Blocking and nonblocking aggregate assignments capture every RHS value
before runtime target selectors. Container leaves snapshot before later
pattern members can mutate their source. Whole fixed subarrays selected
at run time, including structure elements' array members, use the same
stores for ordinary assignment, NBA and delayed NBA. Automatic/static
functions can return structures with fixed-array members. Explicit automatic
structure variables with array members retain their existing limitation.
The [72 original source cases and verification](notes/aggregate-value-capture.md)
cover these forms. Another [74 original cases](notes/blocking-aggregate-timing.md) cover
whole-aggregate blocking delay/event/repeat, per-activation typed snapshots
and differing-bound structure copies. Event-controlled NBAs remain unsupported.

### Repeated blocking event controls

Blocking intra-assignment `repeat(count) @(...)` retains the RHS and count,
then chooses its target after waiting. Nonpositive/X/Z counts skip all event
waits, preserving count width and signedness. Named, mixed, watched and guarded
events use the existing wait machinery. The [75 original source cases](notes/repeated-event-controls.md)
include automatic targets, real/string values, packed concatenations and
container snapshots. Whole fixed arrays and unpacked structures now use the
same blocking timing rules. Event-controlled NBAs remain unsupported.

### Strength-resolution kernel

Three-state gates now carry their L/H side masks through wide vectors, including
unaligned 64-bit boundaries. Another [73 original cases](notes/wide-gate-strength.md)
match both measured Icarus versions. Masked and unmasked drivers share the
existing kernel resolver; this is not arbitrary MOS interval transport.
Delayed three-state outputs on resolved nets remain unsupported, including
short-circuit controls; their [side-timing proposal](notes/delayed-gate-side-proposal.md)
awaits approval.

The shared kernel now exposes complete strength intervals before four-state
projection, following the approved VALUE_SEMANTICS 5.2–5.4 rule. Five added
unit tests compare full endpoints, including 158,400 interval pairs under
11 net kinds. Four compiled mutations pass the old value-only tests but
fail the new range assertions. See the [API and evidence](notes/strength-range-foundation.md).
This is the kernel foundation for propagation; MOS/bidirectional primitives,
strength-aware formatting and resolved-interval metadata through SIR/VM remain unsupported.
The [MOS/switch investigation](notes/mos-switch-investigation.md) preserves
28 exploratory reference programs and the remaining semantic questions;
these observations are not additional implemented features or approved oracles.
The [complete execution contract proposal](notes/mos-switch-proposal.md) is
pending review, including startup and full-strength timing decisions.

### Fork-local declarations

Declarations use the parent's lifetime/initialization path rather than becoming
children. Static storage is shared; uncaptured automatic initialization runs
per entry, including declaration-only forks. [Seventy-two source cases and limits](notes/fork-local-declarations.md)
cover eight contexts and typed storage. Automatic variables captured by children
remain unsupported and are explicitly measured in the expanded comparison.
The [shared-storage and automatic-event proposal](notes/automatic-capture-proposal.md)
is pending review; automatic event variables are also still unsupported.

### Program completion

Programs: initial procedures and continuous assignments use the reactive set,
and calls/forks inherit the caller's group and originating program.
`$exit` ends that origin's initial threads and descendants, and is ignored
outside a program-initial thread. Once all initials of a program end, its
remaining descendants end; after all programs with initials end, simulation
finishes and runs final procedures. Empty programs cause no automatic finish.
The [program matrix](../tools/gen_program_execution_matrix.py) covers 124
suite cases; seven kernel tests and a SIR test cover lifetime, reuse and
metadata. The CLI checks 15 illegal-input cases and two program identities.
This does not enable classes, clocking blocks or concurrent assertions.
Program scopes are not yet included in VCD dumping (WAVEFORM 7).

## Dynamic-container foundation

The [execution amendment](notes/dynamic-containers-proposal.md) was approved
on 2026-10-23. `Context::heap()` implements typed container storage, deep
copying, stable element cells, bounded queue mutation and exact tracing.
Allocation failures roll back new objects; reclaimed entries use generations
so stale tokens cannot name replacement values. These are kernel APIs,
covered by [15 original unit tests](../tests/unit/heap_test.cpp),
[16 conversion/snapshot/descriptor/observation tests](../tests/unit/heap_conversion_test.cpp) and an independent
[`valsem` container model](../src/valsem/include/kirimasim/valsem/containers.h).
The 100-sample check passed 2,520,000 operations; all six deliberate heap
mutations were caught. The heap matrix is included in nightly reference checks.

Copies now accept equivalent nested queue shapes with different capacities;
each retained level uses the destination capacity and propagates truncation
warnings. Shape equivalence is registered once without runtime allocation or
native recursion. `Heap::snapshot` preserves the actual source type/capacity.
The new tests cover 432 type/bound/value combinations, nine value-entry paths,
branching aggregates, 2,500 nested levels and allocation failure at every
stage of ten copy/mutation/snapshot paths. Six additional conversion mutations
were caught by actual heap assertions, and the restored baseline passes.

Typed container descriptors, receiver refs, 16 SIR operations, verifier and
text support are implemented ([SIR_SPEC 5.8](spec/SIR_SPEC.md#58-typed-container-encoding-approved-contract-of-2026-10-23)).
Ten new SIR tests cover 96 positive type/bound combinations, 49 malformed
cases, argument bindings and fixed arrays of container receivers.
The VM now dispatches all 16 instructions to the shared heap. Its
[24 independent SIR tests](../tests/unit/vm_container_test.cpp) cover 216
element/bound/storage combinations, 72 ref-call combinations, selected fixed
arrays, detached cells across task waits, packed writes against `valsem`,
argument side effects, invalid indices, method warning positions and errors.
Actual variable types survive equivalent ref formals, empty values and
selection through forwarded array refs. Size errors and heap allocation
failure map to Error/ResourceLimit with source positions; method warnings
are given once per source position. Eight deliberate VM mutations were caught.

Automatic collection now runs at drained region/slot boundaries. Typed roots
cover slots and fixed arrays, live process/task activations, references to
detached cells, deferred call arguments, NBA snapshots, pending drivers and
held storage. Primitive words are never guessed to be handles. Tests exercise
24 repeated replacement/churn combinations, 12 NBA cases, eight fixed-array
cases, 16 deferred-call cases and root-only changes; pool tables stay bounded
over 100 rounds. Failed marking retains every value. No collection runs inside
a process callback, after termination is requested or during final cleanup.

The heap also exposes read/change hooks: container structure and individual
cell values have distinct identities; notifications follow committed changes,
while equal normalized writes, failed operations and invalid selections do not
emit changes. Five tests cover 40 write combinations, queue bounds, nested
values, failures and hook ownership/isolation. Eight observation mutations were
caught by actual assertions. The scheduler now captures typed heap reads during
monitor evaluation, keeps weak object/cell watch markers and queues notices in
the shared change order. Dependencies reattach after selection or binding
changes; detached cell refs retain their own identity. VM guard/watch calls and
forwarded refs use the same mechanism. Private containers of called functions
are excluded, while global and direct hierarchical reads remain dependencies.
Five new scheduler tests cover precise reevaluation, cancellation/reuse,
bounded lists, hook ownership and resource failures. VM tests add 16 private/
global/hierarchical cases, eight direct/called wait cases and four cleanup/
failure cases. Eight deliberate monitor mutations were caught.

Nonblocking assignment to a dynamically sized array element is illegal
(6.21, 10.4.2), including through a cell ref. Slang rejects direct source forms;
the VM gives the same reason for hand-built SIR. Element NBAs are not a missing
P1 feature. Whole-value NBAs to eligible static variables are a different case.

Front lowering now connects the shared heap to executable SystemVerilog.
The [original source matrix](../tools/gen_dynamic_container_matrix.py) has 240
cases: eight execution contexts, five integral widths (1, 4, 32, 65, 129),
two/four-state elements and additional mechanism combinations.
A [sequence matrix](../tools/gen_container_sequence_matrix.py) adds 256 cases
across the same eight contexts and six widths (also 64), for 496 source cases
in total. It checks fixed/dynamic/queue operands, returned arrays, nested
actual bounds, value independence, held refs and side effects. The
[CLI checks](../tests/cli/run_dynamic_container_tests.py) cover 11 positioned
errors. Existing source bodies and expectations are unchanged.

- Dynamic-array `new[size]`, optional fixed/dynamic initializers, resize,
  `size()` and `delete()`; queue patterns, scalar concatenation, indexing,
  `[$]`/`[$+1]`, delete/insert/push/pop methods and actual destination bounds.
- Integral, real, shortreal, string, null-chandle and nested-container values;
  fixed arrays and unpacked structures containing containers. Assignment,
  initialization, input/output/inout arguments and function returns copy values.
- Ref arguments retain actual bounds, including nested equivalent queue types.
  Waiting calls retain moved/detached element cells. Method receiver bindings
  precede argument evaluation; RHS snapshots precede target selectors.
- Whole-value NBAs to eligible static variables, delayed NBAs and blocking
  intra-assignment delays retain their captured values. Explicit `wait` and
  expression watches use typed heap dependencies, including an empty ref
  forwarded to a called function.
- Unpacked concatenation into dynamic arrays/queues accepts scalar elements,
  nested elements, fixed arrays (including function results), dynamic arrays
  and queues. Operands are evaluated once in source order. Buffers are
  unbounded until the actual destination applies its bounds.
- Reading `q[low:high]` slices evaluates the queue once and snapshots its
  value before the bounds. Bounds are evaluated once left to right; `$`
  uses the captured source length. Negative lows and excess highs are
  clipped; reversed, X/Z and disjoint ranges return empty queues. Wide
  unsigned bounds retain an extra sign bit during normalization.
- `foreach` over one or more explicitly named dynamic dimensions, including
  waits/continue in the body; first-dimension `$size`, `$left`, `$right`, `$low`,
  `$high` and `$increment` queries.

Remaining forms include queue slices as assignment targets,
container conditional values, dynamic-container reduction/ordering/locator methods, mixed
or omitted dynamic foreach dimensions, runtime/nested query dimensions and
fixed-aggregate/class/event elements. Implicit sensitivity (`always_comb`,
`always_latch`, `always @*`, continuous assignments) to container values gives
an explicit unsupported diagnostic: static trigger ranges cannot yet encode
heap changes. Fork capture of enclosing automatic storage remains the existing
SIR_SPEC 7.5 limitation. Class/DPI/VPI roots and complete Icarus parity remain
unfinished. These are incremental language features, not complete container
support.

Ordering methods on fixed arrays also remain unimplemented.
The [array-method amendment](notes/array-methods-proposal.md) for reductions
and ordering is awaiting approval under P1_SCOPE.
`min()` and `max()` also remain unsupported. Their separate
[execution proposal](notes/array-extrema-proposal.md) is awaiting review of
traversal, equal/unknown keys and key side effects. The checked standard
returns at most one element, including tied extreme keys; the installed
Icarus's duplicate-result observations are recorded as reference mismatches.

## System tasks, functions and methods

The names in this table are grouped by purpose. For display/write/strobe/
monitor families, the `b`, `o` and `h` suffix variants are supported too,
including the file-output forms and `$swrite`.

| Purpose | Supported names / forms |
| --- | --- |
| Console output | `$display`, `$write`, `$strobe`, `$monitor`, `$monitoron`, `$monitoroff` |
| String output | `$swrite`, `$sformat`, `$sformatf`; `$psprintf` as an alias of `$sformatf` |
| Diagnostics and termination | `$fatal`, `$error`, `$warning`, `$info`, `$finish`, `$stop`, `$stacktrace` (task and function forms) |
| Time | `$time`, `$stime`, `$realtime`, `$timeunit`, `$timeprecision`, `$timeformat`, `$printtimescale` |
| Conversions | `$signed`, `$unsigned`, `$rtoi`, `$itor`, `$realtobits`, `$bitstoreal`, `$shortrealtobits`, `$bitstoshortreal` |
| Type and array queries | `$bits`, `$typename`, `$isunbounded`, `$dimensions`, `$unpacked_dimensions`, `$left`, `$right`, `$low`, `$high`, `$increment`, `$size` on supported types |
| Bit queries | `$countbits`, `$countones`, `$onehot`, `$onehot0`, `$isunknown` |
| Integer mathematics | `$clog2` |
| Real mathematics | `$ln`, `$log10`, `$exp`, `$sqrt`, `$pow`, `$floor`, `$ceil`, `$sin`, `$cos`, `$tan`, `$asin`, `$acos`, `$atan`, `$atan2`, `$hypot`, `$sinh`, `$cosh`, `$tanh`, `$asinh`, `$acosh`, `$atanh` |
| String methods | `len`, `getc`, `putc`, `toupper`, `tolower`, `compare`, `icompare`, `substr`, `atoi`, `atohex`, `atooct`, `atobin`, `atoreal`, `itoa`, `hextoa`, `octtoa`, `bintoa`, `realtoa`; character indexing |
| Plusargs | `$test$plusargs`, `$value$plusargs`; command-line `+name` / `+name=value` |
| File management/output | `$fopen`, `$fclose`, `$fflush`, `$fdisplay`, `$fwrite`, `$fstrobe`, `$fmonitor` |
| File input/position | `$fgetc`, `$ungetc`, `$fgets`, `$fscanf`, `$fread`, `$ftell`, `$fseek`, `$rewind`, `$feof`, `$ferror`; STDIN |
| String input | `$sscanf` |
| Memory files | `$readmemb`, `$readmemh`, `$writememb`, `$writememh`, including address ranges |
| Random numbers | `$random`, `$dist_uniform`, `$dist_normal`, `$dist_exponential`, `$dist_poisson`, `$dist_chi_square`, `$dist_t`, `$dist_erlang`; `$urandom`, `$urandom_range` with per-process generators seeded by `--seed` |
| VCD control | `$dumpfile`, `$dumpvars`, `$dumpoff`, `$dumpon`, `$dumpall`, `$dumplimit`, `$dumpflush` |

### Formatting and input limits

- `$strobe` and `$monitor` arguments cannot read automatic variables.
- Output conversions: `%b`, `%o`, `%h` / `%x`, `%d`, `%c`, `%s`, `%e`,
  `%f`, `%g`, `%t`, `%m`, `%l`, `%%`, with widths and supported flags.
  Precision and the `+`, space and `#` flags are for real conversions.
  `%u` and `%z` write raw 32-bit words in native byte order, least
  significant word first, with zero padding above the value's width;
  `%u` clears X/Z. Console, file, postponed and string-format output use
  the same implementation. Packed destinations retain NUL bytes; SV strings
  omit them. `%p` and `%v` are not implemented.
- `$sformat` and `$sformatf` accept runtime string or packed-integral
  formats. Missing or extra values warn and continue; X/Z bits in a packed
  format are not supported. Literal string values after the format remain
  values, even when they contain percent signs.
- `$sscanf` and `$fscanf` accept runtime formats: binary/octal/decimal/hex,
  character/string and real conversions, field widths, assignment suppression
  (`*`) and `%%`. Unformatted `%u`/`%z` (also uppercase) read complete
  native 32-bit words into integral/enum/packed targets. Omitted or zero
  width uses the destination bit width; explicit width sets transfer bits.
  Raw whitespace/NUL is payload, and short fields leave the target unchanged.
  Scan conversions `%t`, `%v` and `%m` are not implemented. An X/Z packed format returns `-1` without reading input;
  insufficient output arguments warn and stop before that conversion.
- `$value$plusargs` accepts runtime formats with one conversion:
  `%d`, `%o`, `%h` / `%x`, `%b`, `%e`, `%f`, `%g`, `%s`, including uppercase
  and `%0` forms. The first matching argument wins. No match leaves the
  output unchanged. Multiple conversions, a leading `%%` prefix and X/Z
  packed formats are not supported.
- `$fread` supports scalar integral destinations and one-dimensional fixed
  integral memories. On integral destinations, optional start/count values
  do not limit input; supplied expressions still run once left to right.
  Under SIR_SPEC 8, an integral actual is evaluated for copy-in and its
  destination is found again when a positive byte count is copied back.
  Direct heap-element expressions select the then-current container;
  actual refs retain their established reference semantics. Partial input
  retains unfilled copy-in bits; zero-byte input does not copy back. See the
  [oracle and implementation correction](notes/fread-copyback-correction.md).
  The
  [153-case matrix](../tools/gen_scalar_fread_matrix.py) covers eight widths,
  two/four states and nine execution contexts, plus argument/storage checks.
  One-dimensional memories of unpacked structures may
  contain nested structures with integral leaves. Whole multidimensional
  integral memories, dynamic memories, array/real structure members,
  multidimensional structure memories and unpacked unions are not supported.
  An original two-dimensional byte-memory probe passes the installed Icarus
  but is rejected here; see the [comparison record](notes/p1-icarus-progress.md).
- Memory-file operations accept fixed memories passed through `ref`,
  including forwarding, waiting calls, automatic storage and references
  to array rows. Writemem also accepts `const ref`. See
  [system tasks](spec/SYSTEM_TASKS.md) and [examples](P1_FEATURES.md).

## Command line and waveforms

| Purpose | Implemented interface |
| --- | --- |
| Run / inspect | `kirimasim [options] <files> [+plusargs]`, `kirimasim parse`, `--help`, `--version` |
| Source input | `-f`, `+incdir+`, `+define+`, `--top` through slang |
| Time selection | `--timescale=<unit>/<precision>`, `--mintypmax=min\|typ\|max` |
| Simulation limits | `--max-time`, `--max-time-ok`, `--max-zero-delay`, `--max-slot-events`, `--max-steps`, `--max-call-depth` |
| Live output | `--autoflush` flushes stdout after each display/write/strobe/monitor, including file tasks directed to stdout; default off |
| Stop policy | `--stop=finish\|ignore\|error`; default `finish`; `ignore` resumes the caller, `error` ends with code 1 and reason `stop`; `$stop(2)` requests statistics at the eventual end |
| Wall-clock limit | `--max-wall-time=<seconds>`; whole seconds, 0 disables; compilation excluded; ends with code 4 / `wall_time_limit` and runs `final`; `--max-time-ok` does not apply |
| Random seed | `--seed=<n>` |
| Inspection | `--dump-sir`, `--stats=json` |
| Bounds diagnostics (from `dec94ff`) | `--bounds-check=off\|warn` (default `off`): warn once per source position for invalid packed selections/fixed-array indices and print final access counts; normal read/write/NBA values are preserved |
| VCD output | `--wave=<file>.vcd`, `--wave-scope=<pattern>[,<pattern>...]`, `--wave-glitches`; 4-state waveforms, also controlled by the dump tasks above |

`--max-scc-iter` is accepted but has no effect until P2's static islands.
Other options described in the [CLI specification](spec/CLI.md) are not
automatically implemented: for example, `--cond-x`, `--init-values` and
`--max-procs` are not current driver options. Use `--help` to inspect a build.
FST output is not implemented. The wall-clock limit is checked before events
and at VM branches, including static initialization; it does not preempt
a blocking host operation. Final procedures finish without that deadline.

## Remaining work and scope

| Status | Features |
| --- | --- |
| Required P1 execution gaps | The Icarus-supported subset of dynamic containers, classes, VPI, switch-level modeling, specify paths and timing facilities; FST, `%p`, resolved-strength `%v`, unsupported `$fread` shapes and the limits listed above |
| P1 container integration | Shared heap, typed SIR/VM, automatic collection and heap waits are connected to 496 source cases; remaining language forms are listed above |
| P2 | Tier 2 C++ generation, static-island analysis and further RTL performance work |
| Broader later-phase plans | Support beyond the required Icarus coverage, including broader UVM, constrained randomization, covergroups, concurrent assertions/sampled-value functions, clocking, virtual interfaces, `let` and DPI-C; previous P3/P4 labels cannot defer a required P1 mechanism |
| Extensions and external interfaces to inventory | SDF, encrypted IP, legacy PLI, library maps and tool-specific extensions; compare original probes and define compatibility policy before claiming support |
| Outside the current language targets | VHDL/mixed-language and analog/mixed-signal designs |

The [approved scope](spec/P1_SCOPE.md) controls this classification. Features
such as checkers, `randsequence`, user-defined nettypes, `interconnect` and
`trireg` remain unsupported; whether a form is required in P1 depends on
verified Icarus coverage. Other planned tooling includes an interactive
debugger and a standalone Python API.

There is no compiled-simulation backend yet: executable language features
in this inventory run in tier 1. Python integration through cocotb depends
on the unfinished VPI work; required Icarus-supported interfaces belong to
P1. Completion requires the README gates and functional coverage evidence.

## Evidence and maintenance

- [Simulation suite](../tests/suite/): executable examples and expectations,
  grouped under `values`, `proc`, `sched`, `cont`, `nets`, `hier` and others.
- [Unit tests](../tests/unit/): value semantics, scheduler and runtime checks.
- [Unsupported-form matrix](../tools/gen_unsupported_matrix.py) and its
  [CLI fixtures](../tests/cli/unsupported_matrix/): specific rejected forms
  and source-position diagnostics. This is more precise than a keyword list.
- Mechanism matrices for [streaming](../tools/gen_stream_matrix.py),
  [enum steps](../tools/gen_enum_step_matrix.py),
  [runtime scans](../tools/gen_runtime_scan_matrix.py),
  [string formatting](../tools/gen_runtime_string_format_matrix.py),
  [plusarg formats](../tools/gen_runtime_plusargs_matrix.py),
  [structure memories](../tools/gen_fread_struct_matrix.py),
  [reference memories](../tools/gen_ref_memfile_matrix.py),
  [binary output](../tests/cli/run_binary_format_tests.py) and
  [bounds diagnostics](../tools/gen_bounds_check_matrix.py).
- Implementation: [lowering](../src/front/), [interpreter](../src/vm/),
  [runtime](../src/kernel/) and [driver](../src/driver/main.cpp).

When support changes, update both language versions, the README summary and
the relevant tests. Keep examples in `P1_FEATURES` and normative behavior in
`docs/spec/`; a planned specification item is not evidence of implementation.
