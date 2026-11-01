# 投稿案：program と名前付きブロックの parameter への `defparam` が拒否される

[English](slang-defparam-parameter-scope-issue-draft.md)

状態：**草稿・未投稿**、2026-10-28。slang への報告案です。2 種類のパラメータのスコープが
正しく分類されているかを確認します。`localparam` を `defparam` の対象にできない規則の変更は求めません。

## 観測環境

- kirimasim が固定して使う、未改変の slang コミット `e2d1021661f78b3f17d1edf7ea17f9d6f95fb168`
  （master、2026-10-27）。
- kirimasim `a2420294775a`、Linux x86_64（WSL2）、Clang 20.1.8。
- slang の driver を利用する kirimasim 経由で観測しました。この記録用に単独の slang ツールはビルドしていません。
- program パラメータの例には Icarus Verilog 13.0 も使いました。

## 再現例 1：program のパラメータ

`defparam_prog.sv` として保存します。

```systemverilog
program prg #(parameter int Q = 1) ();
  initial #1 $display("Q=%0d", Q);
endprogram

module top;
  prg p();
  defparam p.Q = 9;
endmodule
```

実行：

```sh
kirimasim defparam_prog.sv
```

観測結果：

```text
defparam_prog.sv:7:12: error: cannot reference program item from outside of a program
```

Icarus Verilog 13.0 はこのファイルをコンパイルし、`Q=9` を表示します。

## 再現例 2：名前付きブロックのパラメータ

`defparam_param.sv` として保存します。

```systemverilog
module leaf;
  parameter int P = 0;
endmodule

module top;
  leaf u();
  initial begin : blk
    parameter int BP = 1;
    #1 $display("P=%0d BP=%0d", u.P, BP);
  end
  defparam u.P = 5;
  defparam top.blk.BP = 6;
endmodule
```

観測結果：

```text
defparam_param.sv:12:12: error: 'BP' is a localparam and so cannot be the target of a defparam
```

## 期待した動作

23.10.1 は、module・interface・program インスタンスの値パラメータを、階層名を使った
`defparam` で変更することを認めます。24.3 の program 内の項目への不正な参照の規則は、
program の**信号**（変数とネット）についてであり、パラメータは信号ではありません。
したがって、最初の再現例は合法なはずです。

23.10.2 は、名前付きブロック・task・function で宣言したパラメータを `defparam` で直接再定義できると
定めます。`BP` は名前付きブロック内の宣言で、上書きを禁止する意味の `localparam` ではありません。
したがって、2 番目の再現例も合法なはずです。

## 原因箇所の絞り込み

- `scripts/diagnostics.txt`：`IllegalReferenceToProgramItem` と、`localparam` を対象とする `defparam` の診断。
- 前者は階層名が program 内へ解決された箇所で出ます。パラメータと program の信号を区別する必要があります。
- 後者は `defparam` の対象を分類する箇所で出ます。名前付きブロックのパラメータが `localparam` に分類されていますが、
  23.10.2 は `defparam` の対象になれると定めます。
- 診断文より詳しい正確なソース位置までは絞り込めていません。

## 重複の検索

2026-10-28 に上流の issue を `defparam program` と `defparam parameter block` で検索し、
一致する issue は見つかりませんでした。重複がないと証明するものではありません。

## 未解決の質問

1. 24.3 の program 信号の規則は、意図的にパラメータを含めていますか。
2. `defparam` の対象を決めるとき、名前付きブロックのパラメータは `parameter` と `localparam` のどちらにすべきですか。
   23.10.2 は `defparam` で再定義できると定めます。
3. task・function のパラメータにも同じことが当てはまりますか（2 番目の再現例は名前付きブロックです）。

issue やコメントは送っていません。
