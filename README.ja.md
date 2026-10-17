# kirimasim

[English](README.md)

kirimasim は、開発の初期にあるオープンソースの SystemVerilog シミュレータです。次のどれも手放さないシミュレータを目指しています。

- **4 値**（0、1、X、Z）を既定とし、4 値の意味を変える最適化をしない
- **IEEE 1800-2023 どおりのスケジューリング**。時刻スロットのすべての領域と、規格の参照アルゴリズムに従う
- **実用的な速さ**。X や Z が無いと証明できるとき、または実行時に確かめられるときは、2 値のコードで実行する

**コードの大半は AI（Claude Code）が生成し、人が仕様・テスト・レビューに責任を持ちます。** 詳しくは [docs/spec/AI_POLICY.md](docs/spec/AI_POLICY.md) と [CONTRIBUTING.md](CONTRIBUTING.md) を見てください。

## 状況（2026-10-17）

> **開発の初期です。実際のプロジェクトにはまだ使えません。** 最初の実行層（バイトコードのインタプリタ）で、RTL と簡単なテストベンチに使う SystemVerilog の広い範囲をシミュレーションできます。クラス、動的配列、キュー、乱数化、並行アサーション、ファイルの入出力、VPI、DPI-C はまだ無く、速さのための作業もまだ始めていません。まだ対応していない構文に会うと、その位置を示して「未対応（unsupported）」のエラーで止まります。黙って読み飛ばすことはしません。

### 段階

| 段階 | 内容 | 状態 |
| --- | --- | --- |
| P0 | 土台：ビルド、CI、詳細仕様書、テストスイート | 完了 |
| P1 | 中間表現（SIR）への変換、IEEE 1800-2023 のスケジューラを持つ実行カーネル、RTL と簡単なテストベンチのための第 1 層（バイトコードのインタプリタ） | **進行中** |
| P2 | 第 2 層：速さのため、SIR を前もって C++ に変換する | 予定 |
| P3 | テストベンチの機能：クラス、動的なデータ、制約付き乱数、並行アサーションとカバレッジ、クロッキングブロックと program、VPI、DPI-C | 予定 |
| P4 | スイッチのレベルの素子、マルチスレッドでの実行、他のシミュレータに組み込むライブラリ | 予定 |

### P1 の合格条件

条件と測り方は [docs/spec/TEST_SUITE.md](docs/spec/TEST_SUITE.md) で決めています。

