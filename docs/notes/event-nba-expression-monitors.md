# Event-NBA expression-monitor ownership and notifications

[日本語](event-nba-expression-monitors.ja.md). Status: locally verified kernel stage
(uncommitted), 2026-10-27, based on main `e94315a0`. This stage does **not** enable
source event-controlled NBA. Whole P1/Icarus functional parity remains unproven.

## Basis and one shared rule

The basis is the approved [event-NBA contract](event-nba-proposal.md), scheduler
item 3 and the lifetime/publication contract, its
[specification mapping](p1-execution-contracts-2026-10-25.md), SCHEDULER_SPEC
9.5/10.8, RUNTIME_ABI 5/6 and VALUE_SEMANTICS' edge/value rules. IEEE 1800-2023
9.4.2, 9.4.5, 10.4.2 and 15.5 identify the background. Per-notification OR
coalescing remains the explicitly approved project choice. No external source,
other project's tests or reproducer, or standard text/example was copied.
No detailed specification, dependency, C ABI or SIR instruction changes here.

`Scheduler::registerNbaEvent` now accepts `MonitorSpec` items alongside concrete
slot and named-event items. The same `initializeMonitor`, `updateMonitor`,
`evalMonitorValue`, `evalMonitorBody`, `monitorDependsOnSlot` and `observesEvent`
operations implement ordinary waits and independently owned NBA monitors.
Two/four-state arbitrary-width values, real values, edge selection and `iff`
therefore use the existing shared value/edge operations. A false guard still
updates the previous expression value and its captured dependencies.

The independent owner retains its origin, monitor state, managed event pins,
heap hooks and activation leases through completion and in-flight callbacks.
Cancelling an ordinary wait or ending the issuer does not detach these monitors.
Completion/discard eagerly removes index entries and releases buffers and weak
watches. Free monitor records use an intrusive link in their unused owner word;
release cannot allocate. The existing owner word fits between process and
64-bit generation; no portable whole-record size claim is made.

## Publication and callback reentry

Both ordinary waits and independent NBA controls publish their complete static
dependencies and retained static event values before the first initialization
callback. Initialization captures heap reads already in progress so a write to
a value just read is not silently lost before its weak watch is installed.
Nested evaluations have separate read-capture scopes and owned scratch buffers;
reentrant registration cannot invalidate the current callback's output buffer.
The recycler returns capacity without moving an oversized buffer into every
persistent monitor. Previous-value assignment copies only the actual value.

The first full-CI attempt exposed an existing conditional-container wait looping:
reads of newly created work containers were followed by their construction
writes, causing repeated self-notification. The
[before/after regression record](../measurements/2026-10-27/nba-monitors-work-object-regression.json)
preserves the incomplete CI observation, a timeout of the frozen pre-fix binary,
and all nine unchanged wait sources passing after the correction. Heap
transactions now announce successful allocations after commit; read capture
excludes those work roots and chains allocation observations through nested
evaluations. Failed transactions announce no roots. No per-cell birth field or
allocation sequence is added. Existing external roots remain observable during
initialization, as checked independently. The two new tests also bound the
negative detector so a broken filter cannot make mutation testing hang.

Pre-publication validation/allocation failures leave payload and cleanup
ownership with the caller. Once accepted, synchronous initialization may cancel
or end the control before registration returns. Runtime allocation failures after
acceptance are reported through `Scheduler::reason()` and still return an accepted
ticket: cleanup has transferred and must not be freed again by the caller.
Tests distinguish both cases. The public header documents this ownership boundary.

This kernel callback capability does not widen the source-language permission
to write variables from event expressions. Existing front restrictions and old
rejection oracles are preserved.

## Committed notification groups

Candidates from direct subscriptions and expression dependencies are merged in
captured issue order and deduplicated by owner. Relevant items run in source
order without short circuiting. The owner decrements once after the complete
notification, including mixed concrete/monitor items and multiple matching
expression items.

An atomic heap `swapValues` publishes both cells before announcing their roots.
A Context-owned final-root flag groups those announcements; even an unwatched
last root closes the group. Activation writes similarly collect matching monitor
references into one notice. Ordinary heap hook calls keep their prior order.
No process, child RNG seed, global state or ever-growing notification sequence is
introduced. Named-event matching checks actual static/captured dependencies;
an unrelated slot-only expression is not reevaluated just because its owner
also has a named-event item.

Deferred monitor groups use reusable Context-owned notice rows. Delivery moves
the references into a local owner and frees the row **before** user callbacks.
Cancellation/reentrant registration can reuse the row without invalidating the
current traversal or delivering old references to the replacement registration.
Callback-induced changes remain deferred to the outermost walk's end.

A [coexistence regression](../measurements/2026-10-27/nba-monitors-coexistence-regression.json)
found during final review reproduced a skipped ordinary waiter when another
ordinary callback erased an earlier independent entry. Slot and named monitor
walks now traverse a committed reference snapshot and compact the current index
after callbacks. This preserves newly added entries, excludes retroactive
notification and does not resurrect canceled references. The four-variant
Active/Reactive × slot/named test preserves all its assertions; a compiled fault
restores the unsafe in-place walk and is detected.

