# Automatic fork capture: migration of historical rejection expectations

[日本語](automatic-capture-expectation-migration.ja.md). Status: proposed 2026-10-25;
**approved and applied on 2026-10-25**, as written below, in a commit of its own.

## Decision requested

Approve the following expectation migration in a commit separate from the
implementation. The [approved shared-storage contract](automatic-capture-proposal.md)
explicitly excludes approval to change existing expectations. The grounds are
IEEE 1800-2023 6.21 (automatic scope lifetime), 9.3.2 (parallel blocks and ref
restrictions) and 13.5.2 (parameter/reference behavior), summarized here in our
own words. No external simulator source or external test code was used.

## Concrete changes

1. `tests/tools/check_fork_declaration_boundary.py`: retain the three existing
   `join`/`join_any`/`join_none` source inputs and the three parent-initializer
   checks. Replace each positioned unsupported diagnostic/exit 2 expectation
   with exit 0, exactly `7\n` on stdout, and no error diagnostic. The automatic
   fork declaration initializes in the parent; its child retains and reads the
   same variable. Update the checker description to reflect approved support.
2. `tests/cli/run_dynamic_container_tests.py`: retain the automatic task's
   queue/fork source. Replace only its unsupported diagnostic expectation with
   successful silent execution and no error diagnostic. Other runtime-error
   and unsupported implicit-sensitivity expectations remain strict and unchanged.
3. `tools/gen_unsupported_matrix.py`,
   `tests/cli/unsupported_matrix.accepted`,
   `tests/cli/unsupported_matrix.stderr` and `tests/cli/run_cli_tests.py`:
   retain `unsupported_matrix/automatic_in_fork.sv` byte for byte, classify it
   as an accepted historical input, and remove its one unsupported diagnostic.
   Generate a new `unsupported_matrix/automatic_in_fork.expected` containing
   exactly `5\n`. The CLI checker compares accepted historical inputs with an
   adjacent `.expected` when present; otherwise it still requires empty stdout.
   All other exit, stdout and diagnostic comparisons remain strict. The source
   generator version stays 19 because no generated source is changed.

The proposed commit contains no `src/` changes, no deleted/renamed source
fixtures, no weakened unit assertions and no performance-budget changes.
This is an expectation change justified by implemented language support,
not a blanket approval for other expectation changes.

## Evidence available before migration

The working implementation passes 186 new original source designs, generated
by `tools/gen_automatic_capture_matrix.py`: six source containers, all three
joins, 2/4-state widths 1/7/65/129, function-return capture, real/shortreal,
string/chandle, wide fixed arrays, flattened structures, queue/dynamic-array
storage, loop reentry/break/continue, nested forwarding, output copyback,
normal ref with join, ref static, guards and named disable. The independent
`check_automatic_capture_metadata.py` verifies all unread typed fields,
empty lexical scopes and forwarding to grandchildren; it also rejects five
illegal or still unsupported boundaries, including normal-ref asynchronous
capture, automatic actual for ref static, const-ref writes and automatic events.

The first full dev run (4,868 tests) reports four failures: the three historical
rejection checks above and the README count check, before regeneration for
new tests. All 186 new designs and all 29 existing SIR dumps pass. There are
the same two known skips. These are observations of the working tree, not a
claim that the complete implementation has finished all validation.

## Validation after approval

Run the unchanged source fixtures against the new expectations, generator
consistency checks and `tools/check_expected_change.py` on each separate
commit. Rerun the full local CI, fixed P1 population and pinned sv-tests;
report remaining support gaps and actual results. Do not infer complete P1
or Icarus coverage from this finite matrix.
