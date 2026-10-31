# Partial-port effective net kind expectation migration

[日本語](port-kind-expectation-migration.ja.md). Status: **awaiting approval (2026-11-01)**.

IEEE 1800-2023 23.3.3.7/Table 23-1 and the approved 2026-10-25 MOS/port contract 4
require effective net kinds per connected physical bit, with one implicit pull/supply
per collapsed component. `mergeNetKinds` is shared with whole-port merging;
unconnected neighbouring bits retain their kinds.

Approval is requested only for the classifications of these six unchanged inputs:

- `tests/cli/unsupported_matrix/net_port_meet_wired_top.sv`
- `tests/cli/unsupported_matrix/net_port_meet_wired_generate.sv`
- `tests/cli/unsupported_matrix/net_port_meet_wired_instance.sv`
- `tests/cli/unsupported_matrix/net_port_meet_tri0_top.sv`
- `tests/cli/unsupported_matrix/net_port_meet_tri0_generate.sv`
- `tests/cli/unsupported_matrix/net_port_meet_tri0_instance.sv`

All six now exit 0 with no stdout or error diagnostic, instead of exit 2 and one
unsupported diagnostic. Preserve source bytes, add these names to the generator
and generated accepted list, and remove only their six stderr entries. Preserve
strict slang's port-coercion warning and driven-here note, checked exactly through
six new adjacent `.warnings` files.

Width conversions, explicit partial inout, uwire, force, all other existing
expectations and performance budgets remain unchanged. After approval, expectation
migration is committed separately from implementation. This is neither push
approval nor a claim of P1 completion.

The concrete patch is prepared locally as `build/port-kind-expectations.patch`.
Applying it only to a disposable review copy passes the full CLI runner and
the unsupported-matrix generator check, including exact warnings/notes.
Repository expectation files remain unchanged pending approval.
