# Strength-network force foundation

[日本語](strength-force-foundation.ja.md). 2026-10-30.

The shared kernel implements node-level force updates under the approved
[MOS/switch contract, section 5](mos-switch-proposal.md#5-force-release-and-transit).
This provides the force semantics needed by the later source bridge and
strength-aware formatting. Source-language force/release on registered
strength nets remains unsupported; existing source and SIR rejection checks
remain in place.

## Native interface

`Scheduler::setStrengthForces` accepts a nonempty span of `StrengthForceUpdate`.
Each update names a configured node and either a scalar `W4` value or
`nullopt` to release it. It returns `StrengthPublication`, distinguishing a
committed snapshot from a failure before publication. Values must be
normalized to bit zero. Invalid nodes, duplicate nodes and invalid values
leave both force state and the published snapshot unchanged. The driver bank
must have started. Repeating an identical nonempty batch is a successful no-op.

`StrengthDrivers::forced(node)` returns the current optional force value.
Invalid and unforced nodes both return `nullopt`; `nodeCount()` distinguishes
their validity. This read-only inspection also permits verification of
rollback after a scheduler error.

These interfaces consume the final override values. They do not select an
owner or implement overlapping source-force ownership. The existing ordinary
hold machinery retains that responsibility until its strength bridge is
implemented. No SIR opcode, VM adapter or external C ABI is added here.

## Resolution and publication

A forced node suppresses its local physical origins, including explicit
implicit-pull origins. One virtual root carries its force value at strong
rank 6. The forced node's own readout is exactly this contribution, including
`[0,0]` for Z. Remote roots still traverse that node and can reach other
nodes. Resistive paths attenuate the virtual contribution normally. These
are the approved project strength choices, not a claim that all reference
simulators make the same choices.

Ordinary origin updates and their inertial deadlines continue while the
readout is forced. Release solves from their current committed states
synchronously; it does not restore a cached pre-force value. Uncertain
conduction, destination net kinds and physical-origin path identity retain
the existing passive-network rules.

One batch solves every affected publication component before publishing
all intervals and projected values together. Callbacks therefore see the
complete new snapshot. Ordinary value notifications still require a projected
value change; interval-only changes notify internal strength subscribers.
Callback-driven origin updates queue later work, and a recursive force batch
returns `Reentrant`. Pending Active/Reactive component work is preserved.

Configuration owns the force state and scratch buffers. A failure or
interruption before publication restores the prior force state and keeps
the old published snapshot. `ResourceLimit` identifies allocation failure;
the appended `Interrupted` status identifies another interrupted solve.
A failure after publication reports `committed=true` and does not roll back
the already visible snapshot. Existing status values retain their numbering.

## Evidence and remaining work

The independent `valsem` overload extends the slow enumeration of conduction
worlds, root points and simple paths. It retains the old three-argument
interface and underlying physical-origin validation. New kernel tests compare
complete intervals against that reference, and separate scheduler tests cover
timers, callbacks, batching, isolation and failure behavior.

[Seven value tests](../../tests/unit/strength_force_value_test.cpp) include
2,880 interval/net-kind/force comparisons and 2,916 transfer/conduction-world
comparisons, plus duplicate paths, cycles, rootless networks and 65-bit
element boundaries. [Ten runtime tests](../../tests/unit/strength_force_runtime_test.cpp)
check synchronous release, queued evaluation sets, pending deadlines,
coherent callbacks and both pre-commit and post-commit failures. Six compiled
semantic faults were caught: weakened virtual strength, lost virtual roots,
missing readout clamp, omitted rollback, consumed queued work and blocked
remote transit. After restoration, all 17 tests pass. Existing SIR output and
PerfCount checks also pass unchanged.

The next integration still needs the source/SIR hold bridge and a separate
reviewed migration of existing registered-force rejection expectations.
`%v`, retained force-observable port identities and complete P1 remain work.
