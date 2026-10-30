# 最近の P1 の機能：使い方と制限

[English](P1_FEATURES.md) · 英語版が主で、日本語版はそれに従います。

**改訂（2026-10-24）：** 旧P2→P3・旧P3→P4・旧P4→P5と付け替え、新しいP2（SIR v1）を設けました（tier番号は不変）。[対応表](notes/phase-renumbering.ja.md)。

2026-10-30に追加した配列クエリの実行時次元・混在次元foreach・除去パルスの報告と、10月29日の初期値の選択・共通入力オプションを
含む、第1層の使い方です。P1は進行中です。対応範囲の全体と
制限は [P1 の対応機能一覧](P1_SUPPORT.ja.md)、残っている作業は
[README](../README.ja.md)を参照してください。
[開発用のビルド](../README.ja.md)の後、リポジトリのルートから実行します。
本文内のSystemVerilog例は1つずつ`demo.sv`に保存します。リポジトリ内のファイルを
指定するコマンドは、そのまま実行できます。

```sh
build/dev/src/driver/kirimasim demo.sv
```

以下の出力例は、設計の標準出力だけを示します。kirimasim 自身のメッセージは
標準エラーに出ます。

## 除去されたパルスの報告

`--report-pulses`は慣性遅延で取り消された変化を報告します。次の例を`demo.sv`に保存します。

```systemverilog
module top;
  timeunit 1ns; timeprecision 1ps;
  logic source = 0;
  wire delayed;
  assign #5 delayed = source;
  initial begin
    #6 source = 1;
    #1 source = 0;
    #5 $display("delayed=%b", delayed);
  end
endmodule
```

```sh
build/dev/src/driver/kirimasim --report-pulses demo.sv
```

```text
delayed=0
```

入力の1ナノ秒のパルスは5ナノ秒の遅延より短いため、`7 ns`で標準エラーへ代入の
ソース位置付きの注記を出します：
`note: inertial delay removed a pulse at 7 ns (--report-pulses)`。
出力は0のままです。フラグを省いても標準出力とカウンタは同じで、パルス注記だけが出なくなります。

異なる値の再評価が保留中の更新を正常に取り消すたびに1件報告し、同じ位置の繰り返しも
省略しません。連続代入・ゲート・UDP・強度の起点と遅延付き受動辺の導通を同じ仕組みで
報告します。同じ保留値は元の予定時刻を保ち、注記を出しません。確定した更新、
transport NBA、実行終了時の後片付けも報告しません。時刻は設計の精度から最大の正確な
整数単位を選び、`$timeformat`は適用しません。`parse`と`--dump-sir`はフラグを受け付けますが、
パルスの報告は行いません。

## ソース変数の初期値

`--init-values=x|zero|one|random`は、初期値のない4値整数のソース変数とメモリの
既定ビットを選びます。次の例を`demo.sv`に保存します。

```systemverilog
module top;
  logic [3:0] value;
  logic [3:0] explicit_value = 4'bxz01;
  initial $display("value=%b explicit=%b", value, explicit_value);
endmodule
```

```sh
build/dev/src/driver/kirimasim --init-values=one demo.sv
```

```text
value=1111 explicit=xz01
```

既定の`x`は規格どおりです。X以外を選ぶと版の行の後に標準エラーへ注記を出します。
変数・メンバーの明示初期値、ネット、2値・整数以外の値は通常の初期化を保ちます。
不正な読み出し、空のキューのpop、unpacked条件式の対応しない要素も標準の既定値を
保ちます。構造体メンバーの宣言初期値はoutput仮引数と暗黙の関数戻り値にも適用します。

static変数は1回、自動変数は宣言を実行するたび、automatic outputと関数の戻り値は
呼出しごとに初期化します。動的配列の`new[n](old)`はコピーした先頭を保ち、
新規の末尾だけに方針を適用します。forkのcaptureは取得した値を保ちます。

`--init-values=random --seed=42`は専用のSplitMix64系列で既知の乱数ビットを生成します。
同じ設計・オプション・シードなら初期値を再現でき、設計の`$random`・`$urandom`の
系列を消費しません。`parse`はエラボレーションだけを行い、`--dump-sir`は初期化の
由来と命令を実行せず表示します。どちらもX以外の注記は出しません。

## 共通の入力オプション

