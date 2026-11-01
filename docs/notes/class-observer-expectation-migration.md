# Ordinary class-observer expectation migration

[日本語](class-observer-expectation-migration.ja.md)

Status: **approved and applied (2026-11-01)**.
The maintainer explicitly approved both presented migration items (three tests);
this record covers only the one class-observer test.

The [implementation](class-independent-observers.md) follows IEEE 1800-2023
8.4/9.4.3 and approved class contract 7. A constant-true `wait` has no object
dependency; unrelated class descriptors do not make it an unsupported operation.

One existing native-SIR unit test,
`ClassVmExecution.UnsupportedObservationCombinationsStillFailBeforeAnyRuntimeValueIsCreated`,
previously required an internal preflight error and zero slots for that case.
The implementation correctly finishes quiescently with initialized slots.
Before approval, full dev found this sole mismatch: 11,068 of 11,069 passed, with zero skips.

Approved migration, preserving the constructed SIR byte for byte:

- Rename the test `OrdinaryConstantWaitCoexistsWithClassMetadata`.
- Require `EndReason::Quiescent` and an empty error instead of an internal
  observation error.
- Require one initialized runtime slot per design slot, and null/zero initial
  values in the existing object and flag slots.
- Keep the zero object-entry assertion and add zero remaining activations.

The exact patch is prepared as `build/class-observer-expectation.patch`.
A copy of the proposed test was compiled and linked to the actual implementation
libraries and passed all 22 ClassVmExecution checks. The same patch is now applied to the repository test and committed separately from `src/`.
Object-dependent rejection tests retain their assertions. Combined full validation follows
integration with the separately approved partial-inout migration. This approval does not
authorize main integration, push or slang posting.
