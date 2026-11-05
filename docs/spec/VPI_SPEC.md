# VPI_SPEC: VPI

[日本語](VPI_SPEC.ja.md) · This English text is the main version; the Japanese version follows it, and where they differ, this text holds.

**Reader context:** [VPI](../GLOSSARY.md#vpi) lets a C plugin inspect and affect
simulation through registered calls and callbacks; start with the [plugin
workflow](../USER_MANUAL.md#use-a-c-plugin-through-vpi) for the executable subset.

**Revision (2026-10-24):** old P2→P3, old P3→P4 and old P4→P5; new P2 (SIR v1) is inserted, with tier numbers unchanged ([mapping](../notes/phase-renumbering.md)).

**Phase scope amendment (approved 2026-10-23):** [P1_SCOPE](P1_SCOPE.md)
advances the Icarus-required subset of later-phase or out-of-scope work into
P1. Existing semantic rules remain; missing details need review before code.
Phase labels below describe the original plan, subject to this amendment.

Priority: P1 for the Icarus-required subset; P4 for the remainder.
Status: **first execution contract approved (2026-11-01)**.

The maintainer approved [sections 1–6 of the P1 VPI contract](../notes/vpi-p1-proposal.md).
They are the operative detailed amendment to this document, CLI 2,
SCHEDULER_SPEC 7/17 and RUNTIME_ABI 9.6. Implementation status and the
finite reference inventory are separate from approval.

**Variable release-result correction (2026-11-03):** `vpiReleaseFlag` fills
non-null `value_p` with the released object value (38.34), using the existing
read conversion and owned buffers. Rejected writes preserve caller output.
[Implementation and checks](../notes/vpi-release-results.md).

## The VPI functions supported

The approved first host API, explicit `--vpi=<file>` loading, startup and
system task/function registration follow contract sections 2–3. Unsupported
API forms remain diagnosed; this subset does not establish full VPI parity.

## Objects

Static reflection, argument-time values and writable bindings, conversions,
Context ownership, handle/iterator lifetimes and errors follow sections 3–5.
The [whole-net amendment](../notes/vpi-net-write-execution.md) connects checked physical net capabilities; selected/delayed net writes and passive deposits remain work.

## Callbacks and their regions

Sections 4–5 preserve the existing scheduler's timing callback regions and
specify synchronous value-change reentry, registration snapshots, cancellation
and teardown. The existing single guarded current-Context pointer exception
applies, with the concrete allowlist entry approved by section 5.

Acceptance requires the source, lifetime, malformed-input, compiled-control
and configuration checks in section 6. The [reference inventory](../notes/vpi-reference-inventory.md)
remains availability evidence rather than native implementation evidence.

Implementation status: the [first tier-1 execution subset](../notes/vpi-execution.md)
is connected. See the [validation record](../notes/vpi-validation.md) and explicit
unsupported API/object boundaries.

**Whole-net write amendment (approved 2026-11-03):** [net-write contract sections 1–8](../notes/vpi-net-write-proposal.md) govern immediate point-net deposits and whole-net force/release through verified physical capabilities. They override the initial contract's net-write rejection within that subset. The [native subset is implemented](../notes/vpi-net-write-execution.md). The direct 2023 edition audit remains unconfirmed. The exact one-assertion/two-case physical-origin count migration was separately approved and applied; all other expectation-migration rules remain.
