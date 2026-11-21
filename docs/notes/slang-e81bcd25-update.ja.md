# slang e81bcd25 への依存の更新

[English](slang-e81bcd25-update.md) · 2026-11-19。

未改変のフロントエンドを `e81bcd2560d54dfd17524df70c0f297e759509da` に固定します。2026-11-19 に確かめた上流の
`master` の先頭で、[`8ffdcdd8`](slang-8ffdcdd8-update.ja.md) の 68 コミット後です。上流の CI はこのコミットで
通っています（19 項目）。kirimasim から報告した [slang #2117](https://github.com/MikePopoloski/slang/issues/2117)
（#110）の修正を含みます。`Token::operator==` が、2 つのトークンの片方だけが info ポインタを持つときに誤った
答えを返していました。そのため一部のビルド（macOS の dev と ASan のプリセット）では、十分な空白の後に自分の行で
書いたサブルーチンの仮引数が拒否され、UVM のパッケージも読めませんでした。kirimasim のテストや利用者から見える
ほかの変更は次のとおりです。

- UDP のエッジ網羅の検査が、入力の多い UDP でも間に合うようになりました。33 入力の UDP で、フロントエンドの
  時間の上限に達しなくなります。
- クラス型の名前が、20.6.1 が `$typename` について定めるとおり、定義したスコープ付きで書かれます。型を名指す
  kirimasim の診断はこの文字列を slang から受け取るので、未対応の組み込みクラスは `std::process`、
  `std::mailbox#(untyped)`、`std::semaphore` になりました。
- 対象を変更するメソッド（`push_back`、`pop_front`、`insert`、`sort` など）は、変更できる左辺値を要求し、
  slang の解析で書き込みとして数えられます。1 つの引数の並びの中で同じ queue にこの呼び出しを 2 つ書くと、
  `-Wread-write` が出ます（13.5、11.4.2）。そう書いていた queue のテスト 80 件は `bd81035d` で書き直しました
  （#138）。
- 1 つの parameter に defparam が複数あるときは、ソースで最後のものを使い、それを指す注記を出します（23.10.1。
  #59 が求めていることです）。警告 `duplicate-defparam` は残り、文言が変わりました。
- `new` 式は、変数の初期値か、ブロッキング代入の右辺でだけ受け付けます（`MisplacedNew`）。
  `array[index] <= new(99)` と書いていた生成テストは `2f67b81f` で変えました。
- 新しいエラー: ワイルドカードのポート並びを持つモジュールが、パラメータのポート並びやヘッダでのインポートも
  持つとき。名前のないサブルーチン引数の値が足りないとき。非 static のクラスのメンバを、その派生でないクラスから
  使ったとき。`ConstEvalNoIfItemsMatched` と `ConstEvalIfItemsNotUnique` の文言が変わり、定数評価で連想配列に
  x か z を含む添字を使ったときは、エラーではなく警告 `invalid-assoc-index` になりました。
- 内部エラーと定数評価の修正（if-else-if の連なり、負の repeat 回数、packed union のメンバ、上限のない範囲の
  `inside`、定数でない添字での queue の `delete`）と、エラボレーションの修正（generate の領域で宣言した
  パラメータ、generate ブロックでインスタンス化した入れ子のモジュール、ワイルドカードのエクスポート、nettype の
  別名、enum の演算子）。

slang の MIT の文面は変わらず、slang 自身の依存も変わりません
（[ライセンスの確認](../measurements/2026-11-19/slang-e81bcd25-licenses.json)）。新しい依存や、新しく入れる
道具はありません。

## 取り込み

- kirimasim のソースの変更は要りませんでした。front は、新しいコミットに対してそのままビルドできます。
- 互換プロファイルのフラグと、既定でエラーになる診断（slang の診断一覧の `compat` の部分）は変わりません。
  診断一覧そのものには、上のエラーの追加と文言の変更があります。
  [現行 pin の一覧](../measurements/2026-11-19/compatibility-e81bcd25-profiles.json)は、前の pin の一覧の
  コミットを新しくしたものです。
- `tools/gen_unsupported_matrix.py`（版 21）は、3 つの組み込みクラスのスコープ付きの名前を
  `tests/cli/unsupported_matrix.stderr` で期待します。slang はそれぞれ自分の形でしか書かないので、この既存の
  期待値は pin と同時に変える必要があります。

## 新旧の比較

どちらも同じ kirimasim のコード（`bd81035d`）を macOS arm64 の AppleClang 21 で組み立て、新しいほうを新しい
slang に対して作りました（[比較](../measurements/2026-11-19/slang-e81bcd25-comparison.json)）。

- 自前のテスト（dev）: どちらも **13,858** 件。古い pin ではすべて合格です。新しい slang では `cli/options` を
  除くすべてが合格です。この 1 件の未対応の行列は、スコープの付かないクラス名を期待していました。この変更で
  直します。
- 固定した P1 の母集団（kirimasim-tests `41085fe3`、1,466 件、dev）: どちらも **1,456 件合格、失敗 0、未対応 10**
  で、各件の判定も、未対応 10 件の判定ツールの出力も同じです。
- `15490e95` の sv-tests を、夜間実行と同じ release の組み立てで: どちらも **344 件中 253 件**、
  `kirimasim parse` で読むものは **593 件中 593 件**で、各件の結果も同じです。dev の組み立てでは、古い slang は
  UVM のパッケージを拒否します（#110）。593 件のうち読めるのは 529 件で、UVM を使うシミュレーションのテスト
  37 件は、未対応の構文ではなくエラーで止まります。新しい slang は dev でも 593 件すべてを読み、
  シミュレーションの合格は 253 件のままです。
- 拡張のプローブ 188 件（dev、Icarus 13.0 と比較）: どちらも必須の差なし、独立した失敗なしで、参照との差は
  同じ 25 件です。判定が変わったのは 1 件です。slang の UDP エッジ網羅の検査で時間の上限に達していた
  `dense_udp_coverage` が通るようになり、評価は 187 件、対象外は `real_net` だけになりました
  （[プローブ](../measurements/2026-11-19/slang-e81bcd25-probes.json)）。
- 新しいテスト `tests/suite/syntax/subroutine_formal_layout_*`（`tools/gen_subroutine_formal_layout_matrix.py`
  が作る）は、パッケージ、モジュール、インタフェース、クラスのそれぞれに、関数とタスクを 40 個ずつ宣言します。
  仮引数は string と int、向きは省略と `input` の明記で、2 つめの仮引数を、自分の行の 1、13、14、18、40 個の
  空白の後に書きます。古い slang の dev の組み立てでは 4 本とも落ちます。落ちるのは、向きを省略した string の
  仮引数で空白が 14 個以上のものです（パッケージ、モジュール、インタフェースでは、空白 14 個の関数は通ります）。
  古い slang の release の組み立てと、新しい slang の両方の組み立てでは 4 本とも通ります。

## 0.1.3リリース後の確認

- この固定版を使うリリースコミット `b5b4cef0` でLinux・macOSのCIが通りました。[CIの実行記録](https://github.com/kurochan001/kirimasim/actions/runs/44812776504)
- #59の「複数のdefparamの最後を適用する」動作には回帰テストの行列を追加しました。[行列の生成器](../../tools/gen_defparam_last_matrix.py)
- signedのpacked配列で残りの次元の範囲が失われる問題（#137）は、この固定版でも再現しました。ほかのフロントエンドの問題はケースごとの検証が必要です。
