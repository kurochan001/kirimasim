# 最近の P1 の機能：使い方と制限

[English](P1_FEATURES.md) · 英語版が主で、日本語版はそれに従います。

2026-10-23 時点の第 1 層の使い方です。P1 は進行中です。対応範囲の全体と
制限は [P1 の対応機能一覧](P1_SUPPORT.ja.md)、残っている作業は
[README](../README.ja.md)を参照してください。
[開発用のビルド](../README.ja.md)の後、例を 1 つずつ `demo.sv` に保存し、
リポジトリのルートから実行します。

```sh
build/dev/src/driver/kirimasim demo.sv
```

以下の出力例は、設計の標準出力だけを示します。kirimasim 自身のメッセージは
標準エラーに出ます。

## 固定配列・構造体全体への遅延付きNBA

集約値の遅延付きNBAは、全要素・メンバーの値を確定してから更新を予約します。
配列は各宣言の左端から対応付けます。その後にコピー元を変えたり、呼び出した
function・taskが戻ったりしても、保持した値は変わりません。構造体には固定配列・
実数・文字列のメンバーも使えます。構造体の配列メンバーを実行時に選ぶ添字は、
通常の代入と共通の格納先のコピー処理で保持します。

```systemverilog
`timescale 1ns/1ns
module top;
  int a[2:1], b[0:1];
  initial begin
    a = '{0,0}; b = '{12,34};
    a <= #2 b;
    b = '{90,91};
    #1; $display("before %0d %0d", a[2], a[1]);
    #2; $display("after %0d %0d", a[2], a[1]);
  end
endmodule
```

```text
before 0 0
after 12 34
```


## 実行時に選ぶ対象への遅延付きNBA

固定配列の実行時の添字、packedのビット・部分範囲、それらの連結への遅延付きNBAに
対応しました。右辺・対象・遅延は発行時に確定し、その後に値や添字を変えても更新先は
変わりません。部分的に有効な範囲は有効なビットだけ更新します。発行元のNBA・Re-NBAを
使い、発行したtask・forkが戻っても更新を保持します。automaticな対象は禁止のままで、
イベント制御付きNBA、配列全体のブロッキング代入内時間制御、refや文字列文字を通じた遅延付き更新は未対応です。

```systemverilog
`timescale 1ns/1ns
module top;
  logic [64:0] words[2];
  int index = 1;
  logic [64:0] data = 65'h10000000000000001;
  initial begin
    words[0] = 0; words[1] = 0;
    words[index] <= #2 data;
    index = 0; data = 0;
    #3; $display("%h %h", words[0], words[1]);
  end
endmodule
```

```text
00000000000000000 10000000000000001
```


## programとreactive側の実行

programのinitialと連続代入はReactiveで動き、#0はRe-Inactiveへ、NBAはRe-NBAへ
入ります。呼び出しとforkは実行するスレッドの領域と起源programを継承し、宣言場所が
module・package・interfaceでも同じです。この例では設計側のNBAの後に入力を読みます。

各programの最後のinitialが終わると残る子孫を終了し、initialを持つ全programの終了後は
暗黙のfinishでfinalを実行します。そのためmoduleの後続の `future` は出ません。
`$exit` は起源のprogramのinitialと子孫を明示的に終了し、programのinitialを起源に
持たなければ無視します。空のprogramだけではfinishしません。anonymous programの
サブルーチンにも対応しますが、外からの参照の制限はslangが保ちます。クラス・
クロッキングブロック・並行アサーション・programのVCDスコープは未対応です。

```systemverilog
module top;
  logic clk = 0;
  int data = 0;
  initial begin #2; data = 5; data <= 7; clk = 1; #10; $display("future"); end
  worker p(clk, data);
endmodule
program worker(input logic clk, input int data);
  int sampled = 0;
  initial begin
    @(posedge clk);
    $display("sample %0d", data);
    sampled <= data;
    #0; $display("before Re-NBA %0d", sampled);
    #1; $display("after Re-NBA %0d", sampled);
  end
  final $display("program final");
endprogram
```

```text
sample 7
before Re-NBA 0
after Re-NBA 7
program final
```

## 無書式のバイナリの語を書き出す

`%u` は2値、`%z` は4値の生データを、32ビットの語にそろえて下位の語から
ホストのバイト順で出力します。`%u` はX/Zを0にし、`%z` はVPIと同じ値と
未知ビットのマスクを書きます。符号付きの値でも、幅の上の端数はゼロで埋めます。
大文字の形にも対応します。

