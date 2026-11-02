# P1 の native FST 波形実行

[English](fst-execution.md) · 2026-11-03。

`--wave=<file>.fst` で native バイナリ FST を出力できます。保守者による
[契約1〜8節](fst-p1-proposal.ja.md)の承認後、[既存 CLI 期待値1件の移行](fst-expectation-migration.ja.md)も
別途承認を得ました。backend・driver の有効化・期待値移行は別コミットです。
新しいライブラリ・ツールのインストール・依存の版変更は不要でした。

## 動作と制限

Context が private な libfst writer を所有します。FST は VCD と数値の対象表、
物理 alias、scope filter、制御のキュー、scheduler の通知を共有します。公開 kernel ヘッダは
C++20 のままで、依存の C API を公開しません。既存 standalone libfst の固定版
`17f6a5d27b2f576d65b550114fbde3c035cc74a8` を、zlib 圧縮・圧縮階層・writer の
worker thread なしで使います。scheduler や実行時の名前検索を追加していません。

整数値は宣言の全幅と0/1/X/Zを保ちます。real/realtime は native double、shortreal は
格納した float から拡張します。既定では slot 終端の値、glitch モードでは同時刻の実効変化と
各 event 発生を記録します。off/on/all・flush・終了・final は既存の共通 dump 経路を使います。
private な specify carrier と、未対応の動的データ・class の格納は引き続き除外します。

FST の `$dumplimit` は**圧縮 block 単位の緩い容量制限**で、最終サイズの厳密な上限ではありません。
階層・現在の block・終了処理で超過する場合があります。0は無制限、到達後は恒久的に記録を止め、
FST の comment metadata に記録します。`$dumpflush` は flush を要求し、close で読めるファイルに
完成させます。作成失敗と検出した writer error は既存どおり警告して続行します。
クラッシュ・ハング・内部エラーは独立した不具合です。

明示的な CLI の `.fst` 指定だけが FST を選びます。`--wave` なしのソース内の
`$dumpfile("name.fst")` は VCD のままです。`parse`・`--dump-sir` は指定を受理しますが、
波形ファイルを作りません。空のベース名と他の拡張子は入力エラーです。
viewer 互換性と性能は**未確認**です。実際の viewer と測定用 workload で別途確認してから評価します。

## 実行の証拠

[検証記録](../measurements/2026-11-03/fst-native-validation.json)に、検査したソースの hash と
コミットの境界を保存しました。

- **native 138件**。幅1/32/65/129/257、2値/4値、slot 終端/glitch、6種類のソース文脈
  （module・interface・program・generate・名前付き block・static task）の行列120件を含みます。
- 独立した reader で、値・時刻・alias・metadata・符号付き0を含む実数・event・filter/空の選択・
  制御・制限・force/specify・終了・エラー・並列の独立 Context を検査しました。共通の scheduling
  ケースは VCD とも比較し、既存 VCD の期待値は変更していません。
- 別途の CLI 移行前に dev 全体 **11,585/11,585** が失敗・error・skip なしで成功しました。
  release/ASan/UBSan は、新しい FST と既存 dump/VCD を含む**重点178件ずつ**が成功しました。
  これは重点検査であり、sanitizer の全件実行を意味しません。
- **コンパイルした意味の変異10/10件を検出**し、ソース hash と138件の正常系を復元しました。
  [変異の記録](../measurements/2026-11-03/fst-native-controls.json)はコンパイル成功と assertion の
  失敗を分け、ビルド失敗を検出件数に加えません。変異カタログ全1,136件の anchor も適用できます。
- 公開ヘッダの C++20 ビルド、禁止語検査、重点 clang-tidy が成功しました。reader が返す符号付きの
  数値の timescale 指数を保つため、理由を記載したテスト内1行だけを抑制し、検査全体は無効化していません。
- 変更しない `display_finish.sv` を実際の CLI で実行し、終了0・正確な stdout `hello\na`・
  通常の時刻0の finish 診断・変数0件の読める FST を確認しました。追加の CLI 検査は129ビットの
  明記したトレースを通常/glitch の両モードで復号し、非実行モードと不正名も検査します。
  変数0件の正常なファイルは、decoder で VCD 階層テキストが出なくても正常です。

ビルド後、`ctest --preset dev -R '^(Fst|All/FstMatrix|cli/options|tools/fst_dependency)'` で再現できます。
reader はテスト専用で、利用者向けの変換コマンドは追加しません。承認された push の前には
local CI 全体を実行し、要約を別途報告します。元の176プローブの Icarus 比較、外部 sv-tests、
固定1,466件の母集団、7晩の fuzz 証拠、任意の `wire real` 例外を、この実装で再測定・再分類していません。
**P1 は引き続き進行中です。**
