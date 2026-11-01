# 投稿案：ファイル名の文脈で整数から文字列への暗黙変換が拒否される

[English](slang-integral-to-string-issue-draft.md)

状態：**草稿・未投稿**、2026-10-28。slang への報告案です。ファイル名や plusargs のキーの変換を
どこで行うべきかを確認します。暗黙変換の一般規則を広く緩める依頼ではありません。

## 観測環境

- kirimasim が固定して使う、未改変の slang コミット `e2d1021661f78b3f17d1edf7ea17f9d6f95fb168`
  （master、2026-10-27）。
- kirimasim `a2420294775a`、Linux x86_64（WSL2）、Clang 20.1.8。
- slang の driver を利用する kirimasim 経由で観測しました。この記録用に単独の slang ツールはビルドしていません。
- Icarus Verilog 13.0 と Verilator 5.050 は、動作の比較だけに使いました。

## 再現例

`int_string.sv` として保存します。

```systemverilog
module top;
  bit [63:0] name = "tmp.txt";
  int fd;
  initial begin
    fd = $fopen(name, "w");
    $display("fd=%0d", fd);
    if (fd) $fclose(fd);
  end
endmodule
```

実行：

```sh
kirimasim int_string.sv
```

観測結果：

```text
int_string.sv:5:17: error: no implicit conversion from 'bit[63:0]' to 'string'; explicit conversion exists, are you missing a cast?
```

Icarus Verilog 13.0 と Verilator 5.050 はこのファイルを受理し、`$fopen` を呼び出します。

packed ベクトルに格納した plusargs のキーや、`$dumpvars` / `$dumpports` のファイル名引数でも
同じ診断が出ます。

## 期待した動作

21.3.1 は、`$fopen` のファイル名に文字列リテラル、`string`、文字列を含む整数型を認めます。
21.6 の `$value$plusargs` のキーも同じ形です。21.7.1 は VCD task のファイル名も同様に述べます。
したがって、整数から `string` への一般的な代入が暗黙変換でなくても、このシステム task の引数は有効です。

修正の選択肢は 2 つあります。

1. システム task の該当する引数位置で整数を受理して文字列に変換し、一般の変換規則は変えない。
2. 規格が意図するなら、一般にも暗黙変換を認める。

前者のほうが範囲が狭く、21.3.1 の記述に合います。

## 原因箇所の絞り込み

- `source/ast/expressions/ConversionExpression.cpp`：`convertAssignment` は、変換先と変換元が
  キャスト互換でも代入互換でない場合に `NoImplicitConversion` を出します。
- `$fopen`、`$value$plusargs`、VCD task の引数の束縛は、システムサブルーチンの定義にあります。
  整数のファイル名を受け入れられる正確な箇所までは絞り込めていません。
- `scripts/diagnostics.txt`：`NoImplicitConversion`。

## 重複の検索

2026-10-28 に上流の issue を `implicit conversion string` と `fopen string` で検索し、
一致する issue は見つかりませんでした。重複がないと証明するものではありません。

## 未解決の質問

1. 一般の暗黙変換規則で扱うべきですか。それとも、ファイル名・plusargs の引数束縛だけで扱うべきですか。
2. 21.3.1 は `$fopen` 以外のどの文脈を含みますか（例：`$readmemh`、`$writememh`、
   ファイル記述子を使う `$fdisplay`）。
3. 印字できないバイトや途中の NUL を含む整数はエラーにすべきですか。

issue やコメントは送っていません。
