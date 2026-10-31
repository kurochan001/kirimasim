# 静的な部分ポートの実効ネット種別

[English](port-kind-execution.md)

IEEE 1800-2023 23.3.3.7・表23-1と、2026-10-25承認の
[MOS/port契約4](mos-switch-proposal.ja.md)に沿い、接続するbitの実効ネット種別を決めてから
暗黙ドライバを作ります。全幅ポートと `Lowerer::connectPartialNetPorts` が同じ
`mergeNetKinds` の表を使います。後者が物理bitのidentity接続をまとめ、ポートの巡回順に種別を
結合し、各組に1つだけ暗黙ドライバの所有位置を記録します。`finishStrengthNets` がbitごとの
種別・初期値・根を作り、明示的な物理ドライバはそれぞれ独立した根として保ちます。

幅・符号・状態域が一致する4値のinput/outputネット接続で、wire/tri、wand/triand、wor/trior、
tri0/tri1、supply0/supply1を扱います。静的なpackedのbit・part、連結、固定配列要素が同じ
対応付けを使い、配列aliasの境界は物理位置へ正規化する前に解釈します。明示ドライバのない組も
対象です。接続bitがpull・supply・wiredの種別になっても、隣接bit・要素は元の種別を保ちます。
複数のポートが同じbitにつながっても暗黙の根は重ねません。tranで結ぶ独立ネットは独立した根を
保ちます。tranはidentityの統合ではありません。supplyの強度7はidentityをそのまま通り、
対応するスイッチ演算を通るところで減衰します。新たに扱う未駆動bitは実効種別の既定値で始まり、
駆動のあるbitは予定された解決の前のXを保ちます。既存のplain wire/triの初期動作は維持します。

SIR/ABIの版、kernel公開ヘッダ、VMの規則、依存は増やしません。明示的な部分inout、このポートを
通るuwireの根の数の合法性、強度輸送を要する幅・符号・状態域の変換、forceで観測可能な接続、
動的・不正な構造添字は既存の境界を維持します。完全な `%v` 書式は引き続き未対応です。

生成器は独自の238入力を作ります。種別の全100組、input/output、module/generate/instance、
1/65/129 bit、昇順・降順のpacked境界、part・連結・配列要素、4値、異なる強度、駆動の除去、
暗黙ドライバだけのネット、同一配列内の異なる実効種別を調べます。C++の表とは独立した種別の規則と
既存の独立した強度点の参照計算を使い、formalとすべての未接続の隣接bitを確認します。
単体テスト8件はSIRの種別・根の所有・強度、aliasの物理位置、ソースを破棄した後の実行、初期値、
重複接続、tranの独立した根、スイッチの減衰を検査します。

[既存6入力の期待値移行案](port-kind-expectation-migration.ja.md)は実装と分けます。既存ソースの
バイト列、ほかの期待値、性能予算は保持します。slangへの投稿は保留しています。
Icarus Verilog・Verilatorを含むGPL/LGPLのソースは読んでいません。

検証結果と固定した145プローブの比較は実行後に記録します。有限の比較だけでP1完了とはしません。
