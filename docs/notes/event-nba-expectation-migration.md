# Migrate historical event-NBA rejection expectations

[日本語](event-nba-expectation-migration.ja.md). Status: **approved and applied
(2026-10-27)** in expectation-only commit `16c033a4`. The user approved the exact
three-file patch. Every historical source byte is unchanged; no `src/` change
or push is included in that commit. The original proposal below is retained
as the decision record; its approval requests and future tense describe the
state before approval. Implementation/integration validation remains in progress.

## Reason and evidence

IEEE 1800-2023 9.4.5, 10.4.2 and the already approved
[event-NBA contract](event-nba-proposal.md) permit the intra-assignment event
control without suspending the issuer. The local source lowering uses the
main typed SIR/VM checkpoint `6f43e387` and the shared kernel. New original
source matrices contain 1,018 designs, all passing direct execution. They cover
10 source containers, 11 write shapes, eight controls, capture/count order,
full-width counts, fresh callback work, origin RNG/depth, return/disable,
invalid destinations, pending order and program completion. These tests do not
establish complete event-NBA or P1/Icarus parity. Reference disagreements,
including the project's deliberate OR/order/RNG choices, are retained in
[the source comparison](../measurements/2026-10-27/nba-front-source-comparison.json).

The unchanged `tests/cli/unsupported_matrix/nonblocking_event.sv` contains six
assignments: initial, always, static task, automatic task, fork branch and
generate. It now exits 0 with empty stdout and informational stderr only.
No trigger fires; pending controls do not prevent quiescence. The first full
dev run has only two failed tests: `cli/options` for the obsolete six rejection
lines, and `tools/readme_counts` awaiting regenerated counts. The new source
matrix, unit tests, dumps and PerfCount pass. Further fault and sanitizer checks
remain independent work; their final records will qualify the implementation.

## Exact requested change

Apply only the [reviewable patch](event-nba-expectation-migration.patch), in an
expectation-only commit separate from `src/` changes, after human approval:

1. Add `nonblocking_event` to the unsupported generator's `ACCEPTED` set.
   Keep generation version 19 and every historical source byte unchanged.
2. Regenerate `tests/cli/unsupported_matrix.accepted`, adding that one name.
3. Regenerate `tests/cli/unsupported_matrix.stderr`, removing only its six
   obsolete diagnostics at 7:5, 10:5, 13:5, 17:5, 22:7 and 27:7.
4. Keep the CLI runner unchanged: acceptance requires exit 0, empty stdout
   and no non-informational diagnostics. Keep every other expected line and
   every source fixture, skip and PerfCount budget unchanged.
5. Run the complete CLI, generator and regression gates after migration.

The [AI policy](../spec/AI_POLICY.md#rules-that-protect-expected-values) says:
“A change of an expected value states its reason and the clause of the
standard, and a human approves it.” This requests only the historical
expectation migration. It does not approve new semantics, relax strict slang
errors, add dependencies, authorize a push, or declare P1 complete.
