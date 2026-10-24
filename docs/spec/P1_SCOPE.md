# P1_SCOPE: execution coverage

[日本語](P1_SCOPE.ja.md) · This English text is the main version.

**Revision (2026-10-24):** old P2→P3, old P3→P4 and old P4→P5; new P2 (SIR v1) is inserted, with tier numbers unchanged ([mapping](../notes/phase-renumbering.md)).

Status: **approved (2026-10-23)**. Records the approved
[scope amendment](../notes/p1-icarus-scope-proposal.md).

P1 targets functional execution coverage at least as broad as Icarus
Verilog 13.0. Existing pass-rate gates remain necessary but do not establish
this target. Record compiler/runtime versions and original mechanism-based
probes; exact-13.0 coverage and complete parity remain unconfirmed.

Standard simulation mechanisms supported by Icarus move into P1 as needed,
including dynamic containers, classes, program execution, VPI, switch-level
modeling, specify paths and timing facilities. This supersedes P4/P5 or
out-of-scope labels for the required subset in SIR_SPEC, SCHEDULER_SPEC,
SYSTEM_TASKS, VPI_SPEC, WAVEFORM and TEST_SUITE and the project summaries.
Those labels retain their original meaning for work beyond this subset.
P3 remains C++ generation. The [P2 direction approved on 2026-10-24](../notes/sir-v1-proposal.md)
permits parallel preparation and moves the final SIR v1 grammar/boundary freeze
to P2 after P1 completion. P1 functional coverage, pass gates and existing
expectations stay in place. Other later-phase work, including constrained
randomization and UVM beyond the comparison scope, retains its existing plan.

Preserve IEEE 1800 value and scheduling semantics, four-state defaults,
the shared execution kernel, strict slang diagnostics, existing P1 gates
and existing test expectations. Document implementation-defined differences;
do not reproduce another simulator's defects. Detailed semantic amendments
must be reviewed before implementation where specifications are incomplete
or conflicting. This approval changes scope; it does not fill those details.

Inventory extensions and external interfaces separately. An extension that
conflicts with strict acceptance needs a documented compatibility option and
semantic proposal before implementation. New dependencies and installations
still require separate approval; this amendment selects none.

This document states a target, not implemented support. Consult
[P1_SUPPORT](../P1_SUPPORT.md) for implemented forms and the
[progress record](../notes/p1-icarus-progress.md) for measured gaps.
