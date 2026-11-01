# VPIの独自参照プローブの棚卸し

[English](vpi-reference-inventory.md)

2026-11-01の測定で、明示的に選んだIcarus Verilog 13.0 stableのコンパイラ・実行系
（`v13_0`）で**参照23/23件が成功**しました。[記録](../measurements/2026-11-01/vpi-reference-inventory.json)には、
独自SVのソース・ハッシュ・期待値・実際の出力、独自C fixture・runner・ツールのハッシュ、
native loaderの観測を含めています。

[最初のVPI契約案](vpi-p1-proposal.ja.md)を絞るための可用性のプローブで、native実装の検査や
IEEE/cocotb全体の検査ではありません。VPIは未実装で、検査したnative実行ファイルは
`--vpi`を入力エラーの終了2で拒否します。nativeのAPI別のケースは実行しておらず、
`native_evaluated`は0、`native_passed`はnullです。nativeの失敗23件とも成功23件とも数えません。
既存のP1一覧・合格率は変更しません。

## ケースと観測

[独自fixture](../../tests/probes/vpi/inventory.c)は、標準の登録・呼出し引数、compiletf、
整数・129ビットsized関数、階層・iteratorの検索、129ビットのX/Zのget/put、固定メモリ要素、
real・packed文字列の変換、時刻のhigh/low繰上がり、argv、finish/エラー照会、変数のforce/releaseを検査します。
callbackは開始・終了、遅延・次時刻、read-write/read-only同期、値変更、解除・取り消し、
native Cへの再入を調べます。

read-writeのケースはNBAの結果1を読み、2を書きます。read-onlyと次のHDL観測はどちらも2です。
再入の専用ケースでは`before put`、`change 1`、`after put`の順になりました。
この順は具体的なnative契約を提案するための参照観測で、移植可能な出力の期待値ではありません。
SCHEDULER_SPECにあるcallback領域の既存規則は引き続き優先します。

検索とlifecycleの対象は、参照のコンパイルで保持されるよう、HDL/VPIの実引数で明示的に参照します。
その参照を持たない初期の調査入力では、対象のオブジェクトが公開されませんでした。
未参照・最適化で除かれたオブジェクトの可視性は未確認で、保持した対象の成功から推測しません。
callback以外のAPIの個別節番号も未確認で、データにも記して第38章を参照します。

## 再現と判定器の変異検査

下の3変数には、同じインストール済み13.0のコンパイラ・実行系・VPIビルドwrapperの
パスを明示します。PATHで偶然選ばれる版には依存しません。ツールは独自ソースと子processの
出力だけを読み、他シミュレータの実装や生成されたシミュレーションのbytecodeは読みません。
Cコンパイラはインストール済みの公開VPIヘッダをincludeしますが、リポジトリへはコピーしません。
外部startup表の宣言は、そのABIに従います。

```sh
python3 tools/vpi_feature_probe.py \
  --iverilog "$VPI_PROBE_IVERILOG" \
  --vvp "$VPI_PROBE_VVP" \
  --iverilog-vpi "$VPI_PROBE_COMPILER" \
  --kirimasim build/dev/src/driver/kirimasim \
  --output build/vpi-reference-inventory.json
```

fixtureへ実際に加えてコンパイルした6変異は、全て判定器が検出しました。
関数の幅、X/Zの欠落、時刻上位の欠落、callbackの取り消し忘れ、同期した値の誤り、
forceのdepositへの置換です。[記録](../measurements/2026-11-01/vpi-probe-checker-controls.json)に
失敗したケース名と、元のfixtureのバイト列を保ったことを記しています。
その後、元の正のfixtureも再び成功しました。`--plugin-source`で変異を加えたコピーを指定でき、
追跡中のソースを変更せず再現できます。検証したのは参照の判定器で、**未実装のnative VPI hostではありません**。

依存・ツールのインストール、既存ソーステスト・期待値の変更は行っていません。
本体のVPI実装や新しいCI件数も主張しません。slangへの投稿は保留を続けます。
VPI_SPECと優先順位の決定に従い、詳細な実行契約のレビューをhost実装より先に行います。
