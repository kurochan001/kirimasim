# Migrate three historical automatic-event rejection checks

[日本語](event-field-expectation-migration.ja.md). Status: **approved (2026-10-25)**.
The user approved the exact three migrations below; application awaits the
corresponding implementation. The original proposal text below requests approval separately
from implementation. The [automatic-capture contract](automatic-capture-proposal.md)
5/6/7 is already approved; its approval explicitly excludes existing oracles.

## Reason and grounds

IEEE 1800-2023 6.8, 6.21 and 15.5, the approved contract, and its normative
mapping to SIR_SPEC 3.2.1/7/8 and RUNTIME_ABI 4/6 require typed automatic-event
fields, fresh construction, retained lexical scopes and shared event identity.
The kernel registry, root visitor and pending-operation ownership are now
implemented. The next SIR/VM/front connection must not keep rejecting canonical
event fields just to satisfy historical unsupported-provider tests.

AI_POLICY's "Rules that protect expected values" says: "A change of an expected
value states its reason and the clause of the standard, and a human approves it."
This migration will be its own commit without `src/` changes. No dependencies,
performance thresholds, existing source fixtures or unrelated assertions change.

## Exact proposed changes

1. `tests/unit/activation_sir_test.cpp`, case 11 of
   `MalformedScopeAndCaptureTablesAreRejectedByTheSharedVerifier`: change only
   the invalid input from `Type{TypeKind::Event, 0}` to
   `Type{TypeKind::Event, 1}`. Canonical event types have width zero. Keep the
   `typed storage` rejection assertion and every other case. Add positive
   canonical scalar/fixed-array event coverage with the implementation.
2. `tests/unit/activation_environment_test.cpp`,
   `UnsupportedOwnedProvidersAndMalformedTablesAreRejected`: make the same
   width-zero to width-one change in its first input. Keep `InvalidLayout`,
   zero live allocations, the exact eleven-case assertion and all other inputs.
   The implementation must reject a noncanonical event type before publishing
   kernel layouts. Canonical event providers will have new positive tests.
3. `tests/tools/check_automatic_capture_metadata.py`: move only the fifth
   original source (the task with `event unread; int value=7;` and its fork
   child) from the rejection loop to an accepted-input check. Keep its complete
   source text byte for byte. Require exit 0, stdout exactly `7\n`, empty stderr,
   one `event.new` in the dump, and event fields in the owner's scoped storage
   and the child's capture view. Keep all four illegal-ref inputs and their
   positioned-error checks unchanged. Update the tool's summary accordingly.

## Timing and acceptance

Apply items 1/2 only when canonical event fields and their real typed provider
are connected. Apply item 3 only when source construction, whole-scope roots,
temporary ownership and typed runtime operations are connected and verified.
Until then the historical checks remain intact; do not claim all-green CI after
enabling a feature if its approved migration has not yet been applied.

Check the migration commit separately with `check_expected_change.py`. Retain
malformed-type coverage and add independent positive scalar/array/capture/root
tests. Run the full local CI and source matrix without replacing old sources,
skipping failures, weakening other checks or changing performance budgets.
