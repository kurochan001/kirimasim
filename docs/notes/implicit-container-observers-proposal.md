# First implicit container observers: proposed execution contract

[日本語](implicit-container-observers-proposal.ja.md) · 2026-11-05.

**Status: sections 1–8 approved on 2026-11-06. The [first delivery and checks](implicit-container-execution.md) are complete locally. Existing expectation migration was approved separately and applied as `b3d20b8b`.**
The [current support inventory](../P1_SUPPORT.md#dynamic-container-foundation)
distinguishes an executable body from the dependency tracking needed to rerun it.
Main `6f5c6984` still rejects contributing implicit container reads.

[P1_SCOPE](../spec/P1_SCOPE.md) requires review of detailed semantic amendments
where specifications are incomplete. SCHEDULER_SPEC 6.3/9.2 determines which
source reads contribute, but the current SIR trigger ranges describe storage
words, not container values. Its 9.5 executed-read monitors do not specify an
implicit observer that must arm without executing user code. This proposal fills
that boundary in SIR_SPEC 7.1/7.2, SCHEDULER_SPEC 9.5 and the typed services of
RUNTIME_ABI 6. It preserves the existing sensitivity rules and P1 gates.

## 1. First executable subset

Support existing integral-element dynamic arrays and queues, including bounded
queues and supported two/four-state widths, when their variable has an ordinary
static typed slot. This includes module/interface/generate storage, qualified
package/unit names, existing non-parameterized class-static storage, and eligible
hierarchical references to static subroutine storage.

Connect these bindings to `always_comb`, `always_latch`, leading `always @*`
procedures and direct continuous assignments where the existing frontend admits
the source form. Initial scope covers whole-container dependencies from size
queries and by-value input arguments. `@*` also permits existing element reads
whose dependency is the entire named container identifier.

Contributing selected-element reads in comb/latch/continuous expressions retain
a positioned unsupported boundary until their longest-static-prefix descriptor
is reviewed. Even if a whole read could subsume a select, this first delivery
need not accept that mixed form. Continuous assignments whose called bodies read
containers retain their current diagnostic in this delivery. Ordinary hidden
function-body reads already excluded from `@*` sensitivity remain executable.

Automatic/ref-based container bindings, nested implicit controls, nested or
nonintegral elements, associative arrays and class-dependent observation are
outside this first delivery. They remain P1 work where required, not new P1
exclusions. Keep strict slang legality and existing source-form restrictions.

## 2. Select dependencies statically

Apply SCHEDULER_SPEC 6.3 and 9.2 before constructing typed dependencies:

- Comb/latch includes eligible reads of called ordinary functions and static
  methods, following the existing transitive read graph. Exclude own/called
  declarations, written ranges, timing-control-only reads and assertion actions.
  Preserve the existing class-reference and instance-method argument-only rule.
- `@*` includes directly read identifiers and actual argument expressions,
  including a container passed by value. It does not import called-body reads.
  A directly qualified static-function variable is not discarded merely because
  it belongs to a subroutine; hidden reads of a function body are a separate case.
- Direct continuous reads use their source operands. This delivery does not
  change the existing called-container-body boundary of continuous assignments.

Record declaration identity, type and eligible storage in front. Do not use
executed reads to decide the static sensitivity set. Arming, rearming and
reattachment must not call a source function, repeat an argument side effect,
execute the process body or allocate a temporary source-language container.
Deduplicate bindings deterministically. Preserve each process kind's existing
write checks, call restrictions and time-zero policy.

## 3. Define value changes independently of token changes

**Proposed interpretation for review:** compare the complete logical value of
each eligible integral container: length and ordered element bits, retaining
0/1/X/Z distinctions and masking unused bits. Compare exact values, not a hash,
the result of `size()`, or the internal container/cell tokens.

This typed dependency is an implicit sensitivity binding, not a new explicit
event-expression form. Keep explicit event-expression legality and scalar
result watches unchanged; do not enable `@(q)` for an aggregate by analogy.

Growing, shrinking, reordering unequal elements or changing an element can wake
the process even when its scalar computed result remains equal. X→Z is a change.
Replacing storage with an equal value, including self-assignment or same-content
reallocation, does not wake it solely because new tokens were allocated. Empty
to empty and a no-op invalid write do not create a value event.

This does not change copying or reference identity: copies/resizes still make
fresh cells where the approved container contract requires them. Queue movement
still preserves retained cells, and old references still retain detached cells.
Logical equality never becomes permission to skip those storage operations.

## 4. Observe committed changes and preserve scheduling

Capture dependencies and a read-only value snapshot when the wait is armed.
Observe both binding replacement and committed structure/cell changes. Judge a
change at publication, including inside a function or callback walk; defer the
scheduled process, not the comparison, until the existing scheduler permits it.
Do not lose A→B→A changes by comparing only the final value after the writer ends.

A multi-cell operation such as a swap has one publication batch. Its temporary
internal states are not events. The first qualifying event ends that process's
current wait and invalidates its other items; later writes before it resumes do
not create duplicate resumes. Rearm according to the existing process-kind rule.

Comb/latch still executes once at time zero; `always @*` initially waits. Direct
continuous processes retain their existing sensitivity while running. Module and
program origins retain their existing Active/Reactive scheduling where the source
form is legal. Introduce no polling timer, delta-delay workaround, hidden process
or new region order, and do not keep a quiescent design alive.

## 5. Typed SIR and shared kernel boundary

Add explicit typed implicit-container dependencies to process metadata, separate
from ordinary integral slot triggers and expression-result watches. Each binding
names an eligible static container slot and its container type descriptor; this
first form covers one whole container variable with an Any-change dependency.
The same source must not also acquire an ordinary token-word subscription.

Before runtime allocation, verify process kind, slot bounds/lifetime/type,
container descriptor and integral element layout. Reject forged handle types,
partial token ranges, incompatible descriptors and unsupported observation forms.
Emit deterministic diagnostic SIR text for the new metadata while preserving old
format-0 goldens that have no such descriptors. This does not freeze SIR v1 or
change the external C ABI version.

The VM mechanically binds verified numeric slots/types to the kernel service.
The shared kernel owns snapshots, value comparison, weak dependency registration,
publication, cancellation and wakeup. Do not add a second VM sensitivity algorithm,
runtime name lookup, a dependency on slang in kernel, or mutable global state.
Public kernel headers remain C++20.

## 6. Ownership, reattachment and failure

Snapshots own normalized integral value words and length, not a source container
copy or GC roots. Watch markers are weak. The source variable's existing typed
root keeps its current value alive. Internal snapshot reads must not pollute an
outer source expression's executed-read capture or invoke user callbacks.

After an equal-valued binding replacement, attach to the replacement's structure
and cells even though no wake occurs. Detach old markers; later writes through
old detached references must not wake the new variable's observer. Use existing
generation checks so reclaimed tokens cannot name a later object accidentally.

Cancellation, scope/process termination, final cleanup and interpreter teardown
remove markers and invalidate queued notices without allocating. Contexts remain
independent. Build registration/snapshot state transactionally; allocation or size
failure ends with the existing ResourceLimit/code 6 and never installs partial
dependencies. Preserve malformed-IR and internal-fault classifications. Add no
undocumented capacity limit or new runtime option.

## 7. Existing rejection expectations to review after implementation

No existing expectation changes in this proposal. The known follow-up comprises
**11 rejection checks in two existing validators**:

- [Dynamic-container CLI checks](../../tests/cli/run_dynamic_container_tests.py):
  five forms (`always_comb`, `always_latch`, `always @*`, direct `assign` reading
  `q.size()`, and comb calling `read_size()`), plus two comb forms that read
  `top.holder.q.size()` directly or through `read_nested()`.
- [ImplicitFront.ActualImplicitContainerReadsRemainDiagnosed](../../tests/unit/implicit_front_test.cpp):
  four forms: wildcard direct size, wildcard `read_argument(q)`, comb
  `read_global()` and latch `read_global()`. Its `assign observed=read_global()`
  rejection remains; source function bodies in continuous assignments are not
  enabled by this first contract.

After independent implementation checks pass, present the old/new judgments and
seek separate expectation-migration approval. Keep migration separate from src
changes, preserve source inputs and verify the retained rejection boundaries.
This proposal's approval does not authorize that migration, test deletion,
renaming, weakened assertions, or any performance-budget change.

## 8. Acceptance and approved scope

The 2026-11-06 approval of sections 1–8 authorizes this first typed observer contract and its
implementation, not P1 completion or a broader interface. No dependency or
installation is selected. Before enabling source acceptance:

- Check kernel snapshots/events against an independent sequence/value model:
  empty/nonempty values, same-size edits, ordering, equal copies, X/Z, bounded
  operations, invalid writes, multi-cell publication and A→B→A inside a walk.
- Cross source contexts, container kinds, widths and two/four-state domains,
  direct/actual/called reads, no-argument `@*`, write exclusions, private called
  storage and qualified static subroutine storage. Check function-call counts
  to prove that arming executes no source code.
- Verify equal-copy reattachment, detached refs, generation reuse, cancellation,
  failed allocations, teardown, Context isolation, and malformed native SIR.
- Compile deliberate faults for token-only/size-only equality, collapsed X/Z,
  delayed final-state comparison, wrong call closure, arming side effects, stale
  reattachment and retained watches. Applicability checks alone are insufficient.
- Preserve existing value, lifetime, scheduler, object/event, explicit-watch and
  wildcard-function tests; run full dev and related release/ASan/UBSan checks,
  clang-format/tidy, then complete local CI before an authorized push. Update
  counts separately from successful JUnit; keep fixed external gates unchanged.

## 9. Evidence and reproduction (not an implementation claim)

The [96-case inventory](../measurements/2026-11-05/implicit-container-review.json)
uses main `6f5c6984` and explicitly selected Icarus compiler/runtime **13.0 stable
(`v13_0`)**. Two historical sources and hashes are retained byte for byte.
Eighteen additional minimal growth probes also match their written size result
on Icarus; native rejects all 20. These establish concrete execution gaps.

The other 76 sources explore the proposed broader rules. Across all 96, native
has 84 unsupported results and 12 matches with proposed outputs. Icarus has 26
matches, 30 mismatches and 40 process failures. The 72 value traces include
element edits, equal copies, transient restoration, X/Z, delete/regrow and called
functions; four sources explore direct continuous assignment. Keep process
failures separate and do not adopt reference differences as correct semantics.
No source of either comparison simulator was read.

At measurement time the broader expected traces were **unapproved candidates**;
the 2026-11-06 contract approval does not turn these observations into accepted
tests or a P1 pass rate. The original 188-probe inventory, optional `wire real`
exception and external populations are unchanged. This proposal adds no runtime
support. The local 2017 reference was checked for static dependency selection
(9.2.2.2.1, 9.2.2.2.2 and 9.4.2.2). Direct confirmation of the detailed
container-value interpretation against the **2023 text remains unconfirmed**.
The 2026-11-06 approval adopts section 3 as the project contract. Direct audit
against 9.2.2.2.1/9.4.2 via the
[official IEEE standard access page](https://standards.ieee.org/ieee/1800/7743/)
remains a separate verification item. The older dependency rules alone do not
establish the whole-value equality rule. No upstream report was sent.

```sh
python3 tools/implicit_container_inventory.py --self-check
python3 tools/implicit_container_inventory.py --kirimasim build/dev/src/driver/kirimasim --output build/implicit-container-review.json
```

For reference observations, add both `--iverilog=<compiler-path>` and
`--vvp=<runtime-path>` and verify their versions. Use `--sources=<directory>` to
retain the original sources. Tool exit zero means observations were recorded,
not that the feature or proposed semantics passed.
