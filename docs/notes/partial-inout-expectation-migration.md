# Partial-inout port expectation migration

[日本語](partial-inout-expectation-migration.ja.md). Status: **approved and applied (2026-11-01)**.

IEEE 1800-2023 7.4/23.3.3.7 and approved MOS/port contract 4 connect retained
static net-net inout bits by physical identity, without a value carrier or a
new root. The approved migration covers only these two existing classifications:

- CLI `tests/cli/unsupported_matrix/port_inout_part.sv`: preserve source bytes;
  move exit 2/one unsupported diagnostic to exit 0/no stdout or warnings/errors,
  as confirmed with the implementation. Add one accepted name and remove only
  its stderr line.
- Unit `ArrayElementPort.PartialInoutKeepsItsPositionedBoundary`: preserve the
  embedded SV source, require successful compilation instead of rejection and
  rename it `PartialInoutMapsItsPhysicalElement`. Also require one identity
  edge, physical element ordinal 1/bit 0, no implicit or explicit origins,
  SIR verification and successful execution with empty stdout.

The concrete diff is `build/partial-inout-expectations.patch`. Inout conversion,
remaining force/uwire and moving/invalid identity boundaries, all other existing
expectations and performance budgets remain protected. After approval, commit
only the expectation migration separately from implementation. This does not
authorize push or establish P1 completion. Slang posting remains on hold.

The review-copy CLI runner/generator and the proposed positive unit all pass.
The unit was separately compiled against the actual implementation libraries.
The same prepared patch is now applied to the repository after explicit approval.


## Approval and application (2026-11-01)

The maintainer explicitly approved both presented migration items, covering
three tests. This record applies only to the two partial-inout tests above.
The exact prepared patch is applied without changing either source design:
CLI acceptance replaces the unsupported diagnostic, and the unit requires
physical identity, the selected element/bit, no roots and successful silent
execution. These expectation changes are committed separately from `src/`.
Combined regression/count validation follows integration with the separately
approved class-observer change. Main integration, push and slang posting are
not authorized by this approval.


Combined dev regression now passes all 11,299 tests without exclusions. See the [integration record](p1-approved-class-inout-integration.md); main integration, push and slang posting remain separate.
