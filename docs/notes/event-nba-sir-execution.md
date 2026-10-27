# Typed event-NBA SIR and transient VM callbacks

[日本語](event-nba-sir-execution.ja.md). Status: local implementation in progress,
2026-10-27, based on main `7cdd2a2e`. Source event-controlled NBA is still
unsupported. This stage is not a P1 exit or a whole-language parity claim.

## Basis and boundary

This implements the already approved [event-NBA execution contract](event-nba-proposal.md)
and its [specification mapping](p1-execution-contracts-2026-10-25.md), following
the [kernel expression-monitor checkpoint](event-nba-expression-monitors.md).
The relevant project sections are SIR_SPEC 5.3/7.2/7.3/8/10, SCHEDULER_SPEC
8/9.5/10.8/13, RUNTIME_ABI 5/6 and VALUE_SEMANTICS 3.11/3.13. IEEE 1800-2023
9.4.2, 9.4.5, 10.4.2, 13.4.4, 15.5 and 18.14 identify the background;
no standard text or examples, other simulator sources, tests or reproducers
were copied. No dependency or detailed-spec change is selected here.

`front` still rejects event-controlled NBA. Existing unsupported diagnostics,
source test bodies, dump expectations and PerfCount budgets are preserved.
The new checks construct original SIR designs directly. They establish the
transport boundary below, not source evaluation order or language coverage.

## One instruction and immutable typed layouts

`Op::NbaEvent` is nonterminating and has no result. Its operands are
`!nN`, the captured integral count, then captured values. A design-owned
`NbaLayout` defines the value types, ordered event items and ordered writes.
`NbaValueId` is a capture ordinal; it is not an SSA temporary ID. Instructions
need no new per-instruction vectors or mutable execution data.

Event items describe fixed slot ranges, captured named-event values and
expression functions, with optional one-bit `iff` functions. Targets describe
fixed slot elements/ranges, dynamically selected fixed-array elements,
packed selections with parent windows, static references and reference bits.
Multiple leaves and zero effective destinations remain one control. Captures
must be raw SSA values or constants with exactly the layout's types.

The verifier checks every layout, including unused rows: IDs, count/capture
shapes and domains, callback roles, target eligibility, selector arity,
reference leaf types and process write-set coverage. `sir::validSlotRange`
is shared with existing verification; `sir::nbaWrites` supplies conservative
static write footprints. Actual reference eligibility is checked when bound.
An invalid actual reference reports a design error before publishing a payload.
A missing leaf diagnostic initially produced `internal`; its native regression
now requires `error` and an event-NBA diagnostic.

Text prints `nba.layout !nN captures=(...)` and `nba.event !nN, ...`, preserving
item/leaf order, selector ordinals and declared types. Empty tables print
nothing, so old dump text is preserved. `formatVersion` remains **0**. No SIR
parser is added and no print/parse/print guarantee is claimed: grammar v1 and
that guarantee belong to P2, while P1 can still add instructions.

## Shared-kernel binding and owned payloads

The VM resolves the typed selectors using the existing index and packed-range
operations. Invalid selections omit only their write, not control evaluation.
The existing bounds-warning reporter is shared with ordinary reads/writes:
`--bounds-check=warn` counts each invalid or clipped write and reports its
source location once. Diagnostics do not change snapshots or clipped values.

Targets, words and independent destination container copies are captured before
registration. The kernel owns their lifetime, count, notification coalescing,
readiness and pending issue order. Front lowering must still snapshot the RHS
before count/LHS effects; copying a value when binding cannot substitute for
that earlier source-language ordering. A single payload covers the whole list.

`kernel::classifyNbaRepeat` extracts the existing full-width normalization rule
into one pure function, shared by registration and the VM. Zero, negative and
X/Z counts enqueue the payload without preparing or initializing callbacks.
The VM does not decrement counts, create a child process or install a resume PC.
The public kernel interface remains C++20 and the C ABI stays 0.3.

## Fresh callback activations with the origin context

