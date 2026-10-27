# Concrete event-NBA controls and callback origins

[日本語](event-nba-concrete-controls.ja.md). Status: local implementation based on
main `31731f90`, verified locally on 2026-10-27. This shared-kernel stage does
**not enable source event-controlled NBA**. P1 remains incomplete.

## Rule and shared entry

The grounds are the approved [event-NBA contract](event-nba-proposal.md), scheduler
items 1–5 and the lifetime contract, its
[specification mapping](p1-execution-contracts-2026-10-25.md), SCHEDULER_SPEC
8/9.5/10.8/13 and RUNTIME_ABI 5/6. IEEE 1800-2023 4.6, 4.9.4, 9.4.2,
9.4.5, 10.4.2 and 15.5 identify the background; the approved coalescing and
pending issue-order choices remain project policy. No external implementation,
other project's reproducer, standard text or example was copied. No detailed
specification, dependency, C ABI or SIR instruction is changed in this stage.

`Scheduler::registerNbaEvent` binds an existing owned payload to an ordered list
of concrete slot and named-event items. Registration is synchronous. It does
not suspend the issuer, replace its ordinary waits, create a language process,
consume the live-child limit or request a child RNG seed. The payload's issue
sequence and group still come from the earlier capture. An empty payload still
registers, counts notifications and evaluates guards.

`NbaRepeat` retains the integral operand's width, signedness and four-state
words. Normalize it once: zero, negative and meaningful X/Z bits bypass event
binding and make the payload ready immediately. Unused top bits are ignored;
a two-state operand with meaningful b bits is invalid. Positive counts retain
all words. `detail::decrementNbaCount` is the single arbitrary-width borrow and
exhaustion operation; it is checked independently across every word boundary.
No execution tier reconstructs that decision.

`slotEventMatches` shares fixed-slot change/edge matching with ordinary waits.
A notification visits all relevant guards of one registration in source order;
any qualifying item decrements that registration once, after every relevant
item, including duplicates and overlapping selections. Separate writes and
named triggers count separately. The existing outer-walk queue delivers callback
writes/triggers afterward, with owned old/new words, without recursive matching.
Candidate tickets are snapshotted before callbacks; registrations created during
one notification cannot consume that notification. Stable indices and serials
protect against table growth and reuse while callbacks run.

## Ownership and cleanup

A control owns its copied count and descriptors, managed named-event pins, the
captured payload's lock and one reference to the original kernel process.
Ordinary wait cancellation, natural process end and kill leave these owners
intact. `releaseProcess` refuses to reuse a pinned origin. The measured macOS/arm64 layout uses a spare
alignment word in the process record; it adds no field to every queued event.
The control supplies no liveness to a program initial and does not prevent
quiescence. Program completion and global termination retain their earlier rules.

`discardNbaEvent`, readiness and cancellation detach each index entry exactly
once. A guard that cancels its own control keeps the payload roots, named pins,
origin and cleanup hook until its invocation returns. The `onRelease` hook is
transferred only on successful registration and called once. During its call,
the record is explicitly releasing and the origin/pins remain owned. Normal
readiness cleanup may register another control. Global/managed cancellation
rejects publication from its cleanup hooks until cancellation returns, so cleanup
cannot keep replenishing the set being torn down. A failed registration leaves
the unbound payload and cleanup ownership with its caller.

Count/item buffers are freed on completion, with only reusable generation rows
and static slot/event index capacities retained. Removed controls leave no stale
live subscription entries. Allocation failures during staging or candidate capture
produce ResourceLimit; no partial subscription or payload update is published.
Quiescence, termination and scheduler destruction tear down controls before
payloads and callback contexts can disappear.

VM `tryRelease` mechanically consults the origin reference; its root publication
includes ended frames while pinned. A release epoch makes the next collection
boundary recheck retained frames and ancestors. This is origin ownership plumbing,
not new instruction semantics. The independent SIR/native test ends one VM
initial containing a frame-only container, resumes a second VM process to force
reclamation checks, and observes the heap before and after control readiness or
discard. A ready payload alone does not retain the ended frame.

