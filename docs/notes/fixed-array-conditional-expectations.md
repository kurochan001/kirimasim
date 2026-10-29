# Fixed-array conditional expectation correction

Status: **approved (2026-10-29)**. The user explicitly approved the
expectation correction in [PR #107](https://github.com/kurochan001/kirimasim/pull/107)
for [Issue #60](https://github.com/kurochan001/kirimasim/issues/60).
This expectation-only PR remains separate from the implementation, as
required by [AI_POLICY](../spec/AI_POLICY.md#rules-that-protect-expected-values).

IEEE 1800-2023 11.4.11 combines unpacked arrays with an unknown condition by
complete elements. An element whose equality is not definitely true takes
its Table 7-1 type default (7.4.5). For a differing logic vector, that means
all bits are X, including bits that happen to match between the operands.
It does not use the bitwise merge rule for a scalar packed vector.

The correction changes the third output line in four generated port tests:
the value and effect variants at widths 8 and 65. Both element pairs differ
(1 versus 7 and 2 versus 9), so both resulting elements must be all X.
The first two output lines for known conditions are unchanged. The value
variants retain call counts 0, 0 and 0; the effect variants retain 1, 2 and 4.

The four source fixtures only add 11.4.11 to their `:lrm:` headers. Their
test bodies are byte-identical. The generator reproduces those headers and
the corrected element defaults; all other generated cases are unchanged.
No tests are removed, renamed, skipped or relaxed, and the PR contains no
`src/` or performance expectation changes.

`tools/gen_fixed_array_port_matrix.py --check` verifies all 358 generated
cases. The companion implementation also has new regressions for complete
elements, nested aggregates, member defaults and operand evaluation.
