# mainへの統合：コンテナの暗黙監視とVPI引数の宣言情報

[English](main-integration-2026-11-06.md) · 2026-11-06。

`6f5c6984`の後に、`e6fd5a97`から`c47cc767`までの10コミットをmainへ取り込みます。
この公開記録自体は文書だけの変更です。P1は進行中であり、有限のソース行列だけではIcarusの
全機能範囲を満たしたとは判定しません。

## 統合する動作と承認

- [型付きコンテナの暗黙監視](implicit-container-execution.ja.md)は、適格な静的な整数配列・キューを扱います。
  契約1〜8節は`30d45404`で承認を記録し、`8873c89a`で実装しました。
  既存11件の拒否判定は別途承認を受け、`b3d20b8b`で移行しました。
- [automaticの整数引数のVPI宣言情報](vpi-argument-declarations.ja.md)は、ソースの範囲とscalar/vector情報を保ちます。
  選択した親は数値bindingを捕捉し、期限切れの通常フレームのviewは再利用した呼出しへアクセスできません。
  実装`c0b047e3`は既存の承認済みVPI契約に従います。

既存の期待値変更は実行時の変更と分離したままです。文書化した型、選択、寿命、callback、net書込みの
境界も保ちます。slangへの投稿は行っていません。他の担当者が所有する未追跡5ファイルは、変更せず、
今回のコミットにも含めません。

## 検証結果と制限

先の[コンテナ監視の記録](../measurements/2026-11-06/implicit-container-validation.json)は、
596件のソース検査、コンパイルした12種類の故障、release／ASan／UBSanの関連テスト各1,003件を記録しています。
その後の[VPIの記録](../measurements/2026-11-06/vpi-argument-declaration-validation.json)は、
dev全件**12,268/12,268**、失敗・skipなし、ソース243件、コンパイルした8種類の故障、
release／ASan／UBSanの関連テスト各69件を記録しています。VPIの補助比較では参照側も通る81件の差を解消しました。
これらは機能単位の区切りであり、統合した全local CIの成功を意味しません。

統合する`c47cc767`で、4構成とlintを含む全`tools/local_ci.py`を実行しました。
全構成のconfigure・警告をエラーとするbuildが成功しました。releaseフラグ、禁止機能、SPDX、
suiteファイルの対応、大域状態、10コミットすべての期待値分離、clang-format、197翻訳単位のclang-tidyも成功しました。
**統合したlocal CI全体は失敗です。** devは31件、releaseは39件、ASanは59件、UBSanは55件の
タイムアウトでした。失敗したCTestの分類はすべてタイムアウトです。実行中にホストのスリープを記録し、
通常のアイドルスリープ抑止だけでは中断を防げませんでした。最初の試行でタイムアウトしたdevの7件と
releaseの30件は、再実行ではすべて通りました。どちらのCI試行中もソースは編集していません。

中断されたdevのJUnitを使うREADME件数チェックも失敗しました。公開する件数は以前の成功した全件実行の
結果を保ち、タイムアウトをPASSに変更せず、失敗を隠すための件数削減もしていません。
devは12,268件で、releaseには`cli/slot_event_default`が加わり12,269件です。
[機械可読の記録](../measurements/2026-11-06/main-integration-validation.json)で結果を区別しています。
この記録は公開前に作成したもので、pushしたコミットの結果は
[GitHub CI](https://github.com/kurochan001/kirimasim/actions/workflows/ci.yml)が独立に記録します。

## P1の範囲と保った外部測定

再測定した[188プローブ](../measurements/2026-11-06/icarus13-vpi-argument-checkpoint.json)は、
既存ソースと期待値の組をすべて保っています。P1評価対象187件、唯一の任意ケース`real_net`、
必要範囲の`dense_udp_coverage`のタイムアウトと、別に扱う参照側の不一致25件です。
クラッシュ・ハング・独立した不具合を正常扱いにはしません。任意としたIcarusの`wire real`拡張は、
標準のreal変数、対応する連続代入、ユーザー定義net型を除外しません。

固定母集団は1,456/1,466、失敗なし、未対応10件のままです。sv-tests、fuzz、性能とその他の外部観測も、
[検証件数](../VALIDATION_COUNTS.ja.md)にある測定日を保ち、この公開作業では再測定していません。
Icarusで必要なVPI、specify、class、コンテナの残る境界はP1の作業です。