## Verification

The [validation record](../measurements/2026-10-27/nba-monitors-validation.json)
records final source hashes, full local CI, unit tests, fuzz settings and explicit
expectation protection. All 21 CI checks pass: dev/ASan/UBSan pass 7,073/7,075
and release passes 7,074/7,076, with zero failures and the same two known skips.
All 742 unit tests pass, including the 27 new kernel tests. The new 27 kernel tests cover independent ownership,
mixed/duplicate items, false guards, all four-state edge pairs, 65/129-bit values,
real values, captured managed-event replacement, heap/index replacement, atomic
swaps, activation ranges, initialization writes, nested callbacks, synchronous
self-cancellation, accepted/rejected allocation failures and bounded row reuse.
They are kernel tests, not newly supported SystemVerilog designs.

[Compiled controls](../measurements/2026-10-27/nba-monitors-controls.json) test
32 deliberate faults in monitor/concrete-control behavior and retained VM
origins. The [initial run](../measurements/2026-10-27/nba-monitors-initial-controls.json)
compiled and caught 27/29; two unused-variable warnings prevented compilation,
so they were not counted as detectors. Both fault replacements were
[corrected and detected](../measurements/2026-10-27/nba-monitors-corrective-controls.json).
The final controls are rebuilt and rerun after the no-allocation release and work-root corrections;
all source files are restored before positive testing.

The mixed-item model runs both scheduling groups with 12 controls, six items per
control and 80 operations per round. At scale 100 it checks 4,000 rounds,
48,000 controls and 320,000 notifications. The earlier concrete-control model
is also rerun at scale 100. These are finite independent model checks, not the
seven-night whole-value gate or proof of language parity.

[Native population/sv-tests and versioned probes](../measurements/2026-10-27/nba-monitors-comparison.json)
are compared with the preceding concrete-control checkpoint. The separate
[matched release measurements](../measurements/2026-10-27/nba-monitors-benchmark.json)
follow PERFORMANCE 5 with five runs of each baseline/large case. The three
designs measure cost when independent NBA controls are dormant; they do not
measure source event-NBA execution or establish a whole-workload speedup.

Median simulation seconds before/after in five alternating AB/BA pairs are
2.746624/2.719253 for `nba_shift` (100,000 cycles), 0.904969/0.897051 for
`proc_wait` (100,000) and 0.110472/0.113744 for `nba_burst` (500,000 updates).
Each paired run has its own +n=1 warmup. All sample result lines match. Burst
peak RSS is 74.625/74.59375 MiB. The
[initial serial batches](../measurements/2026-10-27/nba-monitors-benchmark-initial.json)
showed about 35% higher `nba_shift` elapsed time after the change; that difference
did not recur with balanced ordering. Both records retain host load and all raw
samples. Load/order sensitivity prevents a quiet-host or whole-workload speed
claim, and these observations do not replace the performance/nightly gates.

## Remaining source boundary and newly observed gaps

Typed `nba.event` SIR descriptors, verifier/text support, front lowering and VM
transport still remain. Fresh transient callback activations must preserve the
issuer's RNG/scope/report context after return and must not borrow popped task
activations. RHS/count/LHS capture order, skipped-count evaluation, aggregate
payloads and source matrices remain required. No source event-NBA gap is closed
by this kernel stage.

[Three additional original ordinary-event probes](../measurements/2026-10-27/nba-monitors-source-investigation.json)
observe more differences, separately from the stable 95-probe inventory:

- A direct hierarchical read of a static local of the **same** called function,
  combined with that function changing its private value during initialization,
  prints `0:1` in both native checkpoints and `1:1` in both Icarus versions.
  `Interpreter::depsOf` globally removes slots belonging to called functions;
  `keepHeapRead` similarly excludes their private graphs. Direct-versus-callee
  read provenance and aliasing need a shared dependency-rule fix and matrix.
  This kernel publication fix alone cannot restore an absent dependency.
- A called function writing an enclosing scalar or queue is rejected by native
  front, while both Icarus versions compile and print `1:1` / `1:11` respectively.
  SIR_SPEC 7.2 and RUNTIME_ABI 5 retain source purity restrictions. Widening them
  requires a concrete human-approved amendment and separate review/migration
  of the old rejection expectations; the six approved execution contracts do
  not authorize silently deleting unrelated restrictions.

Reference observations do not prove a complete standard interpretation. The
next source work must examine the full direct/callee/alias/activation matrix
against the cited project sections and clauses. The seven gaps in the existing
95 probes are not an exhaustive remaining-feature count. Linux execution and
whole-language coverage are not confirmed; run the same gates on Linux and
continue the full P1 scope rather than treating this local checkpoint as exit.
