# Migrate the historical nmos rejection input

[日本語](mos-expectation-migration.ja.md). Status: **proposal (not approved)**.

## Reason and measured evidence

IEEE 1800-2023 28.7/28.10/28.13/28.16 and the approved
[MOS/switch contract 1/6/7](mos-switch-proposal.md) require directed MOS
transmission. `front` now connects six directed MOS kinds to the existing SIR
strength operations and shared kernel. New original source tests cover static
strengths, captured controls, selected array data, primitive arrays, initial
three-state side fields and inertial delays. This is a partial implementation
of that contract; passive networks, registered-net force and full-interval
formatting remain incomplete.

The unchanged historical `tests/cli/unsupported_matrix/switch_mos.sv` now exits
0, prints no stdout and emits no warning/error (only the normal informational
banner and quiescence message). Its module, generate and child-module instances
are all accepted. Preserving the old rejection by fixture shape would violate
CLAUDE.md's mechanism rule. The old expected diagnostics are obsolete; the
existing input should continue to be executed, requiring silent success.

## Exact requested migration

Apply in a separate commit with no `src/` changes, after human approval:

1. Add only `switch_mos` to `tools/gen_unsupported_matrix.py`'s `ACCEPTED` set.
   Keep source-generation version 19 and all source rendering unchanged.
2. Regenerate `tests/cli/unsupported_matrix.accepted`, adding only `switch_mos`.
3. Regenerate `tests/cli/unsupported_matrix.stderr`, removing only the three
   `switch_mos.sv` diagnostics: line 12 column 10, line 4 column 8, line 9 column
   8. Keep the three bidirectional `switch_bidirectional` diagnostics and all others.
4. Keep every source fixture byte for byte and every other expectation intact.
   Do not add a replacement stdout oracle: the CLI runner already requires
   silent success for accepted inputs without an adjacent `.expected` file.
5. Repeat the complete CLI and local CI checks and record their actual results.

AI_POLICY's expected-value rule says: “A change of an expected value states its
reason and the clause of the standard, and a human approves it.” This proposal
requests only the migration above, not additional specification, dependency,
managed-event safepoint, publication or push authorization. It is unapplied.

The complete [unapplied patch](mos-expectation-migration.patch) passes
`git apply --check`. An in-memory generation with the additional acceptance
entry preserves all source fixtures and changes only the two generated lists.

## Full validation before migration

The [implementation record](source-mos-execution.md) completes 17/21 local CI
checks. All four preset test steps fail only `cli/options`, whose output names
this unchanged input and exactly these three missing diagnostics. All other
CTest cases pass or are the same two known skips. The migration is still
unapproved and unapplied; no success is claimed for the complete CI run.
