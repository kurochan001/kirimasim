# Migrate historical passive-switch rejection expectations

[日本語](passive-expectation-migration.ja.md). Status: **proposal (not approved)**,
2026-10-26. The patch is unapplied.

## Reason and evidence

IEEE 1800-2023 28.8 and the approved [MOS/switch contract 3/6/7](mos-switch-proposal.md)
require passive transmission. The new source lowering connects all six passive
kinds to the existing typed SIR and shared-kernel graph. Its first 139 original
source designs pass, with scalar strengths, both directions, controls, primitive
arrays and port connections, complete-interval sensitivity, cycles and delays.
Independent disconnected graphs are solved separately before one atomic
publication; the 65/129-bit mixed-control designs no longer enumerate a cross
product of unrelated edges. Further validation remains in progress.

The unchanged `tests/cli/unsupported_matrix/switch_bidirectional.sv` contains
bare tran instances in a module, generate block and child module. It now exits
0 with empty stdout and no non-informational diagnostics. The unchanged bare
`wire a,b;tran m(a,b);` input in `tests/tools/check_source_mos_boundaries.py`
also exits 0 without stdout/errors. A rootless always-conducting network is
legal and quiesces with Z values; neither input requests output. Preserving
rejection by fixture name/shape would violate CLAUDE.md's mechanism rule.

## Exact requested change

Apply the [unapplied patch](passive-expectation-migration.patch) in its own
commit, with no `src/` change, only after human approval:

1. Add only `switch_bidirectional` to the unsupported generator's `ACCEPTED`
   set. Keep generation version 19 and every source byte unchanged.
2. Regenerate `tests/cli/unsupported_matrix.accepted`, adding that one name,
   and `tests/cli/unsupported_matrix.stderr`, removing only the three old
   switch_bidirectional diagnostics (12:10, 4:8, 9:8).
3. Keep all six source-MOS boundary inputs. Change only the bare tran input's
   expected result to exit 0, empty stdout and no non-informational stderr.
   The other five inputs retain exit 2, positioned diagnostics and their
   existing assertions. No skip, deletion or replacement input is added.
4. Run the complete CLI and boundary checks and the appropriate regression
   checks, recording their results. No publication/push approval is requested.

The [AI policy](../spec/AI_POLICY.md#rules-that-protect-expected-values) requires
that a changed expectation states its reason and standard clause and receives
human approval. This requests only the two historical expectation migrations;
it does not approve new waveform semantics, dependencies, other pending
amendments or changes to P1 completion. It is not applied to the working tree.
