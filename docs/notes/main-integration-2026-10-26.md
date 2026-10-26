# Main integration of passive switches and SSA event regions

[日本語](main-integration-2026-10-26.ja.md). Status: **earlier main integration
checkpoint (2026-10-26)**, published as `1c970f5d`. The subsequent automatic-event
publication is linked below.
[Full local CI](../measurements/2026-10-26/main-integration-validation.json) passes
all 21 checks on `3ab8a7661eb6`; the final validation-document commit changes no
runtime/test bytes and receives its own expectation-separation and doc checks.


**Later main publication:** [automatic events and call transfers](automatic-event-main-integration.md)
were subsequently published as main `304e1aec`, with newer counts and hosted
validation. The integration and pending-implementation statements below retain
this earlier `1c970f5d` checkpoint's scope and evidence.

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

The 16 focused CLI/tool checks pass after migration. Full local CI passes all
21 checks, including every configure/build/test, format/tidy, policy,
expected-change and generated-count check:

- dev: 5,541 passed out of 5,543; zero failed, two known skips.
- release: 5,542 passed out of 5,544; zero failed, two known skips.
- ASan and UBSan: each 5,541 passed out of 5,543; zero failed, two known skips.
- All 113 tracked C++ units pass clang-tidy; release flag checks cover 113 units.
- All 14 unpushed commits pass the expectation guard individually. The final
  validation-document commit is checked separately.

The known skips are `suite/cb/input_sample_1step` and
`suite/nets/net_port_drivers_part`. The validation JSON records exact runtime
source hashes, versions and local CI steps; source and expectation bytes remain
unchanged after the validated head. Historical checkpoint JSON files preserve
their original preapproval/unmerged state, not the current integration status.
Linux, minimum-compiler and hosted GitHub Actions results are not claimed here.
Population, sv-tests, probe and five-simulator speed results are earlier
measurements, not refreshed by this publication.

P1 remains incomplete. The finite probes and population pass rate do not
establish feature parity; no test input, budget, dependency or runtime meaning
is changed by this integration beyond the approved expectation migration.
