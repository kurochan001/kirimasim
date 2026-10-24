# Icarus 13.0と開発版による比較

[English](icarus-13-comparison.md)。2026-10-24、`c6d9c3d`（fork宣言の変換、文書更新後の表示`392687e`）のdevビルド、
macOS/arm64で計測しました。追加インストールはせず、他のシミュレータの
ソース・テストも読んでいません。

## 確認した版と結果

既存のバイナリで、コンパイラdriver・プリプロセッサ・parser/elaborator・runtimeが
すべて`13.0 (stable) (v13_0)`と表示することを確認しました。追加の比較対象は、
コンパイラ・runtimeともに`14.0 (devel) (s20260301-411-gca6b8d376)`です。
両方で[`icarus_feature_probe.py`](../../tools/icarus_feature_probe.py)の独自84プローブを実行しました。

- **13.0 stable：** Icarusだけが通る差分24件、参照結果の不一致4件。
  [個別の可搬的な記録](../measurements/2026-10-24/icarus13-array-port-probes.json)。
- **14.0 devel：** Icarusだけが通る差分30件、参照結果の不一致7件。
  [個別の可搬的な記録](../measurements/2026-10-24/icarus14-array-port-probes.json)。

開発版だけの追加6差分は、配列の和、キューの並べ替え、最小・最大の重複なし・空の
プローブです。13.0はこれらをコンパイルできません。重複・withキーの最小最大の3件も
13.0では実行されず、開発版で観測した追加3件の参照結果の不一致にはなりません。
件数の違いは比較する版の違いです。kirimasimの未対応機能が減った数にはせず、
開発版で観測した範囲と承認待ちの意味の修正案も引き続き追跡します。

共通の参照側の不一致4件は、packedな親境界の選択、virtual dispatch、
動的配列の問い合わせ、1ビットX/Z回数の繰り返しイベントです。
参照が不一致でもkirimasimの未対応がなくなるわけではなく、クラスのvirtual dispatchは
依然未対応です。JSONは両エンジンの状態を独立に記録し、P1の作業から消しません。

一覧は規格の仕組み81本、右辺順序を検査する`project_policy` 2本、
`icarus_extension` 1本（`wire real`）です。順序の2本ではIEEEは左右のどちらを先に
評価してもよく、期待値は既存の承認済みkirimasimの順序です。
拡張とプロジェクトの方針は、規格共通の要件とは区別して記載します。

## 固定長配列ポートと解決対象のネット配列を一覧へ追加

`fixed_array_ports`と`resolved_net_array`は両参照で通り、nativeでは位置付きの未対応診断です。
以前の82件の状態と通過した比較出力は同じです。本体のソースは変えず、一覧の拡張で
差分は22/28件から24/30件へ増え、参照不一致4/7件の一覧は変わりません。
以前の82件の[13.0](../measurements/2026-10-24/icarus13-automatic-capture-probes.json)と
[開発版](../measurements/2026-10-24/icarus14-automatic-capture-probes.json)は、当時の件数と由来を保ちます。

[配列ポート・解決の修正案](fixed-array-ports-proposal.ja.md)には、違法入力7本を含む
独自59本を別に記録しました。モデルと参照の差はレビュー用の証拠で、実装や期待値の
承認ではありません。宣言順の別名対応、連続的な型付きコピー、要素ごとの解決、
未接続inputのpullを扱い、さらに広い部分ポートの強さ伝搬は別契約を必要とします。

## automatic eventを一覧へ追加

automatic eventの段階では、`automatic_event_capture`は両参照で通り、nativeではautomatic eventの位置付き未対応診断です。
以前の81件の状態と通過した比較出力は同じで、この測定に伴う本体コードの変更はありません。
新しい仕組みを加えたため差分は21/27件から22/28件へ増え、参照不一致4/7件の一覧は同じです。

[automatic記憶域の共有・eventの修正案](automatic-capture-proposal.ja.md)には、独自32本の
設計用プログラムを別に記録しました。承認待ちで、提案出力を新しい期待値や実装済みの
根拠にはしません。以前の81件の[13.0](../measurements/2026-10-24/icarus13-fork-declaration-probes.json)と
[開発版](../measurements/2026-10-24/icarus14-fork-declaration-probes.json)は、旧nativeとの比較も含め
そのまま残します。

