# Delayed-side boundary expectation migration

[日本語](delayed-side-boundary-migration.ja.md). Status: **approved (2026-10-25); integrated**. The user separately approved
this exact expectation-only migration after the implementation checks.

The six execution contracts were approved on 2026-10-25. Existing expectation
changes remain separately reviewed under CLAUDE.md and AI_POLICY. This migration
has no production source change; its isolated branch is
`codex/delayed-side-expectations`, based on `39cacb37`.

Only `tests/tools/check_delayed_gate_boundary.py` changes an oracle. Its six
old delayed cases require rejection of a now-approved executable form.
Replace that rejection with exact five-line simulation output. Preserve all
six existing undelayed acceptance checks, all widths (1/65/257), both direct
and short-circuit function controls, return-code checks, and error checks.
No skip or existing suite/expected/PerfCount file changes.

The new delayed program first settles strong 0 against weak 1. Changing to H
retains 0 before the minimum delay and resolves 1 afterwards. Changing H to L
keeps 1 before that same delay, then resolves X. Each comparison prints 1,
with labels `settled`, `pending`, `H`, `L_pending`, `L`. The expected output
comes from the approved complete-state contract, not simulator output.
Primary clauses: IEEE 1800-2023 28.6, 28.16/Table 28-9, VALUE_SEMANTICS 5.8,
SCHEDULER_SPEC 12 and SIR_SPEC 5.4. The same-projection timing is an explicit
project choice; existing Icarus observations are not the new oracle.

Review `git show codex/delayed-side-expectations` and the
[approved contract](delayed-gate-side-proposal.md). Implement and check the
new execution path separately, with independent new kernel/SIR/source tests
and compiled controls. Do not accept this migration by weakening assertions
or by accepting either rejection or execution at runtime.
