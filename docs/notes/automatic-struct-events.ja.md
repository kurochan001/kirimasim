# automatic構造体のeventメンバー

[English](automatic-struct-events.md) · 英語版が主です。2026-10-30。

automaticなunpacked構造体のスカラーeventメンバーを、既存の型付きフレーム、capture、
管理eventの処理へ接続しました。承認済みの[automatic capture契約1〜6節](automatic-capture-proposal.ja.md)
に沿う接続で、event値の新しい意味、SIR命令、カーネルの依存、VMの処理は追加していません。

既定初期化のeventメンバーは、宣言を実行するたびにそれぞれ固有のオブジェクトを作ります。
入れ子の構造体もメンバー順を保ち、明示的なnullの既定値はnullです。宣言時のコピー、
代入パターンによる初期化、input構造体引数ではeventの同一性を共有し、通常のメンバーは
従来どおり値をコピーします。初期値があるeventメンバーでは、不要なeventを先に構築しません。
メンバーの既定値と、`--init-values`の4モードの意味も維持します。

captureしたメンバーは元のactivationを共有します。待機、`.triggered`、trigger、待機する
input呼出し、遅延アクションの入力捕捉は既存のeventオペランドを使います。所有者が戻った後も
保持された子孫は同じeventを使い、キャンセル時は待機とbindingを解放します。
自動回収によってeventの物理行を再利用します。

```systemverilog
module top;
  typedef struct { event ready; int value; } packet_t;
  task automatic launch(input int number);
    packet_t packet;
    packet.value = number;
    fork
      begin #1; ->packet.ready; end
      begin @(packet.ready); $display("%0d", packet.value); end
    join_none
  endtask
  initial begin launch(17); #2; end
endmodule
```

`launch`が先に戻っても`17`を出力します。

## 維持するevent値の境界

一般のevent再束縛は、引き続き承認済みのソース対応範囲外です。eventメンバーを持つ集約値の
全体代入も、直接のevent値の操作と同じ位置付き未対応診断に揃えました。以前のstatic構造体の
経路では、この制限を迂回できていました。eventを含むoutput/inout構造体引数と、構造体の
戻り値も拒否します。同一性を共有する入口は、宣言時の初期化とinputコピーです。
これは実装の境界であり、これらの形がSystemVerilogとして不正だという意味ではありません。

automaticな構造体配列・固定unpacked配列メンバーの制限は保ちます。スカラーeventの戻り値・
条件式、再束縛、event値全般は残作業です。`51cda314`の組合せ不足の測定は履歴として保持し、
今回の対応はE01と初期化・inputの一部の組合せで、一覧全体の完了とは扱いません。

## 検証

[新規CLI行列](../../tests/cli/run_struct_event_tests.py)の180検査は、実行場所8種類、入れ子、
初期値4モード、コピー・nullの既定値、構築回数、input転送、capture、保留trigger、遅延入力、
回収、位置付き拒否を検査します。nullメンバーへの待機も、既存の実行時エラーを保ちます。

[ユニットテスト6件](../../tests/unit/struct_event_test.cpp)は、ソースとASTの破棄後に実行します。
同一時刻スロットのeventの同一性、並行した保持activation、待機の取り消し、通常の値コピー、
終了時の生存scope・eventの解放、6,000回の構造体宣言での物理行の再利用も確認します。
既存のソースevent境界、SIR出力、PerfCountの期待値は維持しています。

既存の故障注入2件は、コンテナsnapshotの欠落・event既定構築の欠落という故障内容を変えず、
共有した初期化式に合わせて置換箇所だけを更新しています。
既定event構築の欠落、初期値があるeventの余分な構築、集約値・outputの再束縛の有効化、
コンテナsnapshotの欠落、構造体内のeventオブジェクトの誤共有という6故障をコンパイルし、
新規ユニットテストと既存のsnapshot回帰がすべて検出しました。復元後の検査も成功しています。
