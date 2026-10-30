# ref先の配列要素・packed選択への遅延NBA

[English](delayed-ref-selections.md)。2026-10-30。

[スカラーref経由の遅延NBA](delayed-ref-nba.ja.md)を、固定配列refの要素・配列全体と、
スカラー／配列ref先のpackedビット・部分選択へ拡張しました。N01の遅延refで残っていた
これらの形を、[SIR_SPEC 5.3・8](../spec/SIR_SPEC.md)と
[SCHEDULER_SPEC 5・13](../spec/SCHEDULER_SPEC.md)の既存の静的対象・保持・切り詰めの規則へ接続します。

```systemverilog
`timescale 1ns/1ns
module top;
  logic [15:0] values[2];
  task automatic issue(ref static logic [15:0] r[2]);
    int index=1, position=8;
    r[index][position+:8] <= #2 8'h5a;
    index=0; position=0;
  endtask
  initial begin
    values='{default:'0};
    issue(values);
    #3; $display("%h %h", values[0], values[1]);
  end
endmodule
```

出力は`0000 5a00`です。呼出し時の実際の部分配列の基点を保持し、発行時の添字を
仮引数の配列範囲で解決します。ref転送・呼出し終了・発行元disableで宛先や値は変わりません。
右辺は遅延式・対象選択の副作用より前に保持します。期限・発行順・NBA/Re-NBAの起源・
変更通知・回収時のrootは共有カーネルの既存処理を使い、`wait fork`の子にはなりません。

診断用SIR形式0に`nba.ref.elem.delay`と`nba.ref.bits.delay`を追加しました。列挙の末尾へ
追加し、既存の命令番号を保持します。先頭のrefフレーム変数の型記述から要素型・仮引数の
次元を取り、packed形は既存の静的な親範囲も持ちます。検証器は引数数・添字・遅延・
値の領域／幅・親の境界を検査します。VMは実際の静的な対象・配列の残る要素数・型を
検査してから添字のオフセットを加えます。automatic・heapセル・readonlyな宛先は禁止です。

packed選択は親の内側で切り詰めてから格納位置へ変換し、負の位置では対応する右辺の
下位ビットを読み飛ばします。X/Z・表現できない位置・完全な範囲外は書き込まず、
部分的に有効な選択は有効なビットだけ予約します。隣接ビットはNBA適用時点の値を保ちます。
境界警告は親範囲を使い、ソース上の対象ごとに1回です。キュー・動的配列の要素全体の値は
実際の宛先の型記述へコピーし、予約中の回収rootとして保持します。kernel API・依存・
値の規則は追加せず、SIR v1文法確定も別作業です。

## 検証と残る制限

新規[CLI行列](../../tests/cli/run_delayed_ref_selection_tests.py)は独自67ソースの402検査です。
parse・SIR・初期値4モードで、1/32/65/129ビットの2/4値の配列要素、昇順・降順・多次元、
仮引数の異なる範囲と部分配列転送、1/8/65/129ビットのpacked親を検査します。
負・部分的に有効・X/Z・広い位置、部分選択の両方向、packed構造体、配列全体の保持、
コンテナ、real/shortreal/string/null chandle、副作用、発行元disable、Reactive起源のRe-NBAも含みます。

[ユニット8件](../../tests/unit/delayed_ref_selection_test.cpp)はソース・ASTの破棄後に実行します。
`valsem`との独立した91比較、戻った部分配列ref、現在の隣接ビット、独立したContext、
割当てを伴う4,000回の呼出し後のコンテナ保持、不正なSIR、nativeでの寿命・型・要素数の
検査、未知／広すぎる遅延、親範囲の警告の重複抑止、Postponedからの書込み禁止を検査します。
完了時に活動leaseと予約データを解放します。

遅延・実際の基点・仮引数の添字・親のオフセット・右辺の下位読み飛ばしの消失、
実際の要素数／親範囲の検証漏れ、Postponedからの書込み、frontendの親情報欠落の9種類の
故障をコンパイルして検出しました。復元後も検査は成功し、既存の出力・SIR期待値・
PerfCountの予算は変更しません。

親の位置自体が実行時に変わる形、親の定数範囲自体が格納先の外にある形は従来の未対応です。
文字列文字への遅延NBA（N04）、広いevent値、遅延を伴わないref配列／packed選択のNBAも
既存の境界を維持します。automatic・動的コンテナのセルへのNBAは追加しません。
固定したslangのソース上の制限も維持します。`51cda314`のN01測定は過去の記録です。
P1全体・外部シミュレータとの完全な互換性は未確認で、別途の合格条件の検査が必要です。
