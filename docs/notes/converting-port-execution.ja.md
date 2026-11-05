# 変換を伴うnetポートの方向付き代入

[English](converting-port-execution.md)

[方向付きcarrier](../GLOSSARY.ja.md#directed-carrier)は変換した値を伝え、[identity edge](../GLOSSARY.ja.md#identity-edge)は物理ネットの寄与を共有するものです。

IEEE 1800-2023 4.9.6・23.3.3.7、SIR_SPEC 3.5と承認済みMOS/port契約4に沿い、
変換を伴うinput/outputでは方向付きの暗黙代入を保ちます。値を通常のstrongで変換する
carrierであり、物理的なidentity接続や、元の強度の逆方向への輸送ではありません。

`Lowerer::lowerPorts` が、物理的な強度のidentityを必要とする接続を登録する前に区別します。
`sameIntegralType` が幅・符号・状態域を比較し、`hasNetConversion` が型のwrapper・選択・
連結を反復で辿ります。明示的なcastは表現が同じでも値の演算として保ち、表現を変えない
暗黙wrapperは透過します。既存のlowering・coerce・通常のネット解決が代入を実行します。
VMの特別扱い、kernel/SIR命令、公開ヘッダ、依存、ABIの変更はありません。

実引数は全体のネット、静的なpacked部分、連結、固定配列要素を含みます。符号拡張・0埋め・
切り詰め・X/Zを2値の値へ変換するcastは既存の値の規則を使います。2値の*ネット宣言*を許す
変更ではなく、その不正な型はslangが厳格に拒否します。同型の構造的なポートはidentityと
物理的な強度の出所を保ちます。明示的な部分inout、残るforce/uwireのidentityの制限、
不正・動的なidentityの添字は既存の境界を保ちます。

生成器は独自の324設計を追加します。input/output、1〜131 bit、全体・part・配列・連結、
module/generate/instance、昇順のpacked境界、符号、明示・入れ子・2値castを調べます。
独立した参照計算でソースのbitを変換し、strongのcarrierとweakのローカルの寄与を解決します。
4値・駆動の除去・一様でないデータの後に、内外の読み出しと未接続の全bit・要素を確認します。
単体8件は通常のstrongのdriver slot、方向、cast、変換時のsupplyの減衰、強度を観測する
接続先、identityの保持、net/inoutの厳格な境界を検査します。ソースファイルを破棄した後に
作ったSIRを実行します。

[既存3入力の期待値移行](converting-port-expectation-migration.ja.md)は2026-11-01に承認され、
実装と別コミットにします。ソースと既存9件の警告期待値は不変で、新規3件の警告/noteを
完全照合し、厳格な診断を保ちます。既存スイートの期待値・性能予算は保持しています。
Icarus/VerilatorやGPL/LGPLのソースは読まず、slangへの投稿も保留しています。
有限の比較だけではP1完了とはしません。

## 検証（2026-11-01）

dev全件のJUnitは10,785件すべて成功し、失敗・スキップは0です。新規324ソースと単体8件が
成功しました。変換carrierへidentityを要求する、幅・符号を無視する、明示castを透過する、
連結内のcastを見失う、の変異5件を実際にコンパイルして検出しました。各変異で関連する既存
901件（単体94・ソース807）も成功しました。ソースとビルドを復元し、全1,072アンカーが
適用可能です。変更箇所のclang-tidy、format、期待値分離も成功しました。件数は成功JUnitから
別に更新し、固定した先頭で `tools/local_ci.py` の全4構成と静的検査を実行します。

[156プローブの実装前](../measurements/2026-11-01/icarus13-converting-ports-baseline.json)は
native `ddf48e2b` とIcarus 13.0（v13_0）、[実装後](../measurements/2026-11-01/icarus13-converting-ports.json)は
実際のnative `632b7ecf` を使います。実装前後で全ソース・期待値のバイト列と参照側の状態が一致し、
従来145件のnativeの状態も同じです。新規11形はすべてnativeで成功しました。新たに測定した
参照側が通る不足5件（出力の切り詰め・拡張・符号拡張と、入力の入れ子・2値cast）を解消し、
実装前の不足8件は3件になりました。残るのはspecifyパス、密なUDPのフロントエンドの実行上限、
Icarus拡張のrealネットです。任意collapseによる新たな相違6件は、従来の参照不一致15件と
分けて保ち、それに合わせて承認済みcarrierの規則を変えていません。保存診断からは
インストール位置とshellのプロセスIDだけを除きます。14.0・母集団・sv-tests・nightly fuzzは
再測定していません。3入力の期待値承認をmain反映・pushの承認とは扱いません。
