# 最近の P1 の機能：使い方と制限

[English](P1_FEATURES.md) · 英語版が主で、日本語版はそれに従います。

2026-10-23 時点の第 1 層の使い方です。P1 は進行中です。対応範囲の全体と
残っている作業は [README](../README.ja.md) を参照してください。
[開発用のビルド](../README.ja.md)の後、例を 1 つずつ `demo.sv` に保存し、
リポジトリのルートから実行します。

```sh
build/dev/src/driver/kirimasim demo.sv
```

以下の出力例は、設計の標準出力だけを示します。kirimasim 自身のメッセージは
標準エラーに出ます。

## 実行時に決まる列挙型の移動数

`next(n)` と `prev(n)` は変数の移動数を受け取り、宣言の順に進み、端で反対の端に
回ります。呼び出しは値を返し、呼び出し元の変数は書き換えません。

```systemverilog
module top;
  typedef enum bit [7:0] {IDLE = 1, RUN = 5, DONE = 9} state_t;
  state_t state, advanced, previous;
  int unsigned steps = 2;
  initial begin
    state = IDLE;
    advanced = state.next(steps);
    previous = state.prev(steps);
    $display("next=%s prev=%s original=%s",
             advanced.name(), previous.name(), state.name());
  end
endmodule
```

```text
next=DONE prev=RUN original=IDLE
```

移動数は、負数や幅の広い引数も含め、32 ビットの `int unsigned` に変換してから
使います。0 なら現在の有効な要素を返します。現在の値が列挙の要素でなければ、
基底の型の既定の初期値を返します。呼び出し元の式と移動数は、左から右に
それぞれ 1 回評価します。[値の意味の仕様書の 7.7 節](spec/VALUE_SEMANTICS.ja.md)
を参照してください。

## 64 ビットを超えるストリーミング代入先

ストリーミング代入先は、64 ビットを超える packed の値、固定長の unpacked の
配列と構造体に対応します。この例は 128 ビットをバイトに分け、`<<8` で順を逆にし、
その後にノンブロッキング代入を予約します。

```systemverilog
module top;
  timeunit 1ns;
  timeprecision 1ns;
  typedef logic [7:0] byte_t;
  logic [127:0] payload;
  byte_t bytes [0:15];
  initial begin
    payload = 128'h00112233445566778899aabbccddeeff;
    {>>{bytes}} = payload;
    $display("forward %h %h", bytes[0], bytes[15]);
    {<<8{bytes}} = payload;
    $display("reverse %h %h", bytes[0], bytes[15]);
    {>>{bytes}} <= payload;
    payload = '0;
    #1;
    $display("nba %h %h", bytes[0], bytes[15]);
  end
endmodule
```

```text
forward 00 ff
reverse ff 00
nba 00 ff
```

ノンブロッキング代入は、`payload` が変わる前の右辺の値を保持します。4 値の
代入先は X と Z を保ち、2 値の代入先は 0 に変換します。配列は foreach の順、
構造体はメンバの宣言の順です。スライスの幅は定数にします。automatic な変数は
ノンブロッキング代入先にはできません。動的なコンテナと `with` は P3 の予定で、
入れ子のストリーミング代入先も未対応です。最後の短いスライスの扱いは
[値の意味の仕様書の 8 節](spec/VALUE_SEMANTICS.ja.md)を参照してください。

## 実行時に決まる読み取り書式

`$sscanf` と `$fscanf` は、文字列や packed の整数値の書式に対応します。
配列の要素や関数の戻り値も使えます。この例は、作業ディレクトリに
`scan-input.txt` を作ります。

```systemverilog
module top;
  string formats [2];
  int value, matched, fd;
  initial begin
    formats[0] = "%d";
    formats[1] = "%h";
    for (int i = 0; i < 2; i++) begin
      matched = $sscanf("10", formats[i], value);
      $display("string %0d %0d", matched, value);
    end
    fd = $fopen("scan-input.txt", "w");
    if (fd == 0) $fatal(1, "cannot open scan-input.txt for writing");
    $fwrite(fd, "10 10");
    $fclose(fd);
    fd = $fopen("scan-input.txt", "r");
    if (fd == 0) $fatal(1, "cannot open scan-input.txt for reading");
    for (int i = 0; i < 2; i++) begin
      matched = $fscanf(fd, formats[i], value);
      $display("file %0d %0d", matched, value);
    end
    $fclose(fd);
  end
endmodule
```

```text
string 1 10
string 1 16
file 1 10
file 1 16
```

戻り値は代入できた出力の数です。残りの出力は元の値を保ちます。X/Z を含む
packed の書式は、出力への代入やファイルの読み進めをせずに -1 を返します。
実行時の書式に対して出力が足りなければ、警告を出し、足りない変換の直前で
読み取りを止めます。未対応の変換や出力の型と合わない変換は、診断と終了コード 2
で実行を終えます。定数の書式は引き続きエラボレーション時に検査します。

対応する読み取りの変換は `%b`、`%o`、`%d`、`%h`/`%x`、`%c`、`%s`、
`%f`/`%e`/`%g` で、欄の幅、代入の抑制、`%%` も扱います。
`$sformat`、`$sformatf`、`$value$plusargs` の実行時の書式は未対応です。
[システムタスクの仕様書の 6.3 節](spec/SYSTEM_TASKS.ja.md)と
[SIR の実装の制限](spec/SIR_SPEC.ja.md)を参照してください。

## 条件を省略した for ループ

`for (;;)` の省略した条件はループを続けます。例えば `break` で抜けられます。

```systemverilog
module top;
  timeunit 1ns;
  timeprecision 1ns;
  int ticks;
  initial begin
    ticks = 0;
    for (;;) begin
      #1;
      ticks++;
      if (ticks == 3) break;
    end
    $display("ticks %0d", ticks);
  end
endmodule
```

```text
ticks 3
```

シミュレーション時刻が進まないループは、`--max-steps=<n>` で実行を制限できます。
上限と終了コードは [CLI の仕様書](spec/CLI.ja.md)を参照してください。

## 最小・標準・最大の遅延

```systemverilog
module top;
  timeunit 1ns;
  timeprecision 1ns;
  initial begin
    #(2:5:9);
    $display("delay %0d", $time);
  end
endmodule
```

`--mintypmax=min`、`--mintypmax=typ`、`--mintypmax=max` を付けると、
それぞれ `delay 2`、`delay 5`、`delay 9` を出します。既定は `typ` です。
[CLI の仕様書](spec/CLI.ja.md)と[スケジューラの仕様書](spec/SCHEDULER_SPEC.ja.md)
を参照してください。
