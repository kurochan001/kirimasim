# Phase renumbering (2026-10-24)

Status: **approved phase-plan decision (2026-10-24)**. [日本語](phase-renumbering.ja.md).

On 2026-10-24, old P2→P3, old P3→P4 and old P4→P5; a new P2 is inserted. P1 scope and exit criteria and tier numbers are unchanged. The new P2 technical contract is [proposed separately](sir-v1-proposal.md), not approved by this renumbering.

| Before | Now | Work |
| --- | --- | --- |
| P1 | P1 | Existing tier 1 scope, including Icarus-required mechanisms |
| — | P2 | SIR v1 and kernel boundary ready for tier 2 |
| P2 | P3 | tier 2: C++ AOT generation |
| P3 | P4 | Broader testbench features, beyond the required P1 subset |
| P4 | P5 | Switch-level, multithreading and embedding beyond the required P1 subset |

tier 1 = VM; tier 2 = generated C++; tier 3 = possible JIT. Phase P3 implements tier 2. The decision whether to adopt tier 3 follows P3.

## Inventory and preserved occurrences

Baseline: `c0388da`. Searching all tracked text with ASCII identifier boundaries (including phase tokens adjacent to Japanese) found 439 lines / 571 tokens: a=519 current plans, b=29 history, c=22 identifiers/protected entries, d=1 unrelated use. Line numbers below refer to that baseline. Mixed lines are classified by token, not by line.

### Preserved approval history / 承認済みの履歴

- `CHANGELOG.md`: 433 — dated approval record.
- `docs/notes/binary-scan-proposal.ja.md`: 48 — dated approval record.
- `docs/notes/binary-scan-proposal.md`: 66 — dated approval record.
- `docs/notes/p1-icarus-progress.ja.md`: 7 — dated approval record.
- `docs/notes/p1-icarus-progress.md`: 10 — dated approval record.
- `docs/notes/p1-icarus-scope-proposal.ja.md`: 9, 13 — dated approval record.
- `docs/notes/p1-icarus-scope-proposal.md`: 10, 16 — dated approval record.
- `docs/spec/CLI.ja.md`: 5 — dated amendment history.
- `docs/spec/CLI.md`: 5 — dated amendment history.
- `docs/spec/CODEGEN_SPEC.ja.md`: 5 — dated explanation of 2026-10-15 move.
- `docs/spec/CODEGEN_SPEC.md`: 5 — dated explanation of 2026-10-15 move.
- `docs/spec/SCHEDULER_SPEC.ja.md`: 485 — dated account of advancement into P1.
- `docs/spec/SCHEDULER_SPEC.md`: 490 — dated account of advancement into P1.
- `docs/spec/TEST_SUITE.ja.md`: 9, 263 — dated amendment / approved quoted P1 condition.
- `docs/spec/TEST_SUITE.md`: 10, 264 — dated amendment / approved quoted P1 condition.
- `docs/spec/UNSPECIFIED_BEHAVIOR.ja.md`: 374 — dated approval field.
- `docs/spec/UNSPECIFIED_BEHAVIOR.md`: 374 — dated approval field.

### Preserved identifiers / 識別子

- `docs/spec/UNSPECIFIED_BEHAVIOR.ja.md`: 5, 331, 332, 333, 334, 335, 336, 372, 395, 427 — stable decision key, not a phase.
- `docs/spec/UNSPECIFIED_BEHAVIOR.md`: 5, 331, 332, 333, 334, 335, 336, 372, 395, 427 — stable decision key, not a phase.
- `tools/gen_memory_file_matrix.py`: 11 — UNSPECIFIED_BEHAVIOR decision key P4.

- `tests/suite/skips.txt`: 7 — protected skip entry, including its original phase annotation; the new adjacent comment supplies the current phase.

### Non-phase / フェーズ以外

- `docs/review/oracle_audit.md`: 188 — process P2 in event-order example.

## Judgment notes / 判断した箇所