```systemverilog
module top;
  logic [3:0] value = 4'b1xz0;
  string format_value, result;
  int fd;
  initial begin
    fd = $fopen("binary.bin", "wb");
    $fwrite(fd, "%u%z", value, value);
    $display("bytes=%0d", $ftell(fd));
    $fclose(fd);
    format_value = "%z";
    result = $sformatf(format_value, value);
    $display("string length=%0d", result.len());
  end
endmodule
```

```text
bytes=12
string length=2
```

ファイルと `$swrite`・`$sformat` の packed 出力先は全バイトを保ちます。
文字列型にはNULを含められないので、文字列の出力先と `$sformatf` はゼロのバイトを
取り除きます。バイナリを保存するにはファイルか packed 変数を使ってください。
`$sscanf`・`$fscanf` も同じ語形式を読みます。次の入力例を参照してください。

## 無書式のバイナリの語を読み込む

`%u`・`%z` は下位の32ビット語からホストのバイト順で読みます。幅の省略と0は
代入先のビット幅を語へ切り上げ、正の幅はバイト数でなく転送するビット数です。
その幅の上を落としてからゼロ拡張・切り捨てを行います。`%z` は4値の出力先では
X/Zを保ち、2値の出力先では不明なビットを0にします。plusargsの書式には使えません。

空白・NULもデータで、packedの `$sscanf` 入力でも同じです。書式自身の空白は
従来どおり読み飛ばします。完全なフィールドだけを代入し、不足時は読めた分を消費し、
出力先を保ち、ファイルのEOFを立てます。完全なフィールドの直後はEOFの先読みを
しません。幅0は承認済みの本仕様の選択で、Icarusの実測とは差があります。

```systemverilog
module top;
  logic [3:0] written = 4'b1xz0, loaded;
  logic [32:0] wide = '1;
  string fmt;
  int fd, count;
  initial begin
    fd = $fopen("binary.bin", "wb");
    $fwrite(fd, "%z", written);
    $fclose(fd);
    fd = $fopen("binary.bin", "rb");
    fmt = "%z";
    count = $fscanf(fd, fmt, loaded);
    $display("read=%0d value=%b bytes=%0d eof=%0d", count, loaded, $ftell(fd), $feof(fd));
    $fclose(fd);
    count = $sscanf(32'h44444444, "%4u", wide);
    $display("read=%0d low nibble=%h", count, wide);
  end
endmodule
```

```text
read=1 value=1xz0 bytes=8 eof=0
read=1 low nibble=000000004
```

## 参照引数で受け取ったメモリを読み書きする

readmem/writemem 系は、固定メモリへの `ref` に対応します。自動変数、参照の転送、
待機するタスク、多次元配列の行への参照でも使えます。writemem は `const ref` も
使えます。アドレス範囲・列挙型の検査・X/Zの変換は、直接の呼び出しと共通です。

```systemverilog
module top;
  logic [7:0] memory[2:4];
  int fd;
  task automatic load(ref logic [7:0] m[2:4]);
    $readmemh("input.mem", m);
  endtask
  function automatic void save(const ref logic [7:0] m[2:4]);
    $writememh("output.mem", m);
  endfunction
  initial begin
    fd = $fopen("input.mem", "w");
    $fwrite(fd, "12 34 56\n");
    $fclose(fd);
    load(memory);
    save(memory);
    $display("%h %h %h", memory[2], memory[3], memory[4]);
  end
endmodule
```

```text
12 34 56
```

## 設計が待っている間の出力を見る

`--autoflush` は display・write・strobe・monitor 系のタスクごとに標準出力を
フラッシュします。標準出力宛のファイルタスクも含みます。既定は無効で、出力の内容や
スケジューリングは変えません。次のプロンプトは、設計が標準入力を待つ前に届きます。

```systemverilog
module top;
  int character;
  initial begin
    $write("ready> ");
    character = $fgetc(32'h8000_0000);
    $display("read=%0d", character);
  end
endmodule
```

```sh
printf x | build/dev/src/driver/kirimasim --autoflush demo.sv
```

```text
ready> read=120
```

## `$stop` の動作を選ぶ

```systemverilog
module top;
  initial begin
    $display("before");
    $stop(1);
    $display("after");
  end
  final $display("final");
endmodule
```

`--stop=ignore` で実行すると、次の出力になります。

```text
before
after
final
```

既定の `--stop=finish` は `after` を出さず、終了コード 0 で終了します。
`--stop=error` は同じ出力で終了コード 1 です。どちらも終了理由は `stop` のままです。
無視して継続する場合も、引数が 1・2 なら呼び出し位置の報告を出します。
引数 2 は、継続後の実行終了時に統計を要求します。

