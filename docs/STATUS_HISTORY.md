# Status history

[日本語](STATUS_HISTORY.ja.md) · This English text is the main version.

The [README](../README.md) gives kirimasim's current status. This file
keeps the dated entries that the README carried until 2026-10-30:
implementation checkpoints, comparison-probe counts and validation records.
They were moved here unchanged except for link paths. Each entry keeps the
revision, date and counts of its time and does not describe current
support; for that, see the [P1 support inventory](P1_SUPPORT.md) and
"What does not work yet" in the README. The detailed record of each
checkpoint is in [P1 progress](notes/p1-icarus-progress.md), and
user-visible changes are in the [CHANGELOG](../CHANGELOG.md).

## Checkpoints listed under "Status (2026-10-29)"

Common input options are available from `dd2c0188`: `-I` for include paths,
`-D` for macros, and `-y` / `--libdir=` for module libraries. The short forms
accept attached or separate operands. See the
[runnable example](P1_FEATURES.md#common-input-options) and
[user manual](USER_MANUAL.md#list-sources-and-include-directories) for
lookup rules, macro values and the existing plus forms.

The two known suite skips now run with their original expected outputs:
input-only clocking blocks with `#1step` sampling and strength-preserving
static partial net ports. See the [approved clocking subset](notes/clocking-input-1step-proposal.md)
and [implemented limits](P1_SUPPORT.md). Broader clocking and port forms
remain work; this does not establish P1 completion. Earlier checkpoints below
retain their recorded dates and counts.

The slang pin is updated to [`e2d1021`](notes/slang-e2d1021-update.md),
a development commit after 12.1 including the #2064 class-specifier recovery
fix. [All 21 local CI checks pass](measurements/2026-10-27/slang-e2d1021-main-validation.json)
with existing expectations preserved. Four population string-conversion failures
already occur with the previous pin and remain work; old/new comparisons retain them.

Main includes [source event-NBA lowering](notes/event-nba-source-execution.md)
from `c4be8ade`, alongside the portability/lint fixes `9a81a55e` and `b91bd3d7`. It runs
1,018 new original designs and detects all six compiled source faults. Both
95-probe inventories gain `event_controlled_nba`; six finite-inventory gaps remain.
The old rejection-oracle migration was approved and applied separately in `16c033a4`;
[Main-integration validation](measurements/2026-10-27/nba-front-main-integration.json)
records the pre-push checks. P1 and the remaining source boundaries are unfinished.

Main now includes container conditionals (`a8bb17cc`) and captured NBA payloads
(`7bc5fa85`). The [integration record](measurements/2026-10-27/conditional-nba-main-integration.json)
checks the validated code/tests and each commit's expectation guard. That earlier
checkpoint did not enable source event-NBA.

Main implements [`--max-procs`](notes/process-limit-execution.md)
under the approved CLI 2.7 contract in `624123b2`. It counts unfinished runtime children in
the shared kernel, preserves capacity across record retention/reuse, and stops
before registering an over-limit child. The
[integration record](measurements/2026-10-27/process-limit-main-integration.json)
confirms unchanged validated code/tests and commit-level expectation protection.
Full local CI passes 21/21 steps; P1 remains incomplete.

Main adds [non-parameterized static properties](notes/class-static-execution.md),
including eager initialization of unused declarations, in `9e6a149d`. Its [full local CI](measurements/2026-10-27/class-static-validation.json)
passes 21/21 steps, with 568 new CTest entries and seven caught compiled faults.
The [integration record](measurements/2026-10-27/class-static-main-integration.json)
confirms unchanged validated implementation and tests. Class objects and
parameterized static storage remain work.

The earlier main checkpoint adds [event declaration initialization copies](notes/event-initializer-execution.md)
in `dfddb94b`; the user's three-case oracle migration is separate in `2da32daa`.
The three commits are [integrated into main through `f100cd86`](measurements/2026-10-27/event-initializer-main-integration.json). [Full local CI](measurements/2026-10-27/event-initializer-validation.json)
passes 21/21 checks; dev/ASan/UBSan pass 6,057/6,059 and release 6,058/6,060,
with zero failures and two known skips each. Fresh population, sv-tests and
Icarus 13/14 probes preserve the earlier passes. P1 remains incomplete.
Earlier main checkpoints are retained in the history;
[dated checkpoint notes](notes/p1-icarus-progress.md) preserve earlier
approval, failure and publication states.

At `4c69843`, typed pattern layouts, verification, text and execution are
connected to the shared renderer. Front's literal/runtime argument bridge
remains unfinished, so source `%p` is still unsupported. A display monitor
now observes a change in an existing queue cell. See the
[current implementation and verification scope](notes/pattern-sir-execution.md)
and [runnable example](P1_FEATURES.md#monitoring-an-existing-queue-cell).

UDP evaluation now accepts input counts beyond 32 through the shared kernel.
[106 original sources and independent value checks](notes/wide-udp-inputs.md)
cover late terminals, state, X/Z, delays and strength. A dense-table compilation
scaling problem remains in pinned slang; complete wide UDP source coverage
is unconfirmed and strict diagnostics remain unchanged.

The dynamic-array/queue execution amendment was approved on 2026-10-23.
The shared kernel now has a typed heap with deep copying, stable element
references, bounded queues, exact tracing and allocation-failure rollback.
Its 31 kernel tests cover 432 nested-copy combinations, transactional failure
and actual-type snapshots, alongside 2,520,000 model-checked operations.
Typed container SIR descriptors, references, instructions, verification and
text are implemented. Twenty-four VM tests execute those instructions from original
SIR inputs, including actual-bound refs, detached cells across waits and automatic
collection at drained region/slot boundaries. Typed slots, live frames, cell refs,
deferred calls and pending scheduler values retain their reachable objects.
The scheduler now tracks heap-dependent waits, reattaches changed dependencies
and excludes called functions' private containers. Front lowering now executes
construction, deep copies, element access, queue methods, actual-bound refs,
whole-value delayed assignments, explicit heap waits, dynamic foreach and
first-dimension queries. An original 240-case source matrix spans eight execution
contexts, with 11 additional CLI diagnostic cases. Another 256 source cases
cover runtime unpacked concatenation and reading queue slices, including wide
signed/unsigned bounds, X/Z and operand side effects. Container support remains
partial: slice assignment targets, additional array methods and implicit heap
sensitivity are among the remaining forms. See the
[implementation status](P1_SUPPORT.md#dynamic-container-foundation)
and [approved contract](notes/dynamic-containers-proposal.md).

The [conditional-container implementation](notes/container-conditional-execution.md)
adds `?:` for supported dynamic arrays/queues, including unknown-condition
whole-element defaults, snapshots and nested contents. Its 351 original designs
span nine execution homes; [local CI](measurements/2026-10-27/container-conditional-validation.json)
passes all 21 steps. Existing feature coverage remains partial.

The [concrete event controls](notes/event-nba-concrete-controls.md) now
cover persistent fixed-slot/named subscriptions, full-width counts and callback-origin
ownership. The expression-monitor checkpoint below extends this core.
The SIR/front/VM source path remains unfinished.
[Local validation](measurements/2026-10-27/nba-events-validation.json) passes all
21 CI steps, with 33 new kernel/SIR-native tests and 18 caught compiled faults.

The [expression-monitor checkpoint](notes/event-nba-expression-monitors.md)
is integrated into main as `012af2ca`, adding shared evaluation and grouped
heap/activation notices. [Integration validation](measurements/2026-10-27/nba-monitors-main-integration.json)
records that checkpoint’s pre-push checks; source event-NBA was still unsupported there.

The [typed SIR/VM checkpoint](notes/event-nba-sir-execution.md) `6f43e387` adds
layouts, verification/text and fresh origin callbacks, checked with original native
SIR. The local [source stage](notes/event-nba-source-execution.md) now exercises
front capture order and detached callbacks, with all 21 local CI checks passing.

The [NBA payload foundation](notes/event-nba-payload-foundation.md) adds
owned snapshots and pending issue order to the shared kernel, with 24 independent
unit tests. The concrete core above adds slot/named subscriptions and origin
retention; the main checkpoint above adds expression monitors. Source
`nba.event` transport was still unfinished at those kernel checkpoints.
[Final-layout local CI](measurements/2026-10-27/nba-payload-validation.json) passes all 21 checks.

The [shared strength resolver](notes/strength-range-foundation.md) now
retains complete intervals before value projection, with exhaustive interval
pair checks. [Directed MOS source execution](notes/source-mos-execution.md)
now imports ordinary drivers and captures controls separately. This remains partial:
registered-net force, partial/retained ports and full strength formatting still need work.
[Source passive execution](notes/source-passive-execution.md) now connects all six
bidirectional kinds, including static bit/element/concat mappings and primitive arrays,
to the shared graph. Independent uncertain graphs retain atomic publication.
Its 143 original source designs pass; the historical tran rejection expectations
were [separately approved and migrated](notes/passive-expectation-migration.md)
in `77c9d188`, with every source input unchanged.

Wide three-state gates now retain both L/H side-mask halves across word
boundaries. All 73 new source cases match Icarus 13.0 and development builds.
See [scope and validation](notes/wide-gate-strength.md). Delayed side-only
timing follows the [approved complete-state amendment](notes/delayed-gate-side-proposal.md).

Aggregate blocking/NBA assignments now capture all RHS values before target
effects, including container snapshots before later field expressions.
Runtime-selected fixed subarrays and structure array members use the same
path. See the [capture and verification note](notes/aggregate-value-capture.md).

Whole fixed arrays and unpacked structures now also support blocking
intra-assignment delay/event/repeat controls. Typed private frames retain
the RHS across waits, and the actual target is chosen afterwards. Another
74 original source cases cover these forms and differing-bound structure
copies. See [implementation and separate expectation review](notes/blocking-aggregate-timing.md).

Blocking intra-assignment `repeat(count) @(...)` now retains the RHS and the
full count, then chooses the target after waiting. Nonpositive/X/Z counts
skip the wait. Seventy-five original source cases cover eight execution
contexts and scalar/container snapshots. The separate event-NBA source stage is
listed above. See the [execution and verification note](notes/repeated-event-controls.md).

Fork-local declarations now use parent initialization and shared static
storage without becoming child processes. Declaration-only forks do not wait.
[Scope and 72 original source cases](notes/fork-local-declarations.md)
include all join kinds. [Shared automatic capture from source](notes/automatic-front-capture.md)
now passes 186 new designs; three historical rejection checks were separately
migrated with approval in `11096dc3`. [Typed SIR event execution](notes/event-vm-boundaries.md)
and collection at region/slot boundaries are connected. The approved
[automatic-event instruction collector](notes/instruction-event-execution.md)
now publishes native fields, SSA regions and complete transfer payloads before
physical event-row growth. Automatic scalar/fixed-array declarations without
initializers construct fresh objects and preserve retained descendants.
176 new source designs, 32 verified SIR designs and 12 compiled controls pass.
The [historical rejection-oracle migration](notes/automatic-event-expectation-migration.md)
was approved and applied in `4ba6b63d`. Event value/formal/structure coverage remains partial.

[Runtime fixed-array event selections](notes/indexed-event-execution.md)
now support named/mixed waits, delayed targets and `.triggered` through typed
values, with 160 original source designs. The historical CLI input's six old
unsupported diagnostics were [separately approved for migration](notes/indexed-event-expectation-migration.md)
and migrated in `28f5c972`. The separate
[tran expectation migration](notes/passive-expectation-migration.md) was
also approved and applied in `77c9d188`; its two historical failing checks
are included in the [main integration validation](notes/main-integration-2026-10-26.md).

Fixed-array ports now share or copy corresponding elements while preserving
each declaration's bounds. Net arrays resolve strengths per element with
sparse private drivers. Constant-element force/release, delay and unconnected
defaults/pulls are covered by [358 original cases](notes/fixed-array-port-execution.md).
On the unchanged 87 probes, Icarus-passing gaps fall 24 → 22 per reference;
P1 remains incomplete.

## Probe counts and investigations listed under "P1 exit criteria"

The [implemented inventory](P1_SUPPORT.md) and
[comparison record](notes/p1-icarus-progress.md) track progress. The
inventory has 87 original probes. At the fixed-array-port checkpoint,
Icarus `13.0 (stable) (v13_0)` has 22 passing gaps and four reference result
mismatches. With `14.0 (devel) (s20260301-411-gca6b8d376)`, the counts are
22 and seven. Subsequent automatic capture reduces the historical 13.0-passing
gaps to 21, and the fresh 14.0 run also leaves 21 gaps and seven mismatches;
there is no fresh 13.0 run at this event-VM checkpoint. With directed MOS
execution, a fresh run against both on 2026-10-26 leaves 14 gaps, with four
(13.0) and seven (14.0) reference mismatches.
The later [source-passive checkpoint](notes/source-passive-execution.md),
measured on the same date, reduces the gaps to **eight against each reference**,
with four/seven reference mismatches unchanged. Its stable compiler and runtime
both report `13.0 (stable) (v13_0-dirty)`; the exact observations are retained in
[the Icarus 13.0 record](measurements/2026-10-26/icarus13-source-passive-probes.json)
and [the development record](measurements/2026-10-26/icarus14-source-passive-probes.json).
The later [automatic-event checkpoint](notes/instruction-event-execution.md) reruns
all 87 probes against both binaries: **seven gaps against each**, with the same
four/seven reference mismatches ([13.0](measurements/2026-10-26/icarus13-instruction-events-probes.json),
[14.0](measurements/2026-10-26/icarus14-instruction-events-probes.json)).
These are separate reference baselines. See the
[versioned results and conditions](notes/icarus-13-comparison.md).
Complete functional parity remains unconfirmed. A passing probe inventory
would not by itself establish complete coverage; the inventory must expand
as mechanisms and interfaces are checked.
At the earlier wide-UDP checkpoint, the prior 84 statuses were unchanged. The two newly inventoried wide UDP
examples now pass both references and native execution. A new dense-coverage
source still reaches the native subprocess bound. On the same expanded list,
the retained baseline had 27/33 gaps and the wide-UDP checkpoint 25/31; the
later array/input checkpoint had 24/24; fixed-array ports now reduce this to 22/22.
The [shared automatic-storage and event proposal](notes/automatic-capture-proposal.md)
records 32 original programs and was approved on 2026-10-25. Shared lexical
capture and [automatic scalar/fixed-array event construction](notes/instruction-event-execution.md)
are implemented; the separate historical rejection-oracle migration was approved and applied in `4ba6b63d`.
The [fixed-array port and resolution proposal](notes/fixed-array-ports-proposal.md)
adds 59 original design observations, with legality boundaries kept separate.

A separate [46-program class investigation](notes/class-execution-investigation.md)
records a historical static-initialization mismatch. The
[non-parameterized static-storage implementation](notes/class-static-execution.md)
fixes that omission; object execution remains required P1 work.
A [detailed class runtime amendment](notes/class-runtime-proposal.md)
adds fourteen original boundary observations and was approved on 2026-10-25.

The [MOS/switch investigation](notes/mos-switch-investigation.md)
records 28 original programs and 56 historical reference observations separately
from that inventory. [Directed MOS](notes/source-mos-execution.md) and
[passive switches](notes/source-passive-execution.md) now execute for the
documented static terminals. Registered-net source force, broader partial-port
identities and full strength formatting remain work.

Integral `$fread` now accepts optional start/count arguments, evaluates
supplied value arguments once and follows the approved copy-in/copyback
rules for selected destinations. An inout target is evaluated for its input
value and again when a positive-byte result is copied back. The earlier
capture-once implementation is corrected in the separate
[expectation/source review stack](notes/fread-copyback-correction.md). On the
earlier 61-probe inventory, this reduced the measured gaps from 23 to 22.
The approved [multidimensional memory input](notes/multidim-fread-execution.md)
now runs, with 156 original source cases. Runtime array reductions, stable
ordering and min/max also run through [one typed lowering path](notes/runtime-array-methods.md),
with 270 original cases and four direct unit tests. Min/max return at most one
source element, including tied keys; measured Icarus duplicate results remain
reference mismatches. Effectful/unverified-pure ordering keys and unsupported
aggregate representations remain explicit limitations.

All six remaining detailed [execution contracts](notes/p1-execution-contracts-2026-10-25.md)
were approved on 2026-10-25. The first implementation adds complete-state
[delayed three-state propagation](notes/delayed-gate-state-execution.md),
with 240 original source cases and three direct tests. The exact expectation-only migration was separately approved and integrated.
[All 21 local-CI checks pass](measurements/2026-10-25/delayed-side-integration-validation.json):
dev/ASan/UBSan pass 4,177/4,179 and release 4,178/4,180, with zero failures and
the same two known skips. Other approved P1 mechanisms remain work.

## Validation records listed under "Test results"

The historical full local CI record for `615afa0` (2026-10-25, macOS arm64) has these results:
all 21 checks pass; dev, ASan and UBSan each run 4,652 tests, 4,650 pass and
the two known skips remain; release runs 4,653 and 4,651 pass, with the same
skips ([record](measurements/2026-10-25/activation-environment-final-validation.json)).

The earlier [main integration](measurements/2026-10-27/conditional-nba-main-integration.json)
includes container conditionals (`a8bb17cc`) and captured NBA payloads (`7bc5fa85`).
All 21 local CI checks are confirmed after README regeneration and a dev rerun:
dev/ASan/UBSan 7,013/7,015 and release
7,014/7,016, with zero failures and two known skips each. The expanded
95-probe inventory retains seven Icarus-passing gaps for each reference,
and four/seven reference mismatches for 13.0-stable/14.0-development.
See the [current validation and limits](notes/p1-icarus-progress.md#current-implementation-and-review-status-2026-10-27)
and [portable per-probe records](notes/icarus-13-comparison.md).
The external sv-tests/population counts and performance measurements below
retain their recorded revisions and dates.

## Local fuzz runs listed under "How it is tested"

The nightly workflow runs one-word, scheduler and container-model
cross-checks at 100 times the normal rounds, with a separate step at the
same scale for values of 1 to 260 bits. Local runs of both steps passed on
2026-10-22; they do not add to the consecutive-night count of the P1
criterion. The separate heap run passed 2,520,000 operations on 2026-10-23;
it also does not add a night to that count.
