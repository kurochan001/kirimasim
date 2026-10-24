# Complete-state delayed three-state execution

[日本語](delayed-gate-state-execution.ja.md). Status: implemented mechanism;
expectation-only integration review pending, 2026-10-25.

The user approved the [complete delayed-side contract](delayed-gate-side-proposal.md)
and the five other [P1 contracts](p1-execution-contracts-2026-10-25.md). The
operative amendments are normative through SCHEDULER_SPEC 12,
VALUE_SEMANTICS 5.8, SIR_SPEC 5.4 and RUNTIME_ABI 6. They paraphrase the checked
IEEE 1800-2023 28.6 and 28.16/Table 28-9, with explicit project choices for
same-projection transitions. No other simulator implementation or tests were
read and no new dependency or installation is introduced.

## Implementation

`Scheduler::driveDelayed` owns a value plus copied `DelayedSideUpdate` pairs.
A changed side is a changed output even if its four-state projection remains
X. Committed-state and pending-state comparisons include the fields; equal
pending states keep their original deadline, and returning to the committed
state cancels a different pending update. Side pair order does not change
equality. X/L/H uses the existing minimum delay; delay zero uses Inactive.

On commit the kernel copies target metadata and fields before callbacks can
grow its driver table. An existing notification walk defers every callback
until value and both side fields are stored. The low/high masks retain their
whole widths; each individual side update names a typed scalar bit. Existing
value-only calls retain their original API form through an empty default span.
Tier 1 still calls C++ directly, headers remain C++20 and C ABI remains 0.3.

`Lowerer::resolveNets` finds the output's delayed instruction across all
control-flow blocks and puts it after the actual side computations. It replaces
immediate side stores with trailing target/value pairs of that instruction.
Undelayed and unresolved forms retain their existing path. The verifier checks
complete pairs, scalar two-state targets, fitting values, nonoverlap and every
side's declared write coverage. VM transports the pairs mechanically and does
not invent a second delay or strength policy.

## Independent evidence

[The original generator](../../tools/gen_delayed_gate_state_matrix.py) produces
240 sources and expected outputs: four bufif/notif forms × four one/two/three/
zero-delay specifications × widths 1/33/65/129/257 × module/generate/short-circuit
contexts. A discrete-event oracle applies complete truth-table states against
a weak-1 background, with committed/pending records. Samples are one picosecond
after boundaries, excluding ambiguous Active-region observations. Cases include
L/H/X/Z, side-only pulses, return-to-current cancellation and identical-state
reevaluation. Expectations do not come from simulator output.

[Three direct tests](../../tests/unit/delayed_side_test.cpp) independently cover
owned buffer/target capture, reordered pairs, unchanged deadlines, cancellation,
zero/nonzero delay, both L/H mask halves, malformed SIR and write sets. Guards
observe complete states while creating 4,096 drivers, forcing driver-table
reallocation. Eight compiled omission controls must fail actual source or
kernel assertions, not judging self-checks. Existing SIR dumps and PerfCount
budgets are preserved.

Eight additional [ordinary known-state observations](../measurements/2026-10-25/delayed-side-known-observations.json)
cover all four gates at widths 1 and 65. Both Icarus versions and the new native
pass every expected output; the retained pre-change native rejects all eight.
These original sources are included in the record, separate from the 87 probes.

The [reference observations](../measurements/2026-10-25/delayed-side-reference-observations.json)
compare three original sources on matching installed Icarus 13.0 stable and
14.0 development compiler/runtime pairs. Both disagree on the two nonzero-delay
side-only timing cases and agree on the zero-delay case. This is the approved
project interpretation, not evidence of an IEEE defect in either reference.
It does not change the frozen 87-probe oracles or establish MOS/P1 completion.

## Separate existing-oracle migration

`tools/delayed_gate_boundary` still expects six now-obsolete rejections. It
fails because execution succeeds. The separate branch
`codex/delayed-side-expectations` proposes exact five-line propagation checks
for those six delayed cases and preserves all six existing undelayed checks.
Its [reasoned expectation-only review](delayed-side-boundary-migration.md) is
pending human approval; no existing production expectation is changed here.
The proposal runs successfully on the new implementation. Until that migration
is approved and integrated, the full CTest set has one known failure; a green
full-local-CI result is not claimed. Cross-configuration validation explicitly
excludes only that preserved rejection oracle and checks its proposed stronger
replacement independently.

## Validation checkpoint

[The complete record](../measurements/2026-10-25/delayed-side-validation.json)
identifies source `20274407`, hashes, four configurations, nine successful
quality checks and all eight compiled controls. Dev/ASan/UBSan have 4,176
passes and two known skips among 4,178 tests outside the preserved oracle;
release has 4,177 passes and two skips among 4,179. The oracle is then run
separately in **each** configuration and fails, yielding one failure in the
full observed set (4,179 dev/ASan/UBSan; 4,180 release). Its proposed stronger
replacement passes all 12 cases in every configuration. No existing production
expectation is edited or failure hidden.

The unchanged 87-case cohort retains 24 passing gaps per reference and four/
seven reference mismatches; every previous status and passing judged byte is
retained. Pinned macOS sv-tests remains 244/344 and the P1 population remains
1,451/1,466, with zero failures and 15 unsupported. Linux is not remeasured.
All existing 29 SIR dumps, PerfCount budgets and expectation-protection checks
pass. This is not a successful full `local_ci.py` run: the remaining oracle
review is an explicit integration requirement. Other approved P1 contracts
remain implementation work.
