# Owned NBA payloads and pending issue order

[日本語](event-nba-payload-foundation.ja.md). Status: local implementation,
2026-10-27, alongside the uncommitted conditional-container checkpoint based on main
`1650064d`. This is a shared-kernel prerequisite, **not source event-NBA support**.
P1 remains incomplete.

## Approved contract and boundary

Grounds: the approved [event-NBA contract](event-nba-proposal.md), scheduler
items 4/5 and payload lifetime, mapped by
[P1 execution contracts](p1-execution-contracts-2026-10-25.md); SCHEDULER_SPEC
3/8/13/14 and RUNTIME_ABI 6. IEEE 1800-2023 4.6, 4.9.4, 10.4.2 and 10.6.2
provide the scheduling/assignment context. Pending issue order is the approved
project choice, not a claim that the references implement it. No external
implementation, reproducer or standard example was read/copied. No dependency,
expectation migration, specification change or performance budget is introduced.

The public C++20 `nba.h` and scheduler API separate capture from readiness:

- `captureNba(origin, writes)` copies an ordered list of slot/element/bit-range
  descriptors and captured value words, assigns an issue sequence and records
  the origin's group. Allocation/validation completes before publishing a ticket
  or incrementing `nba.scheduled`. Empty effective payloads are valid. Callers
  omit invalid selections while retaining the surrounding control registration.
- `readyNba(ticket)` commits the payload to NBA/Re-NBA. The caller at readiness
  does not choose its group/order. It neither resumes nor creates a process,
  adds a child nor draws a process RNG seed. A ready ticket cannot be discarded
  or made ready again. An empty payload completes without an update event.
- `discardNba(ticket)` releases an unready payload exactly once. Context-local
  IDs include a nonwrapping generation; completed rows are reusable while their
  value/descriptor buffers are freed. Tickets cannot outlive their Context.

The kernel copies raw domain words, not source-language objects. The tier must
supply already-owned typed snapshots; `visitPendingValues` publishes every
waiting, ready and executing leaf using its slot's type identity. No word is
implicitly treated as a heap handle. Payloads can survive process-record reuse
because they retain no process pointer and capture their group. This API does
**not** yet retain a callback origin, frame, scope or RNG; the forthcoming event
registration must own those independently under the approved lifetime contract.

`enqueueNba` shares one issue counter with existing immediate NBAs. Fresh
ordinary updates retain O(1) append behavior. Older ready captures are inserted
before newer current-ready updates still pending in that group, including a
suffix already moved into Active/Reactive by the reference algorithm. Updates
already applied are never reordered. Timed updates due at slot opening retain
their existing priority/order, independently of their original issue time.
An earlier-slot event control joins the current-ready order when it becomes
ready. Active and reactive groups retain their separate regions.

Delivery keeps the complete payload rooted while invoking ordinary slot writes
in leaf order. Guard callbacks may grow the capture table; the next field is
reacquired by stable ticket rather than a borrowed table reference. A callback
cannot discard the executing payload. Holds/force use existing write semantics.
Termination stops subsequent leaves and drops captures; quiescence also discards
unready captures instead of keeping the simulation alive. Final execution and
an already-requested termination reject new captures. No public counter, C ABI
0.3 entry, SIR instruction or formatVersion is added in this checkpoint.

## Independent verification

The new `NbaPayload` unit group has **24 tests**. It covers both groups, reverse
readiness, 3,840 independently ordered random issues in the default 60 rounds
(nightly fuzz scale/seed apply), readiness during NBA execution, prior-slot
captures, timed-due priority and nonretroactive application; issuer end/kill/
record reuse; generation reuse and committed-ticket rejection; ordered leaves,
word boundaries through 129 bits and both four-state planes; whole-variable
force/release; empty payloads; exact root visits and actual heap collection;
reentrant growth; invalid requests; program completion/final/termination; 200
rounds of bounded row reuse; Context isolation and deterministic allocation
failures with no partial roots/writes/counters or leaked bytes. The pure sequence
step is checked near exhaustion and at its invalid zero sentinel.

