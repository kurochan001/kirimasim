# ワイルドカードの関数呼出しと本体の依存

[English](wildcard-function-dependencies.md) · 2026-11-05。

関数本体の実行と暗黙の感度の収集は別の処理であり、本体での読み出しが監視対象になるとは限らない理由は[ワイルドカード関数のFAQ](../FAQ.ja.md#can-a-wildcard-function-read-a-queue-internally)で確認できます。

実装`86507400`で、`always @*`から呼ぶ通常関数の本体で動的配列・queueを読めるようにしました。従来の受入れ検査は、関数本体の読み取りまで
暗黙の監視が必要なものと扱い、実行できる設計を過剰に拒否していました。

既存の[SCHEDULER_SPEC 6.3](../spec/SCHEDULER_SPEC.ja.md#63-always_comb-の暗黙の感度)は、
ワイルドカードの感度とcomb/latchの関数本体の解析を区別しています
（IEEE 1800-2023 9.2.2.2.2、9.4.2.2）。この承認済みの規則の実装であり、
新しい意味の仕様改定や既存期待値の移行は必要ありません。

```systemverilog
int q[$];
int input_value, observed;
function int read_value(input int n);
    return q.size() + n;
endfunction
always @* observed = read_value(input_value);
```

この手続きを再開させるのは`input_value`の変化だけです。実引数が変わったときに現在のqueueを読み、
queueだけが変わっても本体を実行しません。ワイルドカード手続きは従来どおり時刻0で待ちます。
通常関数をさらに呼ぶ場合や、対応するclassのstaticメソッドでも同じ規則です。
実引数が変われば、関数の戻り値が前回と同じでも本体を実行します。
待機を始める経路が依存の検出のために関数を評価することはありません。名前付きeventの`triggered`を読む関数の境界は従来どおり保ちます。

## 対応範囲と保持した境界

frontendがワイルドカードの暗黙監視を検査するときも、既存のトリガー生成と同様に、
呼び出した関数本体を除外します。実際の式の読み取りは検査を保ちます。
queue・配列の直接読み取り、実引数で読むcontainer変数、`always_comb`・`always_latch`・
連続代入が呼ぶcontainer読み取り関数は、従来の未対応診断を保ちます。
以前の[queueサイズの直接の差2件](../measurements/2026-11-05/container-implicit-followup.json)も残作業です。object依存のワイルドカード評価も拒否を保ちます。

kernel、VM、SIR表現、外部ABI、依存、初期化方針は変更していません。
`wire real`の例外も不変です。一般の暗黙heap監視を実装したわけではありません。

## 独立した検証

[188プローブの変更前](../measurements/2026-11-05/icarus13-star-functions-before.json)と
[変更後](../measurements/2026-11-05/icarus13-star-functions-after.json)は、同じソースと独立して定めた期待値を使います。
明示的に選んだIcarus compiler/runtime **13.0 stable（`v13_0`）**が通る新規4件を、
nativeの拒否から期待一致へ移しました。queue・配列を読む関数、関数の多段呼出し、引数なしの形です。
以前の184入力のソース・期待値・native結果はすべて不変です。
P1評価対象は**187件**、対象外は依存を確認した`wire real`の**1件**で、測定した必須範囲の差は**5→1件**です。
残るdense UDPのfrontend上限は独立した不具合として保持します。生の差2件、参照不一致25件も保ち、
有限の一覧でP1完了を宣言しません。

[生成器](../../tools/gen_star_container_function_matrix.py)で**96ソース**を追加しました。
module・unit・package・interface・generate・class staticの宣言、queue・動的配列、
幅1/33/65/129、直接・多段の関数呼出しを組み合わせます。時刻0の待機、伸長・変更・削除後の
現在値、本体内の読み取りだけでは再開しないこと、戻り値が不変の実引数変更、X/Zを検査します。
nativeは**0/96→96/96件**へ改善しました。別の
[ソース比較](../measurements/2026-11-05/star-function-source-matrix.json)では参照一致48件・参照側のコンパイル拒否48件を記録し、
参照側の結果をnativeの期待値へ置き換えません。

追加単体4件は、実際の数値トリガーのbinding、既存のevent関数の診断、
実際のcontainer読み取りの診断、class監視の境界を検査します。変更していない動的containerのCLI診断・event境界3件・
生成器検査を含む、**対象CTest 105件**がすべて成功しました。

変異2件を実際にコンパイルして検出しました。関数本体の受入れ検査を元に戻すと正常系97件が失敗し、
実際のcontainerの拒否を省くと境界の2件が失敗します。いずれもビルドに成功したうえで意味・診断の検査が失敗し、
元のソースSHAと105件成功の基準実行を復元しています。
[検証metadata](../measurements/2026-11-05/star-function-execution-validation.json)に変異とビルド・テスト結果を記録します。
既存suiteの本体、期待stdout、assertion、性能の予算は変更していません。

除外なしのdevは**12,245/12,245件成功**、失敗・skipなしです。release・ASan・UBSanも関連105件がそれぞれ成功し、変更したC++翻訳単位2件のclang-tidyは診断0です。push前には全local CIが必要です。

```sh
ctest --preset dev -R '^(ImplicitFront[.]|suite/sched/star_container_function_|tools/gen_star_container_function_matrix|cli/dynamic_containers$)' --output-on-failure
python3 tools/gen_star_container_function_matrix.py --check
python3 tools/mutants.py --check
```

公開記録ではhex出力内を含む機械のパスを置換し、判定には元の出力bytesを使います。
外部母集団・sv-tests・fuzz・性能の測定日は元のまま保ちます。slangへの投稿も保留を維持します。
