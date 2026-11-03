# slang d02b7cdaへの依存更新

[English](slang-d02b7cda-update.md) · 2026-11-03。

未改変のフロントエンドを`d02b7cda02d3183f35c6c4d4db2b2fc03f19f4ae`に固定します。
今回確認した上流`master`の先頭で、`e2d1021`から25コミット先、12.1後の開発版です。
[#2072](https://github.com/MikePopoloski/slang/issues/2072)のサブルーチン回復処理、
class・型の名前解決、関数の寿命、乱数seedの束縛、non-ANSI interface portの修正を含みます。
既存依存mimallocはv3.6.1（`56dc5eb80145f9a3ba7ca23d47c6a248b9d8586c`）になります。
両者のMITライセンス本文は不変です（[確認記録](../measurements/2026-11-03/slang-d02b7cda-licenses.json)）。
トップレベルの新しい依存やインストールしたツールはありません。投稿保留を継続し、
上流へのissue投稿やコメントはしていません。

## 統合と独立した対照

[現行profile一覧](../measurements/2026-11-03/compatibility-d02b7cda-profiles.json)では、
`vcs`と`all`に`AllowMultiDrivenDynamicallySizedArrays`が加わります。実行用driverの
厳密な再解析では、従来の2緩和とともに解除します。追加APIテストは動的配列・queue・連想配列、
3種類の競合writer、両モードを検査し、単一writerの対照と既存の連想配列型の制限も保ちます。
parse受理は実行対応を意味しません。解除を省いた対照はビルドが成功し、検査が失敗しました。
復元後のソースhashは一致し、関連検査も成功しました
（[回復処理と対照の記録](../measurements/2026-11-03/slang-d02b7cda-recovery.json)）。

独自の回復行列は、宣言scope 6種、join 3形式、前後の文の配置3種、入れ子本体3種の162入力です。
**parse/runの324検査がすべて成功**しました。関数で禁止されるforkは位置付き入力エラー2で
拒否し、正常な`join_none`対照は成功します。保存した旧バイナリでは、選んだ不正入力4検査で
abortし、正常対照2検査は成功しました。既存の互換モードCLI **409検査**、driver CLI
**1,728検査**も成功しました。

`$random(5)`は従来のseed検査18:15より前に、slangが18:23で代入可能性のエラーとして
拒否します。現行pinでも既存の厳密な`slang-main`診断profileを使います。入力、ヘッダ、
両profileの期待値は変更せず、新しい位置・診断文・終了コード2をすべて要求します。

## 承認済みの期待値移行

最初のdev全体検査は11,787件中11,784件成功・3件失敗でした。新版は暗黙の戻り値変数を
関数と同じ寿命に修正します（IEEE 1800-2023 6.21・13.4.1）。保守者が2026-11-03に
[3ファイルの移行](slang-d02b7cda-update-proposal.ja.md)を承認しました。
コミット`fd10bd23`は実装と分離し、期待値だけを変更します。SIR出力2件をstatic slotにし、
初期値行列では3回目の呼出しでもstatic戻り値が代入値を保持することを要求します。
**122検査が成功**しました。既存の入力とautomatic寿命の検査も維持します。

[独自の寿命観測](../measurements/2026-11-03/slang-d02b7cda-return-lifetimes.json)は失敗も残します。
scalarと幅の広いpacked戻り値のstatic寿命は新版pinで改善します。unpacked aggregateの保持は
kirimasimが明示的にフレームへ置くため、両pinで失敗します。Icarus 13.0もscalarのstatic対照は
失敗し、配列対照はコンパイル時に拒否します。成功への置き換えや規格の寿命の変更はしません。
aggregateの保持は別途P1作業として残し、この依存更新では実装しません。

## 更新前後の比較

[固定母集団の比較](../measurements/2026-11-03/slang-d02b7cda-comparison.json)は、両バイナリとも
**1,466件中1,456成功・失敗0・未対応10**で全件の状態が一致しました。
kirimasim-tests `1fd46b2adb860a8587988a509cf9e01bbe29d8f1`の不変なarchiveを使い、
ローカル変更を使わず変更もしませんでした。sv-tests
`15490e95fd978ada1c1dc59b7c4b53a7edb23b78`も**252/344**を保ち、合否の変化はありません。

**独自の有限プローブ176件**もnativeの状態とstdoutが一致しました
（[比較](../measurements/2026-11-03/slang-d02b7cda-probes.json)）。P1評価対象175件、対象外は
依存を確認した任意のIcarus `wire real`拡張1件です。`dense_udp_coverage`は引き続き時間超過し、
独立した不具合・P1差分として残ります。更新だけでIcarusとの完全な同等性を確認したとはしません。
過去の測定は元のpinの記録を保ちます。Linuxの確認は未実施で、同じgateで確認できます。

## 最終のローカル検証

[`3859f210`でlocal CI全21項目が成功](../measurements/2026-11-03/slang-d02b7cda-validation.json)しました。
dev・ASan・UBSanはそれぞれ**11,787/11,787**、releaseは**11,788/11,788**で、
失敗・スキップは0件です。clang-format、clang-tidy（191翻訳単位）、releaseフラグ、
ソース方針、期待値の分離、生成する件数も成功しました。4構成すべてで選択したSHAの
未改変の依存ソースを使っています。変異定義1,157件はすべて適用可能で、新しい省略対照は
別途ビルドして実行しました。この結果は専用ブランチの検証であり、main作業ツリーの
並行変更は含みません。
