# 固定配列要素のポートと物理的な強度

[English](array-element-port-execution.md)

`Lowerer::staticNetBits` が固定unpacked配列の整数の要素に対応しました。
ソースの宣言範囲で定数indexを検査し、宣言順の位置を求めてからwhole-array aliasの物理slotへ移します。
packedのbit/part選択と連結も同じ変換を使います。IEEE 1800-2023 7.4・23.3.3.7と、
2026-10-25承認の固定配列契約1–2・MOS/port契約4に沿う実装です。

既存の部分ポートの生成器が、input/outputの適格なnet bitをidentity edgeで接続し、
物理ドライバの出所を保持します。解決済みの値を新しい強いドライバにはしません。
配列のresolver、必要な要素だけのdriver、強度の出所、初期状態、感度、共有カーネルは既存の機構です。
新しいSIR命令・公開ヘッダ・VMの意味・依存・ABIの版は追加していません。

両側の幅・符号・値のdomainが等しく、同種の4値wire/triであるinput/outputに対応します。
formalは整数のscalar/packed netで、接続先は配列要素・そのpacked部分・それらの連結を使えます。
whole-array aliasは各宣言の範囲を保ち、逆方向や多次元でも物理位置は宣言順です。
indexはコンパイル時に確定し、範囲内で、物理位置の表現に収まる必要があります。

明示的な部分inout、強度を運ぶ必要がある幅・符号・domain変換、wired・pull/supplyの部分接続、
動く/無効なindex、forceの観測を伴う接続は、既存の位置付きの境界を保ちます。
これらは承認済み契約の残作業です。配列を使うclocking sourceも未対応のままです。
共通の変換は要素の位置を返しますが、clocking側はscalarのpacked記憶域だけを受け付けるため、
その境界を明示的に検査します。

独自の生成行列は208ソースです。module/generate/instance、input/output、packed/unpackedの
昇順・降順、多次元、whole/bit/part/連結、1/65/129ビット、同等・異なる強度、Zへの復帰、
aliasの宣言範囲、parameter/genvar/階層名、packedの多次元を調べます。
独立したbitごとの強度モデルでformalと配列の全要素を検査し、未接続のbitも確認します。
単体7件は物理位置と出所、ソース破棄後の実行、aliasの範囲、変数の一方向のcarrier、
clocking、明示inout、動く/無効なindexの境界を確認します。

6変異を実ビルドしてすべて検出しました。変換の欠落、位置の0固定、strideの欠落、
ソースの範囲を物理範囲で代用、packed位置の0固定、clockingの境界の欠落です。
各変異で既存79単体と361ソースも成功し、ソースとビルドは復元しました。
全1,059変異のアンカーも適用できます。承認済みの既存3入力の期待値移行は別コミット `df1f8912`。
既存のソース本体や性能予算は変えていません。

有限の比較からP1完了とは判定しません。比較では、任意のcollapseをしない参照側の内側の値を、
承認済みのidentity接続の意味と混同しません。Icarus/Verilatorのソースは読んでいません。
slang投稿は保留中です。

## 実行した比較（2026-11-01）

[実装edf36098の145プローブ](../measurements/2026-11-01/icarus13-array-element-ports.json)を、
実際のIcarus Verilog 13.0（v13_0）と比較しました。以前の131件のソースhash・期待値・nativeの状態は同じです。
追加14件は実装前の[棚卸し](../measurements/2026-11-01/icarus13-port-driver-inventory-baseline.json)からソースと期待値を保ちました。
nativeはscalarと多次元の配列要素ポートを通り、新しい14件で参照側が期待どおり動く不足は3件から2件
（部分接続の内側supply0/1）へ減りました。従来のspecify・密なUDPの時間切れ・real net拡張と合わせ、
145件の不足は5件です。任意のcollapseをしない参照側の内側の値の相違11件は、
従来の参照不一致4件と分けて記録します。測定した出力に合わせてソースや期待値を変えていません。
保存する診断のインストール先とシェルPIDだけを伏せました。
14.0、母集団、sv-tests、7晩のfuzz証拠は今回再測定していません。

全dev 10,205件が成功しました。期待値・判定器だけの追補 `55196d38` は、承認済み3入力の元のポート方向の警告とnoteを隣接する `.warnings` で完全に照合します。ほかの受理済み入力は診断なしのままで、エラーは常に拒否します。件数はこのJUnitの後で別コミットに更新します。
