# 投稿案：`uwire` が `inout` ポートとして拒否される

[English](slang-uwire-inout-issue-draft.md)

状態：**草稿・未投稿**、2026-10-28。slang への報告案です。`InOutUWirePort` / `InOutUWireConn` の
診断の根拠となる、まだ見つけられていない条項があるかを確認します。警告の抑止を求めるものではありません。

## 観測環境

- kirimasim が固定して使う、未改変の slang コミット `e2d1021661f78b3f17d1edf7ea17f9d6f95fb168`
  （master、2026-10-27）。
- kirimasim `a2420294775a`、Linux x86_64（WSL2）、Clang 20.1.8。
- slang の driver を利用する kirimasim 経由で観測しました。この記録用に単独の slang ツールはビルドしていません。
- Icarus Verilog 13.0 と Verilator 5.050 は、動作の比較だけに使いました。

## 再現例

`uwire_inout.sv` として保存します。

```systemverilog
module uw_cell(w, d);
  inout w;
  input d;
  uwire w;
  assign w = d;
endmodule

module top;
  uwire n;
  logic d = 1'b1;
  uw_cell u(n, d);
  initial #1 $display("%b", n);
endmodule
```

実行：

```sh
kirimasim uwire_inout.sv
```

観測結果（最初の 2 件のエラーと、1 件目に起因する 3 件目）：

```text
uwire_inout.sv:2:9: error: 'uwire' port 'w' cannot have direction 'inout'
uwire_inout.sv:11:13: error: 'uwire' net 'n' cannot be connected to 'inout' port
uwire_inout.sv:5:10: error: 'uwire' net 'w' cannot have multiple drivers
```

Icarus Verilog 13.0 と Verilator 5.050 は同じファイルをコンパイル・実行し、`1` を表示します。

## 期待した動作

ネット型 `uwire` の `inout` ポートを禁止する規則は見つけられませんでした。
6.6.2 で確認できた制約は、1 ビットを複数ドライバへ接続するとエラーになることと、
双方向パススイッチの双方向端子への接続がエラーになることの 2 点です。
23.3.3.6 は、`uwire` ポートの両側が 1 つのネットに統合されない場合に警告を求めますが、
ポート自体は禁止しません。23.3.3.3 は、`inout` ポートのネットへの接続を認めます。

上の設計のドライバは内部の `assign w = d` の 1 つだけで、親ネットへの経路もポートだけです。
規格が禁止しているなら条項の提示を求めます。禁止していないなら、この 2 つの診断は厳しすぎます。

## 原因箇所の絞り込み

- `scripts/diagnostics.txt`：`InOutUWirePort`（`'uwire' port '{}' cannot have direction 'inout'`）と
  `InOutUWireConn`（`'uwire' net '{}' cannot be connected to 'inout' port`）。
- `source/ast/symbols/PortSymbols.cpp` は、ANSI・非 ANSI のポートに `InOutUWirePort` を出します。
- `source/ast/expressions/MiscExpressions.cpp` は、`inout` ポート接続が `uwire` を駆動するときに `InOutUWireConn` を出します。
- 上流の `CHANGELOG.md` の過去の項目には、`inout` の uwire ポートを正しく禁止する修正の記載があります。
  この記録は、その変更の根拠を問うものです。

## 重複の検索

2026-10-28 に上流の issue を `uwire inout` と `uwire port` で検索し、一致する issue は
見つかりませんでした。重複がないと証明するものではありません。

## 未解決の質問

1. 6.6.2 と 23.3.3 以外に、`uwire` ポートの `inout` を禁止する条項はありますか。
2. ないなら、`InOutUWirePort` と `InOutUWireConn` を削除するか、接続が 2 つ目のドライバを作る場合だけに限定すべきですか。
3. `uwire` ポートに遅延や強度があると結論は変わりますか。

issue やコメントは送っていません。
