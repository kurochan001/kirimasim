# 公開mainの統合と文書の更新記録

[English](main-integration-2026-11-04.md) · 2026-11-04。

公開main `9113955914d878437074f0e3b86a8dca38438080`は、`d73d8922`以降の25コミットを
統合しています。全local CIと同じコミットのGitHub CIワークフローが成功しました。
P1は進行中です。有限の例だけでは、[Icarus対応範囲の目標](../spec/P1_SCOPE.ja.md)全体を
達成したことにはなりません。この記録では、実装・承認済み契約・期待値を変えずに文書を更新します。

## 実装済みの範囲と残る境界

| 分野 | 統合した動作 | この範囲に含まれないもの |
| --- | --- | --- |
| [状態依存のspecify](specify-state-execution.ja.md) | 単純なスカラー`if`・`ifnone`、機能値の到着時の条件捕捉、既存の物理ドライバによる時間処理 | vector・条件付きエッジ依存・受動path、明示的pulse control、SDF、timing check |
| [VPIのnet全体への書込み](vpi-net-write-execution.ja.md) | point netの即時deposit、対象となる登録netのforce/release、HDLと共有する所有権、callbackの再入 | netの部分・遅延書込み、受動deposit、追加API、cocotb連携 |
| [明示クロック本体のclass操作](class-clocked-execution.ja.md) | 通常の信号トリガーの下で、対応するproperty操作・呼出し・生成を実行 | object依存のtrigger・guard・watch評価、objectへの暗黙の感度、既存のproperty対象の制限 |
| [native FST](fst-execution.ja.md)と[強度の書式](strength-format-execution.ja.md) | 既存の共有状態を使うバイナリ波形出力と、捕捉した強度の`%v` | 記載していないソース・objectの形。ビューアとの相互運用と波形の性能は未確認 |

最初の3行が今回統合した実装のまとまりです。FSTと強度の書式は実装済みで、未対応と記していた
古いガイドを修正しました。型・寿命・選択の正確な制限は[P1対応表](../P1_SUPPORT.ja.md)に従います。
既存の期待値移行は、別コミットと承認記録を保っています。

## 公開実装の検証

[機械可読の記録](../measurements/2026-11-04/main-integration-validation.json)に、ソースの版、
local CIの各段階、ツールの版、取得したGitHubの結果、文書のコマンド確認を記録しています。

- 既定の`python3 tools/local_ci.py`は、`91139559`で**全21段階**を通過しました。
  dev・ASan・UBSanはそれぞれ**12,012/12,012**件、Releaseは**12,013/12,013**件で、
  失敗・skipはありません。`cli/slot_event_default`がRelease専用です。
- Releaseのフラグ、ソースの規則、fixtureの対応、生成した件数、整形、**194翻訳単位**のclang-tidy、
  統合した**25コミット**それぞれの期待値検査が通過しました。ログの作業ツリー変更の注記は、
  非公開の未追跡ファイルを保ったためです。追跡対象はcleanで、検証中も変更していません。
- [GitHub CI run 41725997482](https://github.com/kurochan001/kirimasim/actions/runs/41725997482)は、
  同じ完全なコミットに対して`completed`・`success`です。2026-11-04に確認し、最終更新は
  2026-11-03 23:21:34 UTCです。これはワークフローの結論で、新しいNightlyや、すべての任意ジョブの
  成功を主張するものではありません。

classの従来の記録では、全件検証した時点と最後のfixture修正を分けていました。
公開コミットの全local CIは、両方を含みます。以前の生の記録は、元のソースの版を保ちます。

## 機能プローブと以前の外部測定

[180プローブの記録](../measurements/2026-11-04/icarus13-class-clocked-after.json)は、
**P1評価対象179件と対象外1件**です。対象外の`real_net`は、任意のIcarus拡張`wire real`への依存を
確認したソースです。その拒否は未対応のままで、PASSにはしません。標準のreal変数・実数演算・合法な
単一の連続代入・`nettype real`は、既存のP1範囲を保ちます。

必須範囲の差は**`dense_udp_coverage`**です。フロントエンドの時間・出力量の上限に達する独立した
失敗として残し、免除や正常な拒否には扱いません。参照側が通過する生の差は`real_net`も含めて2件です。
参照側の期待値との不一致25件は別に扱います。以前の176入力・期待値・native結果は不変で、追加した
classのクロック処理4件のうち3件が新しい差を解消し、残り1件は待つメソッドの対照例です。

今回の文書更新では、外部母集団を再実行していません。[件数の記録](../VALIDATION_COUNTS.ja.md)の
測定日を保ちます。固定母集団は1,456/1,466件通過、失敗0・未対応10件（2026-10-27）、sv-testsは
両OSで252/344件、IcarusはLinux 211・macOS 210件（2026-11-01）です。参照fuzzの7夜連続の記録は
2026-10-27までです。性能などの過去の観測も、測定した版と日付を保ちます。

## 文書の実行例

`kirimasim 0.0.0 (commit 9113955914d8)`で18項目を確認しました。既定と幅8のcounter、bit queryの
parse・SIR・実行、以前のobject例の3段階、property-refのparse・拒否、X条件のエラー、時刻が進まない
処理の上限、VCD/FST出力とFSTの復号、メモリ読込み、強度のforce/release表示、
統計と波形の指定を付けたbit queryの実行です。
期待したエラーと上限では、非0の終了コードを保ちます。

以前の`unsupported_object.sv`は現在は実行に成功し、元のファイルを変更せず残しています。
[デバッグガイド](../DEBUGGING.ja.md)では、現在の変換の境界を示す
[新しいproperty-refの例](../examples/debugging/unsupported_property_ref.sv)を使います。
parseは成功し、SIR変換は位置付きの未対応診断と終了コード2を返します。
文書用の例はsuiteのテストを追加せず、CTestの件数も変えません。

今回の文書更新では、**ToolsのCTest全201件**が通過しました。**公開Markdown 442ファイル
（日英221組）**の相互言語リンクとローカルリンクに不備はなく、変更した表とMermaidの構成も
日英で一致します。READMEの生成件数、SPDX、空白の検査も通過しました。
`counts.json`・以前の生の測定記録・非公開の未追跡5ファイルは不変です。前述の全local CIは
公開実装に対する以前の実行で、今回の説明文の変更に対して再び全件実行したものではありません。

再現手順は[検証ガイド](../VALIDATION.ja.md)、以前のガイドの時点は
[状況の履歴](../STATUS_HISTORY.ja.md#guide-checkpoints-before-2026-11-04)を参照してください。
slangへの投稿は保留を続けます。
