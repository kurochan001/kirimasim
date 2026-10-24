# Assignment-pattern renderer in the shared kernel

[日本語](pattern-format-foundation.ja.md). Native implementation `156e65c`,
2026-10-24. This is a kernel API foundation; source-language `%p` is still
unsupported and no comparison gap is claimed closed.

SYSTEM_TASKS 3.9 already fixes the output forms for `%p`/`%0p` (IEEE
1800-2023 21.2.1.6). The new public C++20 `pattern.h` API renders borrowed,
rooted typed value views without depending on slang or SIR. It does not add
an external dependency or change the existing format parser/syscalls.

## Values and traversal

`PatternValue` represents integral/real/string/enum leaves, records, arrays,
associative entries, unions, tagged payloads and printable handles. The caller
supplies source field/enumerator/tag spelling, declaration order, associative
key order and Context-assigned handle display ordinals. Reserved/escaped
identifiers carry explicit spelling metadata from front; the kernel neither
runs a lexer nor selects source types. A handle is a scalar leaf; it never
causes object traversal or exposes a native pointer/token.

Records have member names; positional mode removes names recursively. Arrays
use positional elements; associative entries retain keys in the supplied
language order. Unions show their first member; tagged values show tag/payload.
Enum matching compares all meaningful bits including X/Z across two-/four-state
storage, ignoring padding; unmatched values use their declared integer type.
Numbers reuse the existing shared decimal/real formatter. Strings produce
quoted SV literals. The escaper uses SV's named escapes and three-digit octal
for other bytes: C's `\b`/`\r` are not SV backspace/carriage-return escapes
(5.9), so they must not be emitted for those bytes.

An explicit work stack handles nesting; active-path membership rejects cyclic
aggregate views but permits repeated acyclic subvalues. There is no 1,024-byte
truncation. Invalid shapes, cycles and allocation failures return distinct
statuses with no partial output. Scratch allocations use the caller's memory
resource; returned text owns its storage independently. This renderer borrows
values and creates no heap roots or mutable global state.

This API does not establish a layout for an unimplemented source type. In
particular, associative ordering is a caller precondition, tagged values need
a supplied payload view, and handle ordinals are supplied metadata. Actual
class/event identities, typed SIR formatting metadata, front lowering, deferred
capture and runtime-format argument transport still need integration before
source support is enabled. The pending class and event contracts remain pending.

## Independent checks

Fourteen new unit tests check nested/positional records, escaped identifiers,
first-member unions, selected tagged payloads, ordered associative views,
null/ordinal handles, malformed views and resource cleanup. Enum lookup has
112 combinations at seven widths through 257 bits, compared with valsem case
equality rather than the formatter's own match helper. A separate SV-literal
reader round-trips all 256 byte values lexically, including octal-boundary
cases; this is not a claim that an SV string retains NUL at runtime (7.6).
The renderer also handles 20,000 nested arrays and output exceeding 16 KiB.
These are kernel/API checks, not proof of source-type support. Exact text is
checked in unit tests against the approved project form; no source-suite
expectation adopts reference-dependent `%p` spacing.

[Six compiled controls](../measurements/2026-10-24/pattern-format-negative-controls.json)
record build status and exact failing tests: dropped enum upper words, ignored
X/Z, positional names, extra union members, retained completed paths and C
backspace escaping. Every counted control must compile, pass all 22 older
Format tests, and fail Pattern assertions; anchor/judging self-checks are not
evidence. Restored sources rebuild and pass all 36 selected tests.

## Remaining work

Integrate immutable type/field/enum metadata with typed SIR, verifier/text and
front selection, then wire both constant and runtime formatting through this
shared API. Preserve evaluation order, snapshots, container roots, postponed
reports and copyback; support must work in all existing source contexts.
Retire obsolete `%p` rejection expectations in a separate review branch when
that source support is implemented. Keep their original source bodies intact.
Full P1 coverage, source `%p` execution and new Linux results remain unconfirmed.

## Full local validation

On 2026-10-24 all 21 local-CI checks pass with native implementation
`156e65c` plus the strengthened enum, control-registry and count follow-up.
Dev/ASan/UBSan each have 3,329 passes and two known skips out of 3,331;
release has 3,330 passes and the same skips out of 3,332. All 318 unit and
136 Tools tests, C++20 public headers, release flags, banned features, SPDX,
suite files, global state, expectation protection, README counts and LLVM
22.1.8 format/tidy pass. All 513 registered mutations apply. The JSON above
records source/unit/registry hashes and per-check results. Existing source
suite bodies, expected outputs and performance counts are unchanged. External
pass rates retain 2026-10-22 provenance; performance retains 2026-10-23 provenance.
