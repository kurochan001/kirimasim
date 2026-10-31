# class のプロパティ対象の棚卸し（2026-10-31）

[English](class-property-target-inventory.md)

clean な `8e0f8467` と Icarus Verilog 13.0（v13_0）を[独自15プローブ](../measurements/2026-10-31/icarus13-class-property-target-inventory.json)で比較しました。
ソース・SHA-256・書いた期待値・状態・実際の版を残し、参照側の異常終了診断のインストール先と
プロセス番号だけを伏せています。他のシミュレータのソースやテストは読んだり写したりしていません。

測った形では、Icarus は8/129ビットのベクトルとintのビット・部分・indexed選択、propertyのref、
固定配列とpacked構造体のフィールドをコンパイルできません（9件）。property全体のNBAは、
遅延や2値のベクトルも含め、コンパイル後の実行で異常終了します（5件）。
遅延付きblockingのscalar propertyは両側で成功し、固定配列の要素はnativeだけが成功します。
nativeで未対応の残り13形式は承認済みclass契約の残作業です。参照側の失敗を期待値にせず、残作業からも外しません。

この15件からIcarusが成功するnativeの不足は増えませんでした。次は確認済みの名前付きfork disableの7形式を
SCHEDULER_SPEC 10.4/10.7/10.8 と SIR_SPEC 7.2/7.5 に従って進めます。
有限の棚卸しでありclass全体の証明ではありません。前の116件のソースと期待値は変えず、
extendedの末尾に15件を足しました。既存の期待値と仕様は変えていません。
