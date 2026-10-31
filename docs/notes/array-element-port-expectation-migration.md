# Fixed-array element port expectation migration

[日本語](array-element-port-expectation-migration.ja.md). Status: **approved (2026-11-01)**.

IEEE 1800-2023 7.4 and 23.3.3.7, under the approved 2026-10-25
MOS/port contract section 4 and fixed-array contract sections 1–2, permit
static structural mappings to the physical bits of fixed array elements.
`Lowerer::staticNetBits` now maps a complete fixed-array element through its
source bounds to a physical declaration-order ordinal. The existing identity
network preserves the independent physical drivers and their strengths.

The requested expectation-only migration covers exactly these unchanged files:

- `tests/cli/unsupported_matrix/net_port_meet_array_top.sv`
- `tests/cli/unsupported_matrix/net_port_meet_array_generate.sv`
- `tests/cli/unsupported_matrix/net_port_meet_array_instance.sv`

For each input, change exit 2 with one unsupported diagnostic to exit 0,
no stdout and no error diagnostics. Preserve the source bytes. Add the three
names to the generator's accepted set and generated accepted list; remove only
the corresponding three lines from `tests/cli/unsupported_matrix.stderr`.
Retain the other port boundaries (wired, tri0, width conversion), all other
rejection cases and existing suite/performance expectations. Make this migration
in a separate commit from the implementation after approval. This request does
not authorize a push or claim P1 completion.

The maintainer approved exactly this three-input migration on 2026-11-01.

The original port-coercion warning and driven-here note remain. Each of the three inputs has an adjacent generated `.warnings` file; the runner compares it exactly and still rejects every error diagnostic. Previously accepted inputs without such a file still require no diagnostics. The approved condition was no error diagnostics, not suppression of strict slang warnings.
