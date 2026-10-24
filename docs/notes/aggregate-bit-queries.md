# Bit queries on fixed unpacked aggregates

[日本語](aggregate-bit-queries.ja.md). Implemented 2026-10-25 under the
existing SYSTEM_TASKS 8, VALUE_SEMANTICS 8 and IEEE 1800-2023 20.9 rules.

`$countbits`, `$countones`, `$onehot`, `$onehot0` and `$isunknown` now accept
supported fixed unpacked arrays, records and arrays of records with integral
leaves. Front calls the existing `Lowerer::bitstreamOf` used by streaming
expressions before emitting the existing `$countbits` syscall. The shared
kernel still determines which bits match the controls; the VM needs no new
source-type rules or instruction. Scalar handling uses the same path.

The receiver is evaluated once per query. Its aggregate values are captured
before the control arguments, preserving the established left-to-right
argument order and existing two-state field conversions. Controls retain
their expression evaluation, least-significant-bit conversion and duplicate
matching rules.

The original [generator](../../tools/gen_aggregate_bit_query_matrix.py) adds
136 cases. A separate Python character-count model derives expected counts
from the written bit patterns, including two-state X/Z conversion. Cases
cross 1/5/65/129-bit leaves, two/four states, fixed arrays, records and arrays
of records; initial/task/function/fork execution; supported automatic and
array-ref storage; and functions returning arrays or records. Three phases
check one set bit, many set bits and X/Z mixtures. Side-effecting control
functions run once, repeated controls do not count twice, and high control
bits are ignored. Receiver-call cases check exactly 27 receiver evaluations
for 27 queries. All 136 pass locally.

The [representative comparison record](../measurements/2026-10-25/aggregate-bit-query-observations.json)
contains three original array/record/record-array sources, their hashes and
exact expected output. All pass the new native source and fail its retained
baseline. Icarus 13.0 stable and 14.0 development both reject the array
queries at runtime and the record source forms at compilation. Their
rejections are not treated as passing results or as new coverage totals.

This query adapter does not add a new storage representation. Automatic
arrays of records, ref records and ref arrays of records remain unsupported
P1 work. Dynamic-container/string bit streams and other unimplemented stream
types are not enabled. Original exploratory sources for unavailable storage
were kept in the local build observations rather than asserted as supported.
No existing test body, expected output or PerfCount budget changes.

An omission control restores the old scalar-only argument lowering. It is
registered separately from the monitor callback control. Full functional
coverage and the Icarus-passing gaps still require the remaining mechanisms;
these additional standard forms do not establish P1 completion.

The [two compiled controls](../measurements/2026-10-25/p1-coverage-negative-controls.json)
fail new source/kernel tests, excluding mutation-tool self-checks. Restored
sources pass [all 21 local-CI checks](../measurements/2026-10-25/p1-coverage-validation.json):
3,500 passes/two skips in dev, ASan and UBSan, and 3,501/two in release.
