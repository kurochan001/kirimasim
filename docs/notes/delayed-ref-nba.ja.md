# スカラーrefを通じた遅延付きNBA

[English](delayed-ref-nba.md)。2026-10-30。

スカラーrefを通じた単一値全体への遅延付きノンブロッキング代入で、呼出しが戻る前に
静的な宛先と右辺を保持するようにしました。組合せ不足N01のスカラー形への対応です。
[SIR_SPEC 5.3・8](../spec/SIR_SPEC.md)の静的な対象の規則と、
[SCHEDULER_SPEC 5・13](../spec/SCHEDULER_SPEC.md)の既存の保持・時刻の規則を使います。

```systemverilog
`timescale 1ns/1ns
module top;
  int value;
  task automatic issue(ref static int r);
    r <= #2 7;
  endtask
  initial begin
    issue(value);
    #3; $display("%0d", value);
  end
endmodule
```

出力は`7`です。発行したtaskは待機せず、予約した更新は`wait fork`の子にはなりません。
発行元が戻ったりdisableされたりしても更新を保持します。静的な固定配列から選んだ
合法な要素をスカラーrefの実引数にし、別のref引数へ転送することもできます。
呼出し時の実引数を保持するため、後で添字を変更しても宛先は変わりません。

frontendは遅延式を評価する前にコンテナの右辺を保持します。VMは実際のstaticなslotと
要素を一度だけ解決し、値の型を検査して、コンテナを実際の宛先の型記述へコピーします。
予約後の値・期限・NBA/Re-NBAの起源は共有カーネルが保持し、変更通知・回収時のroot・
発行順・時刻オーバーフローの既存処理を使います。0とX/Zの遅延は既存の遅延0の規則です。
表現できる時刻を超える既知の遅延はソース位置付きの設計エラーにします。

診断用SIR形式0に`nba.ref.delay ref, [leaf,] value, delay`を追加しました。任意のleafは
32ビット以内の非負定数です。検証器は引数数、参照・値・遅延の型、型付きコンテナへの
アクセスを検査します。命令は列挙の末尾へ追加し、既存の命令番号を保持しています。
別作業のv1文法確定を意味せず、kernel API・依存・値の意味は追加しません。

## 検証と残る制限

新規[CLI行列](../../tests/cli/run_delayed_ref_nba_tests.py)は独自30ソースの180検査です。
parse、SIR生成、初期値4モードで、1/32/65/129ビットの2/4値、137ビットのpacked構造体、
real/shortreal、string、null chandle、キュー・動的配列全体を検査します。静的配列の
実引数、function/interface/program/generate、ref転送、保持されたref、発行元disable、
同期限の順序、Reactive起源のRe-NBA、右辺と遅延式の副作用も含みます。

[ユニット7件](../../tests/unit/delayed_ref_nba_test.cpp)はソース・ASTの破棄後に動かします。
戻った呼出しの広い実引数、独立したContext、割当てを伴う4,000回の呼出しを挟んだ
コンテナ保持、不正なSIR、未知・広すぎる遅延、実際の型・leaf境界、Postponedからの
書込み禁止を検査します。native命令の挿入でautomatic・heapセル・readonlyな実引数の
拒否も確認します。完了時の活動leaseとNBA予約データは解放されます。

遅延の消失、実引数の要素の取り違え、寿命・型の検査漏れ、Postponedからの書込み、
右辺保持の欠落、遅延型の検査漏れの7種類の故障をコンパイルして検出しました。
復元後も検査は成功し、既存の期待値・SIR dump・PerfCountの予算は変更しません。

[固定配列ref・packed選択への遅延NBA](delayed-ref-selections.ja.md)も接続しました。
文字列の文字要素への遅延NBAは未対応です。automatic・heapセル・readonlyな宛先はエラーで、event値の代入も
追加していません。固定したslangがNBAのautomatic変数参照や`ref static`実引数に課す
ソース上の制限も維持します。`51cda314`でのN01測定は過去の記録として保持します。
P1全体の完了・外部シミュレータとの完全な互換性は未確認で、別途の合格条件の検査が必要です。