[Ten compiled faults](../measurements/2026-10-27/nba-payload-controls.json)
are caught by these tests: discarded snapshot bits, wrong group, readiness
order, ignoring a moved suffix, reversed leaves, omitted roots, canceling ready
work, no row reuse, counter wrap and retained quiescent roots. Original source
bytes are restored and rebuilt before positive verification. Registry checks
are not counted as detectors. A check started before the final restoration
build completed was discarded; only the later completed-build test result is
accepted. Existing source tests/oracles and PerfCount values are unchanged.

## Remaining work

The frontend still diagnoses event-controlled NBA as unsupported. Persistent
slot/named/expression subscriptions, notification identity/coalescing, source-
ordered guards, arbitrary-width repeat counts, dependency reattachment and
transactional registration remain. Retaining callback origin/frame/RNG across
issuer termination, typed `nba.event` SIR, verifier/text and mechanical VM
transport also remain. Those are required before enabling source execution;
an owned payload alone is not a language implementation or a closed Icarus gap.
Host-allocation failure in the existing standard-library region deque is caught
on readiness but is not deterministically injected by the Context PMR tests.
Complete Icarus functional coverage remains unconfirmed.

## Event-storage correction from measurement

The [initial checked layout](../measurements/2026-10-27/nba-payload-initial-validation.json)
passed all 21 CI checks and its ten compiled faults, but the
[500,000-update burst](../measurements/2026-10-27/nba-payload-burst-initial.json)
raised peak RSS from 74.59 to 82.92 MiB (11.2%). Keeping a new issue word in
all event records was unnecessary. The corrected layout reuses the existing
kind-specific generation word for ordinary NBA issues; a captured event keeps
its generation there and gets its issue from the generation-checked payload
row. Timed-due events and non-NBA events remain outside current issue sorting.
This restores the previous event-record footprint without borrowing callbacks
or changing order. The initial measurements/hashes remain historical evidence;
final-layout validation and measurements are recorded separately below.

## Final event-storage measurement

The [corrected 500,000-update burst](../measurements/2026-10-27/nba-payload-burst-benchmark.json)
uses five runs per binary with the independently specified final-value oracle.
Peak RSS is 74.6094→74.5469 MiB
(after/before 0.9992); median simulation time is
0.08298→0.08422 seconds.
Every sample has the expected last-issued value. The final event-word reuse
removes the initial layout's measured queue-memory increase. These local
measurements do not establish a seven-night baseline or a universal speed claim.

## Local checkpoint results

[Source hashes and local CI](../measurements/2026-10-27/nba-payload-validation.json)
record 21/21 checks passing. Dev/ASan/UBSan have 7,013/7,015 passes; release
has 7,014/7,016, with zero failures and the same two known skips. This includes
the prior conditional-container work. C++20 headers, PerfCount, format/tidy and
all 830 registered fault anchors pass. The staged expectation guard also passes.
[Fresh native comparisons](../measurements/2026-10-27/nba-payload-comparison.json)
retain every status/stdout in both 95-probe inventories, 1,456/1,466 native P1
passes (zero failed, ten unsupported) and 244/344 sv-tests passes (62 unsupported),
with no previous pass lost. The seven reference-passing gaps remain.

[Local release measurements](../measurements/2026-10-27/nba-payload-benchmark.json)
use five runs per design at count 100,000, subtracting median +n=1 time under
PERFORMANCE 5. The final binary's result lines match the before binary in every sample;
per-design medians, ratios and peak RSS are retained in the JSON. Both binary hashes,
versions, load, samples and machine properties are recorded. This is a local
two-design observation, not a seven-night baseline or a whole-workload guarantee.
The dedicated queued-event memory result is recorded below separately; this
two-design observation alone does not establish its working-set cost.
