# Ordinary class-observer expectation migration

[日本語](class-observer-expectation-migration.ja.md)

Status: **awaiting maintainer approval (2026-11-01)**.

The [implementation](class-independent-observers.md) follows IEEE 1800-2023
8.4/9.4.3 and approved class contract 7. A constant-true `wait` has no object
dependency; unrelated class descriptors do not make it an unsupported operation.

One existing native-SIR unit test,
`ClassVmExecution.UnsupportedObservationCombinationsStillFailBeforeAnyRuntimeValueIsCreated`,
currently requires an internal preflight error and zero slots for that case.
The implementation correctly finishes quiescently with initialized slots.
Full dev checks confirm this is the sole mismatch: 11,068 of 11,069 pass, with zero skips.

Proposed migration, preserving the constructed SIR byte for byte:

- Rename the test `OrdinaryConstantWaitCoexistsWithClassMetadata`.
- Require `EndReason::Quiescent` and an empty error instead of an internal
  observation error.
- Require one initialized runtime slot per design slot, and null/zero initial
  values in the existing object and flag slots.
- Keep the zero object-entry assertion and add zero remaining activations.

The exact patch is prepared as `build/class-observer-expectation.patch`.
A copy of the proposed test was compiled and linked to the actual implementation
libraries and passed all 22 ClassVmExecution checks. The repository test remains unchanged pending approval.
If approved, only this migration is committed separately from `src/`; object-
dependent rejection tests retain their assertions. This proposal neither
approves the separate partial-inout migration nor authorizes push or slang posting.
