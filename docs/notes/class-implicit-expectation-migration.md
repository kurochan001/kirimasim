# Class comb/latch: review of two old rejection expectations

[日本語](class-implicit-expectation-migration.ja.md) · 2026-11-05.

Status: **approved and applied (2026-11-05)** in separate commit `4e177301`. The implementation follows
the already approved [class contract 7](class-runtime-proposal.md#7-event-dependencies-and-observation)
and [SCHEDULER_SPEC 6.3](../spec/SCHEDULER_SPEC.md#63-the-implicit-sensitivity-of-always_comb).
This request concerns only the two existing unit-test locations below, not a
new semantic contract, main integration, push or slang posting.

References to class objects contribute only method argument expressions to
comb/latch sensitivity. A direct property read with no arguments adds no
implicit property trigger. The body still runs at time zero. Ordinary function
and static-method reads keep their existing sensitivity; instance method and
constructor bodies do not contribute reads. Actual object-dependent explicit
observers and `@*` retain their current unsupported boundaries.

The proposal and pre-review observations below retain their original wording.
The maintainer explicitly approved both locations; their future tense and
20/22 result describe the pre-approval checkpoint.

## Exact proposed migration

Both locations are in `tests/unit/class_front_test.cpp`.

1. `ClassFront.UnsupportedClassBoundariesAreSourceDiagnostics` currently includes
   the source `class Box;int data;endclass module top;Box a;int v;initial a=new;always_comb v=a.data;endmodule`.
   Move this **unchanged source** into the new positive test
   `PlainClassPropertyReadHasNoImplicitPropertySensitivity`. Require successful
   compilation, no implicit triggers on the comb process, quiescent execution,
   `v == 0` and one live object. Keep every other negative case and assertion.
2. `ClassFront.VMStillRejectsImplicitSensitivityBeforeRuntimeStorage` changes a
   compiled initial body to `ProcessKind::Comb` and expects loader rejection.
   That body is now executable. Retain the original source and **all assertions**,
   change only the synthesized kind to `ProcessKind::CombAt`, and rename the test
   `VMStillRejectsAtStarObjectObservationBeforeRuntimeStorage`. It continues to
   require the observation error and zero runtime slots/object entries for the
   remaining unsupported `@*` boundary.

No existing suite source, expected stdout, fixture or performance budget changes.
The original two expectations stay untouched pending approval. After approval,
apply only this migration in a commit separate from `src/` changes. The new
positive case adds one CTest; publish counts only from a passing full dev run.

## Checks before review

The separately compiled and linked proposed copy passes **all 23 ClassFront
tests**, including the six new mechanism tests and the moved positive case.
The original test binary passes 20/22 and fails precisely the two locations
above; this is not reported as full-suite success. The independent new matrix
passes **120 source tests plus its generator check**. Four original small
probes pass their written oracles on Icarus 13.0 and the implementation.

The local concrete diff is `build/p1-class-implicit/expectation-migration.patch`;
the proposed source, compiler/linker commands and test log are beside it.
The main tree, existing expectation lines and private untracked drafts remain
unchanged. Full regression and count publication follow approval.

## Approved application and unfiltered validation

The exact reviewed patch was applied as `4e177301`, without `src/` changes.
The tracked ClassFront binary now passes all **23 tests**. Unfiltered dev
passes **12,140/12,140**, with zero failures/skips, including the count check.
Count-only commit `be1b7e87` uses that successful JUnit; external measurements
retain their dates and values. See the [approved validation record](../measurements/2026-11-05/class-implicit-approved-validation.json).
No class expectation migration remains pending. Main integration, push and
slang posting retain their separate authorization rules.
