# Automatic-storage wait dependencies

[日本語](automatic-storage-observation.ja.md). Status: kernel integration
implemented, 2026-10-25; source capture integration remains work.

## Contract and implementation

This follows sections 2/4/6/8 of the approved
[automatic-capture contract](automatic-capture-proposal.md), mapped to
SCHEDULER_SPEC 9.5 and RUNTIME_ABI 4/6 by the
[approval record](p1-execution-contracts-2026-10-25.md).
The source-language grounds are IEEE 1800-2023 6.21, 9.3.2 and 9.4.
No other simulator implementation or tests were read. No dependency, global
state or external C ABI entry is added.

Previously the scheduler did not consume the
[storage foundation's](automatic-storage-foundation.md) change notices.
`Scheduler::changedActivation` now routes committed normalized changes through
the existing ordered late-change queue. `writeActivation` commits the typed
write and flushes notices before the next instruction. Source evaluators never
run inside the store's observer. Native execution callbacks use this wrapper
for observable writes. Direct store writes remain a storage API; batching
callers explicitly flush before the next observable operation.

`MonitorSpec::activation` supplies lexical extent bindings, optional executed
read capture and independent static sensitivity ranges.
`ActivationStore::region` validates Context identity, generations, fields and
element/bit bounds without reading values or invoking observers. Registration
pins distinct extents transactionally before evaluation. Several fields share
one physical scope; all its typed fields remain retained. Foreign/stale
bindings and invalid ranges fail before expression evaluation.

Successful reevaluation replaces the executed-read footprint. Static ranges
remain, including unexecuted branches. Reads from a called function's private
automatic extents are not implicitly imported. Existing value/edge/iff/truth
and Active/Reactive rules stay in the shared scheduler. A container field
observes both binding replacement and its current heap structure/cells;
the typed root provider retains the physical field once.

Completion, replacement and cancellation detach reads and release leases.
Serial checks reject queued notices for reused monitors. Temporary extent
leases keep a callback's environment valid even when it cancels its own wait.
Hooks are restored after the last owner leaves. Observer scratch allocation
failures use the existing resource-limit path without rolling back committed
writes. The nonallocating typed-root visitor is unchanged.

## Evidence and remaining work

Fourteen original `ActivationScheduler` tests pass, covering 32 four-state edge
combinations, wide fields, independent elements/scopes/Contexts, readonly and
normalized writes, static ranges, parent return, self-cancellation, mixed named
waits, nested guard writes, container replacement, root reclamation and resource
failures at every registration allocation and notification allocation.
Ten thousand replacement waits keep registration storage bounded and return
the extent lease count to its original value.

The initial-evaluation follow-up first reproduced a lost notification: an
expression read a field, wrote it and returned the old value before the first
footprint was attached. The new test failed before the fix and passes after it.
In-progress read captures now participate in change matching, with nested
capture ownership restored on return; notices still drain after the entire wait
is registered. Six [compiled controls](../measurements/2026-10-25/activation-wait-negative-controls.json)
at `e59c3dd9` all fail actual new tests and restored sources build successfully.
The [seventh compiled control](../measurements/2026-10-25/activation-wait-initial-control.json)
at `e9bec784` drops the in-progress capture: the actual initial-evaluation test
fails, and restoring the source makes all fourteen new tests pass.

The [final validation](../measurements/2026-10-25/activation-wait-final-validation.json)
at `efb3f3d1` passes all 21 local-CI checks. Dev/ASan/UBSan pass 4,569/4,571;
release passes 4,570/4,572. There are zero failures and the same two known skips.
The first full run exposed two clang-tidy errors in the new test helper; both
are corrected. Its root assertions still check exactly one visited root, that
root's token and successful collection, using a fixed nonallocating buffer.
Existing expectations, all 29 SIR dumps and PerfCount budgets are unchanged.

The same 87 [comparison probes](icarus-13-comparison.md#automatic-storage-wait-recheck-2026-10-25)
have identical engine statuses and passing judged bytes. Native sv-tests retains
all 344 statuses and 244 passes. Reference sv-tests engines and Linux were not
rerun; these finite checks do not establish full parity.
The [population recheck](../measurements/2026-10-25/activation-wait-population.json)
uses an archive of the exact selected test revision `179c55de9c83`, not the
caller's newer working tree: 1,451/1,466 pass, zero fail and 15 are unsupported.

SIR scope homes/instructions, front lexical capture, VM binding dispatch/unwind,
refs, managed automatic events and automatic display callbacks remain work.
Existing source capture diagnostics stay in place. No Icarus-passing gap is
claimed closed; the finite inventory still has 22 gaps against each reference.
Full P1 coverage and the source acceptance matrix remain unproven. Simulation
wall time was not remeasured for this kernel integration; PERFORMANCE's existing
procedure remains authoritative.

## Next integration boundary

The VM currently exposes mutable frame-word pointers through `frameVar`,
`frameElem` and direct `act_.vars` access. Scoped homes need one typed read/write
path across scalar/wide/packed accesses, references, scanning, binary input,
random-seed copyback and parameter copyback; mirroring fields into ordinary
frame words would break shared identity and notifications. `Lowerer::lowerFork`
currently starts a fresh child `Body`; it needs explicit lexical capture
metadata, full-scope promotion and per-entry ownership before that rejection
can be removed. SIR must verify homes, capture compatibility and ownership
unwind, and root-call storage must precede parameter initialization. These
are remaining implementation steps under the approved contract, not completed
source support or a new amendment. Later checkpoints took the SIR and VM-access
steps: the [SIR metadata/verifier checkpoint](automatic-sir-metadata.md) and the
[VM value-access migration](automatic-vm-frame-access.md). Source capture
remains work.