- Dated amendment sentences and the quoted 2026-10-15 P1 condition remain verbatim. Read their phase numbers using the mapping above. Operative rules that merely carry an approval date are current plans, so their phase references shift.
- `UNSPECIFIED_BEHAVIOR` keys P2/P3/P4 are stable decision identifiers, including first-column keys, warning references and approval history; phase-column cells shift. No filenames, job IDs, CTest labels or C++ symbols containing a phase token were found. `cb/input_sample_1step` is preserved; its protected entry stays verbatim, and a new preceding comment gives current P4 (the expectation guard treats any replacement entry as an added skip).
- ASSERTION_COVERAGE_SPEC, RANDOMIZE_SPEC and LIB_EXPORT_SPEC had Priority P2 despite different delivery phases in the roadmap. Their priorities become P3 mechanically. Reconciling priority with delivery scope requires a later human decision. Some future-feature comments already overlap the expanded P1 scope; this change does not amend feature semantics or the frozen population.
- SYSTEM_TASKS section 1 also used bare numeric phase cells. These now use explicit P1/P4 labels; dated accounts of earlier advancement remain history.
- README generated counts are regenerated from `tools/readme_counts.py`; measured counts and dates are unchanged.
- External `kirimasim-tests/README.md`: no phase P2/P3/P4 occurrence; `kirimasim-tests-work` has no root README. No external file is changed.

## Updated files / 更新したファイル

53 existing files (44 Markdown documents and 9 source/tool/test-comment files), plus this English/Japanese note pair, 55 files total:

- `CLAUDE.ja.md`
- `CLAUDE.md`
- `README.ja.md`
- `README.md`
- `docs/ARCHITECTURE.ja.md`
- `docs/ARCHITECTURE.md`
- `docs/P1_FEATURES.ja.md`
- `docs/P1_FEATURES.md`
- `docs/P1_SUPPORT.ja.md`
- `docs/P1_SUPPORT.md`
- `docs/notes/p1-icarus-progress.ja.md`
- `docs/notes/p1-icarus-progress.md`
- `docs/spec/ASSERTION_COVERAGE_SPEC.ja.md`
- `docs/spec/ASSERTION_COVERAGE_SPEC.md`
- `docs/spec/CLI.ja.md`
- `docs/spec/CLI.md`
- `docs/spec/CODEGEN_SPEC.ja.md`
- `docs/spec/CODEGEN_SPEC.md`
- `docs/spec/LIB_EXPORT_SPEC.ja.md`
- `docs/spec/LIB_EXPORT_SPEC.md`
- `docs/spec/P1_SCOPE.ja.md`
- `docs/spec/P1_SCOPE.md`
- `docs/spec/PERFORMANCE.ja.md`
- `docs/spec/PERFORMANCE.md`
- `docs/spec/RANDOMIZE_SPEC.ja.md`
- `docs/spec/RANDOMIZE_SPEC.md`
- `docs/spec/RUNTIME_ABI.ja.md`
- `docs/spec/RUNTIME_ABI.md`
- `docs/spec/SCHEDULER_SPEC.ja.md`
- `docs/spec/SCHEDULER_SPEC.md`
- `docs/spec/SIR_SPEC.ja.md`
- `docs/spec/SIR_SPEC.md`
- `docs/spec/SYSTEM_TASKS.ja.md`
- `docs/spec/SYSTEM_TASKS.md`
- `docs/spec/TEST_SUITE.ja.md`
- `docs/spec/TEST_SUITE.md`
- `docs/spec/UNSPECIFIED_BEHAVIOR.ja.md`
- `docs/spec/UNSPECIFIED_BEHAVIOR.md`
- `docs/spec/VALUE_SEMANTICS.ja.md`
- `docs/spec/VALUE_SEMANTICS.md`
- `docs/spec/VPI_SPEC.ja.md`
- `docs/spec/VPI_SPEC.md`
- `docs/spec/WAVEFORM.ja.md`
- `docs/spec/WAVEFORM.md`
- `src/driver/main.cpp`
- `src/front/lower.cpp`
- `src/front/lower_expr.cpp`
- `src/front/lowerer.h`
- `src/kernel/include/kirimasim/kernel/abi.h`
- `src/kernel/include/kirimasim/kernel/strings.h`
- `tests/suite/skips.txt`
- `tools/p1_population.py`
- `tools/readme_counts.py`

## Validation (2026-10-24)

`tools/local_ci.py` passed all 21 steps with LLVM 22.1.8: dev/ASan/UBSan each 3,342 tests (3,340 passed, 2 known skips); release 3,343 (3,341 passed, the same 2 skips). Release flags, banned constructs, SPDX, suite files, global state, generated README counts, formatting and tidy passed. Its expected-change step had no upstream to inspect; the first commit also passed the explicit expected-change check against its parent after committing. The guard rejected an inline-comment-only change in the skip list; that entry is now preserved verbatim with a separate current-phase comment. Phase-token audit: all 519 current-plan tokens mapped exactly once, all 52 history/identifier/non-phase/protected-entry tokens preserved. No test body, expected result, skip membership, population entry or measured number changes.
