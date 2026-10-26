# Main integration of passive switches and SSA event regions

[日本語](main-integration-2026-10-26.ja.md). Status: integration validation in
progress, 2026-10-26. Publication was requested by the user. This note will
record the completed local CI before push; no pending check is counted as passed.

## Included changes

- Passive-edge SIR/tier-1 execution and conduction timer observations.
- Source lowering of all six static-terminal passive switch kinds, with atomic
  publication across independent graph groups.
- Typed SSA event-region publication across nested execution and vector relocation.
- The separately approved managed-event instruction-boundary contract; complete
  field/transfer publication and source automatic-event construction remain work.
- Both READMEs' remaining P1 feature inventory and current probe observations.
- `77c9d188`: the separately approved tran expectation migration. Generator
  version 19, every source fixture and all six MOS-boundary inputs are unchanged;
  only the reviewed acceptance/diagnostic expectations change. IEEE 1800-2023
  28.8 and the approved MOS/switch contract 3/6/7 are the grounds.

The local main fast-forwards from `07b7de82`. Existing commits are preserved;
expectation changes remain separate from `src/` changes. The expected-change
guard is checked per unpushed commit, as the push workflow does.

## Validation

The 16 focused CLI/tool checks pass after migration. A full dev run passes
5,541/5,543 tests with zero failures and two known skips. Full local CI is pending.
Results and exact source provenance will be appended when it finishes.
Linux, minimum-compiler and hosted GitHub Actions results are not claimed here.

P1 remains incomplete. The finite probes and population pass rate do not
establish feature parity; no test input, budget, dependency or runtime meaning
is changed by this integration beyond the approved expectation migration.
