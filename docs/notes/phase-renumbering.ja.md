# フェーズ番号の対応（2026-10-24）

状態：**承認済みのフェーズ構成変更（2026-10-24）**。英語版が正本です。

2026-10-24に旧P2→P3・旧P3→P4・旧P4→P5と付け替え、新しいP2を設けました。P1の範囲・合格条件とtier番号は変えません。P2の技術的な詳細は[提案中](sir-v1-proposal.ja.md)で、承認には含みません。

| 変更前 | 変更後 | 内容 |
| --- | --- | --- |
| P1 | P1 | Icarusに必要な仕組みを含む既存のtier 1範囲 |
| — | P2 | SIR v1とカーネル境界の確定 |
| P2 | P3 | tier 2：C++へのAOT生成 |
| P3 | P4 | P1で必要な範囲を超えるテストベンチ機能 |
| P4 | P5 | P1で必要な範囲を超えるスイッチレベル、並列化、組み込み |

tier 1＝VM、tier 2＝生成C++、tier 3＝将来のJIT候補です。 フェーズP3でtier 2を実装します。tier 3の採否はP3後に判断します。

## 棚卸しと保存箇所

基準は`c0388da`です。ASCIIの識別子境界でP2/P3/P4を全追跡テキストから検索し、日本語に接する表記も含めて439行・571出現を分類しました。aは519、bは29、cは22、dは1です。以下の行番号は変更前のものです。同じ行でも履歴と現在の計画は別に扱います。

### b：保存した承認済みの履歴

- `CHANGELOG.md`: 433 — 日付付きの承認記録。
- `docs/notes/binary-scan-proposal.ja.md`: 48 — 日付付きの承認記録。
- `docs/notes/binary-scan-proposal.md`: 66 — 日付付きの承認記録。
- `docs/notes/p1-icarus-progress.ja.md`: 7 — 日付付きの承認記録。
- `docs/notes/p1-icarus-progress.md`: 10 — 日付付きの承認記録。
- `docs/notes/p1-icarus-scope-proposal.ja.md`: 9, 13 — 日付付きの承認記録。
- `docs/notes/p1-icarus-scope-proposal.md`: 10, 16 — 日付付きの承認記録。
- `docs/spec/CLI.ja.md`: 5 — 日付付きの改訂履歴。
- `docs/spec/CLI.md`: 5 — 日付付きの改訂履歴。
- `docs/spec/CODEGEN_SPEC.ja.md`: 5 — 2026-10-15の移動理由の履歴。
- `docs/spec/CODEGEN_SPEC.md`: 5 — 2026-10-15の移動理由の履歴。
- `docs/spec/SCHEDULER_SPEC.ja.md`: 485 — P1へ前倒しした日付付きの記録。
- `docs/spec/SCHEDULER_SPEC.md`: 490 — P1へ前倒しした日付付きの記録。
- `docs/spec/TEST_SUITE.ja.md`: 9, 263 — 日付付き改訂・承認済みP1条件の引用。
- `docs/spec/TEST_SUITE.md`: 10, 264 — 日付付き改訂・承認済みP1条件の引用。
- `docs/spec/UNSPECIFIED_BEHAVIOR.ja.md`: 374 — 日付付きの承認欄。
- `docs/spec/UNSPECIFIED_BEHAVIOR.md`: 374 — 日付付きの承認欄。

### c：保存した識別子・保護対象の行

- `docs/spec/UNSPECIFIED_BEHAVIOR.ja.md`: 5, 331, 332, 333, 334, 335, 336, 372, 395, 427 — フェーズではなく、固定の決定番号。
- `docs/spec/UNSPECIFIED_BEHAVIOR.md`: 5, 331, 332, 333, 334, 335, 336, 372, 395, 427 — フェーズではなく、固定の決定番号。
- `tools/gen_memory_file_matrix.py`: 11 — UNSPECIFIED_BEHAVIORの決定番号P4。

- `tests/suite/skips.txt`: 7 — 元のフェーズ注記を含む保護対象のスキップ行。直前の新コメントで現在の番号を示す。

### d：フェーズ以外

- `docs/review/oracle_audit.md`: 188 — イベント順序の例に登場するプロセスP2。

## 判断した箇所

- 日付付きの改訂文と2026-10-15承認のP1条件の引用は原文を保存します。上の表で読み替えます。日付が添えられた現行の規則は、現在の計画なので番号を付け替えます。
- UNSPECIFIED_BEHAVIORのP2/P3/P4は、先頭列・警告の参照・承認履歴でも固定の決定番号です。フェーズ欄だけを付け替えます。該当番号を含むファイル名・job ID・CTestラベル・C++ symbolは見つかりませんでした。cb/input_sample_1stepの既存行は保存し、直前に現在P4と追記します。期待値保護の検査器が行内コメントの変更もスキップ追加と判定するためです。
- ASSERTION_COVERAGE_SPEC・RANDOMIZE_SPEC・LIB_EXPORT_SPECは元からPriority P2と実装段階が一致していませんでした。機械的にP3へ変更し、優先度と実装段階の整合は別途人が決めます。拡大されたP1範囲と重なる将来コメントもあります。意味や固定母集団は変えません。
- SYSTEM_TASKS 1節は裸の数のフェーズ欄も使っていました。現行欄をP1/P4と明示し、以前の前倒しを記録する日付付きの数字は保存しました。
- READMEの生成範囲はtools/readme_counts.pyから再生成しました。測定件数と日付は変更しません。
- 外部のkirimasim-tests/README.mdにP2/P3/P4はありませんでした。kirimasim-tests-workにはroot READMEがありません。外部ファイルは変更していません。

## 更新したファイル

既存53ファイル（Markdown 44、ソース・道具・テストのコメント9）と本ノートの英日2ファイル、計55ファイルです。

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

## 確認（2026-10-24）

LLVM 22.1.8で`tools/local_ci.py`の全21項目が成功しました。dev・ASan・UBSanは各3,342件（成功3,340、既知のスキップ2）、releaseは3,343件（成功3,341、同じスキップ2）です。releaseフラグ・禁止事項・SPDX・suiteファイル・大域状態・README生成件数・format・tidyも成功しました。期待値保護の項目はupstream未設定で対象なしだったため、コミット後に親との差分を明示した期待値保護検査も成功しました。スキップ一覧の行内コメントだけの変更も追加と判定されたため、既存行は保存し、別コメントで現在の番号を示しました。519個の現計画番号を各1回だけ付け替え、履歴・識別子・無関係・保護対象の52個は保存しました。テスト本体・期待値・スキップ対象・母集団・測定値は変更しません。
