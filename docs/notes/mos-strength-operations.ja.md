# 区間全体を保つMOS演算と強さの変換

[English](mos-strength-operations.md)が正。状態：純粋な演算を実装・検証済み、2026-10-26。
ソースからのMOS/スイッチ実行は残作業です。

**後の承認（2026-10-26）：** 人が別途、[管理eventのsafe point修正](managed-event-safepoints-proposal.ja.md)の
推奨案1を承認し、RUNTIME_ABI 6へ反映しました。以下の未承認という記述は以前の区切りの
状態を記録したものです。完全な公開手順と検証の実装は残作業です。

## 契約と実装

根拠は[承認済みMOS/スイッチ契約](mos-switch-proposal.ja.md)2と
[規範的な仕様への対応](p1-execution-contracts-2026-10-25.ja.md)、
IEEE 1800-2023 28.7・28.9・28.13・28.14、VALUE_SEMANTICS 5です。
実装とテストは独自で、外部シミュレータの実装・テストは読まず、依存・大域状態・
既存期待値・ABIの版も変えていません。

`59522c50`で、`valsem/mos.cpp`に点と制御の選択を列挙する参照実装、
`kernel/mos.h`に確保しないC++20のカーネルAPIを追加しました。

- `transferStrengthPoint`は、恒等接続なら符号付き点を保ち、非抵抗性ならsupplyをstrongに抑え、
  抵抗性なら承認済みの強さの写像`[0,1,1,2,2,3,5,5]`を使います。
- `transferStrength`は像の区間全体を返し、両端を保ちます。3種類の写像はすべて単調です。
- `mosStrength`は全6種類の有向MOSに対応します。制御のbit 0でon/off/不確定を選び、
  X/Zでは導通/非導通の両方を許します。CMOSはn/pの並列な脚を合わせてから1回減衰し、
  確実に導通する脚が1本あれば通します。データZはどの制御の組でもZのままです。

参照実装は、許される制御の選択と全データ点を列挙し、CMOSの並列な脚で同じデータ点を共有します。
カーネルは3状態の導通と区間端点の変換を使い、異なる実装を照合します。
不正な種別・点・区間は空のoptionalで返します。無効なデータをdisabledな素子へ与えた場合も、
妥当な0強度として黙って受理しません。この演算はソースcallback・シミュレーション状態・
確保・例外・RTTIを使いません。

制御はスカラーのbit 0だけを使い、W4の未使用bitで意味を変えません。
ヘッダ単独と既存の公開ヘッダ検査は、例外/RTTI無効を含むC++20でコンパイルできます。
既存のC関数表とformatVersionは変えていません。

## 証拠

独自の`MosStrength`10テストは、次を含みます。

- 承認済みの恒等/非抵抗性/抵抗性の表に対する符号付き48点と、全120区間×3写像の360比較。
- MOS 6種類×全区間×制御16組の11,520比較。単一の脚で未使用の第2制御も含みます。
- n/pの極性・並列な脚・不確定制御・データZ・符号・抵抗性の連鎖の具体例。
  不確定なCMOS制御は独立した選択で、同じXという符号化からBooleanの相関を作りません。
- 正側の同値区間と下流の対向ドライバ。`[3,6]`を既知の1だけにすると動作を失うこと、
  4値を変えない強さだけの変化でも下流の解決結果が変わることを確認します。
- 不正種別・端点・逆順区間・極端な点引数、未使用word bitを立てたスカラー制御の受け渡し。

対象のMOS/strength全24テストが通りました。コンパイルした9故障で、supply保持、
抵抗性の強い階級の誤変換、片端への縮退、不確定制御のoff化、並列の直列扱い、二重減衰、
disabled時の不正データ受理、恒等portの減衰、参照実装のp制御極性の誤りを検出しました。
故障時もC++20ヘッダ検査を含む全体ビルドが成功し、コンパイル失敗を検出数には含めていません。
復元後も対象24件が通り、登録済み668故障すべての適用位置も見つかりました。

