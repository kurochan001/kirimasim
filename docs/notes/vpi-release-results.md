# VPI release result values

[日本語](vpi-release-results.ja.md)

**Later net-write amendment (2026-11-04):** [whole-net execution](vpi-net-write-execution.md) adds the supported immediate net operations. The dated results and initial boundaries below describe this earlier checkpoint.

`vpi_put_value(..., vpiReleaseFlag)` now fills a non-null `value_p` with the
object's value after release (IEEE 1800-2023 38.34). Previously the force was
released, but the caller's output storage retained its old contents. The
[original baseline probe](../measurements/2026-11-03/vpi-release-baseline.json)
shows Icarus Verilog 13.0 returning one while native returned zero, even though
both variables held one. Process exit zero alone did not establish API success.

This corrects the already [approved variable-write contract, sections 3–5](vpi-p1-proposal.md).
`vpi_put_value` first completes the existing typed write/release. Only a
successful release with non-null output then reads the live numeric binding
and uses the same conversion as `vpi_get_value`. Context-owned vector/string
buffers retain their existing lifetime. Failed or rejected writes do not
rewrite output; the existing null-output release behavior is preserved.
No ABI layout, dependency, variable-force or scheduler-region policy changes.

## Checks and remaining boundaries

[The original generator](../../tools/gen_vpi_release_matrix.py) supplies 96
programs: eight containers, six widths (1–129) and bit/logic storage. Its
[independent C plugin](../../tests/probes/vpi/release_result.c) checks scalar,
integer, vector, binary, hexadecimal and real output: 576 release operations.
Four new unit tests also cover ten explicit formats, unknown/high bits,
Context-owned returned storage, real variables and unchanged output on rejection.
The [four compiled controls](../measurements/2026-11-03/vpi-release-controls.json)
are all caught; restored tests pass. Existing expectations are unchanged.

An earlier exploration included automatic `vpiObjTypeVal` selection. Its
16 one-bit cases select different formats in the engines. The
[original exploration](../measurements/2026-11-03/vpi-release-auto-format-observation.json)
is retained separately; classification of scalar versus one-bit-vector
reflection is unconfirmed and needs a dedicated 38.22 comparison. This patch
reuses the existing converter and does not certify automatic-format parity.

[Three additional net-write probes](../measurements/2026-11-03/vpi-net-write-followup.json)
confirm separate Icarus-supported gaps: net deposit, whole-net force/release,
and passive-network force/release. Native reports VPI errors for them. Those
modes remain subsequent integration work under approved contract 4 and require
a detailed amendment before implementation. Their unsupported API result is
not counted as a pass despite process exit zero. The frozen 176-probe gate
and fixed P1 population are not remeasured here. P1 remains incomplete; slang
upstream posting remains on hold.

[The explicit-format comparison](../measurements/2026-11-03/vpi-release-explicit-reference.json)
passes 96/96 original programs on both native and Icarus Verilog 13.0.

The [complete final local CI](../measurements/2026-11-03/vpi-release-validation.json)
on `cc3e344f` passes all 21 checks: dev/ASan/UBSan each 11,785 and release
11,786, without failures or skips; clang-tidy checks 191 files. This includes
both the class task-cast implementation and the VPI correction.
