# slang 62affeb0 への依存の更新

[English](slang-62affeb0-update.md) · 2026-11-12。

未改変のフロントエンドを `62affeb02b95df96a69c36ee5a7c3d6dc8878554` に固定します。2026-11-12 に確かめた上流の
`master` の先頭で、[`6fbb158f`](slang-6fbb158f-update.ja.md) の 16 コミット後です。上流の CI はこのコミットで
通っています（19 項目）。kirimasim から報告した [slang #2104](https://github.com/MikePopoloski/slang/issues/2104)
の修正を含みます。欄のない UDP の表の行は構文の誤りとして報告され、アサーションに届かなくなりました。
kirimasim から見えるほかの変更は次のとおりです。

- 型のない parameter を狙う `defparam` が、新しい値の型を取るようになりました
  （[slang #2105](https://github.com/MikePopoloski/slang/issues/2105)）。NaN や `$` を代入する `defparam` も
  扱えるようになりました。
- 定数の case 式が選ばない `case` の項目は、取られない `if` の枝と同じく、評価されないものとして扱われます
  （#2109）。そのため警告を出しません。
- 試しに評価される関数呼び出しは、診断を出しません。
- 逆向きの範囲選択が、無効なノードを作らなくなりました。
- インラインの enum の値は構文から宣言され、enum の型は必要になったときに作られます。

slang の MIT の文面は変わらず、slang 自身の依存も変わりません
（[ライセンスの確認](../measurements/2026-11-12/slang-62affeb0-licenses.json)）。新しい依存や、新しく入れる
道具はありません。

## 取り込み

- kirimasim のソースの変更は要りませんでした。front は、新しいコミットに対してそのままビルドできます。
- 互換プロファイルのフラグと、既定で誤りになる診断の並びは変わりません。
  [現行 pin の一覧](../measurements/2026-11-12/compatibility-62affeb0-profiles.json)は、前の pin の一覧の
  コミットを新しくしたものです。slang には診断が 2 つ増えました。どちらも通常の誤りです
  （`DefParamImplicitType`、`RecursiveEnum`）。

## 新旧の比較

どちらも同じ kirimasim のコード（main の `e2f6fc41`。履歴の上では文書のコミット 1 つぶん前で、ソースは同じ）
の dev の組み立てで、新しいほうを新しい slang に対して作りました
（[比較](../measurements/2026-11-12/slang-62affeb0-comparison.json)）。

- 自前のテスト: どちらも **13,750** 件。古い pin ではすべて合格です。新しい slang では
  `tools/readme_counts` を除くすべてが合格です。この 1 件は、そのソースに合わせて数の記録を更新している途中
  だったためで、slang には依存しません。pin を動かすコミットは、`tools/local_ci.py` でもう一度確かめます。
- 固定した P1 の母集団（kirimasim-tests `41085fe3`、1,466 件）: どちらも **1,456 件合格、失敗 0、未対応 10**
  で、各件の状態も同じ。
- `15490e95` の sv-tests: どちらも **344 件中 253 件**で、変わった結果はなし。
- 拡張のプローブ **188** 件は、すべて状態と出力が同じ（[プローブ](../measurements/2026-11-12/slang-62affeb0-probes.json)）。
- 表の行が `: 1 ;` の UDP は、古い pin では kirimasim の dev の組み立てを slang のアサーション
  `range.start()` で終わらせていました。新しい pin では `error: expected UDP symbol` になります。
- `module m; parameter W = 8'shFF; ... endmodule` に `defparam a.W = 16'sh7FFF;` を付けると、古い pin では
  実行時に `W=-1`、`$bits(W)=8` と表示していました（`W` の generate の条件は 32767 を見ていました）。
  新しい pin では、`#(...)` での上書きと同じく、`W=32767`、`$bits(W)=16` と表示します。新しいテスト
  `tests/suite/hier/defparam_untyped_matrix`（`tools/gen_defparam_untyped_matrix.py` が作る）は、4 つの値、
  インスタンスの 3 つの置き場所、2 つの上書きのしかたについて、これを確かめます。古い pin では、その出力の
  12 行が違います。

## 未確認

- 新しい pin での CI の Linux のジョブ。push したときに走ります。
- release の組み立ては比べていません。上の比較は dev の組み立てです。
