# Converting net-port expectation migration

[日本語](converting-port-expectation-migration.ja.md). Status: **approved (2026-11-01)**.

IEEE 1800-2023 4.9.6/23.3.3.7, SIR_SPEC 3.5 and approved 2026-10-25
MOS/port contract 4 retain strong directed implicit assignments for input/output
connections requiring width/sign/state conversion or explicit casts. Do not
classify those carriers as an unimplemented identity connection; let the existing
ordinary net resolver consume them.

The maintainer approved only these three unchanged historical classifications:

- `tests/cli/unsupported_matrix/net_port_meet_width_top.sv`
- `tests/cli/unsupported_matrix/net_port_meet_width_generate.sv`
- `tests/cli/unsupported_matrix/net_port_meet_width_instance.sv`

All three now exit 0 with no stdout or error diagnostic, instead of exit 2 and
one unsupported diagnostic. Preserve source bytes, add three accepted names
and remove only the corresponding three stderr lines. Preserve strict slang's
port-coercion warning and driven-here note, checked exactly by three new
adjacent `.warnings` files. The existing nine warning goldens remain unchanged.

All other existing expectations and performance budgets remain protected.
The maintainer explicitly approved this three-input migration on 2026-11-01.
Commit it separately from implementation. This is
neither push approval nor P1 completion. Slang posting remains on hold.
