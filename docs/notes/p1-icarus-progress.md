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
seven of the sixteen original probes pass Icarus and fail kirimasim. Six
are standard mechanisms: dynamic arrays, queues, class objects,
MOS switches, bidirectional switches, specify paths. The seventh, `wire real`, is an
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

## Expanded mechanism inventory

`tools/icarus_feature_probe.py --extended` retains the original 16-case
seed and adds 22 original probes (38 total). `--only <name>` selects an
individual probe. The measured development executables still identify as
Icarus `14.0 (devel) (s20260301-411-gca6b8d376)`, not exact 13.0.

```sh
python3 tools/icarus_feature_probe.py \
  --kirimasim build/dev/src/driver/kirimasim --extended \
  --output build/p1-icarus-extended.json --require-parity
```

25 probes pass Icarus and fail kirimasim after program execution was added. They include subcases of the
existing gaps plus event-controlled NBAs, delayed NBAs with runtime array
indices, nested packed selection targets and strengths across a bit-select
port connection. This count is of probes, not distinct language features;
adding probes can increase it without regressing the implementation.

Net declaration assignments with delay pass both. The separate bare-net
delay probe is rejected by both; array-slice assignment is also rejected
by both in this measurement. The class virtual-method probe runs to code 0
in Icarus but prints the base result 3, instead of the derived result 9
specified by this original test (8.20). The report distinguishes normal-exit
result mismatches from native execution failures, and records expected bytes
alongside actual bytes. None of these outcomes establishes complete support
or absence of support for its whole mechanism. No simulator source was read.

Binary-input regression additionally passes dev (1,575 passes, two known
skips). Three deliberate binary scanner mutations (skipping whitespace,
ignoring destination widths, assigning short fields) are each detected by
its four new binary unit tests; the normal sources pass after restoration.

Final binary-input local CI passed all presets (dev, release, ASan, UBSan),
release flags, banned features, SPDX, suite files, global state, unpublished
commit expectation checks, README counts, clang-format and clang-tidy.
The detailed program amendment is now approved and implemented; see the next section.

## Program execution

The program amendment was approved on 2026-10-23 before source changes.
The shared scheduler now owns program initials, dynamic origin, descendant
termination and `$exit`; SIR supplies program instance identities and the
VM supplies root/child registration. Implicit program port assignments use
the program's reactive set. Calls declared elsewhere retain the caller's
set and origin. Natural completion and already confirmed report calls are
kept distinct from newly canceled execution.

The generated matrix has 124 cases: 96 combinations of subroutine scope,
direct/task/fork entry, instance placement and 32/129-bit values, plus 28
lifetime, exit, port, anonymous-program, deferred-call and stress cases.
Seven new scheduler tests and one SIR test cover kernel semantics, context
isolation and record reuse; CLI checks 15 illegal-input combinations and
two program identities. The former rejection fixture keeps its design body
and checks an unordered output multiset in a separate expectation commit.
Existing suite bodies and expected values remain unchanged.

Dev regression passes 1,711 cases: 1,709 pass and two known skips remain.
The 16-case seed inventory has seven Icarus-passing gaps; the extended
38-case inventory has 25. Program VCD scopes remain unimplemented, as do
classes, dynamic containers, clocking and concurrent assertions. This is
progress toward the full P1 target, not a complete-parity claim.

Program local CI passed dev/release/ASan/UBSan configure, build and tests,
release flags, banned features, SPDX, suite files, global state, all six
unpublished commit expectation checks, README counts, clang-format and
clang-tidy. Four deliberate mutations (module-like program identity,
Active initial execution, lost fork origin and unended descendants) each
fail the program-specific tests; the restored implementation passes.

The next [dynamic-container amendment](dynamic-containers-proposal.md)
fills the missing heap/instruction contract, including stable outdated
reference cells and tracing roots. It is proposed, not approved or implemented.
