# Partial-inout port expectation migration

[日本語](partial-inout-expectation-migration.ja.md). Status: **awaiting approval (2026-11-01)**.

IEEE 1800-2023 7.4/23.3.3.7 and approved MOS/port contract 4 connect retained
static net-net inout bits by physical identity, without a value carrier or a
new root. Approval is requested only for these two existing classifications:

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
