# 検証件数の記録

[English](VALIDATION_COUNTS.md)

[counts.json](counts.json) に保存した最新の記録を、測定日・母集団ごとに示します。
以下の元の 95 プローブは 2026-10-27 の結果です。後の機能範囲は[現行の対応表](P1_SUPPORT.ja.md)と
[176プローブの範囲記録](measurements/2026-11-01/icarus13-wire-real-optional.json)を参照してください。
これらのデータは言語全体の完成率を表しません。リポジトリの CTest 件数は、外部比較と独立して変わり得ます。
ゲートの規則・版・再現方法は[検証ガイド](VALIDATION.ja.md)にあります。

<!-- counts:begin -->
<!-- tools/readme_counts.py が docs/counts.json から書く。手で直さない。 -->

| 合格条件 | 記録した結果 | 測定時の達成状況 |
| --- | --- | --- |
| [sv-tests](https://github.com/chipsalliance/sv-tests) のシミュレーションテスト（固定コミットの手書きテスト344件）のパス数が、Linux・macOSの両方でIcarus Verilog以上 | Linux：kirimasim 244件、Icarus Verilog 211件。macOS：kirimasim 244件、Icarus Verilog 210件（14.0 devel、04cc493。合格条件で指定されたバージョンを各OSでビルド） | 達成 |
| 独自RTLテスト群（1,466件、対象リスト固定）のパス率90%以上 | 1,456件パス（99.3%）、0件失敗、10件は未対応構文の診断で終了 | 達成 |
| 必須テスト（P4が必要な1件を除く）の全件パス | 全件パス | 達成 |
| 高速な値演算と参照実装（`valsem`）の比較で不一致がないこと：試行回数100倍のナイトリーファジングを7晩連続でパス | 7晩連続でパス（2026-10-27まで） | 達成 |
| 標準実行機能がIcarus Verilog 13.0以上（[P1_SCOPE](spec/P1_SCOPE.ja.md)）。承認済みの任意`wire real`例外を適用 | 過去の95プローブ：P1評価対象94、対象外1件（`real_net`）。参照が通る生の差は6/6件、必須範囲の差は5/5件、参照不一致は4/7件（13.0 stable/14.0 devel）。観測結果は不変で、有限プローブだけでは全機能の網羅を示さない | 未達 |

kirimasim の外部 sv-tests は Linux が 2026-10-26、macOS が 2026-10-27、母集団は 2026-10-27、Icarus のプローブは 2026-10-27 に測ったもので、右の 2 列の Icarus Verilog と Verilator の数は 2026-10-17 に、その日の main からビルドして測ったものです。nightly の実行が sv-tests と母集団を測り直し、これと違えば失敗します。

| テスト群 | テスト数 | kirimasim | Icarus Verilog | Verilator |
| --- | --- | --- | --- | --- |
| このリポジトリのテスト（CTest、dev の構成） | 11,354 | 11,354件パス、0件スキップ | — | — |
| [sv-tests](https://github.com/chipsalliance/sv-tests) のシミュレーションのテスト | 344 | 244 | 213 | 321 |
| 独自テストスイートのP1評価対象テスト群 | 1,466 | 1,456 | 1,274 | 1,329 |

テスト種別ごとの件数：ユニットテスト（GoogleTest）1,126、`tests/suite/` 9,948（`tools/` の174個のテストジェネレータで生成）、ツール 193、`tests/dump_sir/` 29、状態とContext 21、コマンドラインとイベントカウント 37。

P1対象外：`real_net` — Icarusのwire real拡張への依存を確認。P1では任意。方針確認日2026-11-01。過去の観測の範囲分類だけを見直し、再測定やPASSへの変更はしていません。
<!-- counts:end -->

`python3 tools/readme_counts.py update` で、両 README と本書の両言語版を同期します。
実際に再測定した入力だけを渡し、外部の過去の件数を現在のビルド件数で置き換えないでください。
`check` は 4 文書すべてを検証します。[旧 README](history/README_2026-11-01.ja.md)には、
文書整理前の表をそのまま保存しています。
