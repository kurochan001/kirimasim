# Deferred assertion label cancellation: proposed contract

[日本語](deferred-label-cancellation-proposal.ja.md)

Status: **approved by the maintainer** (2026-11-03, explicit approval in this conversation).
Existing expectations remain unchanged. The operation and verifier contract below are finalized for implementation.

## 1. Problem and specification boundary

The external fixed cross selection on upstream `f3ea0379` has five output
differences involving `disable` of a named observed deferred assertion.
Its sibling and separate-process observations agree; reports of the disabled
statement still execute. The three directive forms are assert, assume and cover.
The tests cite IEEE 1800-2023 §16.4.4, with §§16.4–16.4.2 and 9.6.2.

SCHEDULER_SPEC 15.3–15.4 currently describes process-wide flush points and
says queues are emptied only at those points. SIR_SPEC 5.7 has no assertion
identity or targeted cancellation operation. The approved amendment below provides selective label cancellation before
implementation changes.
The observed difference does not establish the internal cause of every form.

## 2. Proposed operative amendment

1. A deferred assertion statement has a static identity, distinct from its
   containing process and its action wrapper. Its pass and fail actions share
   that statement identity. An unlabeled statement may retain an internal identity.
2. Executing a legal `disable` resolved to that assertion label cancels that
   statement's still-pending reports, rather than flushing the containing
   process's whole report queue. Sibling statements and other identities remain.
3. Retain expression and input-argument evaluation, including side effects,
   already performed at registration. Cancellation does not undo them.
4. Already settled reports and actions, and output history from earlier time
   slots, are not changed by this pending-report operation. Further active-action
   control, hierarchical forms and assertion-control system tasks are not added
   by this amendment; unsupported forms must remain explicit.
5. Existing process-wide flush points, ordinary block/task control transfer,
   unique/priority reports and ordering of retained reports keep their meaning.
6. Front resolves a label through slang's semantic symbols and emits an explicit
   SIR cancellation operation. It tags report registration with the same identity.
   It does not rewrite source text or recognize test names. Kernel queue handling
   owns filtering and cleanup; VM executes the SIR operation mechanically.
7. Cancellation releases captured argument roots and frame ownership once,
   using the existing discarded-report callback path. State belongs to Context.
8. Extend SCHEDULER_SPEC 15.2–15.4 and SIR_SPEC 5.7 with this operation and
   identity, preserving legacy report-call text when no identity is needed.
   Finalize the precise encoding and verifier checks before implementation.

## 3. Required verification

Build a mechanism matrix before implementing the five symptoms: directive
assert/assume/cover, selected pass/fail action, one or repeated pending reports,
target/sibling/distinct process, before/after registration, and ordinary
inner/task/outer disable controls. Exercise initial, task and fork entry paths
through the same registration rule. Include captured-input side effects,
live action data, retained ordering and cleanup. Cover observed and final
queues where accepted; keep unsupported scope explicit.

Run the 31 existing external deferred-disable cases with unchanged expectations
and the full finite selection. Keep the original failing snapshot. Existing
fixed Icarus/Verilator differences are separate observations and cannot decide
the new oracle. Full local CI, sanitizer checks and existing expectation guards
remain required. This proposal changes no P1 denominator or acceptance gate.

## 4. Finalized SIR representation

`report.call.named "#0"|"final", #N, @S, args...` registers a statement-tagged report.
`report.cancel #N` cancels only still-pending reports of that static identity in the Context.
Identities use the existing resolved-ID namespace range-checked by `numScopes`;
no ordinary block activation is started. Pass/fail wrappers of one assertion share
an ID; sibling statements and distinct instances have different IDs. The verifier
requires no result, a valid ID kind/range, region, function wrapper and matching
argument count, directions and types. Legacy `report.call` text is unchanged.
Settled reports, unique/priority reports and other identities are excluded.
