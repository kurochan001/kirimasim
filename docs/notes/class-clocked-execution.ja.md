# 明示的なクロック処理の本体でのclass操作

[English](class-clocked-execution.md) · 2026-11-04。

通常の`always @(...)`と`always_ff`で、明示したトリガーの評価がobjectに依存しない場合に、
本体で対応済みのobject操作を実行できるようにしました。承認済みの[class契約5〜7節](class-runtime-proposal.ja.md)に沿い、
測定したIcarus 13.0の差3件を解消します。新しい意味の契約・命令・kernel依存・公開ABIは追加せず、
既存suiteのソース・期待値・アサーションを保持しています。

## トリガーの評価と本体の実行

共通の`sir::classExecutionShapeSupported`は、以前は`ff`・`ff-plain`の本体全体を依存の評価器として
扱っていました。そのため、通常の単純なclockでも、本体がプロパティやメソッドを使うだけで拒否していました。
frontと自作SIRのVM loaderが、トリガーslotとguard/watchの呼出し先全体を、本体とは別に検査します。

本体では、既存のobject・property・call・activation経路を使い、プロパティの読取りと全体へのblocking書込み、
直接・virtual call、確保、automaticのローカルhandle、受け手の一度だけの評価を扱います。適格なstaticの
scalar/vector変数への通常のNBAは、プロパティから読んだ値を保持できます。プロパティ自体へのNBA対象を
追加したものではありません。確保を行う呼出し中も受け手を保持し、nullへのアクセスは既存の位置付き実行時エラーです。

```systemverilog
class Box;
  int value = 0;
  function void bump(); value++; endfunction
endclass
module top;
  Box object;
  bit clock = 0;
  always_ff @(posedge clock) object.bump();
  initial begin
    object = new;
    #1 clock = 1; #1 clock = 0;
    #1 clock = 1; #1 $display("%0d", object.value);
  end
endmodule
```

この独自例の出力は`2`です。プロパティが変わっただけでは、明示クロックの本体を起動しません。通常の信号に基づく
iff条件と式イベントは従来の規則を保ちます。comb/latch/`@*`のobject依存、objectを使うトリガー・guard・watch、
監視のref、プロパティのref・部分・NBA対象、objectの書式など、ほかの境界は未対応です。トリガーから呼ぶ関数が、
別の通常関数を経由してobjectの読取りを隠すことも認めません。待つメソッドの制御は既存のgeneralプロセス経路で、
今回新たに有効にした機構ではありません。

## 根拠と検証

[検証記録](../measurements/2026-11-04/class-clocked-execution-validation.json)に、実装`7d5bdfac`、
追加NBA単体検査`775d99a4`、ソースのハッシュ、dev全体のJUnitを結びます。件数だけの更新は`193373d2`です。

- 同じ**独自180プローブ**を[実装前](../measurements/2026-11-04/icarus13-class-clocked-before.json)と
  [実装後](../measurements/2026-11-04/icarus13-class-clocked-after.json)に実行しました。以前の176入力・期待値・
  native結果は不変です。追加4件は、通常alwaysのメソッド呼出し、always_ffのメソッド呼出し、clock付きの
  プロパティ読取り、待つメソッドの対照例です。Icarus 13.0は4件とも通り、最初の3件がnativeの未対応から成功へ
  変わりました。Icarusのalways_ffに対する合成上の警告は、実行結果と別に記録しています。
- 拡張後の一覧は、**P1評価対象179件・対象外1件**の`real_net`です。必須範囲の差は4件から1件、生の差は
  5件から2件に減り、参照不一致25件は別に保持しています。`dense_udp_coverage`は固定版のフロントエンドで
  時間切れとなる**独立した失敗**のままで、成功やP1除外にしません。固定1,466件の母集団、外部sv-tests、
  fuzzのgateは再測定していません。有限のプローブだけでP1全体の網羅を認定しません。
- `tools/gen_class_clocked_matrix.py`の**新規144ソース**は、宣言場所5種、クロック処理2種、幅1/33/65/129、
  4値のfield・method・確保、通常のiff/式イベント、automaticローカル、受け手の副作用を検査し、全件成功しました。
- 新規単体テスト5件は、数値トリガー、virtual呼出しと確保圧力下の受け手保持、不正なcallback呼出し先4変種の
  実行時割当て前の拒否、nullエラー、129ビットNBA値の保持を検査します。既存期待値の移行は不要です。
- [コンパイルしたnative変異5件](../measurements/2026-11-04/class-clocked-native-controls.json)は、受理の回帰2件と
  既存の拒否・呼出し先検査3件をすべて検出しました。コンパイル失敗や登録表の自己検査は検出件数に含めません。
  ソースのハッシュを復元し、変異用の重点CTest 160件が再び成功しました。その後に追加したNBA単体検査も成功しています。
  登録表は**1,193アンカー**を検査し、適用不能は0件です。今回コンパイルした変異は上記5件だけです。
- devの構成・全ビルド・全テストは、失敗・skipなしの**12,012/12,012**です。公開した診断は、hex出力内も含む
  機械固有のパスと参照プロセスのIDを伏せます。判定は元の出力を使い、伏せ字で期待値や状態を変更していません。

```sh
cmake --preset dev && cmake --build --preset dev
ctest --preset dev -R '^(ClassFront|suite/sched/class_clocked_|tools/gen_class_clocked)'
python3 tools/gen_class_clocked_matrix.py --check
python3 tools/mutants.py --check
```

比較のrunnerは`--iverilog`・`--vvp`で実行ファイルを指定できます。導入済みの13.0を指定し、`--extended`で
保存した一覧を再実行できます。参照シミュレータのソースや外部のテストは読んでいません。slangへの投稿は保留です。