## fork宣言と新しく測定した捕捉の差分

fork宣言の段階では、同じ81件で旧nativeの差分は22/28件、現行は21/27件です。`fork_local_declarations`は
nativeと両参照で通過しました。automaticのfork変数を共有して更新する
`automatic_fork_capture`は両参照で通り、nativeでは未対応です。共有するフレーム/寿命の
詳細な契約は修正案としてレビュー待ちです。

以前の79件の状態と通過した比較出力は維持し、小さい一覧の差分20/26件も保っています。
拡張した一覧では、捕捉の追加によって残る差分が1件増えました。参照不一致4/7件は同じです。
[独自72ソースとコンパイル済みの対照](fork-local-declarations.ja.md)で仕組みを広く検査しますが、
fork全範囲の対応とは扱いません。可搬的なデータは旧nativeの結果と現在の比較ツールの
ハッシュも保存します。以前の79件の[13.0](../measurements/2026-10-24/icarus13-wide-gate-probes.json)と
[開発版](../measurements/2026-10-24/icarus14-wide-gate-probes.json)の記録も上書きしません。

## 同じ一覧での広いマスクの比較

以前の広いマスクの段階（`57a64ef`）では、同じ79件・同じ参照版で旧nativeの差分が
13.0に対して21件、開発版に27件でした。その段階で20・26件となり、`wide_tristate_masks`が
両エンジンで通りました。他の共通78件の状態と
参照不一致一覧は変わりません。可搬的な記録には変更前nativeの版と各結果も残し、
[独自73ソース](wide-gate-strength.ja.md)も両参照と一致します。新しく棚卸しした
仕組みの解消であり、以前に数えた差分やP1全体が完了したとは扱いません。

以前の`396131e`の78件も、[13.0](../measurements/2026-10-24/icarus13-blocking-aggregate-probes.json)・
[開発版](../measurements/2026-10-24/icarus14-blocking-aggregate-probes.json)とも上書きせず残します。

## 証拠の限界

以前の`917c715`の75件の記録も、[13.0](../measurements/2026-10-24/icarus13-feature-probes.json)と
[開発版](../measurements/2026-10-24/icarus14-feature-probes.json)の両方を残します。
現在の実装でも、共通75件の各エンジンの状態と差分・不一致一覧は変わりません。
追加した規格の3件は、配列全体のブロッキング遅延・繰り返しイベントでの保持・
異なる範囲の構造体配列コピーです。kirimasimはすべて成功し、両参照はコンパイルを
拒否しました。[実装と期待値の別レビュー](blocking-aggregate-timing.ja.md)を参照してください。

ローカルの有限の一覧で、13.0全体、全組み合わせ、Linuxの結果、拡張・外部インターフェース
全体の対応を示すものではありません。クラス・VPI・スイッチ・specify・タイミングなどの
必要な作業は残ります。P1の目標と既存の合格条件は維持し、この一覧の差分が0でも
完成の根拠にはしません。

可搬的なJSONには版、分類、期待出力、各エンジンの結果、差分、不一致を残しています。
診断は先頭行を保ち、失敗時の標準出力にある可搬的でないbackendのパスは正規化します。
通過した出力は変えていません。完全な診断を含む元の記録は、手元で再生成できます。
過去の外部スイート件数・性能測定の日付とコミットは変更しません。

## 再実行

`IVERILOG_13`と`VVP_13`に同じ13.0のバイナリを指定し、`-V`の出力を確認します。
`iverilog -V`には補助するコンポーネントの版も表示されます。

```sh
python3 tools/icarus_feature_probe.py --kirimasim build/dev/src/driver/kirimasim \
  --iverilog "$IVERILOG_13" --vvp "$VVP_13" --extended \
  --output build/icarus13-array-port-probes.json --require-parity
python3 tools/icarus_feature_probe.py --kirimasim build/dev/src/driver/kirimasim \
  --extended --output build/icarus14-array-port-probes.json --require-parity
```

2本目はPATHを使うため、開発版と決めつけずにコンパイラ・runtimeの版を確認してください。
今回の両比較は差分が残るため終了コード1です。テストスイートのCIとは別の判定です。
