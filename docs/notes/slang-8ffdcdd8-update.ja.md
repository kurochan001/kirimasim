# slang 8ffdcdd8 への依存の更新

[English](slang-8ffdcdd8-update.md) · 2026-11-13。

未改変のフロントエンドを `8ffdcdd8ad237823fbfd72d495082d76e92d4015` に固定します。2026-11-13 に確かめた上流の
`master` の先頭で、[`62affeb0`](slang-62affeb0-update.ja.md) の 6 コミット後です。上流の CI はこのコミットで
通っています（19 項目）。kirimasim から報告した [slang #2116](https://github.com/MikePopoloski/slang/issues/2116)
（#54）の修正を含みます。`a ? b : c -> d` で、11.3.2 の Table 11-2 の順のとおり、条件演算子が含意より強く
結びつくようになりました。これまで slang は、含意を条件演算子の偽の側に入れていました。ほかの変更は次の
とおりです。

- 入れ子の総称クラスについて、slang の診断の巡回が解放済みのメモリを使っていたのを直しました。
- 大きな局所変数の選択の定数評価が速くなりました。
- 依存ファイルを書くオプションが、slang の driver のライブラリから、slang 自身のコマンドラインの道具に
  移りました。kirimasim はその道具を使わず、これらのオプションも slang に渡しません（CLI.md 2.1）。
- 診断を捕まえる道具の追加と、slang の構文の書き換えの変更。kirimasim はどちらも呼びません。

slang の MIT の文面は変わらず、slang 自身の依存も変わりません
（[ライセンスの確認](../measurements/2026-11-13/slang-8ffdcdd8-licenses.json)）。新しい依存や、新しく入れる
道具はありません。

## 取り込み

- kirimasim のソースの変更は要りませんでした。front は、新しいコミットに対してそのままビルドできます。
- 互換プロファイルのフラグと、既定で誤りになる診断の並びは変わらず、slang の診断の一覧も同じです。
  [現行 pin の一覧](../measurements/2026-11-13/compatibility-8ffdcdd8-profiles.json)は、前の pin の一覧の
  コミットを新しくしたものです。

## 新旧の比較

どちらも同じ kirimasim のコード（`f3ae2b37`）の dev の組み立てで、新しいほうを新しい slang に対して作りました
（[比較](../measurements/2026-11-13/slang-8ffdcdd8-comparison.json)）。

- 自前のテスト: どちらも **13,830** 件。古い pin ではすべて合格です。新しい slang では
  `tools/readme_counts` を除くすべてが合格です。この 1 件は作業中のファイルを数えるもので、そこにはこの変更の
  新しいテストが入っていて、記録した数にはまだ含まれていなかったためです。slang には依存しません。pin を
  動かすコミットは、`tools/local_ci.py` でもう一度確かめます。
- 固定した P1 の母集団（kirimasim-tests `41085fe3`、1,466 件）: どちらも **1,456 件合格、失敗 0、未対応 10**
  で、各件の結果も同じ。
- `15490e95` の sv-tests: どちらも **344 件中 253 件**で、各件の結果も同じ。
- 拡張のプローブ **188** 件は、すべて状態、終了コード、出力が同じ
  （[プローブ](../measurements/2026-11-13/slang-8ffdcdd8-probes.json)）。
- 古い pin では、`a ? b : c -> d` と `a ? b : c <-> d` は `a ? b : (c -> d)` と `a ? b : (c <-> d)` の値に
  なっていました。これは `(a ? b : c) -> d` と `(a ? b : c) <-> d` の値と、4 つのビットの 16 通りのうちそれぞれ
  8 通りで違います。新しい pin では後者の値になります。新しいテスト
  `tests/suite/values/conditional_implication_*`（`tools/gen_conditional_implication_matrix.py` が作る）は、
  `->`、`<->`、`||` と並んだ `?:` の 8 つの形を、16 通りの値すべてについて、5 つの置き場所（手続きの代入、
  連続代入、関数の戻り値、generate のループの localparam、`if` の条件）で確かめます。古い pin では 5 本とも、
  それぞれ 16 行のうち 13 行が違って落ち、新しい pin では通ります。

## 未確認

- 新しい pin での CI の Linux のジョブ。push したときに走ります。
- release の組み立ては比べていません。上の比較は dev の組み立てです。
