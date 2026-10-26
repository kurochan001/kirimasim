# Class execution investigation

[日本語](class-execution-investigation.ja.md). Measured 2026-10-24; native
source `52652eb`, repository context `3b2560f`, macOS/arm64 dev build.

Class object execution remains unfinished P1 work. The
[46 original programs and per-engine observations](../measurements/2026-10-24/class-design-observations.json)
record a separate exploratory cohort. They do not expand the established
87-probe comparison, certify class support, or approve new runtime semantics.
No class implementation or existing test expectation changes accompany this record.

**2026-10-27 follow-up:** [non-parameterized static storage](class-static-execution.md)
fixes eager initialization and adds positioned generic-static boundaries. The
observations below retain their original date and pre-fix source.

## What ran

The compiler/runtime versions are Icarus `13.0 (stable) (v13_0)` and
`14.0 (devel) (s20260301-411-gca6b8d376)`. Each program was compiled and run
with each reference and parsed/run separately with native kirimasim, with a
10-second bound per subprocess. The JSON contains each original source,
SHA-256, proposed output where applicable, clause references, commands,
exit status and observations. Backend assertion expressions are omitted;
temporary artifact paths are normalized. No other simulator source or tests
were read and no tools or dependencies were installed.

Native matches one proposed output, reports 42 unsupported forms, differs
on one output and rejects two legality controls. Icarus 13.0 matches ten
proposed outputs, rejects 28 at compilation, differs on three, has three
runtime failures and two boundary observations without a proposed oracle.
Icarus 14.0 matches twelve, rejects 25 at compilation, differs on four,
has three runtime failures and the same two boundary observations.
These are observations against proposed models, not normative pass rates.

## Confirmed implementation boundaries

A property-free static function called through `Helper::value(5)` returns
7 in native execution. Both reference versions reject this source. This
isolated result does not establish general class or static-member support.

An uninstantiated class with `static int value=history::mark()` exposes a
native initialization omission: `mark()` increments a package counter,
but native prints 0 while both references print 1. Strict native parsing
succeeds and execution exits 0. Consequently, the earlier blanket claim
that every unsupported form produces a diagnostic was too strong. Treat
class static initialization as a known defect; successful execution alone
does not establish support. IEEE 1800-2023 6.21 and 8.9 are relevant to its
static lifetime and shared property initialization. A complete initialization
contract, including unused classes and parameterized specializations, still
needs review before class runtime implementation.

Constructor arguments, field hiding, nonvirtual calls, instance const
properties, output copyback and the implicit `always_comb` class-reference
exclusion have model-matching reference observations but remain native gaps.
The constructor-stage probes match development Icarus while 13.0 rejects
them. For virtual dispatch, both references print `3 9` against the proposed
`9 9`; native rejects execution. Preserve the virtual-dispatch requirement
and review it against clause 8.20 rather than copying that reference result.

Null-instance calls and copying a null source are boundary observations,
without approved expected outputs. `$cast` runtime failures and a backend
assertion for a property NBA are preserved as failed reference execution,
not used as simulation oracles. Legality controls for writing an initialized
const property and constructing an abstract class are rejected by all three.

## Remaining contract and verification work

The required P1 class subset needs typed object identity and roots,
constructor stages, static property initialization, inheritance and dispatch,
parameterized layouts, shallow copying, null/cast behavior, method receivers
across suspension, property references/copyback/NBAs, event dependencies,
and object RNG state. Relevant standard areas include 8.4, 8.7, 8.9–8.12,
8.19–8.21, 9.2.2.2.1 and 18.14. Existing SIR/runtime outlines do not specify
all of these details. This investigation is not a detailed amendment or
permission to implement new choices.

Full class coverage, Linux observations and agreement for untested
combinations remain unconfirmed. Verify them with a reviewed detailed
contract, independent kernel/SIR checks and original source matrices covering
these mechanisms and their interactions. See the [current P1 status](p1-icarus-progress.md).

## Documentation-update verification

On 2026-10-24, `tools/local_ci.py` completed all four configure/build/test
sets: dev/ASan/UBSan each have 3,315 passes and two known skips out of
3,317; release has 3,316 passes and the same skips out of 3,318. Release
flags, banned features, SPDX, suite files, expectation protection and README
counts pass. All 46 source hashes, observation totals, model-matched output
bytes and changed-document file links were checked.

The default installed LLVM 23.1.4 fails clang-tidy on additional diagnostics
in existing C++ code. Static checks were rerun with installed LLVM 22.1.8,
matching the version selected by `.github/workflows/ci.yml`; banned features,
SPDX, suite files, global-state checks, expectation protection, clang-format
and clang-tidy all pass. This was a lint-only rerun (`--presets ''` with
`--llvm-bin`), not a second four-configuration test run. Its existing matching
Z3 library was selected for that process; no installation or system setting
changed. LLVM 23 cleanliness remains unresolved.

A subsequent [detailed runtime proposal](class-runtime-proposal.md) was approved on 2026-10-25, with implementation remaining work, and records fourteen additional original boundary observations.
