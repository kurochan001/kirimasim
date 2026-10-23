# P1 expansion toward Icarus coverage

[日本語](p1-icarus-progress.ja.md). Status: in progress, 2026-10-23.

The goal is broader P1 functional support than Icarus Verilog 13.0.
Complete parity is **not confirmed**. The previous pass-rate gates and the
small probes below cannot establish it. The
[scope amendment](p1-icarus-scope-proposal.md) was approved on 2026-10-23
and incorporated into [P1_SCOPE](../spec/P1_SCOPE.md), advancing necessary
P3/P4 and previously out-of-scope work into P1.

Two implementation-and-test rounds have added:

- Unformatted `%u` and `%z` output through the common kernel formatter,
  with arbitrary widths, zero padding, X/Z handling and native word byte
  order. A generated CLI matrix checks 132 programs / 14,256 conversions,
  including file, postponed, packed and string destinations. Three new
  unit tests check raw words, dirty padding and runtime formats. String
  destinations omit NUL; packed and file destinations preserve it.
- Ref-memory readmem/writemem operations, including const-ref writing,
  forwarding, waiting calls, automatic storage and selected rows. The
  63 generated suite cases cover seven shapes in nine homes, both radixes,
  ranges, multidimensional and enum memories, and preservation of adjacent
  rows. Selecting a subarray now completes its remaining coordinates in
  `Lowerer::refOf`, avoiding the invalid partial `ref.make` found by this
  matrix. Existing expectations were preserved.

The original binary and ref-memory tests fail with the pre-change binary
`499198369a07`, respectively on unsupported `%u` and unsupported ref
`$readmemh`; the new implementation passes them. The runnable examples in
both language versions of P1_FEATURES were executed successfully.

Local CI passed on the expanded working tree: dev, release, ASan and
UBSan configure/build/tests, release flags, banned features, SPDX, suite
files, global state, README counts, clang-format and clang-tidy. Dev has
1,572 tests: 1,570 pass and the two previously allowed skips remain.
Existing test changes only add assertions and CMake registrations.
New Python SPDX headers and new/changed document links were also checked.
Publication checks additionally validate each unpublished commit for
changes to existing expectations.

## Reproducing the comparison

The installed comparison pair reports Icarus compiler/runtime
`14.0 (devel) (s20260301-411-gca6b8d376)`. The exact 13.0 release has not
been measured in this round. Only original test inputs and executable
behavior were used; no Icarus source or tests were read or imported.

```sh
python3 tools/icarus_feature_probe.py \
  --kirimasim build/dev/src/driver/kirimasim \
  --output build/p1-icarus-probes.json --require-parity
python3 tests/cli/run_binary_format_tests.py \
  --kirimasim build/dev/src/driver/kirimasim \
  --iverilog iverilog --vvp vvp --container initial \
  --api write --api display --api fwrite --api fdisplay
```

The first command intentionally returns 1 on the current working tree:
eight of the sixteen original probes pass Icarus and fail kirimasim. Seven
are standard mechanisms: dynamic arrays, queues, class objects, programs,
MOS switches, bidirectional switches, specify paths. The eighth, `wire real`, is an
[Icarus extension](https://steveicarus.github.io/iverilog/usage/icarus_verilog_extensions.html);
strict slang correctly rejects it. Supporting it needs an explicit
compatibility policy, not relaxation of the default language checks.

The second command passes four programs / 432 byte-exact conversions in
Icarus. The larger kirimasim-only matrix remains the source of evidence
for the additional containers and string/packed destinations. Icarus
warned and truncated a binary string result at an embedded NUL; kirimasim
follows its approved string-value rule instead. This difference is not
hidden by the comparison tool.

The probes are a seed inventory. VPI, FST, SDF, timing checks, other
Icarus extensions and interactions still need their own original probes.
Both-failing associative-array and Icarus-failing ref-memory probes say
nothing about the full support of those mechanisms. Extend the inventory
as work continues; do not turn a passing seed set into a full parity claim.

The official [13.0 release notes](https://steveicarus.github.io/iverilog/releases/v13-0-release-note.html)
give release context; they are not a complete coverage specification.

## Binary-input preparation

The next continuation checked IEEE 1800-2023 21.3.4.3 and Table 21-7
against the current specifications. Their general whitespace rule and the
choices needed for transfer widths and partial fields need an explicit
[binary-scan amendment](binary-scan-proposal.md), approved on 2026-10-23
together with the phase-scope amendment. The semantic rules are now in
SYSTEM_TASKS 6.3 and SIR_SPEC 8. Binary-input implementation is recorded in the next section;
existing expectations are unchanged.

An original [observation tool](../../tools/binary_scan_probe.py) records
448 cases and distinguishes failed runs from observed behavior. A
nonzero/omitted-width subset completed with 374 observations matching the
approved project rules. Width-zero behavior differs, and grouped runs also
exposed native crashes in two installed Icarus development builds; see
the amendment for the recorded limits. These results guide review rather
than claiming conformance or full parity.

## Binary-input implementation

The third round implements the approved binary scan rules in shared kernel
`readBinary`, with destination widths from SIR. Constant/runtime formats,
file/packed/string inputs and all ten containers use the same semantics.
The CLI matrix checks 40 programs / 17,120 conversions, two continuation
programs and 40 rejected formats/types. Four added unit tests cover raw
bytes, every byte of short fields, widths, masks, suppression and type rules.
Existing suite bodies and expected output remain unchanged.

```sh
python3 tools/binary_scan_probe.py \
  --kirimasim build/dev/src/driver/kirimasim --engine kirimasim \
  --require-kirimasim --output build/binary-scan-all-implemented.json
```

All 448 project-rule observations match. The nonzero-width reference subset
also matches in both tools (374/374 each). The initial feature probe now has
eight Icarus-passing gaps: seven standard mechanisms (dynamic arrays,
queues, classes, programs, MOS, bidirectional switches and specify paths)
and the `wire real` extension. Full parity is still unconfirmed. The next
[program execution amendment](program-execution-proposal.md) addresses a
conflicting `$exit` description and missing execution metadata.
