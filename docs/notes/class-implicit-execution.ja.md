# comb/latch本体のclass操作

[English](class-implicit-execution.md) · 2026-11-05。

暗黙の感度はどの変化で本体を再実行するかを決めるもので、本体内でクラス操作を受理してもオブジェクトフィールドへの感度は追加されず、詳しくは[comb/latchのFAQ](../FAQ.ja.md#can-comblatch-bodies-use-class-operations)で確認できます。

実装`222f7c47`は、承認済みの[class契約7節](class-runtime-proposal.ja.md#7-event依存と観測)と
[scheduler 6.3](../spec/SCHEDULER_SPEC.ja.md#63-always_comb-の暗黙の感度)に従い、
`always_comb`・`always_latch`本体の対応するclass操作を接続します。新たに測定したIcarus 13.0の差4件が
成功しました。[期待値2箇所の移行](class-implicit-expectation-migration.ja.md)は承認済みで、
別コミット`4e177301`へ適用しました。ClassFrontは23/23件、除外なしのdevは失敗・skipなしの
**12,140/12,140件**です。[承認後の記録](../measurements/2026-11-05/class-implicit-approved-validation.json)を参照してください。
この変更のmain反映・push・slang投稿は依頼されていません。

## 暗黙の読取りと実行する本体

従来の共有SIR検査は、実際の暗黙トリガーが通常の信号でも、本体にobject操作があるcomb/latchを丸ごと
拒否していました。frontendもinstanceメソッド内の読取りを通常関数と同じように伝播していました。

`Lowerer::lowerClassReference`はclass参照の接頭部を1回評価し、その読取り・呼出し先を暗黙の感度から
除きます。メソッドの実引数は通常どおり評価します。読取りの呼出しグラフではinstance・constructorの本体と
virtualのoverrideを除き、通常関数とstaticメソッドの読取りは保ちます。書込みの伝播、assertion actionの
除外、時刻0の実行、既存schedulerは不変です。frontendとnative SIRのloaderの両方で、comb/latch本体の
object操作と、検証済みのトリガーを分けて扱います。

```systemverilog
class Box;
  int value = 7;
  function int read_value(int n); return value + n; endfunction
endclass
module top;
  Box object = new;
  int input_value = 1, observed;
  always_comb observed = object.read_value(input_value);
  initial begin
    #1; $display("%0d", observed);
    input_value = 3; #1; $display("%0d", observed);
    object.value = 20; #1; $display("%0d", observed);
  end
endmodule
```

この独自の例は`8`・`10`・`10`を出力します。propertyや受け手だけの変更では起動せず、後で実引数が
変わると、その時点の受け手・値で本体を実行します。propertyだけを読む本体は時刻0に1回実行し、
暗黙のpropertyトリガーは持ちません。無効なlatchは出力を保持し、通常の入力で有効になると再計算します。
生成、virtual呼出し、受け手のroot、nullエラーは既存のobject/callエンジンを使います。

object依存の明示`@`・wait/iff/watch/monitorのcallback、`always @*`、propertyのref・部分・NBA対象は
実装していません。class handle自体が暗黙トリガーになる形も拒否を保ちます。static数値の読取りは利用でき、
未対応のobject書式など、ほかのclassの境界も保ちます。命令・実行時API・依存・初期化指定は追加しません。

## 承認前の根拠と限界

以下は、承認された期待値移行と件数公開より前の観測を保ったものです。冒頭の除外なしの成功結果が
これに加わり、以前の生の記録は変更していません。

[検証記録](../measurements/2026-11-05/class-implicit-validation.json)に、ソースのハッシュ、実行ファイル、
検査対象、JUnitのハッシュ、コンパイルした変異を結びます。

- 同じ**184プローブ**を[実装前](../measurements/2026-11-05/icarus13-class-implicit-before.json)・
  [実装後](../measurements/2026-11-05/icarus13-class-implicit-after.json)に実行しました。
  従来180件のソース・期待値・native結果は不変です。新しいcomb/latchの4件はIcarus 13.0が通過し、
  nativeも未対応から、記述済みの期待出力へ変わりました。
- **P1評価対象183入力、任意の`real_net`除外1件**です。必須範囲の差は5件から1件、生の差は6件から2件に
  減り、参照不一致25件は別に保ちます。`dense_udp_coverage`はfrontendで上限に達する独立した失敗の
  ままです。新しいP1例外は設けません。
- [生成器](../../tools/gen_class_implicit_matrix.py)の**120ソース**は、宣言場所5種、comb/latch、
  幅1/33/65/129、property・method・生成の本体、4値、property/受け手の変更、無効なlatchを検査します。
  全ソースと生成器の検査が成功しました。
- 新規単体テスト6件は、通常の引数関数とstaticメソッドの読取りの区別、割当て圧力下のvirtual・受け手の
  副作用、constructorの読取り、残る監視の拒否、実行時記憶域を確保する前の偽造classトリガー拒否、
  null呼出しの引数の副作用とエラーを検査し、すべて成功しました。
- **devの選択した12,136件**が失敗・skipなしで成功しました。除いたのは、保持した旧拒否期待2件と、
  全件成功後に件数を公開する必要がある`tools/readme_counts`だけです。変更していないClassFrontの
  期待値では22件中20件成功、別にコンパイルした移行案では**23/23件成功**です。
- **コンパイルした変異9件**を検出しました。新規class/暗黙読取りの7件と、アンカーを更新した既存の
  task・assertion actionの読取り規則2件です。latch専用変異ではcombを有効なままにし、latchのテストで
  検出しました。コンパイル失敗や登録表の自己検査は検出に数えません。ソースのハッシュを復元し、
  重点**350件**が再びすべて成功しました。登録表の1,200アンカーはすべて適用可能で、全登録表の
  変異を実行した意味ではありません。
- 対象の5翻訳単位はclang-tidyが成功しました。既存ソース・期待値・予算・`counts.json`・非公開の
  未追跡草稿は不変です。診断内の手元・インストール先のパスと参照プロセスIDは、hex内も伏せます。
  判定は元のプロセス出力で行います。

固定母集団・sv-tests・7夜のfuzz・外部性能は再測定していません。P1全体の同等性は未確認です。
承認された2箇所の移行は別コミットで適用し、除外なしのdevと件数の公開も完了しました。外部gateは変えていません。
後にpushを依頼されたときは、その前に全local CIを実行します。

```sh
cmake --preset dev && cmake --build --preset dev
ctest --preset dev -R '^(ClassFront\.(Implicit|VMRejectsForgedImplicit)|suite/sched/class_implicit_|tools/gen_class_implicit_matrix)'
python3 tools/gen_class_implicit_matrix.py --check
python3 tools/mutants.py --check
```
