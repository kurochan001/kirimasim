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

Thirteen original `ActivationScheduler` tests pass, covering 32 four-state edge
combinations, wide fields, independent elements/scopes/Contexts, readonly and
normalized writes, static ranges, parent return, self-cancellation, mixed named
waits, nested guard writes, container replacement, root reclamation and resource
failures at every registration allocation and notification allocation.
Ten thousand replacement waits keep registration storage bounded and return
the extent lease count to its original value.

Initial full dev validation passes 4,568/4,570: zero failures and the same two
known skips. Existing expectations, all 29 SIR dumps and PerfCount budgets
are unchanged. Compiled controls and four-configuration local CI are checked
separately before publication.

SIR scope homes/instructions, front lexical capture, VM binding dispatch/unwind,
refs, managed automatic events and automatic display callbacks remain work.
Existing source capture diagnostics stay in place. No Icarus-passing gap is
claimed closed; the finite inventory still has 22 gaps against each reference.
Full P1 coverage and the source acceptance matrix remain unproven. Simulation
wall time was not remeasured for this kernel integration; PERFORMANCE's existing
procedure remains authoritative.
