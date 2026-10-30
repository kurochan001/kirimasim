# automatic構造体の固定配列メンバー

[English](automatic-struct-array-members.md)。2026-10-30。

automaticなunpacked構造体で、固定unpacked配列のleafメンバーを扱えるようにしました。
多次元配列と入れ子の構造体にも対応します。組合せ不足A03を、既存の次元付きフレームと
[承認済みautomatic記憶域の契約2–3](automatic-capture-proposal.ja.md)へ接続する変更です。
SIR命令・カーネルの値の意味・依存・VMの経路は追加しません。

整数、幅広い4値ベクトル、real、文字列、動的配列・キューを要素とする固定配列を検査しました。
宣言実行ごとに、宣言した次元順で各要素を初期化します。宣言やメンバーの明示的な初期値を
優先し、対象となる4値要素には既存の`--init-values`を個別に適用します。
2値要素の既定値は従来どおりです。

宣言コピー、代入パターン、集約代入、構造体の戻り値には、既存のleaf・要素ごとの保持規則を
使います。構造体のinput配列leafは、スカラーとして読み出さず、型付き配列のオペランドとして
渡します。input・output・inoutは形と値のコピー規則を保ち、outputのコピー戻しは一度だけです。
対応する配列メンバーは通常の配列引数や配列refにも渡せます。構造体全体のrefは追加しません。

```systemverilog
module top;
  typedef struct { int words[2]='{3,5}; } payload_t;
  task automatic show(input int number);
    payload_t value;
    value.words[0] += number;
    #1;
    $display("%0d", value.words[0] + value.words[1]);
  endtask
  initial show(7);
endmodule
```

`15`を出力します。呼出しやブロック再入ごとの記憶域は独立し、捕捉した兄弟プロセスは元の
配列メンバーを共有して変更を観測します。親が戻っても必要な記憶域を保持します。
コンテナ要素は既存の活動のroot公開に従います。ブロッキング代入内の待機中に元の配列が
変更されても、代入用に保持した右辺の値は変わりません。

## 残る制限

automatic構造体配列、構造体配列をメンバーに持つ形、構造体配列の引数は未対応です。
固定event配列のメンバーも既存の拒否を保ちます。通常の配列メンバーとscalar eventを
同じ構造体に置く形は対応し、eventは既存の保持経路を使います。event再束縛、eventを含む
output/inout構造体引数、構造体全体のref、一般の配列スライスへのコピー戻しは追加しません。

`51cda314`でのA03測定は過去の記録です。今回の完了は固定leaf配列の接続であり、組合せ不足
全体やP1全体、外部シミュレータとの完全な互換性の達成を意味しません。

## 検証

[CLIの25ソース・150検査](../../tests/cli/run_struct_array_member_tests.py)で、宣言先8種類、
入れ子、昇順・降順・負の添字、多次元の幅広い値、文字列・real、既定値、引数と戻り値、
配列メンバーのref、コンテナのコピー、不正な添字、捕捉、待機中の保持を検査します。
初期値4モードはローカル変数・outputの既定値・inputの配列転送でも確認します。

[ユニット5件](../../tests/unit/struct_array_member_test.cpp)は、ソースとfrontendのASTを
破棄してから実行します。捕捉した配列のメタデータ、反復ごとの独立性、待機通知、一度だけの
コピー戻し、Context間の分離、親の終了後にコンテナ配列とscalar eventを保持したまま行う
4,000回の宣言を検査します。終了時の活動lease・eventは0で、event行は再利用されます。
既存の拒否・出力・SIR dump・PerfCountの期待値は変更しません。

コンパイルした故障5種類（配列の既定値漏れ、要素間の乱数初期値使い回し、inputのスカラー転送、
メンバー既定値漏れ、配列内コンテナの保持漏れ）を検出し、復元後のテストも成功しました。
