# P1 expansion toward Icarus coverage

[日本語](p1-icarus-progress.ja.md). Status: in progress, 2026-10-23.

The goal is broader P1 functional support than Icarus Verilog 13.0.
Complete parity is **not confirmed**. The previous pass-rate gates and the
small probes below cannot establish it. The
[scope amendment](p1-icarus-scope-proposal.md) was approved on 2026-10-23
and incorporated into [P1_SCOPE](../spec/P1_SCOPE.md), advancing necessary
P3/P4 and previously out-of-scope work into P1.

## Nested-bound conversion and actual-type snapshots (2026-10-23)

The next heap extension fixes the original nested queue copy failure recorded
below. `Heap::copy` and `construct` accept equivalent element shapes rather
than requiring identical registration IDs. Registration computes shape
equivalence once, ignoring nested queue capacities but preserving kinds,
word masks, state domains, defaults and field positions. Validation performs
no allocation or native recursion; language type checking remains in the tier.

The common iterative `Heap::clone` applies every destination queue bound and
records any truncation in the transaction. Construction, whole copies,
element writes, append, push and insert return that warning only after a
successful commit. `Heap::snapshot` copies using the actual source type and
bounds, preserving independent cells before later side effects.

Eight new kernel tests cover 432 copy combinations, nine value-entry paths,
branching aggregates, 2,500 nested levels, snapshot capacities, descriptor
registration failures and each allocation stage of ten operation paths.
They verify values, source independence, retained cell identities, detached
roots, warnings and rollback/reuse. The positive matrix fails on the previous
implementation and passes with the extension. The original C++20 probe now
returns success instead of InvalidHandle. Six deliberately wrong conversions
were caught by actual heap assertions; the restored baseline passes.

Dev passes 2,128 tests: 2,126 pass and the same two known skips remain.
All 23 heap tests pass. The independent scalar-container model was rerun at
scale 100, seed 20261023, and passed 2,520,000 operations. No existing suite
body or expected output was changed.

Front/VM execution, actual variable metadata for ref bindings, complete roots,
source-level diagnostics and notifications remain unfinished. This change
does not enable SystemVerilog dynamic containers or close the last measured
23 Icarus-passing gaps. The full P1 goal remains unachieved.

## Published documentation and comparison check before this extension (2026-10-23)

The typed container SIR implementation is `a8f63b5`. Source revision
`f00c807` also makes descriptor initialization explicitly nonthrowing, fixing
the clang-tidy rejection of scalar types with static lifetime. Compile-time
checks cover scalar initialization, absent descriptors and numeric IDs;
all 72 Sir/ContainerSir tests pass.

The full local CI run passes dev/release/ASan/UBSan configure/build/tests,
release flags, banned features, SPDX, suite files, global state, expectation
protection, README counts, clang-format and clang-tidy. Dev has 2,120 tests:
2,118 pass, with the same two known skips. Existing suite expectations are
unchanged; the initialization fix adds compile-time checks only.

The extended comparison was rerun with `f00c807`: 44 original probes still
leave 23 Icarus-passing gaps. Both installed Icarus compiler and runtime
report `14.0 (devel) (s20260301-411-gca6b8d376)`. No SystemVerilog container
execution or full parity is claimed. Reproduce this latest measurement with:

```sh
python3 tools/icarus_feature_probe.py \
  --kirimasim build/dev/src/driver/kirimasim --extended \
  --output build/docs-update-icarus-probes-latest.json --require-parity
```

The expected exit code remains 1 because the measured gaps are open.
External sv-tests/population counts and performance measurements retain
their original revisions and measurement dates.

## Typed container SIR implementation (2026-10-23)

The heap foundation now has typed container descriptors/references, 16 SIR
operations, verification and text support (SIR_SPEC 5.8). Captured receiver
references separate binding from argument values. `container.snapshot`
captures the actual source value, `container.copy` creates a value of a
specified type, and `container.assign` applies the actual target variable's
bounds. Raw handle transport does not implement language value copying.

