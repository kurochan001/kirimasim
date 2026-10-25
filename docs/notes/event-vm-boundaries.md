# Executing typed SIR events at existing collection boundaries

[日本語](event-vm-boundaries.ja.md). Status: boundary component implemented and checked, 2026-10-26. Source automatic events remain gated.

## Grounds and implementation

The grounds are the approved [automatic-capture contract](automatic-capture-proposal.md)
5/6/7, SIR_SPEC 3.2.1/7/8, RUNTIME_ABI 4/6 and IEEE 1800-2023 6.8/15.5.
Implementation, test designs and the added event timing workload are original.
No external implementation or tests were read, dependency added, C function-table
ABI changed, performance budget changed or existing expectation weakened.

`7e188c56` connects `event.new`, loaded and legacy slot targets, named/mixed
waits, immediate/nonblocking triggers and `.triggered` to the existing typed
kernel operations. A fresh object's construction pin lasts until its SSA value
is published. The kernel decides identity, null behavior and operation ownership;
the Interpreter transports values and maps error categories to existing results.
Null triggers schedule nothing; null waits give positioned design errors.
Invalid nonnull identities remain internal failures. The optional normalized
delay overload lets the kernel handle null before reporting an unrepresentable
delay, while retaining ordinary nonnull overflow behavior.

The Interpreter publishes typed slots, ordinary frame/array rows, retained
scoped/captured extents and supported input/deferred values. It follows the
existing activation-reference graph and filters pending write snapshots by
their destination slot's type. Runtime waits, pending triggers and monitors
retain their concrete targets. Integers and ref encodings are not inferred to
be event values. Enumeration callbacks allocate nothing and execute no source.

Collection runs only at the **existing** region/slot safe points, outside native
processes, synchronous calls and guard evaluation. SSA cannot cross a wait.
The event collector runs even without a container heap or another heap mutation;
heap collection cadence and termination/final restrictions stay as specified.
The previous hook is restored and outstanding VM callbacks are detached on exit.
Event destination metadata is prepared lazily on the first `event.new`.

For explicit `.triggered` monitors, the kernel captures the objects actually
read, including those read by called functions. The variable/index dependencies
remain, but obsolete statically guessed object lists are removed. Re-evaluation
replaces object dependencies rather than adding every array object forever.
This does not enable implicit named-event sensitivity currently rejected by front.

## Execution evidence

Eleven original `VmEvent` tests verify 35 hand-built SIR designs. They cover
fresh/distinct SSA objects, loaded permanent array selections, null operations,
nine slot/ordinary/scoped scalar/array combinations, Active/Reactive delayed
targets and named/mixed waits, object dependency replacement, input/return sharing,
retained unread arrays, post-Interpreter triggers and resource/hook cleanup.
Another kernel test checks unrepresentable delays and a further test checks
inline and overflow staging at one/four/five named items, including failed
replacement and invalid targets. These are SIR/kernel execution tests; they do
not establish source lowering or an exhaustive event-language contract.

The boundary reuse test makes 128 objects, with a one-tick wait after each,
and reuses one physical row without any container mutation. The unread-array
test observes all three objects while only a different captured field is read.
Program completion keeps its specified termination behavior: final reclamation
is not forced through a forbidden VM collection point.

The [final compiled-control record](../measurements/2026-10-26/event-vm-controls.json)
contains 13 detected faults, including three relocated controls. All compile;
the restored 76 focused cases pass. The [full local CI](../measurements/2026-10-26/event-vm-validation.json)
passes all 21 checks: dev/ASan/UBSan 4,967/4,969 and release 4,968/4,970,
zero failures and two known skips each. Existing dumps (29) and PerfCount pass;
existing expectations are unchanged in this VM stage. README counts come from
the fresh dev JUnit report. The [initial focused controls](../measurements/2026-10-26/event-vm-initial-controls.json)
and [stopped CI](../measurements/2026-10-26/event-vm-initial-ci.json) are retained
so their missing integration coverage is visible.

The fresh [87-probe comparison](../measurements/2026-10-26/icarus14-event-vm-probes.json)
preserves all native/Icarus-14 statuses and passing output bytes. Historical
Icarus-13-passing gaps remain 21; there is no fresh Icarus-13 run. Population,
sv-tests and Linux were not refreshed, and complete parity is not established.

## Integration correction and cost

The first full CI at `7e188c56` found the existing
`VmContainer.HeapMonitorFailuresHavePositionsAndWaitHooksNeverOutliveTheInterpreter`
failure in dev and release. It was stopped rather than reported all-green.
The generic typed-wait error was reported before the existing positioned
heap-monitor error. `39aa6aaa` restores the correct reporting priority without
changing that test. A compiled control reinstates the bad order.

A focused five-pair cost check initially found about 27% higher named-event
roundtrip time. The kernel now stages up to four resolved named items inline
and allocates pin storage only for managed identities. Larger lists retain the
fallible overflow path. Validation and atomic registration still use the same
kernel operation. The test's zero-budget claims concern registry staging,
not every allocation made by the scheduler's other containers.

After full CI, the [final cost observation](../measurements/2026-10-26/event-vm-speed.json)
compares clean release binaries in five alternating pairs at 200,000 iterations.
Startup-adjusted ordinary frame access is 0.25967 → 0.26094 s (about +0.5%);
named-event roundtrip is 0.04154 → 0.04840 s (about **+16.5%**). All counters
match and median peak RSS stays 9.203125 MiB for both workloads/binaries.
The [initial timing](../measurements/2026-10-26/event-vm-speed-initial.json) retains
the regression before inline staging. The remaining event cost is a performance
concern; this small observation is not the seven-night baseline comparison of
PERFORMANCE 5.1 or a full simulation-speed refresh. No gate or threshold changed.

## Remaining work and human decision

Front still rejects automatic default construction. The approved historical
metadata-input migration (item 3 of [the migration note](event-field-expectation-migration.md))
is unchanged and unapplied. Source construction/copies/indexed operations,
unpublished transfers during nested callbacks, complete SSA publication and the
full original source matrix remain work. This stage does not claim all such
paths tested merely because root transport contains a corresponding case.
Event-specific deferred-report arguments and generic pending-write snapshots
are **not confirmed** by separate designs here; confirm them with retained/dropped
holder matrices across delivery, cancellation and failure before source enabling.

The [managed-event safepoint amendment](managed-event-safepoints-proposal.md)
is **not approved**. Instruction-level collection is not implemented. In
particular, the boundary reuse test does not prove bounded no-wait loops,
recursive calls or long function executions; those need the complete publication
protocol and a human decision on that amendment. Container collection inside
native execution is a separate issue and is not enabled by this work.

Full Icarus coverage and P1 remain incomplete. Finite probe results cannot
establish the whole-language completion condition.
