# VPIのrelease結果の返却

[English](vpi-release-results.md)が正本です。

**後続のnet書込み改定（2026-11-04）：** [net全体の実行](vpi-net-write-execution.ja.md)で、対応する即時net操作を追加しました。以下の日付付き結果と最初の境界は、その当時の記録です。

`vpi_put_value(..., vpiReleaseFlag)`が、nullでない`value_p`へ解除後の値を返します
（IEEE 1800-2023 38.34）。従来はforce自体を解除しても、呼出し元の出力領域が更新されませんでした。
[独自の旧版プローブ](../measurements/2026-11-03/vpi-release-baseline.json)では、変数は両者とも1なのに、
Icarus Verilog 13.0の返却は1、nativeは0でした。プロセスの終了コード0だけではAPI成功を示せません。

[承認済みの変数書込み契約3〜5節](vpi-p1-proposal.ja.md)の範囲内の修正です。
既存の型付き書込み・releaseが成功し、出力ポインタがある場合だけ、生きた数値bindingから
読み、`vpi_get_value`と同じ変換を使います。vector・文字列の返却領域はContextが所有し、
従来の寿命を保ちます。拒否・失敗した書込みは出力を書き換えず、従来のnull出力のreleaseも
保ちます。ABI配置、依存、変数のforce、スケジューラ領域の方針は変更しません。

## 確認と残る境界

[独自の生成器](../../tools/gen_vpi_release_matrix.py)が、8種類のコンテナ、6種類の幅
（1〜129）、bit/logic記憶域から96プログラムを作ります。[独立したCプラグイン](../../tests/probes/vpi/release_result.c)が
scalar・整数・vector・2進・16進・real形式の576回のreleaseを検査します。新しいユニット4件は
10種類の明示format、X/Zと上位ビット、Context所有の出力、real変数、拒否時の出力保持も確認します。
[コンパイルした4種類の誤実装](../measurements/2026-11-03/vpi-release-controls.json)をすべて検出し、
復元後のテストも成功しました。既存の期待値は変更していません。

先行調査の`vpiObjTypeVal`による自動formatには、1ビット幅の16件で選択の差がありました。
[最初の観測](../measurements/2026-11-03/vpi-release-auto-format-observation.json)を別に残します。
scalarと1ビットvectorのreflectionの分類は未確認で、38.22に基づく専用の比較が必要です。
本修正は既存の変換器を使い、自動format全体の同等性を認定しません。

[net書込みの追加3プローブ](../measurements/2026-11-03/vpi-net-write-followup.json)で、Icarusが扱う
net deposit、whole-net force/release、受動網のforce/releaseに別の不足を確認しました。
nativeはVPIエラーを返します。承認済み契約4節が後続の統合作業としている範囲なので、詳細改定の
承認後に実装します。終了コードが0でも、未対応のAPI結果を成功には数えません。固定176プローブの
ゲートとP1母集団は今回は測り直していません。P1全体は未完了で、slangへの投稿は引き続き保留です。

[明示formatの比較](../measurements/2026-11-03/vpi-release-explicit-reference.json)は、独自96件すべてで
native・Icarus Verilog 13.0の両方が一致しました。

`cc3e344f`の[最終ローカルCI](../measurements/2026-11-03/vpi-release-validation.json)は
全21項目に成功しました。dev・ASan・UBSanは各11,785件、releaseは11,786件で失敗・
スキップなし、clang-tidyは191ファイルを確認しました。classのtask形式castとVPIの
修正の両方を含む結果です。
