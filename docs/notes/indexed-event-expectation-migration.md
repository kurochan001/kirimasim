# Migrate the historical runtime-indexed event rejection input

[日本語](indexed-event-expectation-migration.ja.md). Status: **proposal (not approved)**,
2026-10-26. This requests only the expectation migration below, separately from
implementation. It does not approve the managed-event instruction-safepoint
amendment or automatic default event construction.

## Reason and evidence

IEEE 1800-2023 7.4.5/9.4.2/15.5, the approved automatic-capture contract 5/7,
SIR_SPEC 3.2.1/7/8 and the typed-event VM connection require loaded fixed-array
event selections. A named wait selects its concrete object once; later changes
of an index are not triggers. No managed allocation or new collection timing is
needed to select an existing permanent event object.

The new front connection uses the existing typed place/index lowering, keeping
constant valid static selections in their compact form. The unchanged historical
source now exits 0, prints nothing and has no diagnostic after the CLI's existing
informational-line filter. An independent smoke source waits for the original
selection at time 2 after the index changes, and observes false for an invalid
selection; a delayed null trigger with a 65-bit overflowing delay is a no-op.
The complete new source matrix and full CI remain to be checked; the current
source is not a claim of complete P1 or automatic-event support.

AI_POLICY's expected-value rule says: "A change of an expected value states its
reason and the clause of the standard, and a human approves it."

## Exact requested migration

In a separate commit with no `src/` changes:

1. Add only `event_var_index` to `tools/gen_unsupported_matrix.py`'s `ACCEPTED` set.
   Keep the source-generation version at 19: source rendering is unchanged.
2. Regenerate `tests/cli/unsupported_matrix.accepted`, adding that single name.
3. Regenerate `tests/cli/unsupported_matrix.stderr`, removing only the six
   `event_var_index.sv` unsupported-index diagnostics at lines 7, 10, 13, 17,
   22 and 27 (column 7, except lines 22/27 at column 9).
4. Keep `tests/cli/unsupported_matrix/event_var_index.sv` byte for byte,
   including all six procedural contexts. Keep every other input, acceptance
   entry and diagnostic. Do not add a replacement `.expected` file: the existing
   CLI runner already requires exit 0, empty stdout and no noninformational
   stderr for this newly accepted historical input.

Apply only after the new original source matrix passes. Check the generator,
unchanged source bytes and the exact six-line/one-entry change, then run the
complete CLI suite and full local CI. Do not silently suppress the old failure
before approval. No dependency, performance budget or unrelated oracle changes.
