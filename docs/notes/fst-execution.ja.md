# P1 の native FST 波形実行

[English](fst-execution.md) · 2026-11-03。

VCDとFSTは実行モードではなく記録形式の違いであり、形式の選択と記録できる信号・制御の区別は[波形のリファレンス](../REFERENCE_MANUAL.ja.md#波形のリファレンス)で確認できます。

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

## 空のFSTとUBSan（2026-11-03）

Linux/GCCのUBSanで、[CI](https://github.com/kurochan001/kirimasim/actions/runs/41559370674/job/125665981343)と
[Nightly](https://github.com/kurochan001/kirimasim/actions/runs/41556717656/job/125657296364)の両方が、
選択した変数が0本のときにlibfstの`fwrite`へNULLのバッファが渡ることを検出しました。
空波形の2テストと`cli/options`が失敗しており、承認済みFST契約3〜5節が求める、
空のファイルも正しく完成させる動作に反していました。

`cmake/fst_stdio.h::kirimasimFstWrite`で、要素サイズか件数が0の書き込みをlibc呼び出し前に処理します。
[WG14 N1548の7.21.8.2](https://www.open-std.org/jtc1/sc22/wg14/www/docs/n1548.pdf)のゼロ長書き込みに
従い、ストリームを変更せず0を返します。`cmake/fstapi_wrapper.c`がlibfstのコンパイル時だけ
このアダプタを適用し、取得したソースと固定版は変更しません。通常の書き込みはlibcの戻り値と
エラー処理を保ちます。全圧縮方式・終了経路がこの1か所を通り、sanitizerの検査も保持します。

[回帰・変異の記録](../measurements/2026-11-03/fst-empty-ubsan-controls.json)には、ゼロ長の
バッファ・サイズ・件数の組合せ6件、正確なバイト列・要素数の検査2件、空ファイルの
圧縮方式・スコープ・時刻・flushの組合せ12件、既存の非空の圧縮方式3件を記録しています。
コンパイルした変異2件をApple clangとGCC 16の両方で検出し、ソースhashの復元後に再び成功しました。
GCCの検査はmacOS上でLinuxと同じ`fwrite`のnonnull宣言を明示しています。
保護を外すと実際の空writerで同じUBSan診断を再現します。修正後のLinux実行は、
Linux/GCCのUBSan CIを実施するまで**未確認**です。既存の実行期待値・テスト件数・P1ゲートは保持します。

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

ビルド後、`ctest --preset dev -R '^(Fst|WidthsDomainsModesAndScopes/FstMatrix|cli/options|tools/fst_dependency)'` で再現できます。
reader はテスト専用で、利用者向けの変換コマンドは追加しません。最終 local CI は `ccded6b8` の
clean な worktree で全21項目が成功しました。dev・ASan・UBSan は各11,585件、release は11,586件が
skip なしで成功し、全189ファイルの clang-tidy も通りました。検証記録の `final_local_ci` に結果を保存し、
その後は文書と記録だけを更新しています。元の176プローブの Icarus 比較、外部 sv-tests、
固定1,466件の母集団、7晩の fuzz 証拠、任意の `wire real` 例外を、この実装で再測定・再分類していません。
**P1 は引き続き進行中です。**
