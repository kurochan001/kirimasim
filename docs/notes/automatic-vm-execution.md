# Executing lexical automatic environments in tier 1

[日本語](automatic-vm-execution.ja.md). Status: Interpreter integration for the
currently represented value homes implemented, 2026-10-25; front source capture
is not enabled.

## Contract and execution

This connects the [environment](automatic-vm-environment.md),
[value transport](automatic-vm-frame-access.md) and
[scoped-reference](automatic-vm-scoped-refs.md) components to `Interpreter`.
The basis is sections 1–4/6–8 of the approved
[automatic-capture contract](automatic-capture-proposal.md), its
[specification mapping](p1-execution-contracts-2026-10-25.md), SIR_SPEC 7/8,
RUNTIME_ABI 4/6 and IEEE 1800-2023 6.21/9.3.2/13.5.2. Implementation and tests
are original; no other simulator's implementation or test was read or copied.
No dependency, CLI switch, public C ABI or physical frame ABI is added.

The existing automatic metadata verifier still runs before slot allocation.
The blanket load rejection of valid new homes is removed. Stable decoded plans
and reference shapes outlive their environments. Each registered `VarsId`
optionally owns an environment; ordinary bodies need no environment allocation.
Scoped/capture rows occupy no ordinary value words. `framePlace` and `refPlace`
resolve their actual binding and selection through the common value boundary,
with no second mutable copy. Kernel normalization and notification remain the
one meaning of the write.

Root owners exist before argument copy-in. Promoted output formals receive their
language defaults before the body executes; safe internal zero storage is not a
declaration initializer. Declarations still require their explicit lowering.
Normal call/task return reads outputs, copies them back once, then releases the
callee's executing owners. A later child mutation of a retained output does not
repeat copyback. Synchronous and deferred calls restore the current body, call
depth, scopes and ownership through cleanup guards even when allocation fails.

`activation.enter` creates fresh block storage, and `activation.leave` releases
its owner and borrowed descriptor tickets. Named control scopes save the actual
`VarsId` and lexical depth; leave/disable unwinds to that depth. Finish detaches
waits/display callbacks before dropping executing owners and imported bindings.
Data ownership ends independently of `tryRelease` and its ancestry/report/queue
obligations. Program-killed processes use the same cleanup. Whole-run cleanup
also invalidates ordinary slot/iff subscriptions before their VM frames expire.

Fork uses the currently executing body, including an automatic task call, rather
than the process's outer frame. It resolves every child environment before
creating runnable children. It reserves the publication lists, registers all
provisional processes, queues them and only then commits parent RNG/birth/join
state. Failure cancels provisional processes and releases their values; stale
queued records retain only their existing scheduler/control obligations. Storage
adds no process and draws no random value. Each successful branch still gets
exactly one child RNG in source order.

Reference creation uses immutable declaring origins and generation-checked
descriptors for owned/captured data. Effective readonly rights also travel in
ordinary/slot/cell reference payloads and are checked before mutation. Captured
ref bindings remain immutable after a formal is reused. The resolver distinguishes
an actual static slot from local-origin classification, preserving the approved
`ref static` rule and the normal-ref restriction to `join`. A borrowed reference
does not gain an owning lease or survive its creating scope merely because a
child retains the underlying value.

Guards/watches borrow the actual waiter's registration and temporarily pin its
whole environment, including empty/unread scopes. Monitor setup includes static
activation ranges from all guard branches, with write targets excluded, and
executed-read capture for the owned bindings. Shared referenced actuals resolve
to activation ranges too. Container variable replacement and heap contents keep
their separate observation paths. Captured borrowed refs enter the typed root
walk; the kernel publishes promoted fields directly, including unread fields of
an extended scope. Callback entry points contain allocation failures.

## Verification

Nineteen new execution tests include 24 width/domain/ref combinations (7–257
bits), all joins, parent/sibling writes, returned owners, block reentry, multilevel
forwarding, both call paths, default outputs, static-qualified ref/const-ref
capture across formal reuse, shared actual refs, current-task guards, static
unexecuted-branch sensitivity, independent owner/control lifetime, unread typed
heap roots, scope/program cancellation and an iff subscription after VM return.
Eighty injected kernel allocation budgets exercise construction and complete
fork-group failure: ResourceLimit is returned, no child result is published and
all scope leases are gone. These budgets do not exhaust every allocation point
of the Interpreter's ordinary C++ containers; component PMR failures are covered
separately by the preceding notes.

Fourteen actual compiled controls cover ten new integration faults, two existing
environment/guard faults and two relocated control/stacktrace faults. The
relocated birth-serial fault initially survived the old disable matrix. An added
test first reclaims one early child while another remains alive, then disables a
later named block: the older child must survive. The compiled fault now fails
that test. Compile failures are never counted as behavioral detections.

At `b17234e4`, the [complete local-CI run](../measurements/2026-10-25/activation-interpreter-final-validation.json)
passes all 21 steps: dev/ASan/UBSan 4,680/4,682 and release 4,681/4,683, zero
failures and the same two known skips. The
[compiled-control record](../measurements/2026-10-25/activation-interpreter-negative-controls.json)
keeps the initial surviving birth fault and its later detected rerun, including
the separate 18-/19-test hashes. Production source is unchanged across those runs.
Existing suite bodies, expected outputs, all 29 SIR dumps and PerfCount budgets
are unchanged.

The fresh [87-probe run](../measurements/2026-10-25/icarus14-activation-interpreter-probes.json)
preserves every native/14.0 status and passing judged byte; the historical 13.0
counterpart still leaves 22 passing gaps. The exact archived
[P1 population](../measurements/2026-10-25/activation-interpreter-population.json)
stays 1,451/1,466, zero failures and 15 unsupported, and pinned
[sv-tests](../measurements/2026-10-25/activation-interpreter-svtests.json) stays
244/344 with every native status unchanged. Linux and other engines for those
two datasets are not remeasured; no fresh clean 13.0 build is claimed.

A [focused ordinary-transport observation](../measurements/2026-10-25/activation-interpreter-speed.json)
uses five alternating pairs after CI: adjusted wall time is 0.2500 s before and
0.2599 s after, about +4.0%, with identical counters. Median peak RSS is 9.1875
and 9.203125 MiB (+16 KiB). This is a small sample, not a confidence interval or
a full simulator comparison. The standard four workloads, source capture and
Linux are not timed, and no performance budget is changed. Finite comparisons
cannot prove full Icarus coverage.

## Remaining work

Front still rejects source automatic fork capture and emits none of these new
homes. Next implement lexical promotion/capture lowering and the approved full
source matrix, including declaration order, loop/block lifetimes, ref qualifiers,
copyback, cancellation, monitors and typed roots. Hand-built SIR execution alone
does not close the source feature gap.

Owned automatic event/ref/object providers remain unsupported; borrowed ref
formals are a different case. The event construction/rebinding work of contract
section 5 is not implemented here. Complete SSA reference provenance and full
descriptor-generation exhaustion remain not confirmed; see the preceding
reference note for the latter's verification method. The two defensive malformed
SIR tests check runtime rights/lifetime without claiming such escaping or mutable
const refs are legal SystemVerilog. P1 and complete Icarus 13.0 coverage remain
unfinished.
