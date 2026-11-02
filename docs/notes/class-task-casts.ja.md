# classのtask形式cast

[English](class-task-casts.md)が正本です。

classの`$cast(destination, source);`を、[承認済み契約5節](class-runtime-proposal.ja.md)に
従って実行します（IEEE 1800-2023 6.24.2、8.16）。既存の対応するclass代入先について
これまで残っていたtask形式を接続したもので、class契約全体やP1の完了ではありません。

`Lowerer::lowerClassCast`が関数・task両形式の代入先refを1回だけ、sourceの副作用より
先に捕捉します。static slot、固定配列要素、automatic frameを、module、package、
interface、program、generateから同じ経路で扱います。インスタンスpropertyのrefは未対応です。

成功時だけ格納する型付き命令`object.cast.required`を使います。共有カーネルの
`Heap::castObject`が既存の互換性を判定します。代入互換のviewとliteral nullは成功し、
狭めるcastは互換の動的オブジェクトを必要とします。型の付いたnullは静的な型を保ちます。
task形式の失敗では代入先を変えず、位置付きの設計実行時エラーと終了コード2にし、finalを
実行します。関数形式の失敗は従来どおり0を返し、格納せず続行します。VMで型の互換性を
決めず、新しい依存や外部C ABIの変更もありません。

## 確認と制限

[独自の生成器](../../tools/gen_class_task_cast_matrix.py)が180件を追加します。
宣言を置く6種類のコンテナ、scalar・static配列・automatic配列の3種類の代入先、10種類の
sourceを組み合わせます。基底/interface view、型付き/literal null、型不一致、対象の添字を
変えるsourceの副作用、source評価中の4096回の一時オブジェクト生成を含みます。さらに2件でvirtual functionと
待機するvirtual task内のframe上の代入先を検査します。新しい
ユニット3件でカーネルのエラー分類、SIRの検証と表示、失敗時の代入先とエラー位置を確認します。
既存の関数形式castのテストは変更していません。

比較演算を含まない独自の成功castプローブはIcarus Verilog 13.0でコンパイルできましたが、
実行時に`$cast`が未定義として拒否されました。承認済み契約に
基づく実装であり、Icarusが実行できると測定した差を埋めた件数には加えません。調査中の空の
UDP入力はIcarusで動きましたが、29.8/A.5.4は入力端子に式を要求するため、標準機能の不足の
証拠とは扱わず、今回は実装しません。slangへの投稿は引き続き保留です。dense UDPの
コンパイル、残るclassのproperty/監視、specify、VPIの境界は残作業です。

[参照プローブ](../measurements/2026-11-03/class-task-cast-reference.json)と
[コンパイルした4種類の誤実装](../measurements/2026-11-03/class-task-cast-controls.json)に、
独自ソースと復元の検査を残します。4種類すべてを検出し、復元後の関連テストも通過しました。