Callbacks are automatic zero-argument nonwaiting functions. A guard returns
one integral bit; a watch returns an integral value of its full width/domain.
Callbacks may have their own private frame and lexical scopes, but no borrowed
frame owner, captured enclosing rows or inherited lexical extents. These are
ordinary SIR functions rather than waiter-frame `Guard`/`Watch` functions.

The callback bundle owns stable invocation records separately from an ordinary
wait's callback vectors. Each invocation uses `callWith` to create/enroll fresh
private work variables and performs normal cleanup on return or failure. It
uses the retained origin frame for RNG, source/report identity and scheduling
group. It does not copy/reseed RNG or attribute a call to the triggering process.
The complete kernel registration owns cleanup before initialization callbacks;
accepted registration can finish synchronously without a second deletion.

An internal wrapper is excluded from source call depth and stacktrace frames.
A regression with an event fired inside a different process's task exposed
that the interpreter's current synchronous depth belonged to the trigger.
The callback now derives logical synchronous depth from the origin's real
call records; cleanup restores the outer interpreter entry's depth. Physical
stack checks still inspect the executing thread. Direct and task-mediated
triggers retain the same origin and trigger RNG continuations at depth limit 1.

`staticReadsOf` binds the same slot, heap, named-event and called-private-object
metadata for ordinary waits and event-NBA monitors. Ordinary waits additionally
resolve their borrowed activation/ref dependencies. Fresh event-NBA callback
locals are not borrowed from the issuer's suspended or returned task frame.

## Verification and remaining work

The local native suite adds 13 SIR verifier/text tests and 16 VM tests. Matrices
cover module/program scheduling, ended/live issuers, ordinary waits, immediate
registration, consecutive notifications, fresh private work fields, arbitrary
count widths, high value/unknown bits, signed/X/Z skipped counts, actual refs,
clipping, warning coalescing, independent heap copies, origin RNG, returned
automatic tasks, duplicate-item order, empty payloads and captured heap reads.
A deterministic Context-memory test injects one failure at every allocation
point of a successful owned-queue/watch run and checks resource outcomes and
complete deallocation at all 58 observed allocation points. It does not inject
C++ default-allocator failures. Failed descriptor construction is also checked
without letting an empty variant escape verification; the library may instead
preserve its previous valid alternative. Malformed/missing layouts return an
internal SIR error before capture storage is read.

[Final compiled controls](../measurements/2026-10-27/nba-sir-controls.json) detect
all 20 faults and record restored source/test/tool hashes. The
[initial 16 controls](../measurements/2026-10-27/nba-sir-initial-controls.json)
all compiled and were detected. The expanded
[preliminary run](../measurements/2026-10-27/nba-sir-preliminary-controls.json)
detected 19/20: a VM-only error-category change could not alter the reason the
kernel had already set. The corrected fault changes that authoritative kernel
request; the unchanged allocation-failure test detects it. All 20 were rebuilt
and detected after this correction. [Final validation](../measurements/2026-10-27/nba-sir-validation.json)
passes all 21 local CI checks: dev/ASan/UBSan pass 7,109/7,111 and release passes
7,110/7,112, with zero failures and the same two known skips. All 773 unit tests
pass. The [native/reference comparison](../measurements/2026-10-27/nba-sir-comparison.json)
retains population 1,456/1,466 and sv-tests 244/344. Both versioned 95-probe
inventories retain their native status/stdout results and seven gaps; these
finite probes do not establish whole-language coverage. The
[performance record](../measurements/2026-10-27/nba-sir-benchmark.json) measures
three dormant-control designs with five alternating AB/BA pairs against main;
it does not measure source event-NBA performance.

Remaining source work includes front capture ordering and aggregate lowering,
source callback/RNG/scope/lifetime matrices, ordinary/event NBA queue interactions
from source, program termination and nested callback cases. Existing rejection
oracles require a separately reviewed migration before they change. The known
direct hierarchical read of a called function's private state and source-purity
differences in the preceding monitor note are not fixed here. Managed-event
payload substitution and full lexical-activation/source-scope matrices remain
not confirmed; exercise those original matrices before enabling the source path. Source purity,
strict slang diagnostics and the whole P1 functional target remain unchanged.
