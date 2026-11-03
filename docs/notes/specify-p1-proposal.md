# First specify module-path execution contract

[日本語](specify-p1-proposal.ja.md). 2026-11-01. **Sections 1–8 approved on 2026-11-01; [first scalar subset implemented](specify-execution.md).**

This is the concrete review required by P1_SCOPE and the priority note's
specify item. Approval of the P1 scope did not approve missing timing details.
The maintainer explicitly approved sections 1–8 below as the first implementation contract. Section 9
records evidence and uncertainty, including the edition limitation. The edition limitation in section 9 remains recorded. No existing
expectation migration is authorized by this approval.

## 1. First implementation boundary

Apply accepted module paths during ordinary compile-and-run, without an opt-in
that silently ignores timing. `parse` continues to parse/elaborate only.
Start with module instances, scalar four-state input nets and scalar `wire`/`tri`
output nets. Simple `=>` and full `*>` paths may connect several scalar inputs
and scalar outputs. Include unconditional edge-sensitive paths with `posedge`,
`negedge`, or no edge qualifier, and their data/polarity annotations.

Each destination has one ordinary internal functional driver: a continuous
assignment or an existing combinational logic primitive with fixed drive
strength. An output can be driven from internal state updated by a procedure,
through that single continuous assignment. Existing distributed delays on that
functional path participate as described in section 3. Instances in hierarchies
and generate scopes retain separate state. Specparams, parameter overrides,
constant real/integral delays and `--mintypmax` are included.

The first source bridge does not admit vector terminals/bit selects, output
variables, inout/passive paths, multiple physical drivers at a destination,
three-state/MOS/UDP destination drivers, state-dependent `if`/`ifnone`, pulse
control declarations, SDF or timing checks. Reject these combinations with
positioned unsupported diagnostics. They remain subsequent P1 work where
required; this is not another P1 exception. Scalar execution does not establish
complete specify coverage. Specparam-only blocks keep their existing behavior.

## 2. Delay normalization and four-state transitions

Normalize to twelve transitions in this internal order:
`01, 10, 0z, z1, 1z, z0, 0x, x1, 1x, x0, xz, zx`.
This order is a proposed internal data contract, not a new external ABI.

One expression supplies every entry. With two expressions R/F, the six
non-X entries become `R,F,R,R,F,F`; in particular, 0-to-Z and 1-to-Z do not
both use the minimum. Three expressions R/F/Z become `R,F,Z,R,Z,F`.
Six expressions directly supply the six non-X entries. Twelve explicitly
supply every entry. For omitted X entries, choose the earliest transition
into X and the latest transition out of X among the possible non-X endpoints (0, 1 and Z).
The code must not substitute the ordinary three-delay gate selector.

Choose each min/typ/max component first. Clamp a negative selected delay to
zero, round once at the defining module's precision using the existing delay
normalization rule, then scale to global ticks. Preserve the existing handling
of nonfinite/unrepresentable delays and time overflow; do not wrap a negative
delay into an unsigned future time. Initial path clocks have epoch zero;
written destination nets keep the existing X startup until their first
functional result is published at its selected deadline.

## 3. Source clocks, path choice and distributed delays

Track each scalar source's latest projected four-state transition and its
simulation time. Strength-only changes are not source logic events. Apply
edge qualification using the existing `valsem` edge definitions; an unqualified
path observes all four-state changes. Keep eligibility for the latest actual
source event, rather than retaining an older qualified event as if it were new.

When the functional destination result changes, select the most recent
eligible source time among paths to that destination. If several qualify at
that same time, select the smallest delay for the actual destination transition.
Compute an absolute deadline from that source time. Publish no earlier than
the current functional arrival time. Thus a distributed arrival at tick 30 and
a path deadline at tick 28 publishes at 30, not at 37. If no path is eligible,
retain the ordinary functional arrival. Initial evaluation uses the same rule
with epoch-zero clocks, not an invented input event or hidden user process.

Polarity and data-source expressions describe the path. They do not replace,
invert or execute the module's functional computation. In particular, a function
call used only as a data-source annotation must not execute or draw random data.

## 4. Pending updates, pulse filtering and regions

Each destination owns its committed functional contribution and an optional
pending complete contribution/deadline. The default error/reject limits equal
the selected path delay: use full inertial filtering. A return to the committed
contribution before a pending leading edge cancels the uncommitted pulse.
An identical pending contribution does not postpone its deadline. Otherwise
replace pending work with an owned payload and a fresh generation; stale timers
cannot publish. Negative-time output pulses are cancelled under the default
non-showing policy. Do not implement transport behavior by retaining every event.

Compare against the committed path contribution, including while the public
net is forced, rather than against its forced readout. All four-state words and
any retained strength metadata have the same pending lifetime. Do not borrow a
VM temporary, span or caller frame.

A due module-path publication is Active work for its module. A deadline already
reached is queued in the current Active processing; it must be visible before
an explicit `#0` observation in Inactive. It is not an ordinary `drive.delay #0`
operation. Existing scheduler ordering, zero-time/resource/wall-time limits,
interruption and final/termination cleanup remain operative.

## 5. Visible nets, ports, force and observation

Prescan path destinations before port collapsing. Keep a private functional
result separate from the visible delayed destination, and do not collapse a
connection that erases that distinction. Reads of the declared destination,
inside or outside the cell, observe its committed delayed value. Only the
functional writer targets the private result. Hidden carriers are not user
reflection or waveform objects.

Preserve the identity of the one original physical driver. Carry its complete
contribution; do not turn a cached resolved network range into a new origin.
Ordinary value notifications, strength notifications and waveform/monitor/VPI
reads must see the same committed public snapshot.