## Verification and limits

The new kernel and SIR/native tests cover both scheduling groups; zero live-child
capacity; synchronous repeat triggers; every four-state transition across all
four edge kinds at widths 1/65/129; element/bit selection; duplicate and overlapping
OR items; guard order and false results; mixed slot/named items; count widths
1/32/64/65/129/257, signed and unknown bypass and top padding; full-width borrow;
kill and record reuse; self-cancellation with root/pin collection; 128 registrations
created from a guard; deferred callback writes; reentrant cleanup; managed-only
cancellation; null events; empty payloads; program completion; quiescence;
termination; destruction; generation churn; Context isolation; staging allocation
failures and notification allocation failure.

A separate bounded reference model compares 20 controls through 50 randomly
chosen notifications for 30 rounds in each scheduling group, with independently
computed OR membership, guards, counts and final issue-ordered output. Nightly
fuzz scale and seed apply. These are original tests, not imported simulator tests.
Compiled negative controls and restoration evidence are recorded in
[nba-events-controls.json](../measurements/2026-10-27/nba-events-controls.json).
The initial control run retains a missed VM-origin fault; adding a second VM
entry makes reclamation run while the old origin is still pinned. All three VM
faults are rerun after strengthening that fixture; the miss is not hidden.

This API currently accepts fixed slot and named-event items only. Expression
monitor ownership/reattachment, coalescing direct and monitored items for one
notification, typed SIR `nba.event`, the verifier/text representation and front/VM
source transport are **still unfinished**. Scalar-only source execution must also
install the VM boundary hook when that instruction is connected; the current
origin test installs collection through its actual container use. Source guard
RNG/scope continuation and complete source matrices are not established here.
No new source design is counted as event-NBA support and the existing comparison
gap remains open. Whole P1/Icarus functional parity remains unconfirmed.

## Verified local checkpoint

[Source hashes and full local CI](../measurements/2026-10-27/nba-events-validation.json)
record **21/21** steps passing. Dev/ASan/UBSan pass 7,046/7,048 and release
7,047/7,049, with zero failures and the same two known skips. The 33 new tests
are 32 kernel tests and one independent SIR/native test; no new source event-NBA
design is counted. All 848 mutation anchors, C++20 public headers, PerfCount and
format/tidy pass. All 18 compiled faults are detected after the fixture correction.
The restored positive set passes 135 tests. A separate seeded 100x model run
passes 6,000 rounds, 120,000 controls and 300,000 notifications; it is not the
seven-night whole-value gate. The staged expectation guard preserves all old oracles.

[Fresh native comparisons](../measurements/2026-10-27/nba-events-comparison.json)
retain all native statuses/stdout in both 95-probe inventories. The fixed P1
population stays 1,456/1,466, zero failed and ten unsupported; sv-tests stays
244/344, 62 unsupported, with no previous pass lost. The seven reference-passing
gaps remain. Linux execution of this checkpoint is not confirmed: rerun full
local CI and the same native gates there before claiming cross-platform evidence.

[Five-run release measurements](../measurements/2026-10-27/nba-events-benchmark.json)
follow PERFORMANCE 5, subtracting median +n=1 elapsed time. Median simulation
seconds before/after are 1.991910/1.951392 for `nba_shift` at 100,000 cycles,
0.570986/0.565011 for `proc_wait` at 100,000 and 0.082521/0.079446 for
`nba_burst` at 500,000 updates. Burst peak RSS is 74.546875/74.625 MiB.
All result lines match. The before binary's hash equals the prior final payload
measurement, and all raw samples/versions/load are retained. These three local
observations measure dormant-control cost, not active event-NBA source speed or
a whole-workload performance improvement. Record-layout inspection on macOS/arm64
confirms Process size 176 bytes and alignment 8 before/after, with the new
32-bit origin count at offset 164, between `observed` at 160 and `serial` at 168.
Other ABI sizes are not confirmed; inspect matching before/after layout dumps there.
