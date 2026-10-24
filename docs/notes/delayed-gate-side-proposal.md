# Delayed three-state gate side propagation

[日本語](delayed-gate-side-proposal.ja.md). Status: **approved (2026-10-25);
implementation in progress**. The original 2026-10-24 proposal body is retained
as the decision record; approval requests, future tense and unapproved-status
statements below describe that original proposal state. The user approved all
six contracts together on 2026-10-25. See the [specification mapping](p1-execution-contracts-2026-10-25.md).
Approval does not claim implemented support, authorize changes to existing
expectations, or introduce dependencies.

## Why review is needed

VALUE_SEMANTICS 5.8 defines L/H as X with one strength side suppressed.
SIR_SPEC 3.5 defines private two-state side fields, initially undriven.
SCHEDULER_SPEC 12 gives the minimum delay for propagation to X/L/H and
defines inertial cancellation for a changed value. These contracts do not
explicitly decide whether an L-to-H change, whose four-state projection
stays X, schedules propagation or changes the side fields immediately.
IEEE 1800-2023 28.16/Table 28-9 does not give a separate L/H-to-L/H row.
This missing detail changes observable resolution against another driver.

[P1_SCOPE](../spec/P1_SCOPE.md) requires review before implementing missing detailed semantics.
The complete experimental delayed-side path was removed from the working
implementation pending this review. The independent wide-mask transport
correction implements the already defined undelayed 5.8 behavior and can
proceed separately. MOS/switch strength propagation and complete P1 coverage
remain unfinished.

## Proposed amendments

Add to SCHEDULER_SPEC 12 and VALUE_SEMANTICS 5.8:

1. A three-state output's inertial state is its value **and both side fields**.
   A different side field is a new output state even if the projection stays X.
   For a new L/H state choose the existing minimum of rise, fall and turn-off;
   zero delay commits in Inactive under the existing rule. Preserve the old
   complete output until this propagation is due.
2. Capture all fields when evaluating the gate. If a pending complete state
   differs, cancel it. If the new complete state equals the committed one,
   schedule nothing. If it equals the pending one, retain that original
   deadline. Otherwise schedule one complete propagation. This includes
   short side-only pulses and changes caused by X/Z controls.
3. Initially, side fields remain undriven as in SIR_SPEC 3.5, until their
   complete propagation commits. Do not publish a newly computed side early
   just because the raw driver slot already contains X.
4. Commit the raw driver and all its side fields before invoking change
   callbacks or waking the resolution process. The update owns its fields;
   later source changes, cancellation and callback-created drivers cannot
   borrow or invalidate them. A user signal wakes ordinary event controls
   only if its resolved four-state value changes, preserving 5.6.

This is a project interpretation for review. It does not assert that the
observed Icarus side-only timing is an IEEE defect. Timing of ordinary
value transitions and the undelayed L/H resolution rules do not change.

Add a mechanical encoding to SIR_SPEC 5.4:

- `drive.delay target, value, rise, fall, off` retains its existing form.
  Optional trailing `(side-bit target, value)` pairs belong to the same
  pending propagation; they are not independent delayed instructions.
- Each side target is a scalar two-state slot bit, with a fitting one-bit
  value and declared write coverage, separate from the primary value.
  Duplicate side targets and incomplete pairs are invalid. Their operands
  must be defined before registration and are copied, not borrowed SSA values.
- Preserve side computations in their actual control-flow block; a gate
  input can call a function through a short-circuit expression. Move the
  delayed instruction after these computations while retaining dominance.

The shared kernel owns the complete-state comparison, one pending generation,
captured fields and atomic publication. The VM only transports typed operands.
A C++20 kernel API can take a span of owned-by-copy one-bit side updates in
addition to the existing value. Side fields cannot hold heap references, so
they introduce no new GC root kind. Keep callbacks outside the commit walk
until all fields are written; copy target information before callbacks can
reallocate driver records. No C ABI 0.3 entry change is proposed by this
internal extension. All execution tiers must use the same kernel path.

## Original observations and acceptance evidence

[Original sources and versioned observations](../measurements/2026-10-24/delayed-gate-side-design-probes.json)
record matching installed 13.0 stable and 14.0 development compiler/runtime
pairs, plus an explicitly identified uncommitted experiment. No other
simulator source or tests were read. Observations are not approved oracles.

For the side-only probe with a weak-1 background, the proposal gives
`first 1`, `L x`, `H_pending x`, `H 1`; both Icarus versions give
`first x`, `L x`, `H_pending 1`, `H 1`. The separate settled 0-to-H probe
gives `0, 0, 1` in all three runs. Agreement on that ordinary transition
does not decide the missing same-projection case.

Acceptance must cover all four bufif/notif gates, one/two/three delays,
zero delay, L/H/X/Z, current/pending equality and cancellation, no deadline
reset for identical re-evaluation, simultaneous value/mask changes, typed
ownership, callback reentrancy/reallocation, wide unaligned side halves,
short-circuit control flow, malformed SIR and writes outside the declared set.
Use independent kernel/SIR checks, original source matrices, reference
comparisons with explicit policy mismatches and compiled negative controls.
Preserve existing expectations and run full local CI. Passing this mechanism
would not prove full MOS/switch support or completion of P1.
