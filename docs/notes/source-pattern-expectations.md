# Migration of obsolete source pattern-format rejections

[日本語](source-pattern-expectations.ja.md)

This expectation-only change follows the already approved SYSTEM_TASKS 3.9
and IEEE 1800-2023 21.2.1.6. It contains no runtime implementation.

The original `tests/cli/unsupported_matrix/display_p.sv` remains byte for byte
unchanged. Its eight former positioned `%p` rejection diagnostics become
successful compilation. Seven `$display` calls execute; its event-controlled
always body is compiled but not triggered. The input moves to `ACCEPTED` and
the CLI checker requires seven lines, each containing the scalar value 5,
without freezing `%p` field spaces. No `%p` stdout golden file is added.
All other rejection diagnostics and accepted-input checks remain in force.

The two original runtime-format inputs, one each for `$sformatf` and
`$sformat`, likewise keep their generated source bodies. Their `%p` conversion
of 7 now succeeds, executes the continuation and leaves the final result 7.
Parse and SIR checks remain required. The positive check rejects warnings,
errors and internal errors and ignores only spaces around that scalar.
The other invalid-format, X/Z-format, argument-count, empty-argument,
evaluation-order and destination-copyback checks are retained: 26 cases.

The migration is kept on `fix/p1-pattern-expectations`, separate from source
implementation. Both existing CLI runners pass with the new native executable;
the generator's `--check` also passes. No suite source, existing performance
budget or unit assertion is weakened. This is not a P1-completion claim.
