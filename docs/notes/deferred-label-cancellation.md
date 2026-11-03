# Selective cancellation of deferred assertion reports

[日本語](deferred-label-cancellation.ja.md)

The maintainer approved the [contract](deferred-label-cancellation-proposal.md)
on 2026-11-03. Implementation is in the working tree on upstream main
`7654825d`, retaining the preceding inside-member repair. Existing suite
inputs and expectations are unchanged.

## Rule and implementation boundary

IEEE 1800-2023 §16.4.4 and the approved SCHEDULER_SPEC 15.2–15.4 amendment
distinguish label-specific cancellation from whole-process flush points.
Front identifies directly labeled deferred immediate assertions through
slang semantic symbols, not source spelling. `Lowerer::scanDisables` records
their identity, `deferAction` gives both action wrappers the same tag and
`lowerDisable` emits `report.cancel` instead of ordinary scope termination.
`report.call.named` carries the tag. SIR_SPEC 5.7 defines both instructions;
the verifier checks their operand kinds/ranges, region, wrapper and arguments.
Legacy unnamed `report.call` text remains unchanged.

`Scheduler::discardDeferredAssertions` implements filtering for both the
existing owner-wide flush and the new identity-specific operation. The
identity is static and instance-specific in the Context. It selects only
unsettled call records; ordinary unique/priority reports, other identities,
confirmed Reactive callbacks and earlier output remain. Argument evaluation
and side effects are not undone. The discard list is allocated before any
queue changes, retained order/counters are committed before callbacks, and
the existing discard callback releases argument roots and owner holds once.
VM forwards the verified ID and keeps existing capture/call behavior.

Initial, task and fork entry paths share this rule. A label directly at the
root of an initial process is an assertion identity, not an outermost block
activation. Cancellation may therefore release a completed owner without
unwinding a block. Broader active-action control, assertion-control system
tasks and unsupported label-resolution forms are not introduced.

## Verification and retained evidence

`tools/gen_deferred_label_matrix.py` creates 18 matrices crossing
assert/assume/cover, Observed/final and initial/task/fork. Five periods cancel
before registration, after two registrations, between them, never and after
settling. Observations cover both wrappers, captured-input side effects,
live report values, sibling order and a distinct process using the same
label spelling. Actions only display, including in Postponed.
Two root-statement controls additionally exercise completed owners.

The [18-case pre-implementation record](../measurements/2026-11-03/deferred-label-before.json)
retains the original differences. [All 20 new inputs now match](../measurements/2026-11-03/deferred-label-after.json); the focused
37-test selection also checks malformed SIR, retained confirmed calls,
unique/priority reports, repeated cancellation and container snapshot roots
after the owner has completed. Existing external deferred-disable cases
match 31/31 with their original expectations.

The external full 2,785-case selection records 2,252 pass, 529 unsupported
and four config failures. Relative to the inside-repair snapshot, only the
five label failures change to pass; all input fingerprints are identical.
One additional finite model passes its whole selection, making 48 of 75.
External evidence is in kirimasim-tests; its build record identifies the
uncommitted sources and actual binary separately from the upstream revision.

Two faults are registered in `tools/mutants.py`: omitting cancellation and
cancelling sibling reports too. [Both compiled faults](../measurements/2026-11-03/deferred-label-mutations.json) cause output differences with simulator exit zero in all 20 inputs, separately from the source-applicability check. [Full local CI and sanitizer validation](../measurements/2026-11-03/deferred-label-validation.json) passes all 21 steps: dev/ASan/UBSan each pass 11,827 tests and release passes 11,828, without exclusions; neither the formal Nightly nor its
seven-consecutive-run condition is replaced by these checks.

## Commands

```sh
python3 tools/gen_deferred_label_matrix.py --check
ctest --preset dev --output-on-failure -R 'deferred_label|AssertionIdentity|NamedCancellation'
python3 tools/mutants.py --check
CTEST_PARALLEL_LEVEL=8 CMAKE_BUILD_PARALLEL_LEVEL=6 python3 tools/local_ci.py
```

After committing the implementation, the standard mutation runner can use
`python3 tools/mutants.py --only 'assertion identity'`. It intentionally
refuses an uncommitted source tree. The isolated experiment uses copies of
the kernel archive and the same suite harness, leaving production sources
and binaries unchanged. No parallel-purpose execution framework is added.
