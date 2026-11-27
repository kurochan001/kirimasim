# Nightly follow-up, 2026-11-28

[日本語](nightly-p1-combat-followup.ja.md) · [Nightly run 45924320526](https://github.com/kurochan001/kirimasim/actions/runs/45924320526). The failing run was on b1ad0cb9. The reproductions used a07f2d66. The following two main commits changed specifications and measurement documents.

- **p1-gates.** Ubuntu 24.04 arm64, GCC 13.3, pinned slang e81bcd25, sv-tests 15490e95fd97, population 41085fe3432b. The Linux sv-tests result is 256/344, previously 253. The population reproduced 1,466/1,466. The recorded Linux Icarus result is 211 and was not remeasured. The measurement on November 28 JST was November 27 UTC. The prescribed updater changed the Linux kirimasim count and date and synchronized the generated records. [Per-test results for replay](../measurements/2026-11-28/linux-svtests-count-replay.json)
- **mutants 19/32.** The mutation that skips visiting the operands of a pure system function survived 14,038 CTest entries. The regression evaluates `$clog2(n++)` under `&&` and `||`. Check both the expression result and n, starting at 4. The unmodified binary produced 4, 5, 4, 5 for AND skipped/taken and OR skipped/taken. The mutant produced 5, 5, 5, 5. The new case has its own expected output. [Regression](../../tests/suite/values/short_circuit_pure_operand.sv) · [Expected output](../../tests/suite/values/short_circuit_pure_operand.expected)
- **slang-main, for the next pin review.** Pinned e81bcd25 passes cli/relaxed_warnings. At the nightly slang revision c441ab91 both string-in-constraint checks fail because the diagnostic disappears for a non-random string subexpression under the updated 1800-2023 rule. At c445fa43 the front end fails to compile against the changed randsequence API (RandSeqProductionSymbol::Rule and getRules). Revisit the front end and diagnostic expectations in the pin review. [Upstream constraint change](https://github.com/MikePopoloski/slang/commit/7bef384e75e8b3aa9d61399948ff3e4dd7ce41af) · [Upstream randsequence change](https://github.com/MikePopoloski/slang/commit/592a0c724397032929e400b9081e5f35c97f9fea)

```sh
python3 tools/readme_counts.py update --os linux --date 2026-11-27 \
  --svtests docs/measurements/2026-11-28/linux-svtests-count-replay.json
python3 tools/readme_counts.py check --os linux \
  --svtests docs/measurements/2026-11-28/linux-svtests-count-replay.json
python3 tools/gen_short_circuit_matrix.py --check
ctest --preset dev -R short_circuit_pure_operand --output-on-failure
```
