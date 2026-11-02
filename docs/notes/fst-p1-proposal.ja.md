# P1 の最初の FST 波形出力契約

[English](fst-p1-proposal.md) · 2026-11-03。

状態：**1〜8 節は 2026-11-03 に保守者が承認済み。実装は進行中です。** 既存の拒否期待値の移行は別途レビューします。
[WAVEFORM 1・5・7 節](../spec/WAVEFORM.ja.md)は現在 FST を未実装とし、コマンドラインの
`.vcd` 以外のファイル名を拒否しています。その変更前に必要な詳細改定です。
[CLI 2.2](../spec/CLI.ja.md)はすでに `.fst` を列挙しており、本案で実装境界を揃えます。
依存の準備だけで、以下の意味が承認されたわけではありません。

## 1. 最初の範囲と選択

空でないベース名の `--wave=<file>.fst` で、明示的にバイナリ FST を選べるようにします。
`.vcd` の選択と既存 VCD の期待トレースは維持します。他の拡張子や単独の `.fst`・`.vcd` は
入力エラーです。不正拡張子の診断は既存の文頭を保ち、両拡張子を列挙できます。
`parse`・`--dump-sir` はオプションを受理しますが、波形ファイルを作りません。

`--wave` がなければ、設計の `$dumpfile`・`$dumpvars` は、ファイル名が `.fst` でも従来どおり
VCD を出力します。暗黙の形式切替、既定 FST モード、新オプション、環境設定、外部変換器は追加しません。
コマンドライン VCD と同様、`--wave` 下の `$dumpfile`・`$dumpvars` は従来の無視した呼出しの警告を保ちます。

## 2. 選択と時刻の仕組みを共有

VCD と同じ数値 dump 表、scope 選択、alias の所有関係、制御キュー、scheduler 通知を使います。
FST は直列化 backend であり、別のシミュレーション監視器や scheduler を作りません。

WAVEFORM 2 の既存の静的な値・scope を、宣言順・深さ優先の決定的な順で記録します。
module/interface/program の instance 階層、名前付き block、generate/task/function、
unpacked structure の member scope を保ちます。interface/program/generate/structure の scope kind は
既存 VCD と同じ対応を使います。automatic 変数、unpacked array、動的値・class 値、parameter、
非公開の機能 carrier は従来どおり除外します。`--wave-scope` も同じパターン・外側 scope の規則です。

既定は最初の slot 末尾の snapshot と、その後の slot 末尾の変化を記録します。
`--wave-glitches` は同時刻の複数変化も含め、既存の通知順で実際の各変化を記録します。
NBA/Re-NBA、specify の公開、force/release、final、中断した slot は既存 dump の通知・終了処理に従います。

## 3. FST の値の表現

- 同じ論理 slot は FST handle 1 個とし、他の宣言名は alias にします。
  値が同じだけの別信号は合流させません。
- 整数は宣言幅全体を MSB 順とし、64 bit を超えても 0/1/X/Z を保ちます。
  net は解決済みの公開値です。新しい driver や拡張強度波形の対応を意味しません。
- real/realtime は FST のバイナリ double とし、shortreal は保存した float から拡張します。
  VCD の十進文字列を介さず、数値と符号付きゼロを保ちます。
- 名前付き event は FST event 型を使い、架空の時刻ゼロ trigger は作りません。
  glitch 記録では trigger ごと、既定では従来の slot ごと 1 回の方針を保ちます。
- 時間単位はシミュレーションの precision、時刻は単調な 64 bit tick です。
  run の日付・版を保ち、選択したコマンドラインを comment に残します。
  metadata や圧縮後の byte 列は他の writer と完全一致を求めません。

準備済みの standalone libfst `17f6a5d27b2f576d65b550114fbde3c035cc74a8` の既存 zlib pack mode と
圧縮 hierarchy を使います。圧縮選択のオプション、依存の更新、writer worker thread は追加しません。

## 4. dump 制御

`$dumpoff`・`$dumpon`・`$dumpall` は、通常の変化の後の slot 末尾に、既存キューと無操作の規則で適用します。
off は整数に X を書き、real の未知値と event は書かず、dump の非 active 状態を記録します。
on は active 状態と現在の snapshot を、dump-all は架空の event なしの現在の snapshot を記録します。
FST writer は同じ非 event 値の繰返しをまとめてよく、契約は復号後の状態・event の発生であり、
VCD の section の文字列表現ではありません。dump が off の間は変化を書きません。