| 条件 | 今 | 達成 |
| --- | --- | --- |
| [sv-tests](https://github.com/chipsalliance/sv-tests) のシミュレーションのテスト（固定したコミット、手書きのテスト 344 本）で Icarus Verilog 以上 | kirimasim 180 本、Icarus Verilog 210 本（14.0 devel、04cc493。合格条件で使う版） | まだ |
| 自前の RTL のテストの母集団（1466 本、一覧を固定）の 90% が通る | 1444 本が通る（98.5%）。失敗は 0 本で、残りの 22 本は後で作る構文で止まる | 達成 |
| 必須のテストの一覧（P3 が要る 1 本を除く） | すべて通る | 達成 |
| 速い値の演算と参照実装（`valsem`）の食い違いが無い：回数を 100 倍にした夜間のファジングを 7 回続けて | ファジングのテストはどのビルドでも通る。夜間の 7 回はまだ | 未測定 |

sv-tests の差の大部分は 7、8、16、21 章にあります。動的配列、キュー、連想配列、クラス（P3）、並行アサーション（P3）、ファイルの入出力と `$random`（P1、まだ）です。自前のテストスイートは別のリポジトリにあり、まだ公開していません。

### できること

以下はすべて第 1 層のインタプリタで動き、テストで確かめています。

- **設計の構造：** module、ポート（input、output、inout）、parameter と `defparam`、`generate`、インスタンスの配列、interface と modport、package とコンパイル単位（`$unit`）、階層名、specparam
- **データ型：** 64 ビットまでの 2 値と 4 値の整数型、`real`、`shortreal`、`realtime`、`string`、列挙型、packed と unpacked の構造体、packed の共用体、固定長の unpacked の配列（多次元、構造体の配列）、unpacked の配列のパラメータ、名前付きイベント、`chandle`（DPI-C までは null だけ）
- **ネット：** `wire`、`tri`、`wand`、`wor`、`tri0`、`tri1`、`supply0`、`supply1`、`uwire`。駆動の強さと、複数のドライバの解決。連続代入とネットの遅延（立ち上がり、立ち下がり、ターンオフ）。組み込みのゲートとプル。利用者定義のプリミティブ（組み合わせと順序）。`alias`
- **手続きと時間：** `initial`、`always`、`always_comb`、`always_latch`、`always_ff`、`final`。ブロッキングと NBA（代入の中の遅延を含む）。遅延とイベント制御。`wait (式)`。名前付きイベント（`->`、`->>`、`.triggered`）。`fork` と `join`、`join_any`、`join_none`。`wait fork`、`disable fork`。どのプロセスからでも働くブロックとタスクの `disable`。`assign`・`deassign` と `force`・`release`
- **関数とタスク：** static と automatic、再帰、`input`・`output`・`inout`・`ref` の引数、引数の既定値、unpacked の構造体と配列の引数、unpacked の構造体と配列を返す関数
- **文と式：** `if`、`case`、`casez`、`casex`、`case inside`。違反の報告を含む `unique`、`unique0`、`priority`。すべてのループ、`break`、`continue`、`return`。ストリーミング、`inside`、ワイルドカードの等価、代入の演算子と増減を含むすべての演算子。代入のパターン
- **アサーション：** 即時の `assert`、`assume`、`cover` と、遅延のもの（`#0`、`final`）
- **システムタスクと関数：** `$display` と `$write`（`b`・`o`・`h` の付いた形も）、`$strobe`、`$monitor`、`$monitoron`、`$monitoroff`、`$sformat`、`$sformatf`、`$swrite`、`$sscanf`、`$time`、`$realtime`、`$timeformat`、`$printtimescale`、`$finish`、`$stop`、`$fatal`、`$error`、`$warning`、`$info`、変換（`$rtoi`、`$itor`、`$realtobits` など、`$signed`、`$unsigned`）、`$bits`、`$typename`、配列の問い合わせ、`$clog2` と実数の数学関数、`$countbits`、`$countones`、`$onehot`、`$onehot0`、`$isunknown`、`$test$plusargs`、`$value$plusargs`
- **スケジューリング：** IEEE 1800-2023 の時刻スロットのすべての領域と参照アルゴリズム。Observed 領域での保留の報告の確定と、Postponed 領域での `$strobe`・`$monitor` を含む

### まだできないこと

| まだのもの | 予定 |
| --- | --- |
| 64 ビットを超える値 | P1。作業中（カーネルの演算はできた） |
| ファイルの入出力（`$fopen` など）、`$readmemh`・`$writememh`、`$random`・`$urandom`、波形（`$dumpvars`、VCD と FST）、イベント制御の `iff`、`%p` の書式、`$stime`、`$timeunit`、`$timeprecision`、`$stacktrace` | P1 |
| クラス、動的配列、キュー、連想配列、乱数化と制約、`randsequence`、カバーグループ、並行アサーションと `$sampled`・`$stable`、クロッキングブロック、virtual interface、`let`、checker | P3 |
| program ブロック。既知の不具合：今は program が module と同じに動き、IEEE 1800-2023 の決まり（スレッドは Reactive 領域に属する）と違う | P3 |
| VPI、DPI-C | P3 |
| 第 2 層（C++ の生成）と、速さのための作業 | P2 |
| MOS と双方向のスイッチ | P4 |

## ビルド

必要なもの：CMake 3.28 以上、Ninja、Python 3、C++23 のコンパイラ（macOS arm64 の Apple clang 21 で開発。CI は x86-64 の Linux でビルドします）。最初の configure で slang と GoogleTest を取得します。

```sh
cmake --preset dev            # ほかに release、asan、ubsan
cmake --build --preset dev
ctest --preset dev
```

| プリセット | フラグ |
| --- | --- |
| `dev` | `-O1 -g` |
| `release` | `-O3` |
| `asan` | `-O1 -g -fsanitize=address -fno-omit-frame-pointer` |
| `ubsan` | `-O1 -g -fsanitize=undefined -fno-sanitize-recover=all` |

ビルドの結果は `build/<プリセット名>/` に出ます。

## 使い方

```sh
kirimasim [オプション] <ファイル>... [+プラスアーグ]...   # エラボレーションしてシミュレーションする
kirimasim parse [オプション] <ファイル>...                # 読んでエラボレーションするだけ
kirimasim --version
```

例として、次の `counter.sv` を動かします。

```systemverilog
module counter(input logic clk, rst, output logic [3:0] q);
  always_ff @(posedge clk)
    if (rst) q <= '0;
    else q <= q + 1;
endmodule

module top;
  logic clk = 0, rst = 1;
  logic [3:0] q;
  counter dut(.clk, .rst, .q);
  always #5 clk = ~clk;
  initial begin
    $monitor("%0t: rst=%b q=%0d", $time, rst, q);
    #12 rst = 0;
    #40 $finish;
  end
endmodule
```

```text
$ build/dev/src/driver/kirimasim counter.sv
kirimasim: info: kirimasim 0.0.0 (commit ...) args: counter.sv
0: rst=1 q=x
5: rst=1 q=0
12: rst=0 q=0
15: rst=0 q=1
25: rst=0 q=2
35: rst=0 q=3
45: rst=0 q=4
kirimasim: info: finish at 52 ns
```

設計の出力は標準出力に、kirimasim 自身のメッセージと `$error` などのメッセージは標準エラーに出ます。

今使えるオプション：

| オプション | 意味 |
| --- | --- |
| `-f <ファイル>`、`+incdir+<ディレクトリ>`、`+define+<名前>[=<値>]`、`--top=<名前>` | ソース、インクルードのディレクトリ、マクロ、最上位のモジュール |
| `--timescale=<単位>/<精度>` | timescale を書いていない設計要素の時間の単位と精度（既定は `1ns/1ns`） |
| `--max-time=<時刻>`、`--max-time-ok` | シミュレーションの時刻で止める。終了コードを 4 でなく 0 にする |
| `--max-zero-delay=<n>`、`--max-steps=<n>`、`--max-scc-iter=<n>` | 時刻が進まない実行の上限 |
| `--dump-sir` | シミュレーションの代わりに中間表現を出す |
| `--stats=json` | 実行の回数のカウンタを JSON で出す |
| `+<名前>[=<値>]` | `$test$plusargs` と `$value$plusargs` のためのプラスアーグ |

終了コード：0 は `$finish`、`$stop`、事象が尽きた。1 は `$fatal`。2 は入力の誤り（未対応の構文を含む）。3 は内部エラー。4 は `--max-time` に達した。5 は時刻が進まない実行の上限に達した。コマンドラインのすべては [docs/spec/CLI.md](docs/spec/CLI.md) にあります。

## 仕組み

```text
SystemVerilog のソース
  -> slang（字句と構文の解析、プリプロセス、型の検査、エラボレーション）
  -> front    （slang のエラボレーションの結果を SIR に変換する）
  -> SIR      （スロットで引く中間表現：記憶のスロットと、型の決まった命令）
  -> analysis （X の到達、静的な島、レベル化、可観測性）
  -> 実行層。どれも共通の実行カーネルの上で動く
```

[slang](https://github.com/MikePopoloski/slang) は改変せず、1 つのコミットに固定して使います。どの実行層も、1 つの実行カーネルにプロセスを登録します。スケジューラと値の演算はカーネルが持つので、別の層のプロセスを 1 つのシミュレーションで一緒に動かせ、層の違いは出力の違いとして表に出ます。詳しくは [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md) を見てください。

## テストのやり方

- **仕様が先。** 値の意味、スケジューリング、システムタスク、SIR を [docs/spec/](docs/spec/) に書き、人の承認を受けてから実装します。規格が決めていない所は、kirimasim がどう読むかを仕様書に書きます。
- **値の意味の参照は 1 つ。** `valsem` は、遅いが明らかに正しい演算の実装です。カーネルの速い演算は、それとファジングで突き合わせます（1 語の値と、複数の語の値の両方）。
- **機構の行列。** 1 つの機構を、それが現れうる所の組み合わせで確かめます（たとえば `disable` なら、ブロックの中、fork の枝、別のプロセス、タスクから、それぞれ手続き、タスク、fork の枝の中で）。テストは `tools/` の生成器が作り、期待値は書いた規則から決まります。生成器は 25 個、`tests/suite/` のテストは 128 本、CTest のテストは全部で 322 本です。
- **陰性対照。** `tools/mutants.py` は、知られた誤り 128 個を 1 つずつコードに入れます。どの誤りでも、どれかのテストが落ちなければなりません。
- **他のシミュレータは証拠であって正解ではない。** Icarus Verilog と Verilator でもテストを動かし、規格と違うところはテストのヘッダに書きます。
- **サニタイザと静的な検査：** AddressSanitizer、UndefinedBehaviorSanitizer、clang-format、clang-tidy を push の前に毎回走らせます。

## 貢献：今は仕様の提案と Issue だけを受け付けています

kirimasim は開発の初期にあるため、今は**仕様の提案と Issue だけ**を受け付けています。コードの PR は受け付けていません（PR を作れるのは協力者だけという設定にしています）。`docs/spec/` の仕様書への提案、不具合の報告、期待値の誤りの指摘には、Issue のフォームを使ってください。受け付けを広げるときは、この節と [CONTRIBUTING.md](CONTRIBUTING.md) でお知らせします。

## 文書

| 文書 | 内容 |
| --- | --- |
| [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md) | ライブラリの分割と実行の仕組み（英語） |
| [docs/spec/VALUE_SEMANTICS.md](docs/spec/VALUE_SEMANTICS.md) | 値、演算、ネット、強さ、ゲート、UDP |
| [docs/spec/SCHEDULER_SPEC.md](docs/spec/SCHEDULER_SPEC.md) | 時刻スロット、領域、待ち、プロセス、disable、保留の報告 |
| [docs/spec/SIR_SPEC.md](docs/spec/SIR_SPEC.md) | 中間表現 |
| [docs/spec/SYSTEM_TASKS.md](docs/spec/SYSTEM_TASKS.md) | システムタスクと関数、メッセージの形 |
| [docs/spec/CLI.md](docs/spec/CLI.md) | コマンドライン、終了コード |
| [docs/spec/TEST_SUITE.md](docs/spec/TEST_SUITE.md) | テストの決まり、P1 の合格条件 |
| [docs/spec/RUNTIME_ABI.md](docs/spec/RUNTIME_ABI.md) | カーネルと実行層の境界 |
| [docs/spec/PERFORMANCE.md](docs/spec/PERFORMANCE.md) | 性能の目標と回数のカウンタ |
| [docs/spec/AI_POLICY.md](docs/spec/AI_POLICY.md) | このプロジェクトでの AI の使い方 |
| [CONTRIBUTING.md](CONTRIBUTING.md) | 貢献の方法（英語） |
| [CHANGELOG.md](CHANGELOG.md) | 変更の記録 |

## ライセンス

kirimasim のライセンスは Apache License 2.0 with LLVM Exceptions（`Apache-2.0 WITH LLVM-exception`）です。[LICENSE](LICENSE) と [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md) を見てください。