Checking 6.22.2 and 13.5.2 established that queue capacities do not distinguish
equivalent ref targets, including nested queues. An original positive check
failed with the initial strict-descriptor verifier; iterative equivalence
now accepts it. Execution must preserve the actual target type/capacity,
including empty variables, rather than replacing it with the formal's bound.

Ten new SIR tests cover 96 type/bound combinations, 49 malformed cases,
input/ref/const-ref arguments, individual/fixed-array container references
and 2,500 nested equivalence levels. Negative cases require the corresponding
diagnostic, so an unrelated rejection cannot hide a missed check.

All nine deliberate mutations (receiver descriptor/kind, size type, copied
element type, descriptor cycle, nonempty constant, ref-bound equivalence,
snapshot type and assignment element compatibility) were caught by actual
SIR tests, excluding mutation-tool anchor checks. The restored baseline passes.

VM execution, front-end production, roots, diagnostics and change notification
are not connected yet. Kernel copying across differing nested queue bounds
and runtime references retaining actual target types still need integration
and tests before the language producer is enabled. The 44 probes and 23
Icarus-passing gaps remain open; the full P1 target is unfinished.

At the SIR stage, an original kernel input with the same integral element type
and inner queue capacity changed from 1 to 3 confirmed that `Heap::copy`
returned InvalidHandle: it required identical element-layout IDs. This is the
mechanism addressed by the nested-bound extension above. At that
stage, dev passed 2,120 tests: 2,118 pass and the same two known skips
remain. Existing suite bodies and expected outputs were preserved.


## Published baseline before this extension (2026-10-23)

The published baseline `6a1830a` includes the five delayed-NBA/packed-target/
named-event rounds and the approved dynamic-container heap foundation.
Its push-time local CI passed dev/release/ASan/UBSan, static checks and all
14 then-unpublished commit expectation checks. Dev has 2,110 tests:
2,108 pass and the two known skips remain. The changes were pushed to main.

The heap has 15 kernel tests, a passing 2,520,000-operation model comparison
and six caught mutations. SIR/front/VM integration, complete safe-point
roots and SystemVerilog dynamic-container execution are unfinished. The
44-probe comparison still has 23 Icarus-passing gaps; exact-13.0 coverage
and complete parity remain unconfirmed. See [P1_SUPPORT](../P1_SUPPORT.md)
for the current inventory and [architecture](../ARCHITECTURE.md) for the
kernel/language boundary.

The extended inventory was rerun with the `6a1830a` binary during this
documentation update: 44 probes and the same 23 gaps. Both installed Icarus
compiler and runtime report `14.0 (devel) (s20260301-411-gca6b8d376)`.
Reproduce the comparison with:

```sh
python3 tools/icarus_feature_probe.py \
  --kirimasim build/dev/src/driver/kirimasim --extended \
  --output build/docs-update-icarus-probes.json --require-parity
```

`--require-parity` currently returns 1 because these gaps remain. This
finite inventory does not establish full language or interface parity.

## Implementation history

The sections below record each round's measurements and decisions. Their
counts and approval status describe that round, rather than the latest
snapshot above.

Two implementation-and-test rounds have added:

- Unformatted `%u` and `%z` output through the common kernel formatter,
  with arbitrary widths, zero padding, X/Z handling and native word byte
  order. A generated CLI matrix checks 132 programs / 14,256 conversions,
  including file, postponed, packed and string destinations. Three new
  unit tests check raw words, dirty padding and runtime formats. String
  destinations omit NUL; packed and file destinations preserve it.
- Ref-memory readmem/writemem operations, including const-ref writing,
  forwarding, waiting calls, automatic storage and selected rows. The
  63 generated suite cases cover seven shapes in nine homes, both radixes,
  ranges, multidimensional and enum memories, and preservation of adjacent
  rows. Selecting a subarray now completes its remaining coordinates in
  `Lowerer::refOf`, avoiding the invalid partial `ref.make` found by this
  matrix. Existing expectations were preserved.