`$dumpflush` は writer の flush を要求し、その後の close で読めるファイルに確定します。
同時に開いた FST reader が完全なファイルを読めるとは約束しません。
finish、静止、runtime 上限、中断、final を含む既存の全終了経路で writer を確定・close し、
架空の最終遷移は追加しません。

## 5. FST の容量制限とエラー

FST の `$dumplimit(n)` は libfst の圧縮データの writer 内部の容量制限と到達照会を使います。
ゼロは制限なしです。到達が報告されたら永久に記録を止め、後の task で値を変えても再開しません。
これは **圧縮 block 単位の緩い制限**であり、VCD の text byte 数や、最終ファイルサイズの厳密な上限ではありません。
hierarchy・現在の block・終了処理で指定値を超えます。制限停止は FST comment metadata に記録し、
読めるファイルへ確定します。バイナリに VCD の `$comment` 文字列を混ぜません。
これは WAVEFORM 4 への形式固有の改定案です。

作成失敗・報告された I/O エラーは既存の dump-file 警告後にシミュレーションを続行する方針を使い、
以後の dump 書込みを無効にします。無効な writer/handle を成功と扱わず、API のエラー・状態を確認します。
クラッシュ・ハング・内部エラーは独立した不具合です。kernel 境界を越える例外や、runtime 上限の回避は追加しません。

## 6. 所有と依存

Context が dump ごとに writer と数値 handle 表を 1 組所有します。
書換え可能な global・既定 Context、実行中の環境変数読取り・信号名検索、隠れた process・background writer は
追加しません。外部 C API を見るのは kernel の非公開実装だけです。公開 kernel header は C++20、
C ABI は不変です。準備済みの依存を再利用し、新しい tool・library・install・他 simulator の実装ソースは不要です。

## 7. 合格の確認

独自のテストと libfst reader API で、hierarchy・alias・4 値全体・native real・時刻・event を復号します。
writer と独立に書いた期待トレースを必要とし、既存 VCD と共通の scheduling を比較します。
日付・identifier・同一値 snapshot の重複・real の表示だけを正規化します。
バイナリ hash の一致や、開けるだけのファイルを正しさの判定に使いません。

1/32/65/129/257 bit、2/4 値、宣言 range・packed structure、module/interface/program/generate、
alias・filter、初期・遅延・ゼロ時間・NBA、event、既定/glitch、制御、緩い制限・flush、force/specify の可視性、
全終了経路、選択が空の場合・open 失敗、順次／並行の独立 Context を検査します。
既存 VCD と PerfCount の期待値は維持します。

X/Z・上位 bit の欠落、alias/filter の破損、slot 時刻の誤り、同時刻変化・event の欠落、
off/on snapshot の欠落、確定処理の欠落を変異としてコンパイルして検出します。
ソースの hash と基準実行を復元します。全 dev、関連 release/ASan/UBSan、公開 header、
了承された push 前の全 local CI を確認し、件数は全件成功した dev JUnit からだけ更新します。

## 8. 仕様と期待値の扱い

承認後、1〜8 節を WAVEFORM 1〜7・CLI 2.2 の FST の有効な改定として記録します。
既存 VCD の契約は維持し、実行を証明してから公開 manual・対応表を日英で更新します。

`tests/cli/run_cli_tests.py` は現在、変更しない `display_finish.sv` と `--wave=w.fst` について
終了 2 を期待します。将来の移行先は終了 0、正確な stdout `hello\na`、通常の時刻ゼロの finish 診断、
別途の復号 FST 検査です。入力と他の期待値は維持します。**この契約の承認は、その期待値移行を承認するものではありません。**
native 実行を示して別途レビューし、本体変更とは分離します。
それまでは既存の公開 RunOptions/DumpSetup 経由で native backend を検証し、CLI の拒否期待値を残せます。

## 9. 準備の証拠と限界

既存依存への独自 API probe で、0/1/X/Z、同時刻の 1→0、3 回の event（同時刻 2 回を含む）、
alias 1 件、off/on activity metadata を保持できました。native double probe は real 3 値を復号しました。
32 byte 制限は tick 3 で到達し、読める 543 byte のファイルになったため、最終サイズ ≤ n とは約束できません。
これは library の機能確認であり、**kirimasim の native FST 実行や viewer の相互運用の証拠ではありません**。
独自 probe のソース・ログは専用 worktree の ignored `build/fst-*` にあり、依存の実装本文や上流テストは
実装の参照に使っていません。viewer 互換性と性能は別途測定するまで未確認です。

先に検討した class の packed property 書込みは、Icarus 13.0 も property index の診断で拒否しました。
標準機能の残作業として保持しますが、確認済みの参照対応 P1 機能を優先します。
