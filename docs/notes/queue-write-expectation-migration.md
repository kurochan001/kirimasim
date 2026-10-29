# Invalid queue writes: separate diagnostic expectation correction

Status: **approved (2026-10-29)**. The user explicitly approved this exact
expectation correction for [Issue #67](https://github.com/kurochan001/kirimasim/issues/67).
The expectation-only change remains separate from the implementation, as
required by [AI_POLICY](../spec/AI_POLICY.md#rules-that-protect-expected-values).
The approval also covers replacing the companion unit test's silent-diagnostic
assertion with an exact warning comparison while retaining all value checks.

IEEE 1800-2023 7.10.1 requires a runtime warning when a queue write has an
invalid index. The write leaves the queue unchanged. The existing generated
queue tests attempt one write with a 129-bit index beyond the queue and one
with an X index, but their exact stderr expectations are empty. Their value
checks are correct; their silence requirement is not.

The correction changes 89 existing stderr expectations. It adds two positioned
warnings to each of 80 existing queue cases: eight execution homes, five
element widths and two value domains.
The diagnostic is `write to an invalid queue index is ignored`. Its location
is the beginning of each assignment expression, derived from the source
text after the unchanged header is added. Both expressions occupy separate
columns on the same line and therefore remain distinct warning sites.

The other nine cases are the scalar `$fread` evaluation fixtures, one per
execution home. Their integral destination is `q[99]` in a one-element queue.
The input contains four bytes, so the call attempts copyback and the invalid
queue write requires the same warning under 7.10.1. The value result remains
`invalid 4 17`: four bytes read, with the queue element unchanged. Integral
input is not suppressed by the zero count argument (21.3.4.4). Unlike a
zero-byte read, this call therefore cannot silently omit its write.

Each scalar-input warning is located at the beginning of the `$fread` call,
which owns the generated copyback store. The generator derives that line and
column from the unchanged source. All existing slang compilation warnings
remain byte for byte and in their original order; the one runtime warning
is appended after them.

`tools/gen_dynamic_container_matrix.py` and `tools/gen_scalar_fread_matrix.py`
now generate those diagnostics. Their versions remain 1 and 2 because every
generated SystemVerilog byte is unchanged. All 393 source fixtures and all
393 stdout expectations retain their SHA-256 fingerprints. The other 304
stderr files, including all dynamic-array matrix cases and scalar-input value
cases, are also unchanged. No tests are removed, renamed, skipped or weakened,
and no `src/` or performance expectation is changed in this proposal.

Both generators' normal runs and `--check` succeed, as does `git diff --check`.
Before/after fingerprints are retained in the local
`build/queue-write-expectations/` verification artifacts. The companion
implementation must match these independently derived diagnostics; existing
queue values, legal end insertion and silent reads remain strict checks.
