# 承認済みclass監視・部分inoutの統合

[English](p1-approved-class-inout-integration.md) · 英語版が正本です。

状態：**期待値移行2項目とも承認・適用済み（2026-11-01）**。
dev全体の回帰検査が成功しました。作業ブランチでの統合であり、main反映・pushは別の指示で行います。
slangへの投稿は保留を続けます。

## 承認と入力の保持

保守者は提示した2項目・既存テスト3件の移行を明示的に承認しました。
[class監視](class-observer-expectation-migration.ja.md)は期待値だけの`dd7a36b6`、
[部分inout](partial-inout-expectation-migration.ja.md)は`5766887f`で、実装とは別コミットです。
定数waitのSIR、CLIのSV、ポート単体の埋め込みSVは不変です。新しい期待値は、正常な実行、
正確な初期化・identity、記載した未確保または根なしの性質を要求します。
無関係な拒否テストと性能予算は保ちます。

class実装`dfce0f33`と部分inout実装`85535e72`のsrcのバイト列は保持しています。
共有のclass監視事前検査・物理ネットidentity・既存スケジューラとカーネルの意味は承認済みのままです。
統合による依存、意味を変えるオプション、ABI版の追加はありません。

## 統合後の検証

[検証記録](../measurements/2026-11-01/p1-approved-class-inout-validation.json)に、
ソースのハッシュと実際の統合実装の版を記録しました。

- class・ポート・CLIの関連525件が、除外なしですべて成功しました。
- dev全体は**11,299件すべて成功**、失敗・skip・除外0です。
- 件数は成功した全体JUnitだけから更新しました。単体1,074、suite9,948、生成器172、
  Tools192、SIR出力29、Context21、CLI・イベント回数35件です。
- 1,082の変異アンカーが一致します。実装のバイト列が不変なので、記録済みのclass6変異と
  inout5変異は引き続き適用できます。元の検証記録はそれぞれの範囲を保持します。

候補全体の検査コマンドは次のとおりです。

```sh
CMAKE_BUILD_PARALLEL_LEVEL=8 CTEST_PARALLEL_LEVEL=8 python3 tools/local_ci.py
```

cleanな候補の先頭を固定し、dev・release・ASan・UBSan、静的規則、期待値の分離、件数生成、
format、clang-tidyを検査します。候補の正確な版と実行結果は
`build/p1-approved-publication-candidate.json`と`build/p1-approved-local-ci.log`へ保管し、
報告には実行の要約を添えます。

## Icarus13.0との固定比較

同じ独自176ソース・期待値を、基準`6affd5a9`と統合`5766887f`で実行しました。
コンパイラ・ランタイムとも13.0/v13_0を明示しました。参照が通るのに失敗する差は**19件→3件**です。
従来156・161・171件のソースhash・期待値・nativeの状態・stdoutは保持しました。
参照の不一致25件は別分類で、追加のwired読み出し4件の原因は未確認のままです。
[基準](../measurements/2026-11-01/icarus13-approved-class-inout-baseline.json)と
[統合後](../measurements/2026-11-01/icarus13-approved-class-inout.json)を参照してください。
残る差はspecify、密なUDPのfrontend処理上限、意図的に保留したIcarusのrealネット拡張です。
参照の出力へ期待値を合わせていません。有限の検査だけでP1完成とはせず、
母集団・sv-tests・nightly fuzzも再測定していません。

## 以前の公開チェックポイント

公開mainの`6affd5a9`ではlocal_ci全21項目が成功し、dev・ASan・UBSanは各10,785件、
releaseは10,786件、clang-tidyは165ファイルでした。これは前の記録であり、
今回統合した検査の件数とは区別します。
