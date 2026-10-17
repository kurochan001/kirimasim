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
| [sv-tests](https://github.com/chipsalliance/sv-tests) のシミュレーションのテスト（固定したコミット、手書きのテスト 344 本）で Icarus Verilog 以上 | kirimasim 179 本、Icarus Verilog 210 本（14.0 devel、04cc493。合格条件で使う版） | まだ |
| 自前の RTL のテストの母集団（1466 本、一覧を固定）の 90% が通る | 1444 本が通る（98.5%）。失敗は 0 本で、残りの 22 本は後で作る構文で止まる | 達成 |
| 必須のテストの一覧（P3 が要る 1 本を除く） | すべて通る | 達成 |
| 速い値の演算と参照実装（`valsem`）の食い違いが無い：回数を 100 倍にした夜間のファジングを 7 回続けて | ファジングのテストはどのビルドでも通る。夜間の 7 回はまだ | 未測定 |

sv-tests の差の大部分は 7、8、16、21 章にあります。動的配列、キュー、連想配列、クラス（P3）、並行アサーション（P3）、ファイルの入出力と `$random`（P1、まだ）です。自前のテストスイートは別のリポジトリにあり、まだ公開していません。

### テストの結果

2026-10-17 に測りました。Icarus Verilog と Verilator は、その日の main の最新の開発版をビルドして使いました。

| テストの集まり | 本数 | kirimasim | Icarus Verilog | Verilator |
| --- | --- | --- | --- | --- |
| このリポジトリのテスト（CTest） | 324 | 322、2 本は飛ばす | — | — |
| [sv-tests](https://github.com/chipsalliance/sv-tests) のシミュレーションのテスト | 344 | 179 | 213 | 321 |
| 自前のテストスイートの P1 の母集団 | 1,466 | 1,444 | 1,274 | 1,329 |

このリポジトリで飛ばしている 2 本は、クロッキングブロックが要るもの（P3）と、ネットの一部どうしのポートを通ってドライバが出会うもの（まだ対応していない）です。sv-tests では Verilator が大きく先を行っています。差の大部分は、kirimasim が P3 に回している動的配列、キュー、連想配列、クラス、UVM で、次がファイルの入出力と `randsequence` です。

自前のテストスイートは kirimasim と一緒に書いたもので、中立な比較ではありません。どのテストも、挙げた規格の節と照らし合わせてあり、期待する出力は Icarus Verilog 13.0（Icarus Verilog 13.0 がコンパイルできないテストは Verilator 5.052）の出力と一致します。どちらも規格に従わない少数のテストは、規格に従っています。kirimasim は P1 の合格条件の決まり（出力、終了コード、kirimasim 自身のメッセージ）で、ほかの 2 つは出力だけで、テストスイートの道具で判定しています。Verilator は 2 値のシミュレータなので、x や z を表示するテストは通りません。

**時間**（テストごとの時間の合計。6 本ずつ並べて実行。1 回の測定で、[PERFORMANCE.md](docs/spec/PERFORMANCE.md) がベンチマークに求める 5 回の中央値ではありません）：

| テストの集まり | シミュレータ | 準備 | 実行 | 合計 |
| --- | --- | --- | --- | --- |
| sv-tests、344 本 | kirimasim | 6.6 秒 | 0.1 秒 | 6.7 秒 |
| | Icarus Verilog | 5.0 秒 | 33.3 秒 | 38.3 秒 |
| | Verilator | 1,551.8 秒 | 59.7 秒 | 1,611.5 秒 |
| 自前のテストスイート、1,466 本 | kirimasim | 14.1 秒 | 0.3 秒 | 14.4 秒 |
| | Icarus Verilog | 20.8 秒 | 8.9 秒 | 29.7 秒 |
| | Verilator | 3,796.9 秒 | 291.7 秒 | 4,088.6 秒 |

同じ測定のうち、3 つのシミュレータがどれも通したテストだけ：sv-tests の 150 本（うち 38 本は拒否されるべきテストで、実行の段階がありません）と、自前のテストスイートの 1,133 本。

| テストの集まり | シミュレータ | 準備 | 実行 | 合計 |
| --- | --- | --- | --- | --- |
| sv-tests、150 本 | kirimasim | 1.4 秒 | 0.1 秒 | 1.5 秒 |
| | Icarus Verilog | 2.3 秒 | 20.9 秒 | 23.2 秒 |
| | Verilator | 127.0 秒 | 19.3 秒 | 146.3 秒 |
| 自前のテストスイート、1,133 本 | kirimasim | 10.8 秒 | 0.2 秒 | 11.0 秒 |
| | Icarus Verilog | 16.1 秒 | 7.9 秒 | 24.0 秒 |
| | Verilator | 3,006.7 秒 | 229.4 秒 | 3,236.1 秒 |

- 準備は、シミュレーションを始める前の作業です。kirimasim は設計を読み、エラボレーションし、SIR に変換するまで（`--dump-sir` で止めた実行の時間）、Icarus は `iverilog`、Verilator は C++ のビルドを含む `verilator --binary` です。実行はシミュレーションで、kirimasim は通しの時間から準備を引いたもの、Icarus は `vvp`、Verilator はビルドしたプログラムです。
- テストは短いので、実行の時間はほとんどが起動の時間です。kirimasim の実行の時間は引き算で求めていて、誤差と区別できないほど小さい値です。`--dump-sir` は SIR の文字も出すので、引き算の結果は小さめに出ます。
- この機械では、ビルドしたばかりのプログラムを初めて起動すると約 0.2 秒かかり、同じファイルの 2 回目の起動は 0.01 秒未満でした（Icarus が作ったファイル 3 つで試しました。macOS がその時間を何に使うのかは未確認）。Verilator の実行はどのテストでもこの初めての起動で、sv-tests の Icarus も、道具がコンパイルしたファイルを直接起動するので同じです。自前のテストスイートの道具は、`vvp` にファイルを渡して起動します。
- Verilator の準備の時間の大部分は、Verilator 自身の実行時ライブラリのコンパイルです。テストごとに別のディレクトリでビルドするので、毎回コンパイルし直します。小さなテスト 1 本を最適化ありで 1 ジョブずつビルドすると、C++ のビルド 2.6 秒のうち約 2.2 秒がこれでした。C++ を作るのは 0.09 秒、設計から作った C++ のコンパイルとリンクは 0.49 秒でした。コンパイル型のシミュレータは、長いシミュレーションでビルドの時間を取り返します。ここのテストは短いので取り返せず、シミュレーションの速さの比較にもなっていません。
- 最初の表では、ビルドできなかったテストは準備の時間だけを数えます。kirimasim は、未対応の構文に会うと早く止まります。
- Verilator は、それぞれのテストの道具と同じようにビルドします。sv-tests の道具は C++ を最適化しないで（`-O0`）、自前のテストスイートの道具は Verilator の既定の最適化と、初期値の乱数化でビルドします。
- 版と機械：kirimasim 8500bdc（release ビルド。sv-tests の本数だけは、program を未対応と報告するようにした 586bd15 で測り直した）、Icarus Verilog 14.0（devel）f190dbb、Verilator 5.053 devel 8668340。Apple M5 Max（18 コア）、メモリ 128 GB、macOS 27.0.1、Apple clang 21。

<details>
<summary>このリポジトリのテストの種類ごとの内訳</summary>

| 種類 | 本数 | 確かめていること |
| --- | --- | --- |
| ユニットテスト（GoogleTest） | 139 | SIR とその検証器（53）、参照モデルと突き合わせるスケジューラ（19）、`valsem`（14）、出力の書式（10）、強さ（9）、入力の読み取り（7）、時間の単位（7）、UDP の表（4）、カウンタ（4）、カーネルの速い演算と `valsem` の突き合わせ：1 語（5）と任意の幅（4）など |
| `tests/suite/` | 128 | 期待する出力を持つシミュレーションと構文のテスト。大半は `tools/` の生成器が作る。値（55）、ネット（23）、手続き（17）、スケジューリング（14）、階層（8）、連続代入（7）など |
| `tests/dump_sir/` | 27 | front が作る中間表現 |
| 道具 | 28 | 生成器がリポジトリのファイルを作り直せること、テストを判定する道具の陰性対照、ベンチマークの設計が動くこと |
| コマンドライン、事象の回数 | 2 | オプションとメッセージ、決まった実行の回数（PerfCount） |

</details>

<details>
<summary>sv-tests の、IEEE 1800-2023 の章ごとの内訳</summary>

sv-tests のコミット 15490e9 に固定し、種類にシミュレーションを含む手書きのテストを、sv-tests の決まりで判定します。`tools/svtests.py` で測りました。

| 章 | 本数 | kirimasim | Icarus | Verilator | kirimasim が止まる所 |
| --- | --- | --- | --- | --- | --- |
| 5 字句の決まり | 1 | 1 | 1 | 1 | |
| 6 データ型 | 16 | 16 | 11 | 14 | |
| 7 集合のデータ型 | 91 | 31 | 47 | 88 | 動的配列（23）、キュー（17）、連想配列（14）（どれも P3）、タグ付きの共用体 |
| 8 クラス | 15 | 9 | 12 | 12 | クラス（P3） |
| 9 プロセス | 6 | 4 | 4 | 6 | シーケンス（P3）。1 本は時刻 0 のプロセスの順に依存し、規格（4.7）はその順を決めていない |
| 10 代入文 | 3 | 3 | 3 | 3 | |
| 11 演算子と式 | 51 | 41 | 37 | 48 | 64 ビットを超える値（7、作業中）、タグ付きの共用体（2） |
| 12 手続きの文 | 2 | 2 | 2 | 2 | |
| 13 タスクと関数 | 14 | 14 | 13 | 14 | |
| 14 クロッキングブロック | 1 | 1 | 1 | 1 | |
| 15 プロセス間の同期と通信 | 2 | 0 | 0 | 2 | mailbox（P3） |
| 16 アサーション | 6 | 0 | 3 | 5 | 並行アサーション（P3） |
| 18 制約付き乱数 | 22 | 8 | 8 | 22 | `randsequence`（12）、`randcase`（2） |
| 20 ユーティリティのシステムタスクと関数 | 38 | 29 | 34 | 37 | `$random` と `$dist_` の関数、`$coverage_control` |
| 21 入出力のシステムタスクと関数 | 27 | 9 | 25 | 26 | ファイルの入出力（15）、`$fstrobe`、`$dumpfile`、`$dumpports` |
| 22 コンパイラ指示子 | 6 | 6 | 6 | 6 | |
| 23 モジュールと階層 | 1 | 1 | 1 | 1 | |
| 24 program | 1 | 0 | 1 | 1 | program（P3） |
| 26 パッケージ | 1 | 1 | 1 | 1 | |
| UVM | 37 | 0 | 0 | 28 | UVM のライブラリにはクラスが要る（P3） |
| UVM の乱数 | 3 | 3 | 3 | 3 | |
| **合計** | **344** | **179** | **213** | **321** | |

P1 の合格条件は、sv-tests が固定している Icarus の版（04cc493。210 本）と比べます。Icarus の最新版は 213 本です。

</details>

<details>
<summary>自前のテストスイートの、IEEE 1800-2023 の章ごとの内訳</summary>

TEST_SUITE.md の P1 の母集団です。テストスイートの 1466 本で、一覧は `tests/gates/p1_population.txt` にあります。テストスイート自体は、まだ公開していません。最後の列は kirimasim が止まる所です。kirimasim が失敗するテストはありません。

| 章 | 本数 | kirimasim | Icarus | Verilator | kirimasim が止まる所 |
| --- | --- | --- | --- | --- | --- |
| 3 設計と検証の構成要素 | 3 | 3 | 1 | 3 | |
| 4 スケジューリングの意味 | 13 | 13 | 13 | 9 | |
| 5 字句の決まり | 20 | 20 | 20 | 16 | |
| 6 データ型 | 226 | 223 | 187 | 200 | 64 ビットを超える値（3） |
| 7 集合のデータ型 | 90 | 87 | 55 | 90 | 動的配列、連想配列、`sort`（3、P3） |
| 9 プロセス | 72 | 72 | 71 | 65 | |
| 10 代入文 | 84 | 83 | 77 | 81 | 64 ビットを超える値（1） |
| 11 演算子と式 | 347 | 347 | 309 | 312 | |
| 12 手続きの文 | 106 | 106 | 96 | 102 | |
| 13 タスクと関数 | 76 | 76 | 50 | 76 | |
| 15 プロセス間の同期と通信 | 3 | 3 | 1 | 3 | |
| 16 アサーション | 21 | 18 | 16 | 19 | `$sampled`、`$stable`（3、P3） |
| 18 制約付き乱数 | 1 | 0 | 0 | 1 | `randsequence`（1） |
| 20 ユーティリティのシステムタスクと関数 | 149 | 147 | 144 | 117 | 64 ビットを超える値（2） |
| 21 入出力のシステムタスクと関数 | 57 | 56 | 56 | 52 | 64 ビットを超える値（1） |
| 22 コンパイラ指示子 | 14 | 14 | 14 | 14 | |
| 23 モジュールと階層 | 45 | 45 | 43 | 42 | |
| 25 インターフェース | 29 | 26 | 18 | 29 | virtual interface（3、P3） |
| 26 パッケージ | 20 | 20 | 16 | 20 | |
| 27 generate | 62 | 62 | 62 | 53 | |
| 28 ゲートとスイッチのレベルのモデル | 22 | 17 | 22 | 20 | MOS と双方向のスイッチ（5、P4） |
| 29 利用者定義のプリミティブ | 6 | 6 | 3 | 5 | |
| **合計** | **1,466** | **1,444** | **1,274** | **1,329** | |

</details>

### シミュレーションの速さ

2026-10-17 に、[bench/](bench/) の小さな設計 4 つを `tools/bench.py` で測りました。PERFORMANCE.md 5.2 が P1 で求める種類の設計です。どの設計も、`+n=` で決めた回数だけ同じ仕事を繰り返し、最後に検査用の値を出します。3 つのシミュレータが出す値は同じでした。

| 設計 | 内容 | `+n=` | kirimasim | Icarus Verilog | Verilator |
| --- | --- | --- | --- | --- | --- |
| `comb_chain` | 32 ビットの算術をする `always_comb` の 8 段。入力は時刻ごとに変わる | 3,000,000 | 4.46 秒 | 7.97 秒 | 0.16 秒 |
| `nba_shift` | `always_ff` の 64 段のシフトレジスタと 16 個の累算器。すべてノンブロッキング | 1,000,000 | 10.48 秒 | 18.69 秒 | 0.16 秒 |
| `proc_wait` | `#` で待つ 64 個のプロセスと、名前付きイベントを待つ 64 個のプロセス | 1,000,000 | 3.43 秒 | 4.93 秒 | 1.76 秒 |
| `fork_join` | 4 つの枝の fork と join を繰り返す。`#`、`->`、`@` を使う | 4,000,000 | 2.77 秒 | 4.14 秒 | 0.87 秒 |

- 時間はシミュレーションだけのものです。回数を決めた実行 5 回の中央値から、`+n=1` の実行 5 回の中央値を引きました。プログラムの起動、設計の読み込み、kirimasim のフロントエンドは両方の実行に入るので消えます（`+n=1` の実行は 3〜8 ミリ秒）。測る前にどのプログラムも 1 回起動したので、ビルドしたばかりのプログラムの初めての起動の時間も入りません。ビルドの時間も入りません（iverilog は設計ごとに 0.01 秒、Verilator は並列のビルドで 1.4 秒）。
- kirimasim と Icarus Verilog は設計を 4 値のままインタプリタで実行し、Verilator は C++ にコンパイルし、2 値で動きます。kirimasim で Verilator に当たるのは P2 の第 2 層（C++ の生成）で、kirimasim は速さのための作業をまだしていません。Verilator との差は、多くのプロセスが待つ設計（`proc_wait`、`fork_join`）でいちばん小さくなりました。
- メモリ：1 回の実行で使った量は、kirimasim が 10 MiB、Icarus が 6 MiB、Verilator が 2 MiB 以下でした。例外は kirimasim の `fork_join` で 4,947 MiB です。終わった fork の枝のフレームを、まだ解放していないためです（既知の不具合。下の表）。
- 版と機械は、上のテストの結果と同じです。`tools/bench.py` で測り直せます。`--n` と `--runs` で短くできます。

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
| program ブロック。program のコードは reactive の領域の組で動くもの（24.3.1）なので、今は未対応として報告する | P3 |
| VPI、DPI-C | P3 |
| 終わった fork の枝のフレームの解放。既知の不具合：今は実行の終わりまで残るので、fork のたびにメモリが増える（`bench/fork_join.sv` で 1 回あたり約 1.3 KB） | 未定 |
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
- **機構の行列。** 1 つの機構を、それが現れうる所の組み合わせで確かめます（たとえば `disable` なら、ブロックの中、fork の枝、別のプロセス、タスクから、それぞれ手続き、タスク、fork の枝の中で）。テストは `tools/` の生成器が作り、期待値は書いた規則から決まります。生成器は 26 個、`tests/suite/` のテストは 128 本、CTest のテストは全部で 324 本です。
- **陰性対照。** `tools/mutants.py` は、知られた誤り 130 個を 1 つずつコードに入れます。どの誤りでも、どれかのテストが落ちなければなりません。
- **他のシミュレータは証拠であって正解ではない。** Icarus Verilog と Verilator でもテストを動かし、規格と違うところはテストのヘッダに書きます。
- **サニタイザと静的な検査：** AddressSanitizer、UndefinedBehaviorSanitizer、clang-format、clang-tidy を push の前に毎回走らせます。

## 貢献：今は仕様の提案と Issue だけを受け付けています

kirimasim は開発の初期にあるため、今は**仕様の提案と Issue だけ**を受け付けています。コードの PR は受け付けていません（PR を作れるのは協力者だけという設定にしています）。`docs/spec/` の仕様書への提案、不具合の報告、期待値の誤りの指摘には Issue のフォームを、質問（Q&A）とまだ形になっていない案（Ideas）には [Discussions](https://github.com/kurochan001/kirimasim/discussions) を使ってください。受け付けを広げるときは、この節と [CONTRIBUTING.md](CONTRIBUTING.md) でお知らせします。

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
