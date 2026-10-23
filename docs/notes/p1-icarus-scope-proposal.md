# P1 scope proposal: Icarus Verilog coverage

Status: **approved (2026-10-23)**. Incorporated into
[P1_SCOPE](../spec/P1_SCOPE.md); implementation remains in progress.

The requested destination is P1 execution support at least as broad as
Icarus Verilog 13.0. This is stronger than the existing pass-rate gates in
TEST_SUITE.md. Passing those gates does not establish this destination.

The current specifications defer some relevant mechanisms to P3 or P4.
The approved amendment is:

- Bring standard-language simulation mechanisms that Icarus supports into
  P1 as needed, including dynamic containers, classes, program execution,
  VPI, and switch-level modeling. Preserve their specified semantics and
  the shared kernel architecture. P2 still denotes C++ generation.
- Include specify paths and timing facilities supported by Icarus even
  where the current inventory leaves them outside scope. Their detailed
  semantics need a reviewed amendment before implementation.
- Keep the existing P1 gates, test expectations and strict front-end
  diagnostics. Add a separately measured compatibility inventory; no
  existing expectation is relaxed to obtain parity.
- Judge standard constructs against IEEE 1800 and the approved value and
  scheduling rules. Document implementation-defined differences rather
  than reproducing another simulator's unspecified choices or defects.
- Inventory Icarus-specific extensions and external interfaces separately.
  An extension that conflicts with strict language acceptance needs its
  own documented compatibility option and semantic proposal before code.
- Keep the requirement for approval of new dependencies and installations.
  This proposal does not select or authorize a dependency.

Affected specifications include SYSTEM_TASKS.md (phase table),
SIR_SPEC.md (dynamic storage), SCHEDULER_SPEC.md (program/process execution),
VPI_SPEC.md and WAVEFORM.md, together with the phase summaries in README
and P1_SUPPORT. Detailed amendments must precede each implementation.

Unformatted `%u` and `%z` output is implemented (SYSTEM_TASKS.md 3.11).
The separate [binary-input amendment](binary-scan-proposal.md) was also
approved on 2026-10-23; its implementation remains pending.

The installed comparison executable currently reports
`Icarus Verilog version 14.0 (devel) (s20260301-411-gca6b8d376)`.
This is a newer-development comparison, not evidence from the exact 13.0
release. Exact-13.0 parity and complete feature parity are not confirmed.
Run original, mechanism-based probes with recorded compiler/runtime
versions to establish each item. Do not read or import Icarus source or
tests. The official [13.0 release notes](https://steveicarus.github.io/iverilog/releases/v13-0-release-note.html)
provide context, not an exhaustive feature checklist. The official
[extensions inventory](https://steveicarus.github.io/iverilog/usage/icarus_verilog_extensions.html)
identifies additional compatibility work; it is not permission to copy code.
