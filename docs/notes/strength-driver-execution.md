# Physical strength origins and scalar inertial execution

[日本語](strength-driver-execution.ja.md). Status: implemented kernel checkpoint,
2026-10-26. Source MOS/switch execution remains work.

## Grounds and scope

`f93da11b` implements the independent-node physical-origin and timing part of the
approved [MOS/switch contract](mos-switch-proposal.md) 1/6/7 and its
[normative mapping](p1-execution-contracts-2026-10-25.md). Other grounds are
VALUE_SEMANTICS 5, SCHEDULER_SPEC 9/12, and IEEE 1800-2023 4.9.5, 10.3.3,
28.7 and 28.16, summarized here in our own words. Implementation and tests are
original. No other simulator implementation or tests were read. No dependency,
C ABI, SIR format version, expected source output, performance budget or
managed-event collection timing changes.

The Context now owns a `StrengthDrivers` bank. Static scalar nodes identify
registered slot/element/bit targets and effective tie kinds; stable physical
origin IDs are ordinals in the copied origin table. Each origin holds its
complete committed range and one owned pending range with a 64-bit generation.
No range is projected and reconstructed to store it. A published node value is
never fed back as a physical origin.

This checkpoint resolves independent nodes. It has **no passive edges** and
cannot implement a connected tran network by treating node readouts as roots.
The caller declares dense component IDs to group atomic publications; these do
not discover connectivity. Later graph construction/verification must determine
real connected components and preserve origin/path correlation. The present bank
is not a substitute for the approved passive-world solver.

## Registration, initialization and resolution

`configureStrengthDrivers` copies both tables once, checks scalar targets against
registered metadata, rejects duplicate slot/element/bit targets, invalid ranges,
missing component IDs and multiple physical roots on `uwire`, and prepares
per-component node indexes and per-node origin indexes. Failed configuration is
retryable and publishes no partial table. Runtime solves visit only that
component's nodes and their origins, using preallocated result/update buffers.

Kinds are wire/tri, wand/triand, wor/trior and uwire. Pull/supply roots must be
materialized explicitly and their node kinds normalized by the caller. The API
rejects tri0/tri1/supply0/supply1 kinds, preventing an implicit root from being
added again inside resolution. It does not yet import ordinary SIR driver slots
or side masks. Nor does it validate source declarations or collapse ports.

Registration writes no net, queues no event, runs no callback and creates no
process. Published initial X stays X until the first scheduled resolution.
Ordinary origins supplied as X keep it; a new MOS origin supplied as `[0,0]`
starts undriven under the approved startup choice. `startStrengthDrivers` queues
initial Active work before device evaluations. The source loader will still have
to enforce that ordering.

`driveStrength` updates an origin and queues its component once in Active,
including when the issuing process belongs to Reactive. Several root changes
before that work runs share a snapshot. Resolution uses the existing finite
point-choice state calculation, with the node's own tie rule, and calls
[`publishStrengths`](strength-store-publication.md) only after all node results
are ready. Complete ranges and projected values commit before callbacks.
Callbacks can drive origins and queue a subsequent Active solve; they do not
reenter the current solve or erase its committed notices. Solves poll the
existing interruption/wall-time hook between nodes and origins.

Device and timer work has no user process, fork child, program origin, RNG or
issuing frame pointer. Pending timers carry copied origin IDs and generations;
the ranges live in Context. A user process can finish before delivery. Context
destruction owns the bank's cleanup. Source-created subscriptions will still
need explicit cancellation before their callback frames are destroyed.

## Scalar inertial timing

`driveStrengthDelayed` receives a captured complete range and normalized
rise/fall/off ticks for one physical output bit:

- An identical pending range retains its original deadline, even if the new
  call supplies other delays.
- A different range cancels the old pending generation. Returning to the
  committed range leaves no pending update; a genuinely new result schedules
  from the current time.
- Destination 1/0/Z selects rise/fall/off. A result that projects to X, including
  one-sided L/H intervals, selects the minimum. A strength-only known-value
  change uses that same destination classification, as approved.
- An explicit zero delay commits the origin in Inactive and queues Active
  resolution. It does not publish during the issuing Active evaluation.
- Delivery checks both pending state and generation. A direct drive cancels an
  existing timer. Each scalar origin has its own state; replacing one output's
  deadline does not replace another output's.
