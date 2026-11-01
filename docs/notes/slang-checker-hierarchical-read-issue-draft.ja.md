# 投稿案：階層名による checker 変数の読み取りが拒否される

[English](slang-checker-hierarchical-read-issue-draft.md)

状態：**草稿・未投稿**、2026-10-28。slang への報告案です。`CheckerHierarchical` の診断が
書き込みだけでなく読み取りも対象とする意図かを確認します。書き込みの検査を緩める依頼ではありません。

## 観測環境

- kirimasim が固定して使う、未改変の slang コミット `e2d1021661f78b3f17d1edf7ea17f9d6f95fb168`
  （master、2026-10-27）。
- kirimasim `a2420294775a`、Linux x86_64（WSL2）、Clang 20.1.8。
- slang の driver を利用する kirimasim 経由で観測しました。この記録用に単独の slang ツールはビルドしていません。
- Icarus Verilog 13.0 と Verilator 5.050 は checker 未対応のため、この例のシミュレータによる参照結果はありません。

## 再現例

`checker_read.sv` として保存します。

```systemverilog
checker c(output int v);
  int x;
  assign v = 1;
  assign x = 2;
endchecker

module top;
  int v;
  c u(.v(v));
  initial #1 $display("x=%0d", u.x);
endmodule
```

実行：

```sh
kirimasim checker_read.sv
```

観測結果：

```text
checker_read.sv:10:33: error: cannot access checker members hierarchically
checker_read.sv:9:5: note: declared here
```

## 期待した動作

17.7.1 は、**代入で** checker 変数を階層名により参照することを禁止しています。
同節の例の `wire x = my_check.a;` と `my_check.a = y;` は両方とも代入です。
上の再現例は `$display` の引数で `u.x` を読むだけで、その変数に代入しません。

kirimasim のテストスイートも、代入の形を不正な設計として保持しています
（`test_illegal_checker_hierarchical_write`、`test_illegal_checker_hierarchical_read`）。
したがって、式での読み取りと、代入での読み取り・書き込みの区別は既にそのモデルにあります。

すべての階層参照を禁止する意図なら、17.7.1 の文面はなお代入に限定している可能性があります。
どちらの読み方を意図するか、確認を求めます。

## 原因箇所の絞り込み

- `scripts/diagnostics.txt`：`CheckerHierarchical`（`cannot access checker members hierarchically`）。
- `source/ast/Lookup.cpp` は、checker のメンバへ階層名で到達したときに診断を出します。
- 現状の検査は、読み取りの文脈と代入の文脈を区別していないように見えます。

## 重複の検索

2026-10-28 に上流の issue を `checker hierarchical` と `checker member hierarchical` で
検索し、一致する issue は見つかりませんでした。重複がないと証明するものではありません。

## 未解決の質問

1. 17.7.1 の代入への言及は、規則を代入先に限定しますか。それとも、すべての階層参照を略して述べていますか。
2. 読み取りが合法なら、どの文脈をエラーとすべきですか（代入先、`force`、イベント制御、ポートの実引数）。
3. 読み取りと書き込みで診断を分けるべきですか。

issue やコメントは送っていません。
