# 固定配列のrefを通じたevent-NBA

[English](event-nba-ref-arrays.md)。2026-10-30。

対応する`ref static`引数を通じて、固定配列の要素をevent-NBAの更新先に選べるようにしました。
組合せ不足N02を[承認済みevent-NBA契約](event-nba-proposal.ja.md)へ接続する変更です。
カーネルの値保持・回数・通知・発行順をそのまま使い、API・命令・依存は追加しません。

右辺、repeat回数、対象の添字は、承認済みの順番で一度ずつ評価します。SIRの参照対象に、
必要な場合だけ仮引数の配列次元と捕捉した添字の番号を保持します。スカラーrefの既存表現・
テキスト出力は変えません。配列対象には`shape=[left:right] ... indices=($n, ...)`を出力します。
SIRは引き続きv1以前の診断用表現であり、parserの文法を新たに固定する変更ではありません。

登録時にVMが、参照する書き込み可能なstatic変数と基点の要素を解決します。配列の形が実際の
残りの記憶域に収まることを検査し、仮引数の宣言順で要素を選びます。具体的な宛先と所有する
値を、既存のカーネルの予約データへ渡します。部分配列のrefを転送し、同等な仮引数の範囲が
異なる場合も、元の基点を保ちます。

```systemverilog
module top;
  int values[3:1];
  event go;
  task automatic issue(ref static int r[3:1]);
    int index=2;
    r[index] <= @(go) 7;
    index=1;
  endtask
  initial begin
    issue(values);
    ->go;
    #1;
    $display("%0d", values[2]);
  end
endmodule
```

`7`を出力します。予約後は、taskの可変な添字や仮引数フレームに依存しません。配列全体への
event-NBAは、全要素で一つの制御・回数を共有します。要素内のpackedなbit・部分選択も、
予約時の位置を保持します。キューを要素とする固定配列には、既存の型付き保持を使い、
taskが戻り、回収が起きても予約した値を保ちます。登録は`wait fork`の子として数えず、
発行後にtaskをdisableしても、既に登録した制御は取り消しません。

範囲外・X/Zの対象添字は書き込みを生みませんが、event制御、回数、guard評価は維持します。
既存のbounds-checkオプションは不正選択を報告します。変数を書き換えるguardに対する
frontendの拒否は保ちます。実際の更新先がない制御でも評価することは、native SIRの
guardで評価回数を観測して独立に検査します。

## 検証と残る制限

[CLIの37ソース・222検査](../../tests/cli/run_event_nba_ref_array_tests.py)で、幅1/32/65/129の
2値・4値、昇順・降順・負の多次元添字、module・interface・program・generate・関数、
戻った／保持されたref、配列全体の値保持、packed選択、不正添字、repeat=0の評価順、
disable、通知、繰り返し登録を検査します。同じソースを初期値4モードでも実行します。

[ユニット6件](../../tests/unit/event_nba_ref_array_test.cpp)は、ソースとASTを破棄してから
実行し、不変の配置情報を検査します。Contextの分離、部分配列の基点、共有する回数、
4,000回の宣言を挟むキューの保持、native guardの評価回数、不正な次元数・型・番号・
要素数のオーバーフロー・leafオフセット、実際のstatic記憶域の境界を検査します。
完了時の予約データ・制御・活動leaseは解放されます。

添字の無視、基点消失、不正添字への書き込み、実際の形状チェック漏れ、空の予約データでの
制御省略、次元数チェック漏れの6種類の故障をコンパイルして検出しました。復元後も検査は
成功し、既存の期待値・SIR dump・PerfCountの予算は変えていません。

静的な[packed親範囲](event-nba-ref-windows.ja.md)にも対応しました（N03）。
[スカラーref経由の単一値全体の遅延NBA](delayed-ref-nba.ja.md)も接続しました（N01のスカラー形）。
ref配列の要素・ref先のpacked選択への遅延NBA、automatic変数・動的コンテナの
要素へのNBA、広いevent値の対応は残作業です。`51cda314`でのN02測定は過去の記録であり、
今回の対応だけでP1全体や外部シミュレータとの完全な互換性を達成したとは判定しません。
