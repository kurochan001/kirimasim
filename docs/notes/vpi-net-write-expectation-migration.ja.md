# VPI netの物理登録：観測テストの件数移行

[English](vpi-net-write-expectation-migration.md) · 2026-11-03。

**状態：この1アサーション・2ケースの移行は2026-11-03に別途承認済み、適用済みです。**
[承認済みのnet全体への書込み契約1〜2・4節](vpi-net-write-proposal.ja.md)は、名前から到達する
入力port netも、完全な物理的な権限として扱います。書込み可能な変数を捏造したり物理起源を省いたりせず、
既存の連続driverを登録する必要があります。

`tests/unit/module_path_observation_test.cpp::observePath`は、両ソースで
`ctx.strengthDrivers().originCount() == 1`を要求しています。完全な物理登録では、無条件のケースは、
既存の弱い出力qのdriverと、既存の入力aの暗黙port driverの2起源です。条件付きでは、さらに既存の
入力bのport driverを加えて3起源になります。ソースdriverを新しく作ったものではなく、以前はqだけを
物理登録していました。

共通のアサーションを次へ変更する案です。

```cpp
EXPECT_EQ(ctx.strengthDrivers().originCount(), conditional ? 3U : 2U);
```

元のソースのバイト列と、値・時刻・強度、VPI登録・callback、VCDのalias、不正IRの全アサーションを
保持します。module pathの出力qを駆動する物理起源が正確に1本であることも明示して、qのdriverを
重複させないという元の検査を保ちます。ケースのskip・削除・名前変更、最小値や範囲への緩和は行いません。

対象のCTestは次の2件です。

- `ModulePathObservation.VpiBothPortNamesFormattingAndWaveformSeeCommittedPathSnapshot`：
  旧件数1、実測・新件数2。
- `ModulePathObservation.ConditionalPendingPublicationKeepsVpiFormattingAndWaveformCoherent`：
  旧件数1、実測・新件数3。

現在の失敗は、この登録数のアサーションだけです。凍結したnativeの記述済み期待値58件と、
ほかのkernel/VPI/pathの重点確認133件は成功しています。この2件の旧アサーションが残る間は、
全体の回帰成功とは扱いません。[記録](../measurements/2026-11-03/vpi-net-write-origin-count-migration.json)に、
変更していないテストファイルのハッシュと正確な件数を保ちます。

根拠は承認済みのnet物理権限契約、確認した1800-2017 38.34・10.6.2、既存のソースport driverモデルです。
2023本文の直接照合は未確認のままです。言語の公開値の期待やP1ゲートを変えず、物理登録の件数を更新します。
別途承認後に、`src/`を含まない別コミットでアサーションを移行し、両ケースを再検査します。