`-I`はインクルードパス、`-D`はマクロ、`-y`はモジュールライブラリの探索を指定します。
次の例は[ユーザーマニュアルのソース](USER_MANUAL.ja.md#最初のシミュレーション)を使います。

```sh
build/dev/src/driver/kirimasim --top=tb \
  -Idocs/examples/user_manual/include -DCOUNTER_WIDTH=8 \
  -y docs/examples/user_manual docs/examples/user_manual/tb.sv +cycles=6
```

```text
width=8 cycles=6
t=20 ns q=1
t=30 ns q=2
t=40 ns q=3
t=50 ns q=4
t=60 ns q=5
t=70 ns q=6
```

`settings.svh`はインクルードパス、`counter.sv`はライブラリのディレクトリから見つけます。
`-I`・`-D`・`-y`は連結形・分離形に対応し、ライブラリには`--libdir=<dir>`も使えます。
既存の`+incdir+`・`+define+`も使えます。`-DCOUNTER_WIDTH=8`はコンパイル時のマクロ、
`+cycles=6`は実行時に読み取る引数です。値なしの`-DNAME`は`1`、`-DNAME=`は空に展開します。
シミュレータ間でコマンドを共有する場合は、マクロ値を明記してください。探索規則は
[マニュアル](USER_MANUAL.ja.md#ライブラリモジュールの探索)、受理形式は
[承認済みCLI仕様](spec/CLI.ja.md)を参照してください。

## クラスのstaticプロパティ

staticプロパティは、オブジェクトを生成しなくても初期化されます。
[未使用宣言の独自テスト](../tests/suite/proc/class_static_discovery_unit_unused.sv)を実行します。

```sh
build/dev/src/driver/kirimasim tests/suite/proc/class_static_discovery_unit_unused.sv
```

```text
calls=2 trace=12
```

2つのstatic初期化がmoduleのinitialより先に実行され、instance fieldの初期化は実行されません。
修飾名・別名・継承名・staticメソッドから通常の型付き記憶域を操作します。
[実装と制限](notes/class-static-execution.ja.md)を参照してください。
パラメータ付きstatic記憶域とオブジェクト実行は残作業です。

## 未終了のruntime processの上限

```sh
build/dev/src/driver/kirimasim --max-procs=1 tests/cli/process_limit_matrix/burst_module_join_1.sv
```

```text
final=0
```

この独自設計は1つのforkで子を2つ作ろうとします。上限1では2つ目の登録を拒否し、
子が開始する前に停止します。finalを実行し、理由`resource_limit`・終了コード6で終わり、
診断に`--max-procs=1`を示します。この設計を完走するには2を指定し、0なら上限を無効に
できます。既定は1000000です。静的プロセスは数えません。終了した子は、子孫やレポートの
ために記録が残っていても枠を返します。[実装・検証・制限](notes/process-limit-execution.ja.md)を参照してください。

## 固定配列ポート

固定配列のinput/outputと互換netのinoutが、宣言順に要素を対応させて実行できます。
たとえば親の`wire [7:0] a[4:5]`と子の`input wire [7:0] p[7:6]`を接続すると、
`a[4]`と`p[7]`、`a[5]`と`p[6]`が同じ実体になります。`$left(p)`は7のままです。
変数を接続するコピー経路も全RHSを先に捕捉し、要素の更新に応答します。
強度、force/release、pull・既定値、programを含む[検証と制限](notes/fixed-array-port-execution.ja.md)を参照してください。

## 32入力を超えるUDP

既存のUDP規則のまま、32入力を超える評価に対応しました。独自の33入力の組合せ例を
次のように実行できます。

```sh
build/dev/src/driver/kirimasim tests/suite/nets/wide_udp_module_comb_33.sv
```

```text
zero 0
one_input 0
both 1
unknown x
dominate 0
other_zero 0
high_z x
z_zero 0
```

[106ソースの証拠と制限](notes/wide-udp-inputs.ja.md)に順序UDPと広い入力の転送も含めます。
密なエッジの網羅性表ではslangのコンパイル時間が増える問題が残り、UDPソース全体の対応完了
とは扱いません。

## fork内の宣言と親での初期化

fork内の宣言は、親で寿命どおり初期化し、子に数えたり`join_any`を満たしたりしません。
static記憶域は共有し、捕捉のないautomatic宣言は入るたびに初期化します。
宣言だけのforkは待機せず進みます。[独自72ソースと制限](notes/fork-local-declarations.ja.md)を
参照してください。[automaticの共有捕捉](notes/automatic-front-capture.ja.md)は寿命・型・joinを
またぐ新規186ソースで通りました。旧未対応3テストの期待値は別途承認後に`11096dc3`で移行済みです。
automatic scalar/固定配列eventの既定値も、下記の範囲で実装しました。

## taskの復帰後も保持するautomatic event

初期化子のないautomatic scalar eventは、宣言を実行するたびに固有objectを作ります。
固定配列の各要素も別のobjectで、昇順・降順・多次元には通常の型付き添字規則を使います。
子はtaskの復帰後も囲んでいるautomatic範囲を保持します。次の独自例は、独立したtask活動を
2つ作り、`join_none`の後で両方の子孫を待ってから合計を表示します。

```sh
build/dev/src/driver/kirimasim tests/suite/sched/automatic_event_capture_module_join_none_scalar_named_active.sv
```

```text
total=3 errors=0
```

もう1つの独自例は、時刻を進めずに関数の二次元宣言へ64回入り直し、全要素が毎回
未発火の状態から始まることを検査します。

```sh
build/dev/src/driver/kirimasim tests/suite/sched/automatic_event_default_function_rank2.sv
```

```text
checked=256 errors=0
```

collectorは物理event行を増やす前に全typed rootを公開し、コンテナheapとfinalの回収時点は
保ちます。[実行と検証](notes/instruction-event-execution.ja.md)を参照してください。
[eventの入力引数](notes/event-input-execution.ja.md)は、同期/待機呼び出し・固定配列・再帰・
遅延アクションで同じobjectを共有します。[宣言初期化](notes/event-initializer-execution.ja.md)も、
nullを含むscalarと固定配列patternで既存event objectを共有します。staticは時刻0前に一度、
automaticは宣言へ入るたびに初期化します。[automatic構造体のeventメンバー](notes/automatic-struct-events.ja.md)も
同じ型付き転送を使います。scalar eventの戻り値/条件式、広い再関連付け/比較の契約は残作業です。

```sh
build/dev/src/driver/kirimasim tests/suite/sched/event_initializer_capture_module_join_none_named_managed_nba.sv
```

```text
total=3 errors=0
```

## 広いネットでL/Hを解決する

広い解決対象ネットでも、三状態ゲートの片側のL/H強度マスクを保持します。
両半分がワードの途中に来る場合にも対応します。[独自73件の参照比較と制限](notes/wide-gate-strength.ja.md)を
参照してください。解決対象ネットの遅延付き三状態出力は、承認済みの
[sideの遅延案](notes/delayed-gate-side-proposal.ja.md)に従い、値と両方のside欄を一つの状態として伝えます。
[実装](notes/delayed-gate-state-execution.ja.md)を参照してください。

```systemverilog
module top;
  logic [64:0] data = '0;
  logic control = 'x;
  wire [64:0] q0, q1;
  bufif1 g0[64:0](q0, data, control);
  bufif1 g1[64:0](q1, data, control);
  assign (weak1, weak0) q0 = '0;
  assign (weak1, weak0) q1 = '1;
  initial begin
    #1; $display("%0d %0d", q0 === '0, q1 === 'x);
    data = '1;
    #1; $display("%0d %0d", q0 === 'x, q1 === '1);
  end
endmodule
```

```text
1 1
1 1
```

## 代入先の副作用より前に集約値を保持する

通常の集約代入・NBAは、実行時の代入先の添字より先に右辺を保持します。
これは承認済みのkirimasimの順序で、IEEEは左右の相対順を未規定としています。
代入先の関数が元の値を変えても、保持した値は変わりません。コンテナのleafも
後のパターンのメンバーより先に捕捉します。[契約と検証](notes/aggregate-value-capture.ja.md)を
参照してください。

```systemverilog
module top;
  typedef struct { int value; } row_t;
  row_t a[2];
  int index = 0, trace = 0;
  function automatic int choose();
    trace = trace * 10 + 2;
    return index++;
  endfunction
  function automatic row_t source_value();
    trace = trace * 10 + 1;
    index = 1;
    return '{7};
  endfunction
  initial begin
    a[0] = '{0}; a[1] = '{0};
    a[choose()] = source_value();
    $display("%0d %0d", trace, index);
    $display("%0d %0d", a[0].value, a[1].value);
  end
endmodule
```

```text
12 2
0 7
```

実行時に選ぶ固定の部分配列と、構造体要素の配列メンバー全体も代入先にできます。
明示的なautomaticの配列メンバー付き構造体には既存の制限が残ります。
集約値全体へのブロッキング代入内の遅延・イベント・repeatにも、専用の型付きフレームの
スナップショットで対応しました。[実装と別レビューの検証](notes/blocking-aggregate-timing.ja.md)を
参照してください。イベント制御付きNBAは独立した保留処理で実行します。
[ソース対応と残る制限](notes/event-nba-source-execution.ja.md)を参照してください。

## イベントを繰り返し待つブロッキング代入

代入内の`repeat`は、右辺を保持してから回数を1回評価します。待機中に元の値や回数を
変えても、保存した値と待つ回数は変わりません。0・負・X/Zの回数ではイベントを待たず、
回数の幅と符号を保ちます。ブロッキング代入先は待機後に選びます。
[契約と検証](notes/repeated-event-controls.ja.md)も参照してください。
固定配列・unpacked構造体全体にも同じ時間制御の規則を適用し、対象を実行時に選べます。
イベント制御付きのノンブロッキング代入も、[記載したソースの範囲](notes/event-nba-source-execution.ja.md)で実行できます。

```systemverilog
`timescale 1ns/1ns
module top;
  bit clk = 0;
  int q = 1, data = 7, count = 2;
  always #1 clk = ~clk;
  initial begin
    q = repeat(count) @(posedge clk) data;
    $display("%0d %0t", q, $time);
    $finish(0);
  end
  initial begin #2; data = 9; count = 7; end
endmodule
```

```text
7 3
```

## キュー要素の変更をmonitorで表示する

`$monitor`は、既存queueの要素値を変更したときにも再表示します。
次の例は同じコンテナのハンドルを保ったまま、先頭要素を変更します。

```systemverilog
module top;
  int values[$];
  initial begin
    values.push_back(0);
    $monitor("%0d", values[0]);
    #2 values[0] = 9;
  end
endmodule
```

```text
0
9
```

[検証記録と制限](notes/pattern-sir-execution.ja.md)を参照してください。
ソース`%p`は引き続き未対応です。heapを使うfile monitorの組み合わせは、
[独自ソース20件](notes/heap-file-monitor-lifetime.ja.md)（2026-10-25）で検査しました。
コールバックからの再登録・再確保と、その他のソースの文脈には、より広い証拠が必要です。

## 動的配列とキュー

構築、値のコピー、要素アクセス、キューのメソッドを、共通の型付きヒープで実行します。
代入・値引数でコピーした要素は独立し、refは実際の上限と安定したセルを保ちます。
[対応範囲と残作業](P1_SUPPORT.ja.md#動的コンテナの共通基盤)も参照してください。

```systemverilog
module top;
  int a[], q[$], last;
  initial begin
    a = new[3];
    a[1] = 7;
    q = '{4, 5};
    q[$+1] = a[1];
    last = q.pop_back();
    $display("%0d %0d %0d", a.size(), q.size(), last);
    foreach (q[i]) $display("%0d", q[i]);
  end
endmodule
```

```text
3 2 7
4
5
```

## 混在次元のforeach

foreachでは、固定unpacked次元、動的配列・キュー、固定packed次元を混在させられます。
この例は行1→0、各キューの添字0から昇順、packed添字2→0の順に反復します。

```systemverilog
module top;
  logic [2:0] data [1:0][$];
  int bits, ones;
  initial begin
    data[1] = '{3'b101, 3'b010};
    data[0] = '{3'b111};
    foreach (data[row,item,bit_index]) begin
      bits++;
      if (data[row][item][bit_index] == 1'b1) ones++;
    end
    $display("bits=%0d ones=%0d", bits, ones);
  end
endmodule
```

```text
bits=9 ones=6
```

外側の次元ほどゆっくり変化します。固定範囲は、負の境界を含め宣言の向きを保ちます。
合法な省略次元は反復しません。例えばpacked語の動的配列で`foreach (a[,bit_index])`とすると、
packed添字だけを反復します。全変数を省略した場合は本体を実行しません。省略した次元の後の
名前付き動的次元と、foreach変数への代入は引き続きフロントエンドが拒否します。

添字と各動的次元へ入った時点の上限はframeに保持するため、待機や同時に呼び出す
automatic処理でも独立した状態を保ちます。`break`はforeach全体を終了し、`continue`は
最内の名前付き次元を進めます。内側の通常ループは自身のbreak/continueの範囲を保ちます。
反復対象をforeach中にサイズ変更した結果はIEEE 1800-2023の12.7.3で未定義であり、
この実装で移植可能なサイズ変更規則は追加しません。固定unpacked複合型を要素とする
動的コンテナは引き続き未対応です。

## 実行時の配列クエリの次元

`$left`・`$right`・`$low`・`$high`・`$increment`・`$size`の次元番号に、実行時の式を
指定できます。宣言の境界を保ち、最外のunpacked次元からpacked次元へ番号を付けます
（IEEE 1800-2023 20.7）。動的配列・キュー・文字列の第1次元は現在の長さを使い、
空の場合の右端・上限は-1です。範囲外やX/Zの実行時次元は全ビットXを返します。
広い次元番号は、狭い型へ変換する前に範囲を検査します。

```systemverilog
module top;
  logic [2:-1] data[-2:0];
  initial begin
    for (int dim = 1; dim <= 2; dim++)
      $display("%0d %0d %0d", $left(data, dim), $right(data, dim), $size(data, dim));
  end
endmodule
```

```text
-2 0 3
2 -1 4
```

固定次元の問い合わせでは対象の値の式を評価せず、動的な第1次元では1回評価します。
次元番号の式も1回だけ評価します。`$dimensions`・`$unpacked_dimensions`は値を読まず
型をたどるため、階層参照や入れ子の動的型にも対応します。

内側の動的次元を選ぶ定数の次元番号は、20.7.1どおり言語エラーです。
内側に動的次元や文字列の次元がある型への実行時次元クエリは、引き続き位置付きの
未対応診断にします。先に個々の部分配列を選択すれば、その第1次元を照会できます。
連想配列と未対応の値の表現は残作業です。

## 動的配列・キューの条件式

[ローカルのコンテナ条件式](notes/container-conditional-execution.ja.md)は、結果の記憶域が独立する
`?:`に対応します。既知条件は片側だけを評価します。不定条件なら左から両値を捕捉し、同じ長さの
配列は要素全体が確実に等しい場合に残し、それ以外を型の既定値にします。異なる長さなら空です。
整数要素をビット単位で統合する処理ではありません。次の例では4値の既定値Xを使います。

```systemverilog
module top;
  logic [3:0] a[$], b[$], result[$];
  logic condition;
  initial begin
    a = '{4'd3, 4'd6};
    b = '{4'd3, 4'd7};
    condition = 'x;
    result = condition ? a : b;
    $display("%b %b", result[0], result[1]);
  end
endmodule
```

```text
0011 xxxx
```

## 実行時の連結とキューのスライス

unpacked連結では配列・キュー全体と個々の要素を組み合わせられます。
キューの読み取りスライスは要素をコピーし、その後コピー先を書いても元の値は変わりません。
境界は実行時の式を使え、X/Z、空・逆順の選択は空のキューを返します。
スライスを代入先にする形は引き続き未対応です。

```systemverilog
module top;
  int q[$], r[$];
  initial begin
    q = '{4, 7};
    r = {0, q, 9};
    q = r[1:$-1];
    q[0] = 8;
    $display("%0d %0d %0d %0d", q.size(), q[0], q[1], r[1]);
  end
endmodule
```

```text
2 8 7 4
```

## automatic構造体の固定配列メンバー

automaticなunpacked構造体で、固定leaf配列メンバーに対応しました。入れ子の構造体の
多次元配列、宣言・メンバーの既定値、構造体のinput・output・inoutと戻り値、
配列メンバーのref、forkによる保持に、既存の型付き記憶域と要素ごとのコピーを使います。
[実行例と検証](notes/automatic-struct-array-members.ja.md)を参照してください。
automatic構造体配列と固定event配列メンバーは未対応です。

## automatic構造体のeventメンバー

automaticなunpacked構造体のスカラーeventメンバーも、単独のautomatic eventと同じ
型付き記憶域を使います。入れ子、宣言時のコピー・パターン、nullのメンバー既定値、inputコピーに
対応します。forkの子孫は待機・保留triggerの間も元の構造体を保持します。
[実行例と検証](notes/automatic-struct-events.ja.md)を参照してください。
一般の再束縛、eventを含むoutput/inout・戻り値の構造体、automaticな構造体配列・
固定配列メンバーは、この対応範囲には含めません。

## 名前付きeventの状態と関数での読み出し

未発火のeventは64ビットの最大時刻でも偽で、発火したeventはそのタイムスロットだけ真です。
通常の関数でこの状態を読め、明示的なguardは関数内static eventへの依存も保持します。
関数経由の暗黙の名前付きevent感度は、引き続き位置付き未対応診断になります。
[新規27ソースとカーネル20テスト](notes/managed-event-registry.ja.md)で修正と管理eventの
土台を検査しました。後続の[カーネル実行時所有](notes/managed-event-runtime.ja.md)で
型付き待機・trigger・monitorのpinを追加し、23テストとコンパイルした16故障の検出を確認しました。
後続の[公開済み命令境界の回収](notes/instruction-event-execution.ja.md)で、native/SSA/転送の完全な
rootとautomatic scalar/固定配列の宣言既定値を接続し、ソース176件と12故障の検出が通りました。

## 名前付きイベントの混合待ちと項ごとのiff

1つのイベント制御で、名前付きイベント・信号エッジ・式の監視を混ぜられます。
各項の任意の `iff` は、その項のイベントが起きたときに評価します。偽・Xなら
待ちを保ち、条件の変化だけでは再開しません。最初に条件を満たした項が待ち全体を
終え、他の名前付き項・購読・監視も解除します。同じ時刻でも、次の待ちで同じ
イベントへ再登録できます。programはReactive側で再開します。[実行時の固定配列event添字](notes/indexed-event-execution.ja.md)は、
named/mixed待ち、遅延対象、明示的な`.triggered`に対応します。対象は登録時に1回評価し、
nullを待つ場合は位置付き実行時エラーの方針を保ちます。C++と追加方式のC ABI 0.3は同じカーネルへ到達します。

```systemverilog
`timescale 1ns/1ns
module top;
  event go;
  logic enabled = 0;
  bit clk = 0;
  initial begin
    @(go iff enabled or posedge clk);
    $display("first %0t", $time);
    @(go iff enabled or posedge clk);
    $display("second %0t", $time);
  end
  initial begin
    #1; ->go;
    #1; enabled = 1;
    #1; ->go;
    #1; clk = 1;
  end
endmodule
```

```text
first 3
second 4
```


## packedメンバー・部分範囲の中の実行時選択

実行時に選ぶ子の範囲で、静的に選んだ親の境界を保持するようになりました。
親の有効なビットだけを書き、隣のメンバー・packed要素は変えません。複合代入の
読み取りも同じ親の境界を使います。slot・automaticフレーム・refはカーネルの共通の
切り詰めを使い、staticな対象ではNBA・遅延付きNBAにも対応します。
この例では下位バイトのビット7だけを書き、上位バイトを保ちます。親の位置も
実行時に変わる入れ子や、親の定数範囲自体が格納先をはみ出す形には未対応が残ります。

```systemverilog
module top;
  logic [1:0][7:0] value = 16'h5500;
  int index = 7;
  initial begin
    value[0][index+:3] = 3'b111;
    $display("%h", value);
  end
endmodule
```

```text
5580
```


## ストリーミング対象への遅延付きNBA

ストリーミング対象の `<=` に代入内遅延を使えるようになりました。
ビット順、右辺の左側からの切り詰め、短い最終ブロック、2値への変換は通常の
ストリーミングと共通です。右辺を先に、遅延と対象の添字をそれぞれ1回評価してから、
共通の遅延書き込みで各leafを保持します。その後に値や添字を変えても予約した更新は
変わりません。automaticな対象、入れ子のストリーミング対象、`with`の制限は残ります。

```systemverilog
`timescale 1ns/1ns
module top;
  logic [7:0] bytes_out[2];
  logic [15:0] data;
  initial begin
    bytes_out = '{0,0}; data = 16'h1234;
    {<<8{bytes_out}} <= #2 data;
    data = 0;
    #3; $display("%h %h", bytes_out[0], bytes_out[1]);
  end
endmodule
```

```text
34 12
```


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
refや文字列の文字要素を通じた遅延付き更新は未対応です。イベント制御付きNBAは、
別途[対応範囲と残る制限](notes/event-nba-source-execution.ja.md)を記載しています。

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

## 無書式のバイナリのワードを書き出す

`%u` は2値、`%z` は4値の生データを、32ビットのワードにそろえて下位のワードから
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
`$sscanf`・`$fscanf` も同じワード形式を読みます。次の入力例を参照してください。

## `$fread`で整数へバイナリ入力する

整数の代入先ではceil(幅/8)バイトを上位から読みます。start・countを指定しても
読み取りを制限しません（21.3.4.4）。指定した式はプロジェクトの引数評価規則に従い、
左から1回ずつ評価します。整数の実引数はSIR_SPEC 8に従いコピー入力時に読み、正の
バイト数を書き戻す時に代入先を再評価するため、添字の副作用は両方の位相で実行されます。
途中のEOFでは未入力ビットのコピー入力値を保ち、0バイトなら書き戻しません。
先の未承認の1回捕捉を訂正した理由は[訂正記録](notes/fread-copyback-correction.ja.md)を参照してください。

```systemverilog
module top;
  bit [15:0] value;
  int fd, n;
  initial begin
    fd = $fopen("bytes.bin", "wb");
    $fwrite(fd, "%c%c", 8'h81, 8'h12);
    $fclose(fd);
    fd = $fopen("bytes.bin", "rb");
    n = $fread(value, fd, 19, 0);
    $display("%0d %h", n, value);
    $fclose(fd);
  end
endmodule
```

```text
2 8112
```

メモリの代入先ではstart・countが対象の要素を選びます。残り1次元ならstartは実際の添字、
2次元以上なら0からの線形位置です。数値の小さい添字から大きい添字へ、右端の次元を最速で
進めます。[承認済みの実装](notes/multidim-fread-execution.ja.md)では、選択済みの接頭添字も
後の引数の副作用より前に保持します。動的メモリと多次元の構造体メモリは未対応です。

## 無書式のバイナリのワードを読み込む

`%u`・`%z` は下位の32ビットワードからホストのバイト順で読みます。幅の省略と0は
代入先のビット幅をワードへ切り上げ、正の幅はバイト数でなく転送するビット数です。
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
それぞれ 1 回評価します。[値の意味論の仕様書の 7.7 節](spec/VALUE_SEMANTICS.ja.md)
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
ノンブロッキング代入先にはできません。動的コンテナのストリーミング、`with`、
入れ子のストリーミング代入先は未対応で、P1の必須部分は[P1_SCOPE](spec/P1_SCOPE.ja.md)に従います。最後の短いスライスの扱いは
[値の意味論の仕様書の 8 節](spec/VALUE_SEMANTICS.ja.md)を参照してください。

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

## 実行時に決まるプラス引数の書式

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

最初に一致したプラス引数を使います。一致しなければ 0 を返して出力の値を保ち、
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
[値の意味論の仕様書の 3.11 節](spec/VALUE_SEMANTICS.ja.md)を参照してください。

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

## 実行時の配列メソッド

固定配列・動的配列・キューで`sum`・`product`・`and`・`or`・`xor`・`sort`・`rsort`・
`reverse`・`min`・`max`を共通の処理で実行します。

```systemverilog
module top;
  int a[];
  int smallest[$];
  initial begin
    a = '{8, 3, 6, 3};
    $display("sum %0d", a.sum());
    smallest = a.min() with (-item);
    $display("chosen %0d %0d", smallest.size(), smallest[0]);
    a.sort();
    $display("sorted %0d %0d %0d %0d", a[0], a[1], a[2], a[3]);
  end
endmodule
```

```text
sum 20
chosen 1 8
sorted 3 3 6 8
```

min/maxはキーではなく元の要素を返し、同値でも1個です。キーに副作用があっても、
受信配列は先に値を保持します。整列は純粋なキーを必要とし、要素のセルを指すrefを保って
値を移動します。空の縮約、不明な比較、parameterの畳込みを含む
[検査済みの契約と制限](notes/runtime-array-methods.ja.md)を参照してください。
