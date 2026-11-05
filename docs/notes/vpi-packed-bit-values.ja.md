# VPIのpackedビットを符号なしで扱う

[English](vpi-packed-bit-values.md) · 2026-11-05。

packedな1ビットの選択は親が符号付きでも符号なしのviewを持ち、出力への影響は[VPIビット選択のFAQ](../FAQ.ja.md#does-a-vpi-bit-selection-inherit-its-parents-sign)で確認できます。

実装`64b31665`で、Icarus 13.0が対応するP1の値の境界を修正しました。
`vpi_handle_by_index`がpackedな親の符号を選択ビットへコピーしていたため、
符号付きベクトルの1を`vpiIntVal`・`vpiRealVal`・`vpiDecStrVal`で読むと−1を返していました。
修正後は1を返し、親の宣言上の符号と値は保ちます。

既存の[VALUE_SEMANTICS 2](../spec/VALUE_SEMANTICS.ja.md#2-式の型と幅)
（IEEE 1800-2023 11.8.1）の選択結果を符号なしとする規則と、承認済みの
[VPIのobject・値の契約](vpi-p1-proposal.ja.md#3-関数オブジェクト値)に従います。
仕様改定や既存期待値の移行は必要ありません。

## 対応範囲と検査

選択したviewの型を正規化し、既存のmetadata・変換・callbackで共有します。
親宣言、物理binding、宣言上の添字、隣接ビットと既存の書込み権限を保ちます。
ABI、依存、スケジューリング、forceの方針は追加していません。

独自の[生成器](../../tools/gen_vpi_packed_bit_matrix.py)は**432ソース入力**を作ります。
module・子module・generate・interface・program・static task、`logic`・`bit`・`wire`、
親の符号の有無、負の添字を含む昇順・降順の範囲、幅1/32/33/65/129/257を組み合わせます。
引数と名前から取得するhandle、両端・中央の添字、数値・vectorでの読み取り、承認済みの
自動scalar形式、親の保持、選択ビットの変更、隣接ビットの保持、型に応じた0/1/X/Zの
callback値を検査します。

[行列の記録](../measurements/2026-11-05/vpi-packed-bit-matrix.json)にはソースのhash、
独立して定めた期待値、nativeの変更前後の結果を残します。同一入力は**216/432→432/432**へ改善しました。
修正した216件のうち144件は、参照側の数値読み取り用の検査とも一致します。
残り72件は、参照側がビットhandleを返さない2値の親を使います。

追加した単体テスト3件は、親を変えない変換、両方向のpackedメモリ要素の格納先・添字、
符号なしのcallback値とX/Zを区別する変更を検査します。符号の正規化を省く変異を実際に
コンパイルし、追加したCTest 4件すべてで検出しました。元のソースSHAへ戻し、基準の4件も
再び成功しています。[検証metadata](../measurements/2026-11-05/vpi-packed-bit-validation.json)に
変異とビルド・テストの結果を記録します。

除外なしのdevは**12,144/12,144件成功**で、失敗・skipはありません。
release・ASan・UBSanも**VPI関連のCTest各52件**が成功しました。最後に単体テスト内だけの
offsetの型を整え、新規単体3件を4構成すべてで再確認しています。変更した翻訳単位2件の
clang-tidyは診断0です。後に承認されたpushを行う前には、全local CIが必要です。

## 参照側の制限とP1の残作業

明示的に選んだIcarusのcompiler/runtime **13.0 stable（`v13_0`）**は、数値読み取り用の
**288/432件**で一致します。残る2値の144件はビットhandleがnullになり、参照側の非合格の観測として残します。
参照用の検査ではcallbackと`vpiObjTypeVal`を別にします。Icarusはこれらのビットcallbackを拒否し、
自動出力はintegerを選びますが、承認済みのhost方針はscalarです。nativeの強い検査には両方を残します。
参照側の制限を新しいP1対象外やnativeのPASSへの置換には使いません。

以前の184プローブと`wire real`の例外1件は、範囲・測定日を保ちます。
このVPI行列は別の仕組みの棚卸しで、P1は未完了です。
別の[2ケースの追跡調査](../measurements/2026-11-05/container-implicit-followup.json)では、
`always_comb`・`always @*`のqueueサイズの暗黙感度をIcarusが通す一方、nativeは拒否することを確認しました。
これらは184プローブ外の追加の必須範囲の差で、任意の拡張ではありません。
object依存の監視、残るspecifyやVPIの形も残作業です。slangへの投稿は保留を維持し、
非公開の草稿は変更していません。

```sh
python3 tests/cli/run_vpi_packed_bit_tests.py --kirimasim build/dev/src/driver/kirimasim
ctest --preset dev -R '^(Vpi.Packed|cli/vpi_packed_bit$)' --output-on-failure
python3 tools/mutants.py --check
```

数値読み取りの参照検査では、明示的に選んだIcarus 13.0の`--iverilog`・`--vvp`実行ファイルも指定します。
コマンドの成功はnativeの期待一致を表します。参照側は`--report`の`reference_matches`を別に確認してください。
432ソースはCLIのCTest 1件であり、CTestを432件追加した意味ではありません。
外部母集団、sv-tests、fuzz、性能の証跡は今回測り直していません。
