# 集約値のブロッキング代入内時間制御

[English](blocking-aggregate-timing.md)。2026-10-24に`6195846`で実装しました。
期待値の別レビュー用ベース`36aa2ae`と組み合わせた`396131e`で検証しています。

以下のイベント制御付きNBAの制限は、この検証時点のものです。現在の対応範囲と残る制限は
[ソースevent-NBAの変換](event-nba-source-execution.ja.md)に記載しています。

## 契約と実装

固定配列・unpacked構造体・構造体配列全体へのブロッキング代入で、遅延・イベント・
繰り返しイベント制御を使えます。SCHEDULER_SPEC 8、VALUE_SEMANTICS 3.1/3.13、
IEEE 9.4.5の既存規則を実装し、意味の新しい選択・SIR命令・カーネル依存・VMの
実行経路は追加していません。

frontは制御を評価する前に、右辺の全leafを専用の型付きフレームへ保持します。
コンテナは独立したスナップショットにし、後のパターン式による元の変更も隔離します。
フレームは待機をまたぐ型付きGCルートです。制御後に実際の左辺の添字を1回評価し、
保存した値を宣言順にコピーします。SSA一時値は待機をまたぎません。同時に呼ばれる
automatic taskは別の活動の格納域を持ち、対応する配列全体のrefも同じ経路を使います。

代入互換な構造体配列のコピーも、左右それぞれの宣言範囲で位置を対応付けます。
以前は代入先の添字を元にも流用し、範囲が違うと既定値をコピーしていました。
通常代入・即時/遅延付きNBA・今回のブロッキング時間制御を訂正し、入れ子と実行時の
prefixも保ちます（7.6）。

イベント制御付きNBAは未対応で、別の[所有権・更新順の案](event-nba-proposal.ja.md)が
必要です。ブロッキング待機で独立した保留制御を代用しません。明示的なautomaticの
構造体の[固定leaf配列メンバー](automatic-struct-array-members.ja.md)には対応しました。
automatic構造体配列とevent配列メンバーの宣言制限は維持します。ストリーミング代入先や
未対応の配列スライスは有効にしません。

```systemverilog
`timescale 1ns/1ns
module top;
  int a[2], b[2];
  initial begin
    b = '{7, 9};
    a = #2 b;
    $display("%0t %0d %0d", $time, a[0], a[1]);
  end
  initial begin #1; b = '{40, 41}; end
endmodule
```

出力は`2 7 9`です。

## 独自ソースと期待値の別レビュー

[`gen_blocking_aggregate_timing_matrix.py`](../../tools/gen_blocking_aggregate_timing_matrix.py)が
独自74ソースとstdout/stderr期待値を生成します。

- 64件：initial・static/automatic task・fork・interface・package・program・generate、
  1/32/65/129ビット、2値/4値。各件で`#d`・`#0`・エッジ・名前付きと信号の混合repeat・
  0回・実行時の対象・配列メンバー・実数/文字列・不正な対象を検査します。
- 10件：同時のautomatic ref呼び出し、異なる宣言範囲の多次元配列、構造体配列と
  その入れ子、通常/NBAの範囲対応、コンテナルートとパターンの副作用、混合`iff`、
  狭い/広い非正数・X/Z回数、129ビットの保留、Xを含む対象添字を検査します。

保持した変更前バイナリの表示版は`8616ee56b510`です。新規72件を未対応とし、2件は
値を誤りました。新実装では74件すべて成功し、既存のSIR出力29件も期待値を変えず成功します。

既存のCLI入力`unsupported_matrix/intra_delay_array.sv`には合法な遅延付き代入が
6本あり、従来は未対応診断を期待していました。9.4.5と承認済みのスケジューラ契約では
実行が正しく、古い拒否期待値が更新対象です。**期待値の変更は`src/`変更のない
`36aa2ae`に分離し、別PRにします。** 入力ソースは1バイトも変えず、既存の受理入力
一覧へ移し、終了コード0・stdoutと診断が空であることを検査します。実装PRはこの
レビュー用ブランチをベースとし、古い期待値の変更を含めません。新しい値検証のソースで、
受理だけを実行の正しさとして扱わないようにします。実装がない期待値だけのブランチでは、
この過去入力の拒否によりテストが失敗することを予期しています。

## 検証

両レビューを組み合わせたdevでは3,055件中3,053件成功し、既知の2件はスキップです。
意図的な誤実装5種類もコンパイルし、すべて新しいstdout期待値の不一致で検出しました。
制御後の捕捉・制御前の対象評価・再開後の右辺再評価・最初のleafの欠落・元への対象添字の
流用です。アンカー・判定器の自己検査は検出証拠から除外し、新規ソースだけの実行でも
5種類すべて検出しました。その後ソースを復元して再ビルドしています。
全構成の結果は[最新進捗](p1-icarus-progress.ja.md)に記録します。
機能比較には配列全体の遅延・繰り返しイベント・構造体配列の範囲対応の3件を
追加します。参照のコンパイル失敗とIcarusだけが通る差分を区別し、これらの追加だけで
P1全範囲を証明したとは扱いません。過去の性能はここでは測り直していません。

```sh
python3 tools/gen_blocking_aggregate_timing_matrix.py --check
ctest --preset dev -R blocking_aggregate_timing --output-on-failure
python3 tools/mutants.py --only 'blocking aggregate timing:'
python3 tools/mutants.py --only 'structure array copy:'
```
