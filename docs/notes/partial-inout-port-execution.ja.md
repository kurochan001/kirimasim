# 静的な部分inout netポート

[English](partial-inout-port-execution.md)

選択したネットの同一性の共有は逆方向の値代入を2本置くこととは異なり、ドライバの二重計上を避ける仕組みは[部分inoutのFAQ](../FAQ.ja.md#does-a-partial-inout-port-create-two-value-drivers)で確認できます。

**承認後の統合更新（2026-11-01）：** 旧受理期待値2件は別コミットで移行済みです。[統合記録](p1-approved-class-inout-integration.ja.md)が、以下の承認前の検証状態に代わる最新の記録です。

IEEE 1800-2023 7.4・23.3.3.7と承認済みMOS/port契約4に沿い、静的なnet同士のinoutのbitを、
強度を減衰しないidentityで接続します。`lowerPorts` が方向付きの値carrierを作らずに記録し、
`connectPartialNetPorts` が検査・有効化します。駆動元・重複駆動・強度の観測がない接続も対象です。
対象外の形は宣言位置の未対応診断を保ちます。

両側で既存の物理bitの対応、表23-1の種別結合、暗黙の根の所有、初期値、物理ドライバの取り込みを
共有します。全体aliasはソースの境界を保持し、静的な配列要素・packed選択は物理序数へ対応してから
接続します。ポートの経路を物理的な根や解決済みの値を持つドライバとして増やしません。
supplyの強度7はidentityをそのまま通り、実際のスイッチだけが強度を減衰します。
SIR/ABIの版、VMの規則、kernel公開ヘッダ、依存は増やしません。

幅・符号・4値域の一致する静的な整数bit・part・連結を扱い、固定配列要素と互換なwire/tri、
wired、pull、supplyの種別を含みます。inoutの変換、動的・不正な添字、遅延のあるnet端子、
forceを観測するview、uwireは既存の位置付き未対応診断や厳格なslang診断を保ちます。
input/outputのidentityとstrongの変換carrierは従来の規則を保ちます。

独自の生成器は221設計を追加します。種別の全組、暗黙の根だけの接続、異なる実効種別、
module/generate/instance、1/65/129 bit、昇順・降順、part・連結、固定配列の次元と全体aliasの
viewを含みます。独立したbitごとの種別・強度の参照計算を使い、未接続bit・要素をすべて確認します。
単体8件は、根なし・1駆動のmetadata、inout carrierの不在、alias序数、初期supply、暗黙の根の
重複防止、実際の受動の循環、拒否する対応を検査します。ソースを破棄した後の実行も既存のVM/kernelを
使います。

[既存2件の分類移行案](partial-inout-expectation-migration.ja.md)は実装と分けます。CLIと単体内の
SVソース、ほかの期待値、性能予算は保持します。Icarus/VerilatorやGPL/LGPLのソースは読まず、
slangへの投稿も保留しています。有限の比較だけではP1完了とはしません。

## 承認前の検証記録（2026-11-01）

221ソースと単体8件が成功しました。inout受理の省略、carrierの重複に依存した有効化、
identity公開の省略、tranとしての減衰、符号の変換の受理、の変異5件を実際にコンパイルして
検出しました。各変異で関連する既存1,232件（単体101・ソース1,131）も成功し、移行案の
既存単体1件はこのlegacy群から除いています。ソース・ビルドを復元し、全1,077アンカーが
適用可能です。変更箇所のclang-tidy・format・期待値分離も成功しています。

`cli/options`と既存の部分inout拒否単体だけを除いたdevは、選択した11,013件がすべて成功し、
失敗・スキップは0です。具体的な移行パッチを確認用コピーだけへ適用し、CLI全体と生成器が成功、
強化した受理単体も実際のライブラリへ別途コンパイルして成功しました。リポジトリの既存期待値は
承認を受けるまで保持しています。最終JUnitに基づく件数更新と、固定した先頭での全21項目の
local_ciは承認後に実行し、以前の件数の記録はそれまで元の範囲を保ちます。

[171件の実装前](../measurements/2026-11-01/icarus13-partial-inout-baseline.json)はnative `b56fae24`、
[実装後](../measurements/2026-11-01/icarus13-partial-inout-ports.json)は実際のnative `85535e72` を使い、
Icarus 13.0（v13_0）と比較しました。全ソース・期待値のバイト列・参照側の状態が一致し、
従来156件のnativeの状態も不変です。新規15形はすべてnativeで成功し、参照側も通る新規不足11件を
解消しました。実装前の不足14件は、同じ未解決3件（specify、密なUDPのfrontend上限、Icarusの
realネット拡張）になりました。新たなwiredの読み出しの相違4件は、従来の参照不一致21件と別に保ちます。
参照側の原因は**未確認**で、種別と対応だけを変えた全体・bit・part・連結の独自比較で、処理系の
ソースを読まずに切り分けられます。その値へ合わせて期待値を変えていません。保存診断からは
インストール位置とshellのプロセスIDだけを除きます。14.0・母集団・sv-tests・nightly fuzzは
再測定せず、main反映・pushもしていません。slangへの投稿は引き続き保留しています。