The original binary and ref-memory tests fail with the pre-change binary
`499198369a07`, respectively on unsupported `%u` and unsupported ref
`$readmemh`; the new implementation passes them. The runnable examples in
both language versions of P1_FEATURES were executed successfully.

Local CI passed on the expanded working tree: dev, release, ASan and
UBSan configure/build/tests, release flags, banned features, SPDX, suite
files, global state, README counts, clang-format and clang-tidy. Dev has
1,572 tests: 1,570 pass and the two previously allowed skips remain.
Existing test changes only add assertions and CMake registrations.
New Python SPDX headers and new/changed document links were also checked.
Publication checks additionally validate each unpublished commit for
changes to existing expectations.

## Reproducing the comparison

The installed comparison pair reports Icarus compiler/runtime
`14.0 (devel) (s20260301-411-gca6b8d376)`. The exact 13.0 release has not
been measured in this round. Only original test inputs and executable
behavior were used; no Icarus source or tests were read or imported.

```sh
python3 tools/icarus_feature_probe.py \
  --kirimasim build/dev/src/driver/kirimasim \
  --output build/p1-icarus-probes.json --require-parity
python3 tests/cli/run_binary_format_tests.py \
  --kirimasim build/dev/src/driver/kirimasim \
  --iverilog iverilog --vvp vvp --container initial \
  --api write --api display --api fwrite --api fdisplay
```

