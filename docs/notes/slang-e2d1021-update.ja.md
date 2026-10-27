# slangの固定先をe2d1021へ更新

[English](slang-e2d1021-update.md)。状態：人が依頼した依存更新をローカルで検証済み、
2026-10-27。既存の期待値は変更しません。

## 選んだ版と出典

変更せずに使う依存は
[`e2d1021661f78b3f17d1edf7ea17f9d6f95fb168`](https://github.com/MikePopoloski/slang/commit/e2d1021661f78b3f17d1edf7ea17f9d6f95fb168)です。
更新を依頼された時点で確認した上流`master`の先頭で、12.1リリース後の開発コミットです。
以前の固定先`16bd3b920dccfbcde000195f9d38d9b00432c9c8`から24コミット進んでいます。
動くブランチ参照ではなく、完全なSHAで固定します。

一次資料は[上流の比較](https://github.com/MikePopoloski/slang/compare/16bd3b920dccfbcde000195f9d38d9b00432c9c8...e2d1021661f78b3f17d1edf7ea17f9d6f95fb168)、
[選択した版の変更履歴](https://github.com/MikePopoloski/slang/blob/e2d1021661f78b3f17d1edf7ea17f9d6f95fb168/CHANGELOG.md)、
[Issue #2064のメンテナ回答](https://github.com/MikePopoloski/slang/issues/2064)です。
不正なclassメンバーspecifierから回復した後の内部エラーの修正
`a9f9110c276564c13204ded0e760a292e700e600`が、選択した版の祖先に含まれます。
ほかに定数リテラル/選択、構造体パターンの反復、型/名前の解決、ポートの修正があります。
変更履歴には、bitstream型ではない型への`$bits`を厳格に拒否する変更もあります。
SYSTEM_TASKS 8はすでにその拒否を要求しています。

`cmake/Dependencies.cmake`でSHAを指定し、slangは改変しません。既存の設定で、
slang自身のテスト・ツール・インストールは無効のままです。以前からのmimalloc依存は、
測定したdevビルドでは**v3.6.0**、コミット`b18ab4eef383f655e760881b02eff8af02800879`になりました。
新しい最上位ライブラリやツールのインストールは追加しません。夜間ジョブの識別子`slang-main`は維持し、
そのcloneは上流の既定ブランチに従います。実際のブランチ名は`master`です。

## 独立した検証

基準はkirimasim `13fe2b65`と旧固定版です。新しくdevをビルドし、既存**8,138件**は
失敗0・既知のスキップ2件でした（[基準の記録](../measurements/2026-10-27/slang-e2d1021-baseline.json)）。
依存の固定先だけを変えた後も同じ8,138件が通り、既存の期待値・入力・ダンプは維持しました。

新規の独自[class specifier回復検査](../../tests/tools/check_class_specifier_recovery.py)は、
コンパイル単位/module/packageの宣言×function/task/constraintメンバー×正しい1種類と不正な3種類のマーカー×
parse/runの**72通り**です。旧binaryは正しい18通りを受理し、不正な54通りはInternal（終了3）でした。
新binaryは正しい入力を受理し、不正な54通りを位置付きのclass specifier診断とInputError（終了2）にします。
CLI 5の入力エラー境界を検査し、言語の意味や診断の厳しさは変更しません。
[差分記録](../measurements/2026-10-27/slang-e2d1021-recovery.json)に独自入力・結果・凍結した両binaryのhashを保存します。
上流のテストやIssueの再現コードは写さず、他のシミュレータのソースも読みません。
恒久的な回帰検査も、旧binaryでは失敗し、新pinでは72件すべて成功しました
（[対照検査](../measurements/2026-10-27/slang-e2d1021-control.json)）。

[最終検証](../measurements/2026-10-27/slang-e2d1021-validation.json)でlocal CI全21項目が成功しました。
dev/ASan/UBSanは8,137/8,139、releaseは8,138/8,140、失敗0・同じ既知スキップ2件です。
4つのpresetすべてで、選択したSHAのslangソースが未改変であることを確認しました。
既存ダンプ29件とPerfCount予算も変更せずに成功しています。

[旧版/新版の比較](../measurements/2026-10-27/slang-e2d1021-comparison.json)でsv-tests 244/344、
2版の固定95プローブのnativeの状態/標準出力を維持しました。固定P1母集団は両版とも
**1,452/1,466、失敗4件・未対応10件**です。[個別比較](../measurements/2026-10-27/slang-e2d1021-population-differential.json)で
1,466件すべての終了状態と失敗出力が同じことを確認しました。失敗は`ch6/test_string_atoreal`、
`ch6/test_string_atoreal_2`、`ch6/test_string_conversion`、`ch6/test_string_realtoa`で、
負の文字列からrealへの変換が0になる観測があります。kirimasim `13fe2b65`では旧pinでも失敗し、
この依存更新の退行や成功件数には数えません。以前のkirimasimでの母集団測定は当時の記録として維持します。

最初の比較harnessは既存失敗0を仮定し、全計測を保存後に停止しました。
[初回の記録](../measurements/2026-10-27/slang-e2d1021-initial-comparison.json)を保存し、追跡検証では
全ケースを比較して4失敗を明示的に維持します。期待値の変更や失敗0の主張ではありません。
[第三者表示](../../THIRD_PARTY_NOTICES.ja.md)にも選択したslang/mimallocの版を反映し、
共通のMIT本文は両方の公式配布物と一致することを確認しました
（[記録](../measurements/2026-10-27/slang-e2d1021-licenses.json)）。

## 公開前の最新mainとの統合

コミットとpushの依頼後、mainが5コミット先の`563241b9`まで進んでいたため、
依存更新をそのmainへrebaseしました。READMEの生成件数は`tools/readme_counts.py`で
整合させました。この依存更新コミットは、そのmainに対して`src/`・詳細仕様・既存の期待値を変更しません。

[統合後の検証](../measurements/2026-10-27/slang-e2d1021-main-validation.json)でlocal CI全21項目を再実行し、成功しました。
dev/ASan/UBSanは8,150/8,152、releaseは8,151/8,153、失敗0・同じ既知スキップ2件です。
4つのpresetすべてで選択したslangを未改変で使用しています。
[母集団](../measurements/2026-10-27/slang-e2d1021-main-population.txt)と
[sv-tests](../measurements/2026-10-27/slang-e2d1021-main-svtests.json)も再実行し、
母集団は成功1,452件・既存失敗4件・未対応10件、sv-testsは244/344を維持しました。
以前の成功ケースの減少はありません。上記の先行計測は、当初の`13fe2b65`を基準とした記録として残します。

## 制限

P1やclass実行全体の完了ではありません。回復できた構文エラーもエラーのままです。
この版のLinuxの結果は未確認で、同じ検査が必要です。母集団の既存4失敗とP1全範囲の同等性は残作業です。
退行を隠すために既存の期待値を変えてはいけません。過去の日付付き報告の依存版は、計測当時のまま維持します。
