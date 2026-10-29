# automatic 変数の fork 捕捉：過去の未対応期待値の移行

[English](automatic-capture-expectation-migration.md) が正。状態：2026-10-25 に提案。
**同日に承認され、下のとおり、それだけのコミットで適用した**。

## 承認を求めること

以下の期待値移行を、実装と別のコミットで行うことの承認を求める。
[共有ストレージの承認済み契約](automatic-capture-proposal.ja.md) は、既存期待値の
変更の承認を含まない。根拠は IEEE 1800-2023 6.21（automatic スコープの寿命）、
9.3.2（並列ブロックと ref の制約）、13.5.2（引数と参照）であり、ここでは
自分の言葉で要約する。他シミュレータのソースやテストコードは使っていない。

## 具体的な変更

1. `tests/tools/check_fork_declaration_boundary.py`：既存の join / join_any /
   join_none の3入力と、親での初期化の3確認を残す。位置付き未対応診断・終了2を、
   終了0、標準出力が厳密に `7\n`、エラー診断なし、という期待値に変更する。
   fork 宣言は親で初期化され、その子は同じ変数を保持して読む。説明文も更新する。
2. `tests/cli/run_dynamic_container_tests.py`：automatic task の queue/fork の
   ソースを残し、その未対応診断の期待だけを、正常終了・標準出力なし・エラーなしに
   変更する。他の実行時エラーと暗黙感度の未対応期待値はそのまま厳密に確認する。
3. `tools/gen_unsupported_matrix.py`、`tests/cli/unsupported_matrix.accepted`、
   `tests/cli/unsupported_matrix.stderr`、`tests/cli/run_cli_tests.py`：
   `unsupported_matrix/automatic_in_fork.sv` はバイト単位で変えず、受理される
   過去入力へ分類し、未対応診断1行を除く。新しい
   `unsupported_matrix/automatic_in_fork.expected` に厳密に `5\n` を生成する。
   CLI の確認器は、受理する過去入力の隣に `.expected` があればそれと比較し、
   なければ従来どおり標準出力なしを要求する。他の終了値・出力・診断の比較は
   弱めない。生成ソースは変わらないのでジェネレータの版は19のままとする。

提案するコミットに src/ の変更、既存ソース入力の削除・改名、ユニットの
アサーションの弱化、性能予算の変更は入れない。言語機能の実装を根拠にした
この期待値変更だけの提案であり、他の期待値を変更する包括的承認ではない。

## 移行前の確認結果

作業中の実装は `tools/gen_automatic_capture_matrix.py` が生成する独自186設計で
通った。6種類の記述場所、3種類の join、幅1/7/65/129の2値・4値、関数が戻った後の
捕捉、real/shortreal、string/chandle、広幅固定配列、構造体、queue/動的配列、
ループ再入場と break/continue、孫への転送、output の一度だけのコピー戻し、
join 内の通常 ref、ref static、guard、名前付き disable を含む。
`check_automatic_capture_metadata.py` は、読まれない型付きフィールド全件、空の
字句スコープ、孫への転送を検査する。また、通常 ref の非同期捕捉、ref static に
対する automatic 実引数、const ref への書き込み、automatic event など5境界を拒否する。

最初の dev 全件（4,868件）は、上の旧期待値3テストと、新規テスト追加後の
件数再生成前の README チェックが失敗した。新規186設計と既存 SIR ダンプ29件は
すべて通り、既知のスキップは従来と同じ2件だった。これは作業ツリーの観測であり、
実装全体の検証が完了したという主張ではない。

## 承認後の確認

既存ソース入力を変更後の期待値で実行し、ジェネレータとの一致と、分離した各コミットに
対する check_expected_change.py を確認する。全 local CI、固定 P1 母集団、固定
sv-tests を再実行し、残る機能差と実測結果を報告する。この有限の行列だけで P1 全体や
Icarus 全範囲の対応を達成したとは判断しない。