Integrate the already approved net-force model: force clamps the public net
immediately, while functional updates and pending path deadlines continue.
Release reads the current committed path contribution synchronously. A deadline
that elapsed while forced is not lost; a still-future deadline is not advanced
by release. Existing per-bit override ownership and Context isolation remain.
Additional physical drivers/passive combinations are outside the first bridge
and must not be silently flattened into it.

## 6. SIR, VM and kernel boundary

Use verified static path rows: numeric source/destination bindings, owner and
functional-driver identity, edge qualification, normalized twelve-delay data,
and source locations. Mutable clocks, pending generations/payloads and
subscriptions belong to the Context. Retain a stable identity for each path
and destination. Independent instances cannot share their state.

The frontend reads pinned slang's typed `TimingPathSymbol` data without changing
slang, and records paths before optimizations erase their ports. The SIR verifier
checks terminal domain/width, source/destination ownership, duplicate/conflicting
bindings, delay shape, valid identities and the single functional writer.
Malformed native IR fails before allocating a running model. Unsupported source
forms get source errors, never verifier/internal errors.

The VM transports typed values and numeric bindings mechanically. The shared
kernel contains the timing algorithm with no slang, SIR or LLVM dependency.
Keep public kernel headers C++20 and add any new header to the existing header
check. Do not change the public C ABI, add a dependency, create mutable globals,
look up signal names at runtime or create hidden user threads/RNG owners.
Existing designs without paths keep their fast path and PerfCount expectations.

## 7. Admission and later work

A block containing an unsupported path/timing/pulse construct is not partially
executed as a successful design. Keep strict frontend diagnostics and report
unsupported combinations explicitly. Independent crashes, hangs and internal
errors remain failures. Neither a reference's compile acceptance nor parsing
alone counts as timing execution.

The follow-up [simple state-dependent contract](specify-state-dependent-proposal.md)
sections 1–8 were approved on 2026-11-03 and are [implemented](specify-state-execution.md).
Conditional edge-sensitive paths remain outside that subset, and direct 2023
edition review remains unconfirmed. The
reviewed 2017 text uses the condition's least-significant bit and pessimistic
X/Z enabling, which differs from an ordinary `if`; three reference observations
differ. Vector timing has two explicitly diagnosed reference limitations.
Do not copy either behavior into a supposed standard implementation.
Timing checks, pulse controls, multi-driver/inout/variable/vector forms remain
in the inventory rather than being declared unnecessary for all of P1.

## 8. Acceptance and migration discipline

Implement in reviewable increments: independent value/timing reference; native
Context scheduling; typed SIR/VM; source and port connection; then integration
and documentation. Each increment needs useful execution checks, not only
matching source spellings.

Test all twelve logic transitions and five delay-list sizes, epoch-zero startup,
real rounding/min:typ:max/negative/zero values, latest-input and tie selection,
short/equal/long pulses, mixed distributed/path delays, edge polarity and
annotation non-evaluation. Cover pending force/release, inside/outside reads,
waveforms/monitors, isolation, cancellation and failure/teardown. Add a bounded
source-context matrix for stimuli in procedures, tasks/functions, fork,
interfaces/programs and generate scopes where legal. Native tables also need
malformed and wide-slot-address boundary checks.

Compiled native mutations must catch wrong X selection, loss of newest-source
selection, additive delay, zero in the wrong region, stale timers, erased port
identity, executed annotations and discarded forced pending work. Build failures
and the registry's own checks do not count as semantic detection. Freeze source
while mutating or running full local CI; restore hashes and rerun the baseline.
Run full dev and relevant release/ASan/UBSan, and update counts only from a
successful complete dev JUnit in a separate commit. Full local CI precedes push.

The generated `tests/cli/unsupported_matrix/specify_path.sv` and its rejection
expectation were retained during review. After execution was proved, the
maintainer separately approved its exact [classification migration](specify-expectation-migration.md), preserving the source and all other rejection checks. That approval is separate from this contract. Re-run the frozen `specify_path` probe without changing its source hash or expected output; other existing expectation migrations still need separate review.

## 9. Evidence and edition limitation

The [102-case inventory](specify-reference-inventory.md) records Icarus 13.0
execution and the unchanged native rejection. It includes 87 matching written
traces, six reference differences and nine observations without a portable
oracle. No native implementation is claimed. The external checker has six
compiled controls; those are not native simulator mutations.

Relevant primary text was inspected in IEEE 1800-2017, chapter 30, including
30.4.3, 30.4.4.1, 30.5.1–30.5.3, 30.6 and 30.7. It was read from the
[MIT-hosted copy of the IEEE publication](https://rfsoc.mit.edu/6S965/_static/F24/documentation/1800-2017.pdf).
The [official 2023 distribution](https://www.accellera.org/downloads/ieee) is
identified, but its body could not be retrieved here (official endpoint HTTP
418). **Direct 2023 clause-by-clause confirmation remains unconfirmed.**
Do not label the inspected material as the 2023 edition. The concrete rules
above are the proposed native contract, not a claim that the edition audit or
P1 parity is complete. Compare these clauses against IEEE 1800-2023 through
the official GET distribution during review; any conflicting required rule
must amend this contract before the affected code is implemented. No standard
text, diagram or example is copied into the repository.

Implementation checkpoint (2026-11-02): sections 1–8 are connected through SIR/VM/kernel. The unchanged historical rejection input was separately approved for [classification migration](specify-expectation-migration.md). The 102-case reference inventory in section 9 is the pre-implementation checkpoint, not a current native execution claim; see [execution evidence](specify-execution.md). The edition limitation remains.
