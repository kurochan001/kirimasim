# P1 supported features

[日本語](P1_SUPPORT.ja.md) · This English text is the main version.

**Revision (2026-10-24):** old P2→P3, old P3→P4 and old P4→P5; new P2 (SIR v1) is inserted, with tier numbers unchanged ([mapping](notes/phase-renumbering.md)).

The [approved P1 scope](spec/P1_SCOPE.md) now targets Icarus Verilog
13.0-or-broader execution support, advancing the necessary later-phase
mechanisms. This is a target; unsupported forms below remain unimplemented.
See the [comparison progress](notes/p1-icarus-progress.md) for measured gaps.
The remaining-work section distinguishes required P1 coverage from broader
later-phase plans.

This inventory includes main commits `a8bb17cc` (container conditionals) and
`7bc5fa85` (captured NBA payloads; source event-NBA remains unfinished), with an
[integration record](measurements/2026-10-27/conditional-nba-main-integration.json).

This inventory describes tier 1 as of 2026-10-27 on main, including process-limit
implementation `624123b2` ([integration record](measurements/2026-10-27/process-limit-main-integration.json))
and class-static
implementation `9e6a149d` ([integration record](measurements/2026-10-27/class-static-main-integration.json)).
The earlier main baseline `ca72b91f` includes
[event initialization copies](notes/event-initializer-execution.md); main now also supports
[non-parameterized class static properties](notes/class-static-execution.md).
The supported range also includes automatic
scalar/fixed-array event defaults, retained task/fork extents, published
instruction-point event collection, fixed-array ports and element-wise net resolution, runtime array reduction/ordering/min/max, multidimensional integral
binary input, fixed-aggregate bit queries, heap-backed file-monitor lifetime checks, dynamic arrays
and queues, wide three-state masks, whole-aggregate blocking timing, value capture before target effects, repeated blocking
event controls, corrected integral `$fread`
copyback, delayed NBA snapshots,
bounded packed selections and mixed named-event waits. The strength-interval
API and [partial directed MOS source execution](notes/source-mos-execution.md)
and [source passive switches](notes/source-passive-execution.md) are documented separately;
registered-net force, partial port identities and full strength formatting remain work. Check `kirimasim --help` for a build's options and
`--version` for its revision. The [current validation and limits](notes/p1-icarus-progress.md#current-implementation-and-review-status-2026-10-27)
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
| Shared declarations | Packages, imports, `$unit`, interfaces and modports | Virtual interfaces and class objects are not implemented; non-parameterized static properties are described below |
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
| Event controls | Signal edges, integral expressions, `iff`, `wait (expr)`, named events mixed with signals/watches, per-item named-event `iff`, `->`, `->>`, `.triggered`; runtime fixed-array named-event selections; automatic scalar/fixed-array event defaults, input arguments and declaration initialization copies | Existing variable-write restrictions on event/wait conditions remain; no event-controlled `->>` or `always_ff` trigger on a computed expression. Scalar event-return/conditional expressions and automatic structure event fields remain unsupported; output/inout rebinding is not added |
| Concurrency | `fork` / `join`, `join_any`, `join_none`, `wait fork`, `disable fork`, parent execution of fork-local declarations with shared static storage, shared lexical automatic capture, disable of supported named blocks and tasks | Capture follows the typed subset below; object fields and named-fork disable remain incomplete; automatic event limits are listed below |
| Subroutines | Static/automatic functions and tasks, recursion, default arguments, `input`, `output`, `inout`, `ref`, `const ref`; fixed-array and unpacked-structure arguments/results; tasks that wait | No unpacked-structure `ref` argument, array-of-structures argument, automatic array of structures, or array-slice output/inout actual |
| Nets and resolution | `wire`, `tri`, `wand`, `wor`, `tri0`, `tri1`, `supply0`, `supply1`, `uwire`; multiple drivers and drive strengths | No `trireg`, user-defined nettypes or `interconnect` |
| Continuous drivers | Continuous assignments and their rise/fall/turn-off delays, including net declaration assignments | A bare delayed net declaration such as `wire #5 n;` is not implemented; its delay would apply to every driver |
| Primitives | Built-in logic gates and pulls; six directed MOS and six passive switch kinds on eligible fixed terminals; combinational/sequential UDPs | Switch support is partial: registered-net force, partial ports and full strength formatting remain; no UDP instances with unconnected ports |
| Net aliases | Aliases of compatible whole nets | No partial/concatenated/array aliases, incompatible net types, or merging groups already joined separately by aliases or ports |
| Procedural drivers | `assign` / `deassign`, `force` / `release` on supported integral targets | No real/whole-unpacked-array target or force of a scalar net connected through a port; constant array elements are supported |
| Assertions | Immediate `assert`, `assume`, `cover`; deferred immediate assertions (`#0`, `final`) | No concurrent assertions or module-item deferred assertions; a deferred assertion action cannot call a task that may wait |
| Scheduler | All time-slot regions and the reference scheduling algorithm; pending reports in Observed; strobe/monitor output in Postponed | Program initial threads inherit their group and origin through calls/forks; clocking blocks and concurrent assertions remain unsupported |

`randcase` weights wider than 32 bits are not implemented.

The [NBA payload foundation](notes/event-nba-payload-foundation.md) adds
kernel-owned value capture, later readiness and common pending issue order.
Its 24 independent tests do not enable source event-controlled NBA. The
[concrete-control core](notes/event-nba-concrete-controls.md) adds persistent fixed-slot
and named-event subscriptions, full-width counts and retained callback origins.
The main [expression-monitor checkpoint](notes/event-nba-expression-monitors.md) adds
shared evaluation, retained dynamic dependencies and grouped heap/activation
notifications. That checkpoint preceded the typed SIR/VM work below.

The [typed SIR/VM checkpoint](notes/event-nba-sir-execution.md) `6f43e387` adds the typed
instruction/layouts, verifier/text and fresh callback bridge for original native
SIR. It does not enable source event-NBA; front lowering and source matrices remain.

### Class static properties and remaining object boundary

[Non-parameterized static properties](notes/class-static-execution.md) use ordinary
typed slots and pre-time-zero initialization, including unreferenced declarations.
Qualified/aliased/inherited access, hidden redeclarations and static methods share
the same lowering paths as other static variables. Supported types include packed
2/4-state values, real/shortreal, string, event, fixed arrays, supported unpacked
structures and integral queues/dynamic arrays. Waiting tasks, NBA, sensitivity and
argument directions retain the existing restrictions of each type.

Parameterized class static storage is explicitly unsupported, including unused
specializations with initializer effects. Property-free generic static methods
remain usable. Object construction, handles, fields, dispatch, RNG and lifetime/GC
remain incomplete. The approved [class runtime contract](notes/class-runtime-proposal.md)
is broader than this implementation. The [46-program investigation](notes/class-execution-investigation.md)
retains its dated pre-fix observations; finite checks do not establish full class support.

### UDP input counts and compilation limits

The fixed 32-input evaluator/transport limit is removed. [106 original sources](notes/wide-udp-inputs.md)
cover counts through 257, and independent kernel/SIR tests cover wide terminal
matching, state order and operand validation. Direct kernel/reference checks
also use 65,537 inputs; source compilation at that count is unconfirmed.
Dense hold-row tables can exceed bounded compilation probes in pinned slang.
They remain a tracked frontend limitation; no diagnostic or dependency is relaxed.

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
Delayed three-state outputs on resolved packed nets now propagate value and
both side fields as one owned state. Same-projection changes, cancellation,
equal pending deadlines, zero delay and short-circuit control blocks are covered
by [240 original sources and three direct tests](notes/delayed-gate-state-execution.md).
The exact expectation-only migration was separately approved and integrated;
all 21 local-CI checks now pass with zero failures and the same two known skips.

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
approved on 2026-10-25, including startup and full-strength timing decisions;
its full implementation remains work.

The [pure MOS value-operation foundation](notes/mos-strength-operations.md) now
covers all six MOS kinds and signed identity/nonresistive/resistive transfers.
Ten new tests check all 120 intervals and 11,520 control/device combinations;
nine compiled faults are detected. This is a C++20 kernel/value-semantics
component. [Typed strength SSA execution](notes/strength-ssa-execution.md) now
connects make/MOS/projection through the VM, with 13 tests, 2,034 hand-built
VM designs and nine caught compiled controls. Full local CI passes all 21 checks.
[Complete net publication](notes/strength-store-publication.md) now stores
published intervals in each Context and commits all ranges/values before callbacks.
Nineteen new publication/width tests and sixteen compiled controls pass; full local
CI passes all 21 checks. [Physical origin execution](notes/strength-driver-execution.md)
now adds stable driver tables, per-scalar inertial updates, coalesced Active work
and independent-node resolution through that publisher. Sixteen new tests and
thirteen compiled controls cover complete ranges and timing. Full local CI passes all 21 checks.
[Declared-network SIR execution](notes/strength-network-execution.md) now connects
verified static nodes/origins, captured load, scalar drive/delay and separate
strength sensitivity through the VM to those shared kernel APIs. Nineteen new
tests include 13,465 hand-built SIR executions; sixteen compiled controls pass.
Full local CI passes all 21 checks.
[Directed MOS source execution](notes/source-mos-execution.md) now adds front
lowering, owned ordinary-driver import and separately captured controls. Its
154 new source designs and six boundaries pass, with eleven compiled faults
caught. Icarus 13.0 matches 113 complete designs; 41 observed index/startup/transition
differences are recorded. The historical nmos CLI rejection migration is applied separately on main
(`6f301460`). Registered-net force/release,
partial/retained port identity and full strength formatting remain incomplete.
[Passive kernel execution](notes/passive-network-execution.md) now adds exact
physical-origin paths, uncertain-edge worlds and scalar conduction timing to
the real scheduler/publisher, with nineteen tests, 14,445 reference states and
28 compiled faults caught. [Passive-edge SIR execution](notes/passive-sir-execution.md)
now connects verified edge tables and owned scalar control/delay instructions
through tier 1, with seven tests including 4,560 independent reference designs.
[Source passive execution](notes/source-passive-execution.md) now lowers all six kinds,
including static selections/concatenations and 1/9/65/129-bit arrays. Its 143
original source designs pass, and nine compiled new/reused faults are caught.
Independent graph groups retain one atomic publication. Icarus 13.0 compiles
135 designs and matches 110; vector-control/startup/mixed-wired observations
remain recorded. The 87-probe gaps fall to eight against both references.
Historical tran rejection expectations were separately approved and migrated
in `77c9d188`, with all source inputs unchanged. Complete switch support and P1
remain unfinished; see [main integration validation](notes/main-integration-2026-10-26.md).

### Fork-local declarations

Declarations use the parent's lifetime/initialization path rather than becoming
children. Static storage is shared; uncaptured automatic initialization runs
per entry, including declaration-only forks. [Seventy-two source cases and limits](notes/fork-local-declarations.md)
cover eight contexts and typed storage. The subsequent source-capture work below
supports shared automatic variables. [Automatic scalar/fixed-array event
defaults](notes/instruction-event-execution.md) now construct fresh objects per
entry and preserve retained descendants. [Scalar/fixed-array event inputs](notes/event-input-execution.md)
share object identity across calls and retained descendants. [Declaration initialization](notes/event-initializer-execution.md)
also shares the object, including null and fixed-array patterns. Automatic
unpacked-structure event fields remain unsupported.

### Automatic-storage execution boundary

Context-local generational scopes, shared bindings, leases, readonly rights
and typed roots have [14 original tests and a full kernel-storage generation probe](notes/automatic-storage-foundation.md).
The [wait-dependency connection](notes/automatic-storage-observation.md) adds
14 tests, [SIR metadata/verifier](notes/automatic-sir-metadata.md) 16,
[value transport](notes/automatic-vm-frame-access.md) seven,
[environment construction](notes/automatic-vm-environment.md) 21 and
[scoped references](notes/automatic-vm-scoped-refs.md) 11.
The [Interpreter adapter](notes/automatic-vm-execution.md) now executes valid
represented homes, scope entry/exit, shared fork environments, current-task
callbacks and immutable refs. Nineteen execution tests cover calls/copyback,
reentry, all joins, typed roots, cancellation, program completion and resource
failure; 14 compiled controls are detected. Ordinary frames share the layout
and value boundary without a mirrored scoped value.
Front now [lowers shared lexical capture from source](notes/automatic-front-capture.md):
186 new designs and a full-field/extent checker pass, and five compiled faults are
detected. At that capture checkpoint, the automatic-fork probe passed and the historical
Icarus-13-passing gap count was 21. The [current comparison](notes/icarus-13-comparison.md#automatic-event-recheck-2026-10-26-latest)
has seven gaps against each reference. The three historical rejection checks were migrated
separately in approved commit `11096dc3` without changing their source fixtures.
The [managed-event kernel checkpoint](notes/managed-event-registry.md) now supplies
generational identity, runtime pins and retained-scope event roots, with 20 tests
and a full 32-bit generation audit. The [runtime ownership checkpoint](notes/managed-event-runtime.md)
now supplies typed kernel wait/trigger/monitor operations and automatic pending-operation
pins, with 23 new tests and 16 caught compiled faults.
[Typed field providers](notes/event-activation-roots.md) and
[SIR event execution in the VM](notes/event-vm-boundaries.md) now connect typed
storage, waits/triggers and object-read capture at existing region/slot collection
boundaries. Ordinary/captured arrays and input/return transport are tested in
hand-built SIR. The subsequent [automatic-event checkpoint](notes/instruction-event-execution.md)
implements fresh scalar/fixed-array declaration defaults, complete native/SSA/transfer
publication for the currently supported event SIR, and collection before physical-row
growth. Twelve VM tests contain 32 verified designs; 176 new generated source designs
and 12 compiled controls pass. Container heap timing and final exclusions remain in
force. One [historical rejection-oracle migration](notes/automatic-event-expectation-migration.md)
was approved and applied in `4ba6b63d`. [Source event inputs](notes/event-input-execution.md) now use this typed transport,
including waiting calls and deferred snapshots. [Declaration initializers](notes/event-initializer-execution.md)
now use the same typed copy entry for scalar values and fixed-array patterns.
Automatic structure event fields remain unsupported; object/ref interfaces and the broader
event value model remain partial. Conservative SSA retention is bounded by typed
body words, not minimal last-use liveness. See the note for limits and measurements.

Source [runtime fixed-array event selections](notes/indexed-event-execution.md)
now pass 160 original designs and 16 strict policy boundaries, with four compiled
faults caught. This covers loaded named/mixed waits, delayed targets and explicit
`.triggered`; constant valid operands retain existing dumps. The historical CLI
input is preserved byte for byte. Its [separately approved migration](notes/indexed-event-expectation-migration.md)
adds one acceptance entry and removes six obsolete diagnostics. At that earlier
checkpoint all 14 CLI tests and all 21 local CI checks passed; dev/ASan/UBSan
passed 5,139/5,141 and release 5,140/5,142, with zero failures and two known skips
per preset. The later source-passive checkpoint has the two separate historical
tran-rejection failures described above.

### Fixed-array ports and net arrays

Whole fixed-unpacked input/output ports and compatible net inout connections
execute in declaration order, including differing bounds/directions and rank
two. Copies capture the complete RHS first. Typed alias views, element-wise
strength resolution, sparse private drivers, constant-element force/release
and inertial delay, unconnected defaults and pulls share the existing kernel.
Supported integral/real/shortreal/string values and program origin are retained.
See [implementation and 358-source validation](notes/fixed-array-port-execution.md).
Partial-array/ref ports, resolved array three-state sides and explicit array
aliases remain work. Strict two-state-net/chandle-port diagnostics stay enabled.

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
This does not enable class objects, clocking blocks or concurrent assertions.
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

Typed container descriptors, receiver refs, 17 SIR operations (16 at the
2026-10-23 foundation, plus `container.swap.values` since 2026-10-25), verifier
and text support are implemented ([SIR_SPEC 5.8](spec/SIR_SPEC.md#58-typed-container-encoding-approved-contract-of-2026-10-23)).
Ten new SIR tests cover 96 positive type/bound combinations, 49 malformed
cases, argument bindings and fixed arrays of container receivers.
The VM now dispatches all 17 instructions to the shared heap. Its
[25 independent SIR tests](../tests/unit/vm_container_test.cpp) cover 216
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

The [conditional-container implementation](notes/container-conditional-execution.md)
adds `?:` for supported dynamic-array/queue representations: known conditions
evaluate one side, unknown conditions capture both left to right; unequal lengths
return empty and unequal/unknown-equal elements take their type default. Nested
elements compare contents and the result has independent storage. Its 351 source
cases span nine execution homes, refs, functions, delayed NBAs and explicit waits.
Unsupported element representations and mixed fixed/dynamic operand forms remain
unimplemented or unconfirmed. Existing conditional fixed-aggregate lowering is
separate and unchanged.

Remaining forms include queue slices as assignment targets,
other locator methods and shuffle, mixed
or omitted dynamic foreach dimensions, runtime/nested query dimensions and
fixed-aggregate/class/event elements. Implicit sensitivity (`always_comb`,
`always_latch`, `always @*`, continuous assignments) to container values gives
an explicit unsupported diagnostic: static trigger ranges cannot yet encode
heap changes. Fork capture of enclosing automatic storage remains the existing
SIR_SPEC 7.5 limitation. Class/DPI/VPI roots and complete Icarus parity remain
unfinished. These are incremental language features, not complete container
support.

### Runtime array methods

Fixed arrays, dynamic arrays and queues support `sum`, `product`, `and`, `or`,
`xor`, `reverse`, `sort`, `rsort`, `min` and `max` through one typed lowering
path. Integral reductions retain width/signedness from the element or `with`
expression; nonempty folds start at the first value, preserving singleton Z.
Empty product returns one, empty and all ones, and the other empty reductions
zero, under the approved project contract. Runtime call expressions follow
these rules; parameter forms folded earlier by slang remain a limitation.

Ordering supports integral, real/shortreal and string keys and stable ties.
Only definitely true comparisons move a value; X/Z/NaN comparisons retain
order. Existing element refs remain at their indices; refs inside nested owned
values move with those values. Pure ordering keys are evaluated once per
element before any movement. Effectful keys and calls to user functions whose
purity is unverified receive an explicit unsupported diagnostic.

Min/max return an independent unbounded queue: empty for empty input, exactly
one original element otherwise. They snapshot the receiver before evaluating
keys, including effectful keys, exactly once each. Declared traversal order
and the first tied/unknown candidate are retained. Named/nested `with` and
nested dynamic-container elements with scalar keys are tested. Fixed aggregate,
class and event elements and unsupported source representations remain work.
The [270 original cases and four direct unit tests](notes/runtime-array-methods.md)
cover contexts, wide values, real/string/NaN keys, references and key effects.
The approved [reduction/ordering](notes/array-methods-proposal.md) and
[extrema](notes/array-extrema-proposal.md) decisions record reference differences.

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

[Bit queries on fixed aggregates](notes/aggregate-bit-queries.md) use the
existing streaming conversion for integral-leaf fixed arrays, records and
arrays of records (136 new source cases). Supported automatic values,
array refs and aggregate-returning functions use that same lowering. This
does not enable unsupported aggregate storage or dynamic/string bit streams.

The [assignment-pattern renderer](notes/pattern-format-foundation.md) is a
shared-kernel API foundation. [Typed SIR/execution](notes/pattern-sir-execution.md)
is connected, while front format-argument integration remains, so source
`%p` stays unsupported; this does not enable class identities or associative
array ordering.

A display monitor follows a value change in an existing queue cell, checked
with [original source and SIR](notes/pattern-sir-execution.md).
[Twenty source cases and scheduler failure/lifetime tests](notes/heap-file-monitor-lifetime.md)
also check heap-backed file monitors, container replacement, file closure,
descriptor reuse and callback termination. Reentrant callback registration
and full source-context coverage remain unconfirmed.

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
- `$fread` supports scalar integral destinations and multidimensional fixed
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
  Integral memories with two or more remaining dimensions use linear
  start/count positions, numerical low-to-high traversal and the rightmost
  dimension varying fastest. A selected prefix is captured before later
  arguments; one remaining dimension retains actual-index addressing.
  The [156-case matrix](notes/multidim-fread-execution.md) covers direction,
  partial input, counts and initial/automatic/ref storage. One-dimensional
  memories of unpacked structures may contain nested structures with integral
  leaves. Dynamic memories, array/real structure members, multidimensional
  structure memories and unpacked unions remain unsupported.
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
| Live runtime processes | `--max-procs=<n>`; default 1000000, 0 disables. Counts unfinished runtime children, excluding static processes and retained ended records. Rejected children do not start; ends with `resource_limit` / code 6 and runs `final` ([evidence](notes/process-limit-execution.md)) |
| Simulation limits | `--max-time`, `--max-time-ok`, `--max-zero-delay`, `--max-slot-events`, `--max-steps`, `--max-call-depth` |
| Live output | `--autoflush` flushes stdout after each display/write/strobe/monitor, including file tasks directed to stdout; default off |
| Stop policy | `--stop=finish\|ignore\|error`; default `finish`; `ignore` resumes the caller, `error` ends with code 1 and reason `stop`; `$stop(2)` requests statistics at the eventual end |
| Wall-clock limit | `--max-wall-time=<seconds>`; whole seconds, 0 disables; compilation excluded; ends with code 4 / `wall_time_limit` and runs `final`; `--max-time-ok` does not apply |
| Random seed | `--seed=<n>` |
| Inspection | `--dump-sir`, `--stats=json` |
| Bounds diagnostics (from `dec94ff`) | `--bounds-check=off\|warn` (default `off`): warn once per source position for invalid packed selections/fixed-array indices and print final access counts; normal read/write/NBA values are preserved |
| VCD output | `--wave=<file>.vcd`, `--wave-scope=<pattern>[,<pattern>...]`, `--wave-glitches`; 4-state waveforms, also controlled by the dump tasks above |

`--max-scc-iter` is accepted but has no effect until P3's static islands.
Other options described in the [CLI specification](spec/CLI.md) are not
automatically implemented: for example, `--cond-x` and `--init-values`
are not current driver options. Use `--help` to inspect a build.
FST output is not implemented. The wall-clock limit is checked before events
and at VM branches, including static initialization; it does not preempt
a blocking host operation. Final procedures finish without that deadline.

## Remaining work and scope

| Status | Features |
| --- | --- |
| Required P1 execution gaps | The Icarus-supported subset of dynamic containers, classes, VPI, switch-level modeling, specify paths and timing facilities; FST, `%p`, resolved-strength `%v`, unsupported `$fread` shapes and the limits listed above |
| P1 container integration | Shared heap, typed SIR/VM, automatic collection and heap waits are connected to 496 source cases; remaining language forms are listed above |
| P2 (planned; direction approved) | SIR v1 text/parser, common Op schema and kernel boundary; prepare alongside P1, freeze after P1 completion; exact grammar/API/reference details await review ([plan](notes/sir-v1-proposal.md)) |
| P3 | Tier 2 C++ generation, static-island analysis and further RTL performance work |
| Broader later-phase plans | Support beyond the required Icarus coverage, including broader UVM, constrained randomization, covergroups, concurrent assertions/sampled-value functions, clocking, virtual interfaces, `let` and DPI-C; previous P4/P5 labels cannot defer a required P1 mechanism |
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
