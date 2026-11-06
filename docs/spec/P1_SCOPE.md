# P1_SCOPE: execution coverage

[日本語](P1_SCOPE.ja.md) · This English text is the main version.

**Reader context:** This document defines the completion target, while [P1
Support](../P1_SUPPORT.md#remaining-work-and-scope) records what currently executes and
which gaps remain.

**Revision (2026-10-24):** old P2→P3, old P3→P4 and old P4→P5; new P2 (SIR v1) is inserted, with tier numbers unchanged ([mapping](../notes/phase-renumbering.md)).

Status: **approved (2026-10-23)**. Records the approved
[scope amendment](../notes/p1-icarus-scope-proposal.md).

P1 runs the scope of the [closed exit list](#closed-exit-list-approved-2026-11-07) below and compares it
with Icarus Verilog 13.0 on a fixed list of 188 original probes. It makes no claim
of parity with Icarus beyond that list. Existing pass-rate gates remain necessary.
Record compiler/runtime versions with each comparison.

The mechanisms the closed list names are in P1. This supersedes P4/P5 or
out-of-scope labels for those forms in SIR_SPEC, SCHEDULER_SPEC, SYSTEM_TASKS,
VPI_SPEC, WAVEFORM and TEST_SUITE and the project summaries. Those labels retain
their original meaning for work beyond them.
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

## Optional Icarus `wire real` extension (approved 2026-11-01)

Support for Icarus Verilog's `wire real` extension is not a mandatory P1
condition. Cases with a confirmed dependency on this extension do not
prevent P1 completion when unsupported. Lack of this extension alone is
neither a P1 blocker nor a missing IEEE 1800 standard execution feature.
This explicit exception qualifies all Icarus-compatibility wording in the
P1 completion criteria; other criteria remain in force.

The exception does **not** include `real` variables or arithmetic, a single
continuous assignment to a `real` variable where the standard permits it,
or user-defined net types declared with `nettype real`. Those mechanisms
retain their existing P1 treatment. This amendment neither excludes them
nor adds `nettype real` as a new mandatory P1 feature. No implementation
is added, removed or disabled by this decision.

Classify only reviewed extension-dependent cases, with names, source hashes
and reasons. A name or source substring containing `real` is not evidence
for exclusion. Split mixed tests as needed so that independently testable
standard behavior remains evaluated. The currently confirmed inventory
case is `real_net`, recorded in `P1_OPTIONAL_PROBES` in
`tools/icarus_feature_probe.py`; changed sources need dependency rechecking.

Keep unsupported results as unsupported/compilation failures, never PASS.
A proper compilation error is acceptable for the extension. Crashes,
hangs, internal errors and other independent execution/infrastructure
failures remain failures under the existing defect rules, even for an
out-of-scope case. Report evaluated and excluded totals separately, with
reasons. Raw historical observations are preserved; current P1 decisions
use the required-scope classification and independent-failure checks in
[TEST_SUITE](TEST_SUITE.md#optional-extension-classification).

## Closed exit list (approved 2026-11-07)

The earlier target, execution coverage at least as broad as Icarus 13.0, could
not be judged: a finite list cannot show it, and finding out what it required
had no end. The [proposal](../notes/p1-closed-exit-proposal.md) gives the
grounds and the measurements. P1 ends when these four hold; a form outside them
does not hold P1 open.

1. **The passing conditions of [TEST_SUITE](TEST_SUITE.md):** sv-tests, the fixed
   population, the required tests, the `valsem` cross-check, and the M8 report with
   its human review records.
2. **The fixed probe list:** `tests/gates/p1_probes.txt` holds 188 probes by name
   and source hash, and `tools/icarus_feature_probe.py --extended --gate
   tests/gates/p1_probes.txt --require-parity` passes against Icarus 13.0. Changing
   the list takes a person's approval.
3. **The named mechanisms, as implemented when the list was fixed:** dynamic
   containers, classes under the approved contract, programs, ports and strengths,
   switch-level modeling, the first VPI subset, scalar and state-dependent specify
   paths, VCD and FST. Each passes its mechanism matrix and is described in
   [P1_SUPPORT](../P1_SUPPORT.md). This asks for no new feature.
4. **A stated ground for each probe whose Icarus output differs from the expected
   value** (25 when the list was fixed): the clause of the standard or the approved
   contract that the expected value follows.

What the list does not name moves out of P1:

| Forms | Move to |
| --- | --- |
| Class and dynamic-container forms beyond the matrices; VPI beyond the first subset; `%p` handle and storage forms; `$fread` shapes | P4 |
| Switch-level modeling beyond the matrices; remaining specify paths, pulse controls and timing checks | P5 |

A wrong result is a bug in any phase. The comparison tool keeps running as
information, and a form it finds later goes on the P4 or P5 list.

**`dense_udp_coverage`:** this probe's 33-input UDP makes slang's exhaustive check
of edge coverage run past the limit. kirimasim keeps slang unmodified, so it cannot
close this. For this one source hash, a time limit that `kirimasim parse` alone
also reaches is not a failure, and the probe counts as out of scope. A crash, an
internal error or any other limit stays a failure, and so does a changed source.

## Approved P1 execution amendments (2026-10-25)

The following operative contracts are normative amendments to the sections
identified in each contract. They supersede earlier incomplete or excluded
forms only as explicitly specified. Evidence sections retain their original
provenance and do not claim implementation. The
[approval record](../notes/p1-execution-contracts-2026-10-25.md) fixes scope and
preserves existing expectations and external ABI versions.

- [Fixed array ports and element-wise resolution, sections 1–6](../notes/fixed-array-ports-proposal.md).
- [Shared automatic activations, captures and automatic events, sections 1–8](../notes/automatic-capture-proposal.md).
- [Event-controlled NBA: scheduler, lifetime and SIR/kernel boundary contracts](../notes/event-nba-proposal.md).
- [Delayed three-state side propagation: the Proposed amendments section](../notes/delayed-gate-side-proposal.md).
- [Full-strength MOS and passive networks, sections 1–9](../notes/mos-switch-proposal.md).
- [Class identity, initialization, ownership and execution, sections 1–10](../notes/class-runtime-proposal.md).

## Approved scalar specify-path contract (2026-11-01)

The maintainer approved [specify contract sections 1–8](../notes/specify-p1-proposal.md)
as the operative first-subset amendment to SIR_SPEC 6 and scheduler path timing.
The [first subset is implemented](../notes/specify-execution.md); its legacy
expectation migration was separately approved. This does not change P1 gates or
close the edition check recorded in section 9, and further specify coverage remains.

## Approved state-dependent scalar specify contract (2026-11-03)

The maintainer approved [state-dependent scalar specify sections 1–8](../notes/specify-state-dependent-proposal.md)
as an operative amendment to the first specify contract and SIR_SPEC 6.
The [simple conditional/fallback subset is implemented](../notes/specify-state-execution.md).
The exact two-case expectation migration was separately approved and committed.
P1 gates are unchanged; the directly unconfirmed 2023 edition check in section 9
remains recorded.

## Approved FST waveform contract (2026-11-03)

The maintainer approved [FST contract sections 1–8](../notes/fst-p1-proposal.md) as the operative amendment to WAVEFORM 1–7 and CLI 2.2. The prepared libfst dependency is reused. [Native execution and public CLI checks](../notes/fst-execution.md) now pass, with the existing one-case expectation migration separately approved and committed. P1 gates and the optional `wire real` exception are unchanged.
