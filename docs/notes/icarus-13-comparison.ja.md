# Icarus 13.0と開発版による比較

[English](icarus-13-comparison.md)。2026-10-24、実装`917c715`のdevビルド、
macOS/arm64で計測しました。追加インストールはせず、他のシミュレータの
ソース・テストも読んでいません。

## 確認した版と結果

既存のバイナリで、コンパイラdriver・プリプロセッサ・parser/elaborator・runtimeが
すべて`13.0 (stable) (v13_0)`と表示することを確認しました。追加の比較対象は、
コンパイラ・runtimeともに`14.0 (devel) (s20260301-411-gca6b8d376)`です。
両方で[`icarus_feature_probe.py`](../../tools/icarus_feature_probe.py)の独自75プローブを実行しました。

- **13.0 stable：** Icarusだけが通る差分20件、参照結果の不一致4件。
  [個別の可搬的な記録](../measurements/2026-10-24/icarus13-feature-probes.json)。
- **14.0 devel：** Icarusだけが通る差分26件、参照結果の不一致7件。
  [個別の可搬的な記録](../measurements/2026-10-24/icarus14-feature-probes.json)。

開発版だけの追加6差分は、配列の和、キューの並べ替え、最小・最大の重複なし・空の
プローブです。13.0はこれらをコンパイルできません。重複・withキーの最小最大の3件も
13.0では実行されず、開発版で観測した追加3件の参照結果の不一致にはなりません。
件数の違いは比較する版の違いです。kirimasimの未対応機能が減った数にはせず、
開発版で観測した範囲と承認待ちの意味の修正案も引き続き追跡します。

共通の参照側の不一致4件は、packedな親境界の選択、virtual dispatch、
動的配列の問い合わせ、1ビットX/Z回数の繰り返しイベントです。
参照が不一致でもkirimasimの未対応がなくなるわけではなく、クラスのvirtual dispatchは
依然未対応です。JSONは両エンジンの状態を独立に記録し、P1の作業から消しません。

一覧は規格の仕組み72本、右辺順序を検査する`project_policy` 2本、
`icarus_extension` 1本（`wire real`）です。順序の2本ではIEEEは左右のどちらを先に
評価してもよく、期待値は既存の承認済みkirimasimの順序です。
拡張とプロジェクトの方針は、規格共通の要件とは区別して記載します。

## 証拠の限界

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
  --output build/icarus13-feature-probes.json --require-parity
python3 tools/icarus_feature_probe.py --kirimasim build/dev/src/driver/kirimasim \
  --extended --output build/icarus14-feature-probes.json --require-parity
```

2本目はPATHを使うため、開発版と決めつけずにコンパイラ・runtimeの版を確認してください。
今回の両比較は差分が残るため終了コード1です。テストスイートのCIとは別の判定です。
