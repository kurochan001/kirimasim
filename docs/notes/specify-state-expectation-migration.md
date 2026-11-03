# State-dependent specify rejection-classification migration

[日本語](specify-state-expectation-migration.ja.md) · 2026-11-03.

**Status: exact two-case migration separately approved on 2026-11-03 and applied.**
The maintainer separately approved this concrete migration after the new execution
results were shown. Approval of the execution contract itself did not authorize it.

Before this migration, two generated inputs in
`tools/gen_specify_matrix.py::rejections()` required a positioned
`state-dependent specify path` unsupported error and exit 2.
The approved simple `if`/`ifnone` implementation executes both unchanged inputs
with **exit 0 and empty stdout**. They contain no display statement. Their
existing source names and bytes must remain unchanged:

- `reject_conditional.sv` — SHA-256 `03f6e2a34acc1a342fce5b6104279f04d04e54ee2263154e9322dc15705a8523`.
- `reject_ifnone.sv` — SHA-256 `135e6f1eec2549edcdd73b67703cd326fde7089db5ed603d387d14280fd444e9`.

Move only these two classifications to the admitted execution list, preserving
these generated filenames (including their historical `reject_` prefix), sources,
`--mintypmax=typ` and normal compile-and-run. The new oracle is exit 0, exact empty
stdout and no error diagnostic. Do not delete a test, substitute its source,
skip execution or loosen other rejection reasons. Retain all other 119 cases:
105 exact timing traces and 14 positioned rejections already pass.

The grounds are IEEE 1800-2017 30.4.4.2/30.4.4.4 as inspected, and the maintainer's
[approved state-dependent contract](specify-state-dependent-proposal.md) sections
1–8. Direct 2023 text confirmation remains unconfirmed as recorded in section 9.
This migration updates an implementation boundary; it does not redefine language
semantics or P1 coverage. The separate original 40-case matrix now passes native
oracles, including the X/Z cases that differ from the reference. Broader source,
SIR and kernel verification continues before integration.

The two-case migration is recorded in a commit containing no `src/` changes.
All 121 generated source names and byte sequences are unchanged; exactly the two
approved classifications differ. The complete 121-case native timing/boundary
runner passes after migration. The expected-change guard is checked separately. No other input, performance oracle, external population or gate
denominator is part of this request.