[故障検出の証拠](../measurements/2026-10-26/mos-strength-controls.json)に、9件すべての
正確なsource hashと失敗テストを残しました。
[全local CI](../measurements/2026-10-26/mos-strength-validation.json)は17確認成功・CTest4段階失敗で、
dev/ASan/UBSanは5,138/5,141、releaseは5,139/5,142成功、各既知スキップ2件です。
失敗は全構成で同じ`cli/options`だけです。MOS/strength・既存dump・PerfCountはすべて通り、
新しいMOSの失敗ではなく、[別途承認待ち](indexed-event-expectation-migration.ja.md)の
index付きeventの旧未対応期待値6件による失敗です。既存期待値は変えていません。
README件数は新しいdev全件JUnitから生成し、この1失敗も残しました。

**後続の期待値だけの接続：** `28f5c972`で[承認済みのevent移行](indexed-event-expectation-migration.ja.md)を
適用し、上記の無関係なCLI失敗を解消しました。
[移行後のlocal CI](../measurements/2026-10-26/indexed-event-migration-validation.json)は全21確認成功です。
MOS実装は変更せず、移行前の失敗も過去の検証として保持します。

## 必要な接続

`0ec3cc57`の[型付きstrength SSA](strength-ssa-execution.ja.md)でmake/MOS/射影をVMへ接続し、
完全なpacked値・型検査・block転送・入れ子呼び出しを検証しました。
[後続のnet公開](strength-store-publication.ja.md)でContext区間記憶域・内部通知・一括公開も追加しました。
契約の残りのstrength load/drive接続、物理driver/originの状態と同一性、
素子への内部通知の接続、有向driverのスケジュールとinertial状態、worldの相関を保つ独立の受動回路solver、
force/releaseとport、書式、ソース実行全体が次の作業です。
点の写像は受動solverの出力元ごとの強さ伝搬に使えますが、このAPIだけでsolverや相関規則を
実装したとは扱いません。有向MOS出力は無向edgeではなく物理的な出力元として保持します。
従来netの初期化と、別途承認済みの新素子の起動方針にも実際のスケジュール接続が必要です。

この純粋演算の段階ではソースの新規比較やIcarusとの差分縮小は主張しません。
過去のIcarus 13合格との差分21件は必要な残作業で、機能全体の一覧ではありません。
母集団・sv-tests・Linux・速度比較も更新していません。P1とIcarus全範囲の対応は未完了で、
旧event CLI移行は承認・適用済みで、別の管理event safepoint案は未承認です。

## 後続の物理origin段階

[物理originの実行](strength-driver-execution.ja.md)、`f93da11b`で、コピーした独立node/origin表、
完全区間の解決、スカラ単位の慣性世代、Active処理の集約をcomponent publisherへ接続しました。
新規16テストとコンパイルした13故障でカーネルの新経路を確認しました。
上のdriver/時間処理の該当残作業はこの進展で置き換わり、以前の測定値はその段階の記録です。
SIR load/driveと静的表、ソース感度、受動graph、登録netのforce/release、port、書式、ソースMOSは残作業です。
公開net値を物理rootに戻したり、向き付き読み出しの循環で受動solverを代用したりするものではありません。

## 後続のSIR網への接続

[宣言した網の実行](strength-network-execution.ja.md)、`cea83470`と`faadf4b7`で、選択したmetadata load、
スカラの即時/遅延origin drive、正規化した静的node/origin宣言、分離した強さ感度をVMへ接続しました。
共通kernelの待機の片付けで、frame破棄前に購読を解除します。新規19テストとコンパイルした16故障で
この接続を検査しました。上の該当する残作業はこの進展で置き換わり、以前の測定はその段階の記録です。
ソース変換/通常driverの取り込み/制御の捕捉、受動graph/solver、登録netのforce/release、port、書式、
ソースの全実行は残作業です。