## 実時間に上限を設ける

```systemverilog
module top;
  int count;
  initial forever count++;
  final $display("final");
endmodule
```

```sh
build/dev/src/driver/kirimasim --max-wall-time=1 --max-steps=0 demo.sv
```

`final` を出し、理由 `wall_time_limit`・終了コード 4 で終了します。
上限はシミュレーション開始からの整数秒で、コンパイルは含みません。0 で無効です。
`--max-time-ok` ではこの終了コードを変えません。カーネルはイベント実行前、第 1 層は
静的初期化を含む分岐で検査します。待っているホスト側の処理を強制中断するものでは
ありません。final は、期限に止められず完了します。

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
[システムタスクの仕様書の 6.3 節](spec/SYSTEM_TASKS.ja.md)と
[SIR の実装の制限](spec/SIR_SPEC.ja.md)を参照してください。

## 実行時に決まる文字列への出力書式

`$sformat` と `$sformatf` は文字列や packed 値の実行時の書式に対応し、
関数の戻り値も使えます。書式の引数だけを解釈し、値の文字列に含まれる `%` は
データとして出力します。

```systemverilog
module top;
  string fmt, written, returned;
  initial begin
    for (int i = 0; i < 2; i++) begin
      fmt = i == 0 ? "%0d" : "%04h";
      $sformat(written, fmt, 10);
      returned = $sformatf(fmt, 10);
      $display("%s|%s", written, returned);
    end
  end
endmodule
```

```text
10|10
000a|000a
```

数値・文字列・実数の変換は既存の書式の規則に従います。`%t` は呼び出し元の
時刻単位と `$timeformat`、`%m` と `%l` は呼び出し元のスコープとセルを使います。
足りない値は何も出さず、余分な値は出力せず、呼び出し箇所ごとに 1 回警告します。
渡した値は、余分なものもすべて評価します。不正な書式や型の組み合わせは診断と
終了コード 2 で実行を終えます。X/Z を含む packed の書式と `%p` は未対応です。
定数の書式は引き続きエラボレーション時に検査します。
[システムタスクの仕様書の 3.13 節](spec/SYSTEM_TASKS.ja.md)を参照してください。

## 実行時に決まるプラスアーグの書式

`$value$plusargs` は、文字列や packed の整数値の実行時の書式に対応します。
関数の戻り値も使えます。この例は、同じ呼び出し箇所で最初の `+count=42` を
10 進数、次に 16 進数として読みます。

```systemverilog
module top;
  string fmt;
  int value, matched;
  initial begin
    for (int i = 0; i < 2; i++) begin
      fmt = i == 0 ? "count=%d" : "count=%h";
      matched = $value$plusargs(fmt, value);
      $display("%0d %0d", matched, value);
    end
  end
endmodule
```

`build/dev/src/driver/kirimasim demo.sv +count=42 +count=99` で実行します。

```text
1 42
1 66
```

最初に一致したプラスアーグを使います。一致しなければ 0 を返して出力の値を保ち、
その添字の関数も呼びません。対応する書式と変換は
[システムタスクの仕様書の 9 節](spec/SYSTEM_TASKS.ja.md)に従います。
実行時の不正な書式や出力の型と合わない変換は、診断と終了コード 2 で実行を
終えます。X/Z を含む packed の書式と、変換より前に `%%` がある書式は未対応です。

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

## 実行時の範囲外アクセスの警告

`--bounds-check=warn` は、範囲外や X/Z を含む添字を知らせます。値に一部だけ
重なる部分選択も対象です。

```systemverilog
module top;
  logic [7:0] value = 8'ha5;
  logic [3:0] selected;
  int index = 6;
  initial begin
    for (int repeat_count = 0; repeat_count < 2; repeat_count++) begin
      selected = value[index +: 4];
      $display("%b", selected);
    end
  end
endmodule
```

```text
xx10
xx10
```

`build/dev/src/driver/kirimasim --bounds-check=warn demo.sv` で実行します。
標準エラーには選択の位置で 1 回警告し、最後に、1 か所で 2 回の不正なアクセスが
あったと報告します。`--bounds-check=off`（既定）でも設計の出力とイベント回数は
同じです。packed 値の読み書き、固定長の unpacked 配列、automatic/ref の格納先、
NBA の代入先を検査します。回数は読み書きごとに数えるため、複合的な更新では
読み取りと書き込みの両方を含むことがあります。[CLI の仕様書](spec/CLI.ja.md)と
[値の意味の仕様書の 3.11 節](spec/VALUE_SEMANTICS.ja.md)を参照してください。

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