The first command intentionally returns 1 on the current working tree:
seven of the sixteen original probes pass Icarus and fail kirimasim. Six
are standard mechanisms: dynamic arrays, queues, class objects,
MOS switches, bidirectional switches, specify paths. The seventh, `wire real`, is an
[Icarus extension](https://steveicarus.github.io/iverilog/usage/icarus_verilog_extensions.html);
strict slang correctly rejects it. Supporting it needs an explicit
compatibility policy, not relaxation of the default language checks.

The second command passes four programs / 432 byte-exact conversions in
Icarus. The larger kirimasim-only matrix remains the source of evidence
for the additional containers and string/packed destinations. Icarus
warned and truncated a binary string result at an embedded NUL; kirimasim
follows its approved string-value rule instead. This difference is not
hidden by the comparison tool.

The probes are a seed inventory. VPI, FST, SDF, timing checks, other
Icarus extensions and interactions still need their own original probes.
Both-failing associative-array and Icarus-failing ref-memory probes say
nothing about the full support of those mechanisms. Extend the inventory
as work continues; do not turn a passing seed set into a full parity claim.

The official [13.0 release notes](https://steveicarus.github.io/iverilog/releases/v13-0-release-note.html)
give release context; they are not a complete coverage specification.

## Binary-input preparation

The next continuation checked IEEE 1800-2023 21.3.4.3 and Table 21-7
against the current specifications. Their general whitespace rule and the
choices needed for transfer widths and partial fields need an explicit
[binary-scan amendment](binary-scan-proposal.md), approved on 2026-10-23
together with the phase-scope amendment. The semantic rules are now in
SYSTEM_TASKS 6.3 and SIR_SPEC 8. Binary-input implementation is recorded in the next section;
existing expectations are unchanged.

An original [observation tool](../../tools/binary_scan_probe.py) records
448 cases and distinguishes failed runs from observed behavior. A
nonzero/omitted-width subset completed with 374 observations matching the
approved project rules. Width-zero behavior differs, and grouped runs also
exposed native crashes in two installed Icarus development builds; see
the amendment for the recorded limits. These results guide review rather
than claiming conformance or full parity.

## Binary-input implementation

The third round implements the approved binary scan rules in shared kernel
`readBinary`, with destination widths from SIR. Constant/runtime formats,
file/packed/string inputs and all ten containers use the same semantics.
The CLI matrix checks 40 programs / 17,120 conversions, two continuation
programs and 40 rejected formats/types. Four added unit tests cover raw
bytes, every byte of short fields, widths, masks, suppression and type rules.
Existing suite bodies and expected output remain unchanged.

```sh
python3 tools/binary_scan_probe.py \
  --kirimasim build/dev/src/driver/kirimasim --engine kirimasim \
  --require-kirimasim --output build/binary-scan-all-implemented.json
```

All 448 project-rule observations match. The nonzero-width reference subset
also matches in both tools (374/374 each). The initial feature probe now has
eight Icarus-passing gaps: seven standard mechanisms (dynamic arrays,
queues, classes, programs, MOS, bidirectional switches and specify paths)
and the `wire real` extension. Full parity is still unconfirmed. The next
[program execution amendment](program-execution-proposal.md) addresses a
conflicting `$exit` description and missing execution metadata.

## Expanded mechanism inventory

`tools/icarus_feature_probe.py --extended` retains the original 16-case
seed and adds 22 original probes (38 total). `--only <name>` selects an
individual probe. The measured development executables still identify as
Icarus `14.0 (devel) (s20260301-411-gca6b8d376)`, not exact 13.0.

```sh
python3 tools/icarus_feature_probe.py \
  --kirimasim build/dev/src/driver/kirimasim --extended \
  --output build/p1-icarus-extended.json --require-parity
```

25 probes pass Icarus and fail kirimasim after program execution was added. They include subcases of the
existing gaps plus event-controlled NBAs, delayed NBAs with runtime array
indices, nested packed selection targets and strengths across a bit-select
port connection. This count is of probes, not distinct language features;
adding probes can increase it without regressing the implementation.

Net declaration assignments with delay pass both. The separate bare-net
delay probe is rejected by both; array-slice assignment is also rejected
by both in this measurement. The class virtual-method probe runs to code 0
in Icarus but prints the base result 3, instead of the derived result 9
specified by this original test (8.20). The report distinguishes normal-exit
result mismatches from native execution failures, and records expected bytes
alongside actual bytes. None of these outcomes establishes complete support
or absence of support for its whole mechanism. No simulator source was read.

Binary-input regression additionally passes dev (1,575 passes, two known
skips). Three deliberate binary scanner mutations (skipping whitespace,
ignoring destination widths, assigning short fields) are each detected by
its four new binary unit tests; the normal sources pass after restoration.

Final binary-input local CI passed all presets (dev, release, ASan, UBSan),
release flags, banned features, SPDX, suite files, global state, unpublished
commit expectation checks, README counts, clang-format and clang-tidy.
The detailed program amendment is now approved and implemented; see the next section.

## Program execution

The program amendment was approved on 2026-10-23 before source changes.
The shared scheduler now owns program initials, dynamic origin, descendant
termination and `$exit`; SIR supplies program instance identities and the
VM supplies root/child registration. Implicit program port assignments use
the program's reactive set. Calls declared elsewhere retain the caller's
set and origin. Natural completion and already confirmed report calls are
kept distinct from newly canceled execution.

The generated matrix has 124 cases: 96 combinations of subroutine scope,
direct/task/fork entry, instance placement and 32/129-bit values, plus 28
lifetime, exit, port, anonymous-program, deferred-call and stress cases.
Seven new scheduler tests and one SIR test cover kernel semantics, context
isolation and record reuse; CLI checks 15 illegal-input combinations and
two program identities. The former rejection fixture keeps its design body
and checks an unordered output multiset in a separate expectation commit.
Existing suite bodies and expected values remain unchanged.

Dev regression passes 1,711 cases: 1,709 pass and two known skips remain.
The 16-case seed inventory has seven Icarus-passing gaps; the extended
38-case inventory has 25. Program VCD scopes remain unimplemented, as do
classes, dynamic containers, clocking and concurrent assertions. This is
progress toward the full P1 target, not a complete-parity claim.

Program local CI passed dev/release/ASan/UBSan configure, build and tests,
release flags, banned features, SPDX, suite files, global state, all six
unpublished commit expectation checks, README counts, clang-format and
clang-tidy. Four deliberate mutations (module-like program identity,
Active initial execution, lost fork origin and unended descendants) each
fail the program-specific tests; the restored implementation passes.

The next [dynamic-container amendment](dynamic-containers-proposal.md)
fills the missing heap/instruction contract, including stable outdated
reference cells and tracing roots. It was later approved on 2026-10-23;
the heap foundation and remaining integration are recorded below.

## Runtime targets of delayed NBAs

The next round implements the existing 10.4.2/9.4.5 and SCHEDULER_SPEC 5/13
rule for fixed-array indices, packed bit/part positions and concatenations.
`Lowerer::storePlace` supplies the same evaluated places used by ordinary
writes; `nba.elem.delay` and `nba.bits.delay` pass captured ranges and
values to the shared kernel's existing NBA scheduling. No new semantic
choice or dependency was needed. Ref/string-character delayed targets and
event-controlled NBAs remain unsupported.

The new matrix has 64 cases: eight containers, 1/32/65/129-bit widths and
two/four-state storage. It checks descending/negative/multidimensional
indices, single evaluation, value and delay capture, partial and X/Z/wide
indices, X/Z data, concatenations, packed members, ordering and NBA/Re-NBA.
CLI checks 42 execution/diagnostic combinations and both SIR spellings;
one new verifier unit checks valid forms and ten negative rows check
malformed operands and forbidden frame targets. The old rejection fixture
keeps its design body and now checks acceptance in a separate expectation
commit, justified by 10.4.2. New tests check the resulting values.

Dev passes 1,778 tests: 1,776 pass and the two known skips remain. The
38 original comparison probes now have 24 Icarus-passing gaps; the original
16-case seed still has seven. The compiler/runtime remain
`14.0 (devel) (s20260301-411-gca6b8d376)`; exact-13.0 and full parity remain
unconfirmed. The runnable example in both P1_FEATURES versions was checked.

## Aggregate delayed NBAs

A further round extends the same scalar delayed-store mechanism to whole
fixed arrays and unpacked structures. `Lowerer::flushStores` delays an
already captured aggregate snapshot. `copyArrayStorage` retains runtime
prefix indices of structure array members and traverses remaining dimensions
from each side's own left bound; ordinary aggregate assignments share this
path. No new value/scheduling choice or dependency was introduced.

Another 64-case matrix crosses the same eight containers, four widths and
two domains. It covers differing array bounds, self-overlap, patterns with
counted side effects, default patterns, real/string/fixed-array members,
dynamic source and destination structure indices, and zero-delay NBA/Re-NBA.
Existing aggregate, structure and SIR tests also pass. Dev has 1,843 tests:
1,841 pass and the two known skips remain. The runnable example was checked.

Two new original probes make the extended inventory 40 cases. Whole-array
and unpacked-structure delayed NBAs pass kirimasim; the installed Icarus
compiler rejects both, so these are not Icarus-passing gaps. There are still
24 such gaps, and seven in the original 16-case seed. Exact-13.0 and full
parity are unconfirmed. Event-controlled NBAs, blocking aggregate timing,
refs/string-character delayed targets and further aggregate forms remain
work. The dynamic-container proposal was subsequently approved; its current
implementation status is summarized at the top of this document.

## Delayed streaming NBAs

A third round reuses `Lowerer::lowerUnpack` and `storePlace` for streaming
NBA targets with a delay. It preserves existing 11.4.14.3/VALUE_SEMANTICS 8
reordering and captures the RHS, delay and selectors once in the established
order. Another 64-case matrix crosses eight containers, four widths and
two domains, with right/bit/byte/65-bit orders, partial final blocks,
leftmost-source truncation, X/Z, runtime selectors, whole arrays and #0.
Dev passes 1,908 tests: 1,906 pass and the two known skips remain.
The bilingual example was executed. No existing expectations changed.

A new original streaming-delay probe brings the extended inventory to 41;
kirimasim passes it and the installed Icarus compiler rejects it. There
remain 24 Icarus-passing gaps and seven in the original seed. These finite
observations do not establish full parity or exact-13.0 support.

## Bounded nested packed targets

The fourth round retains a static parent bit range in `store.bits`,
`nba.bits`, their delay form and `store.ref.bits`. `Lowerer::narrowRuntime`
keeps that range, and compound reads select it before their child. The
kernel's `inside` clips relative positions inside the parent and returns
absolute storage ranges; narrow/wide VM paths only decode these operands.
Frame ranges are verified and represented in SIR text too. Runtime-position
parents and out-of-storage constant parents remain explicit unsupported forms.

The new 112-case matrix crosses 14 storage/execution forms, four widths
and two domains. It checks X/Z and wide indices, partial ranges, compound
reads, single evaluation, adjacent bits/elements, slots, frames, refs,
packed members, ordinary NBA and delayed NBA. A new kernel test compares
20,000 bounded writes with composed `valsem` operations (scaled with the
existing fuzz setting); a new SIR test checks range validation and text.
The historical packed-member rejection fixture keeps its input body and
moves to acceptance in a separate expectation commit.

Dev passes 2,023 tests: 2,021 pass and the two known skips remain. The
bilingual example was executed. A new original boundary probe and the
previous nested-select probe pass both tools: the extended inventory now
has 42 probes and 23 Icarus-passing gaps; the seed remains seven gaps.
The compiler/runtime versions and exact-13.0/full-parity limitations have
not changed.

All three delayed-target mutations, all three aggregate mutations and all
three streaming mutations were detected by their respective new execution
tests, excluding the mutation-tool self-checks. An initially surviving
aggregate-index mutation exposed a missing differing-bound member case;
that case was added before final validation. Restored sources pass.

## Mixed named-event waits and per-item iff

The fifth round registers named events, slot subscriptions and expression
watches in one scheduler wait generation. `Scheduler::waitAny` owns the
registration; `trigger` evaluates only the triggered item's guard, retains
false guards and cancels stale items when any item wakes the process.
`Lowerer::lowerTiming` emits typed named items and guards in `wait.event`;
the VM decodes them. A single unguarded named wait keeps its old form.
C ABI 0.3 appends `wait_mixed`, preserving all previous field offsets and
entry signatures. Both APIs call the same kernel implementation.

The 64-case matrix covers eight containers and eight event shapes,
including same-slot rearming, false/X guards, changing a condition without
an event, named/signal/watch mixtures, NBA triggers and Reactive resumes.
Five new scheduler tests check guard invocation, bounded false-item lists,
first-item cancellation, cancellation before guard access, queued triggers
and registration before watch initialization. A C ABI test checks the new
entry and retained table prefix; a SIR test rejects edges on named items.
The old named-iff rejection fixture keeps its body and migrates to
acceptance in a separate expectation commit. The example was executed.

Dev passes 2,095 tests: 2,093 pass and the two known skips remain. Across
these five rounds, 368 suite cases were added, along with kernel/SIR/CLI
checks and generator checks. The comparison inventory now has 44 original
probes: mixed named-event waiting passes both tools; named-event iff passes
kirimasim and is rejected by the installed Icarus compiler. There are
still 23 Icarus-passing gaps, seven in the original seed. Compiler/runtime
versions remain `14.0 (devel) (s20260301-411-gca6b8d376)`; exact-13.0 and
complete parity are unconfirmed.

The [dynamic-container amendment](dynamic-containers-proposal.md) was
approved on 2026-10-23 after this round. Its implementation status is
recorded below. These additions do not close the full P1 objective.

## Mutation validation of all five rounds

All 15 deliberate mutations, three per mechanism, were detected by the
new execution/reference tests. The packed-target checks catch losing the
parent bound, losing its offset and reading the whole root in a compound
assignment. Named-wait checks catch ignoring false iff, reading canceled
guards and registering named items in a different generation. All restored
baselines pass. Mutation-tool self-checks were excluded from this evidence.
The entries are retained in `tools/mutants.py` for future validation.

The publication check is `tools/local_ci.py`: dev/release/ASan/UBSan builds
and tests, release flags, banned features, SPDX, suite files, global state,
each unpublished commit's expectation boundary, README counts and both
Clang tools. Its summary accompanies publication. Existing expectation
migrations remain separate from implementation commits. The dynamic-container amendment was subsequently approved; the P1
objective is not complete.

## Approved dynamic-container heap foundation

The user approved the complete dynamic-array/queue amendment on 2026-10-23,
including its project choices. SIR_SPEC 4.2.1 and RUNTIME_ABI 6 record the
contract in both languages before implementation.

`Context::heap()` now owns a typed heap with generation-checked container
and element-cell tokens. `Heap::construct`, `copy`, `fromValues` and nested
writes make independent values; queue insertion, deletion and movement keep
the identities of retained cells. Old refs can read and write detached
cells. Destination queue bounds apply to copying and insertion, and the
kernel returns warning categories for the execution tier to report.
Flattened element layouts preserve integral words, real/string/opaque-handle
words and typed nested-container edges, without slang or SIR dependencies.

Allocation uses a selectable standard memory resource. Transactions roll
back every newly allocated container and cell on failure; failed mutations
leave the variable, old refs and output buffers unchanged. Public operations
return errors and do not leak allocation exceptions. Iterative copying and
tracing handle deep nesting without recursive native calls. Exact collection
follows explicit container/cell roots, retains reachable detached cells and
reuses reclaimed entries with new generations. Marking failure performs no
sweep. The caller must supply all roots at an allowed safe point.

Fifteen new kernel tests include 56 integral width/domain/bound combinations
and 450 operations each (25,200 total), compared after every operation and
collection with the independent list-based `valsem::ContainerValue` model.
Other tests cover 2,500 nested levels, real/shortreal/string/aggregate layouts,
same-size resize, self-copy, queue truncation, every allocation point of
construction/copy/mutation/marking, 4,096 table-reuse cycles and two isolated
Contexts. Six deliberate mutations (wrong cell, shared nested value, leaked
rollback cells, missed marking edge, ignored generation and wrong insertion
position) were all caught by actual kernel tests, excluding helper checks.
The restored baseline passes. A 100-sample run seeded with `20261023` also
passes 2,520,000 operations; independent samples keep its cost proportional
to the scale. The nightly reference gate now includes the heap tests.

Reclamation and rollback use pre-sized free-list storage and indexed writes,
so their no-allocation contract is structural. A further fault-injection test
reuses reclaimed entries, fails partway through copying and retries before
collection, checking old reference contents and stale-token rejection. Missing
(X/Z) construction sizes return InvalidSize; invalid root kinds are rejected
even with token zero.

This is the shared kernel foundation. The front end and execution tiers are
not connected to these heap operations yet; SystemVerilog dynamic-array/queue
support and automatic safe-point root publication remain unfinished. The
44-probe inventory and its 23 Icarus-passing gaps therefore remain unchanged.
This foundation does not complete the approved amendment or the P1 target.


The final source validation at `3d7b1a4` passes `tools/local_ci.py`:
dev/release/ASan/UBSan configure, build and tests; release flags; banned
features; SPDX; suite files; global state; all 13 then-unpublished commit
expectation checks; README counts; clang-format and clang-tidy. Dev has
2,110 tests: 2,108 pass and the same two known skips remain. The unit assertion
review notices reflect indenting the unchanged matrix and replacing its
word comparator with an equivalent fatal assertion; comparisons of length,
a and b remain present. No suite body or existing expected output changed.
