# VPI value formats selected from the declared object

[日本語](vpi-object-value-formats.ja.md) · 2026-11-03.

Within the approved [VPI contract 3–5](vpi-p1-proposal.md), `vpiObjTypeVal`
now selects scalar, integer, real or vector output from the reflected declaration
(IEEE 1800-2023 38.22; release results use the same conversion under 38.34).
Previously every non-real object selected vector output. Ordinary reads,
value-change callbacks and variable release now share `Vpi::Impl::convert`.
Explicitly requested formats retain their existing conversions and lifetimes.

Slang's canonical scalar type is retained as optional static reflection metadata
in SIR, verified against one-bit integral storage and forwarded to the VPI host.
A one-bit packed declaration remains a vector. `vpiScalar`/`vpiVector` for these
declarations and packed-bit handles use that metadata rather than a width test.
SIR text distinguishes `scalar`, `integer` and `vector` reflection rows. This is
an implementation of the existing approved typed-reflection contract, not a
final SIR v1 grammar. The published C VPI ABI and dependency pins are unchanged.
The kernel header remains C++20, and the kernel has no slang/SIR dependency.

The host's integer object kind selects `vpiIntVal`, real storage `vpiRealVal`,
declared scalar/packed-bit handles `vpiScalarVal`, and the remaining supported
packed declarations `vpiVectorVal`. X/Z, signed integer conversion, output-buffer
ownership and callback snapshots remain in the common conversion. Explicit
format reads avoid an additional metadata lookup. No source is reevaluated to
decide the format.

## Independent coverage

The [original generator](../../tools/gen_vpi_object_format_matrix.py) creates
**87 programs**: 72 variable cases across eight declaration containers and nine
types, plus 15 read-only net cases. The independent C plugin checks argument and
name reads, variable force/release results and actual value-change callback
payloads. Net probes only read the resolved value; they add no net-write mode.
The [before/after record](../measurements/2026-11-03/vpi-object-format-matrix.json)
retains every source hash and outcome: native **50/87 → 87/87**.

Six added C++ tests cover declared scalar versus one-bit vector, signed values,
all four scalar states, explicit-format preservation, real/wide values, nested
callback reads, packed indexing, frontend metadata and rejection of malformed
reflection. [Six compiled semantic controls](../measurements/2026-11-03/vpi-object-format-controls.json)
are caught after successful builds, with restored source hashes and passing
tests. A preliminary control with an invalid C++ qualification failed to build
and was not counted; its corrected version was rebuilt and caught semantically.

The Icarus 13.0 comparison retains **17/87 exact matches**. In 64 cases its
integral callback reports `value format 12 not supported`; other observations
include one-bit-vector/integer format differences and package lookup. These are
preserved as differences, not relabeled as successful reference tests. The native
oracle follows the standard's declared-object categories and the approved
reflection model. Finite comparison does not establish full VPI parity.

## Scope still remaining

This checkpoint covers the supported whole static numeric declarations and
packed-bit handles. Complete automatic classification of expression-only values,
all selected/aggregate objects and time-specific formats remains work. Net
deposit/whole-net force, further VPI APIs and cocotb integration remain under the
existing boundaries. No test is excluded from P1 on account of these limits;
the `wire real` exception, fixed probes and gate population are unchanged.
The earlier [automatic-format observation](../measurements/2026-11-03/vpi-release-auto-format-observation.json)
is retained as a historical exploration. No existing expectation or source
fixture was changed. Upstream posting remains on hold.

## Final local validation

At `d5a308ab`, `python3 tools/local_ci.py` reported all 21 steps successful:
dev, ASan and UBSan each passed **11,794/11,794**, and release passed
**11,795/11,795**, with no skipped tests. Clang-format, clang-tidy (191
translation units), the public C++20 header build and the remaining checks
passed. Tracked files were held fixed throughout the run and their hashes
were checked afterwards. Untracked local report files caused the script's
dirty-tree label; they were not included in the change and remained untouched.

The branch has no upstream, so the script's commit expectation check did not
run. Both new commits were checked separately with `check_expected_change.py`
and passed. The [validation record](../measurements/2026-11-03/vpi-object-format-validation.json)
retains the tested commit, per-profile counts, timings and those separate checks.
Linux, minimum-compiler and nightly validation were not rerun here. Main
integration and push have not been performed for this checkpoint.