- Invalid input is rejected before changing state. Time overflow sets the
  existing scheduler overflow flag, returns `InvalidValue`, and preserves the
  previously pending range/generation. The future SIR caller must consume the
  flag through the ordinary positioned-diagnostic path. Generation exhaustion
  requests resource-limit termination rather than wrapping.

Registration and publication allocation failures return kernel errors.
Scheduling allocation failures request resource-limit termination before the
new origin state is installed. Publication failure can leave committed origin
inputs newer than unchanged published outputs; execution terminates rather than
exposing a partial component. No failed publication is retried after commitment.

## Validation

Sixteen original tests include:

- All 120 intervals against all 15 strength points for six resolving tie kinds;
  uwire checks all intervals with its single legal origin. The 12,600 node
  results are compared with independent `valsem` enumeration. A 129-bit,
  three-element layout also checks packed reads at varying positions and tails.
- Ordinary X/new-device Z startup, explicit pull/supply roots, 14 malformed
  static-table cases, stable captured tables, coalesced writes and Context
  isolation. Twenty-four allocation budgets cover failing and successful
  configuration; each Context then fails the first publication allocation,
  with no partial net.
- Complete two-node publication before callbacks, callbacks queuing the next
  distinct snapshot, and a kernel circuit propagating three complete input
  ranges through all six directed MOS operations via internal subscriptions.
  No synthetic user processes are created. This circuit is not source lowering.
- Thirty destination transitions, repeated-deadline retention, same-value
  strength replacement, committed reversion, immediate cancellation,
  independent scalar timers, explicit zero/Reactive region counters, maximum
  simulation time, and interruption before publication.

All thirteen [compiled controls](../measurements/2026-10-26/strength-driver-controls.json)
are caught after successful full builds, including the public C++20 probe.
Compilation failures do not count as detections. Baseline and restored production
pass 91 cases, including all 29 unchanged SIR dumps and PerfCount. All 704
registered controls apply. The separate standalone header syntax check passes
as C++20 with exceptions and RTTI disabled.

[Full local CI](../measurements/2026-10-26/strength-driver-validation.json) passes
all 21 checks: dev/ASan/UBSan 5,187/5,189 and release 5,188/5,190, zero failures
and two known skips each. Static checks, generated counts and all existing
expected values pass. The implementation commit also passes a manual
expected-change check. Results are local macOS arm64; no source changed after
validation. README counts come from the fresh full dev JUnit.

A [focused ordinary-path measurement](../measurements/2026-10-26/strength-driver-speed.json)
compares release code `99dafab4` and `f93da11b`, five alternating pairs at
200,000 iterations after build/CI contention ended. Startup-subtracted estimates
are 0.252392458→0.257728333 seconds for frame access (+2.1%) and
0.047633000→0.047149750 seconds for legacy events (−1.0%). All counters match
exactly; median peak RSS is 9.203125 MiB for both binaries on both workloads.
These small-sample ratios do not establish a speedup or a stable regression.
Neither workload configures the new bank, so physical-origin traffic/source
MOS cost is unmeasured. Following PERFORMANCE 4/5, PerfCount passes and wall
clock is reported, not a pass/fail gate; no threshold or budget changed. This
is not the seven-nightly-result baseline or a refreshed external-simulator table.

## Remaining work and limits

SIR static network rows, `strength.load` and `strength.drive`/delay, captured
runtime selectors, declared write ownership, the front-end connection and
source device sensitivity remain work. The approved passive solver, conditional
edge timing, port identities/collapse, registered-net force/release, `%v`, and
full primitive-array/source matrices are also missing. Ordinary driver side
initialization must be imported without changing its previous unused masks.
Force is still rejected on registered metadata slots by the earlier publication
API; this checkpoint does not enable that combination.

No source-level Icarus gap reduction is claimed. No new Icarus 13 comparison,
P1 population, sv-tests, Linux or minimum-compiler result is claimed. Exhaustive
global allocator failure injection and reaching a 64-bit generation limit are
**not confirmed**; fault-inject the scheduler's default allocator and seed a
near-exhaustion state in a dedicated test facility to check those paths.
Cancellation leaves stale ID/generation events in the ordinary event queues
until visited; one pending range per origin does not promise one allocated
queue entry under arbitrarily fast replacement.

P1/full Icarus coverage remains incomplete. The managed-event instruction-
safepoint amendment is independent and remains unapproved.
