# 静的な部分inout netポート

[English](partial-inout-port-execution.md)

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
