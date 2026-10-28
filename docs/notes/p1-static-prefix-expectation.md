# Separate expectation migration for Issue #64

This change contains no implementation change and does not modify the
SystemVerilog fixture. It must be reviewed separately from the implementation
of [Issue #64](https://github.com/kurochan001/kirimasim/issues/64), as required
by the repository's expectation-change policy.

The `always_comb` in `tests/dump_sir/procedures.sv` reads `mem[1][3:0]`.
Its existing SIR golden subscribes to all eight bits of `mem[1]`. IEEE
1800-2023 9.2.2.2.1 and 11.5.3 specify the longest static prefix, so the
subscription must cover only bits 3 through 0 of that element. An update
confined to bits 7 through 4 must not restart the block. The implementation
and the independent simulation regressions establish this distinction.

Only this trigger range changes in the golden. Instructions, write ranges,
process order, source positions and format version remain the same.

The migration alone does not make the old implementation conform: the
implementation change depends on this expectation migration. Keep the two
changes in separate PRs and validate their combined result before integration.
Neither a weakened check nor an implementation that retains the old broad
sensitivity is the proposed resolution.
