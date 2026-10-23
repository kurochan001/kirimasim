# CLI：コマンドライン

[English](CLI.md) · 英語版が正本で、これはその日本語版です。食い違ったときは英語版に従います。

優先度：P1。状態：**承認済み（2026-10-15）**。2026-10-16 に、`--max-time` の書き方と打ち切り方（2.5 節）と、終了の理由の行の時刻の書き方（4 節）を足した（承認済み）。同日、時刻が進まない実行の上限の既定値（2.6 節）と、slang の言語の版（2.1 節）を足した（承認済み）。2026-10-18 に、コード 2 に設計が起こす実行時の誤りを含めた（5 節。承認済み）。同日、4 節の版の行の例を、シードの無い今の形にした（承認済み）。2026-10-19 に、2.6 節の上限に `--max-slot-events` を足した（承認済み）。同日、資源の上限（2.2 節、2.7 節）と終了コード 6（5 節）を足した（承認済み）。同日、壁時計の上限を 2.2 節、2.7 節、5 節に足した（承認済み）。同日、`--stop` を 2.2 節と 5 節に足した（承認済み）。同日、P2 以降のオプションに `--race-nba` を足した（2.3 節。承認済み）。同日、`--init-values`、`--cond-x`、`--bounds-check` を 2.2 節に、`--init-values` が書く行を 4 節に足した（承認済み）。同日、`--autoflush` を 2.2 節に足した（承認済み）。同日、`--mintypmax`、`--report-pulses`、`--wave-glitches` を 2.2 節に足した（承認済み）。同日、P2 以降のオプションに `--randomize-fail` と `--assert-incomplete` を足した（2.3 節。承認済み）。同日、上限の報告の最後のプロセスと直前の事象（2.6 節）と、診断の名前の付け方（6 節）を足した（承認済み）。同日、6 節から UNSPECIFIED_BEHAVIOR.md の 7.3 節の名前の一覧を指した（承認済み）。同日、slang のエラボレーションの上限を 2.1 節に足した（承認済み）。同日、ソースの中の config から取るトップを 2.1 節に足した（承認済み）。2026-10-21 に、`--seed` を SYSTEM_TASKS.md の 13 節で決め、版の行にシードを足した（2.2 節、4 節。承認済み）。同日、スタックが尽きる前に止まることを 2.7 節に足した（承認済み）。設計仕様書の「外部インターフェース」の案を、実装する段階ごとに整理し、決まっていなかった点（出力の行き先、`--dump-sir`、プラスアーグ）を足した。

この文書は、kirimasim のコマンドラインの形、オプション、終了コード、kirimasim 自身が出すメッセージの形を定める。終了の理由とコードの意味は SCHEDULER_SPEC.md の 16 節、システムタスクの出力は SYSTEM_TASKS.md に従う。

## 1. コマンドの形

```text
kirimasim [オプション] <ファイル>... [+プラスアーグ]...     コンパイルして実行する（既定）
kirimasim parse [オプション] <ファイル>...                  構文の解析とエラボレーションだけを行う
kirimasim --version | --help
```

- サブコマンドを書かないときは、コンパイルして実行する。
- `parse` は、P0 からある形のまま残す（テストスイートの P0 の確認に使う）。
- コンパイル済みのモデルを使い回す `build` と `run <モデル>` は P2、DPI ライブラリを作る `lib-create` は P4 で足す（設計仕様書の「外部インターフェース」）。

## 2. オプション

「段」は、P1 の中で実装する順（M3a、M4、M7 など）。その段より前には受け付けない（知らないオプションとして誤りにする）。

### 2.1 入力（slang にそのまま渡す）

- **言語の版（2026-10-16 に承認）：** kirimasim は、slang を常に IEEE 1800-2023 の版（slang の `--std=1800-2023`）で動かす。slang の既定は 1800-2017 で、その版では、大きさの指定の無い 32 ビットを超える 10 進のリテラルを 32 ビットに切り詰めるなど、規格の 2023 年版と違う扱いがある（5.7.1。slang v12.0 で確かめた）。版を選ぶオプションは置かない（CLAUDE.md の「隠れたつまみを作らない」）。

| オプション | 意味 | 段 |
| --- | --- | --- |
| `-f <file>` | ファイルの一覧を読む | M3a |
| `+incdir+<dir>`、`+define+<名前>[=<値>]` | インクルードの場所と、マクロの定義 | M3a |
| `--top=<名前>` | トップのモジュール、または config（モジュールと同じ名前の config は `<名前>:config` で指す）。複数書ける。書かないとき：ソースに config が 1 つあれば、その config（design 文がトップのセルを名指す。33.5.4。2026-10-19 に追加、承認済み）。2 つ以上あれば、2 つ目の位置のエラーで `--top` を求める。どちらでもなければ slang が選ぶ（どこからも参照されないモジュール）。ライブラリとして読むファイルの config は数えない | M3a |
| `-y <dir>`、`+libext+<ext>`、`--libdir=<dir>` | ライブラリの検索 | M7 |
| `-W<名前>`、`--compat=<名前>` | slang の警告と互換モード | M7 |
| `--max-hierarchy-depth=<n>`、`--max-generate-steps=<n>`、`--max-constexpr-depth=<n>`、`--max-constexpr-steps=<n>` | slang のエラボレーションの上限：インスタンスの階層の深さ、generate ブロックの手順の数、定数の評価の深さと手順の数。既定は slang の既定（UNSPECIFIED_BEHAVIOR.md の 6.1 節の A11、A12。2026-10-19 に承認） | M7 |
| `--timescale=<単位>/<精度>` | timescale の指定が無いファイルの既定（SCHEDULER_SPEC.md の 1 節） | M4 |

### 2.2 実行

| オプション | 意味 | 段 |
| --- | --- | --- |
| `--max-time=<時刻>` | この時刻に達したら打ち切る（理由 time_limit、コード 4）。時刻は単位付き（例 `10us`）。書き方と打ち切り方は 2.5 | M4 |
| `--max-time-ok` | time_limit をコード 0 として扱う | M4 |
| `--max-scc-iter=<n>`、`--max-zero-delay=<n>`、`--max-slot-events=<n>`、`--max-steps=<n>` | 時刻が進まない実行の上限（理由 zero_time_limit、コード 5）。意味と既定の値は 2.6 | M4 |
| `--max-procs=<n>`、`--max-call-depth=<n>` | 資源の上限（理由 resource_limit、コード 6）。意味と既定の値は 2.7（2026-10-19 に承認） | M7 |
| `--max-wall-time=<秒>` | 壁時計の上限（理由 wall_time_limit、コード 4）。2.7（2026-10-19 に承認） | M7 |
| `--stop=finish\|ignore\|error` | `$stop` で何をするか：終わる（既定）、続ける、コード 1 で終わる（SCHEDULER_SPEC.md の 16.2 節。2026-10-19 に承認） | M7 |
| `--init-values=x\|zero\|one\|random` | 初期値の無い 4 値の変数とメモリの初期値：x（既定。規格のとおり）、0、1、`--seed` から決める乱数のビット（VALUE_SEMANTICS.md の 1.4 節。2026-10-19 に承認） | M7。`random` は `--seed` の後 |
| `--cond-x=off\|warn\|error` | `if` と `?:` の条件、`case` の式が x か z のビットを含んだことを知らせる。`warn` は箇所ごとに 1 回出し、終わりに回数を出す。`error` は最初の 1 回で、設計が起こす実行時の誤りとして終わる（VALUE_SEMANTICS.md の 3.9 節、4.1 節、4.2 節。2026-10-19 に承認）。既定は `off` | M7 |
| `--bounds-check=off\|warn` | 実行時の範囲の外の添字と位置を、箇所ごとに 1 回知らせる（VALUE_SEMANTICS.md の 3.11 節。2026-10-19 に承認）。既定は `off` | M7 |
| `--autoflush` | 表示のタスクのたびに標準出力を書き出す（SYSTEM_TASKS.md の 2 節。2026-10-19 に承認）。既定は無効 | M7 |
| `--seed=<n>` | `$urandom`、`$urandom_range`、`randcase` の既定の種：2^64 未満の整数で、既定は 0（SYSTEM_TASKS.md の 13 節。2026-10-21 に追加、承認済み） | M7 |
| `--wave=<file>.fst\|.vcd`、`--wave-scope=<パターン>` | 波形の出力と範囲（WAVEFORM.md の 5 節。`.fst` は libfstapi の決定を待つ） | M7 |
| `--wave-glitches` | 時刻スロットの終わりの値だけでなく、その中のすべての変化を波形に記録する（2026-10-19 に承認） | M7。`--wave` と同じ |
| `--mintypmax=min\|typ\|max` | `min:typ:max` の遅延のどれを使うか。既定は `typ`（SCHEDULER_SPEC.md の 12 節。2026-10-19 に承認） | M7 |
| `--report-pulses` | 慣性の遅延が消したパルスを情報として知らせる（SCHEDULER_SPEC.md の 12 節。2026-10-19 に承認）。既定は無効 | M7 |

実装状況（2026-10-23）：第 1 層は、packed 値の選択と固定長の unpacked 配列の
アクセスに `--bounds-check=off|warn` を使える。automatic/ref の格納先と NBA も
含む。最後の集計では不正な読み書きの回数とソース位置の数を出す。
[VALUE_SEMANTICS.md の 3.11 節](VALUE_SEMANTICS.ja.md)と
[使い方](../P1_FEATURES.ja.md)を参照。

`--autoflush`・`--stop=finish|ignore|error`・`--max-wall-time` も第 1 層で実装済み。
標準出力のフラッシュは Context の Output、停止方針と実時間の検査は Scheduler が
持つ。実時間は、静的初期化を含む SIGINT の検査点で調べ、final は中断しない。
[使用例](../P1_FEATURES.ja.md)と、CLI の
[実行中の出力](../../tests/cli/run_autoflush_tests.py)・
[停止方針](../../tests/cli/run_stop_tests.py)・
[実時間の上限](../../tests/cli/run_wall_time_tests.py)のテストを参照。

### 2.3 出力と開発用

| オプション | 意味 | 段 |
| --- | --- | --- |
| `--dump-sir` | SIR を作り、検証器を通したあと、テキスト形式（SIR_SPEC.md の 9 節）で標準出力に書いて終わる。実行はしない。検証器が誤りを見つけたら、標準エラーに書いてコード 3 で終わる | M3a |
| `--stats=json` | 回数カウンタを、終わったときに JSON で標準出力の最後に書く（PERFORMANCE.md）。テストスイートの標準出力の比較では使わない | M3a |
| `--log=<file>` | 標準出力の写しをファイルにも書く | M7 |
| `--diag-format=json` | エラーと警告を、ID と規格の節番号付きの JSON で標準エラーに書く | M7 |

P2 以降のもの（`--tier`、`--tier-check`、`--state`、`--report-state-opt`、`--race-check`、`--race-seed`、`--race-nba`（2026-10-19 に承認。SCHEDULER_SPEC.md の 11 節）、`--public`、`--preserve`、`--disable-pass`、`--cxx`、`--cache-dir`。それに 2026-10-19 に承認した、失敗した `randomize()` の扱いを選ぶ `--randomize-fail=warn|error` と、終わりに未完了のアサーションの試行の扱いを選ぶ `--assert-incomplete=ignore|warn|fail`。どちらも P3）は、その段で本書に足す。

### 2.4 プラスアーグ

- `+` で始まる引数のうち、`+incdir+`、`+define+`、`+libext+` でないものは、実行時のプラスアーグとする。書かれた順のまま、`$test$plusargs` と `$value$plusargs` に渡す（SYSTEM_TASKS.md の 9 節）。
- プラスアーグはファイルの名前として扱わない。

### 2.5 `--max-time` の書き方と打ち切り方（2026-10-16 に承認）

- 書き方：0 以上の 10 進の数（小数を含んでよい）と、単位 `s`、`ms`、`us`、`ns`、`ps`、`fs` のどれか。間に空白を入れない（例：`10us`、`1.5ns`）。それ以外はコード 2 の誤りにする。
- 値は、設計全体の精度（SCHEDULER_SPEC.md の 1 節）の刻みに換算する。刻みに割り切れなければ、遅延と同じく、0 から遠い方へ丸める（SCHEDULER_SPEC.md の 1 節。64 ビットに収まらなければコード 2）。
- 打ち切り方：その時刻**以下**の事象はすべて実行する。その時刻より後の事象が次に来るとき、時刻を `--max-time` の値にして、理由 time_limit で終わる。`final` は実行する（SCHEDULER_SPEC.md の 16.2 節）。

### 2.6 時刻が進まない実行の上限（2026-10-16 に承認）

| オプション | 数えるもの | 既定の値 |
| --- | --- | --- |
| `--max-zero-delay=<n>` | 1 つの時刻スロットの中で、事象を Active か Reactive の領域へ移した回数（SCHEDULER_SPEC.md の 3 節の「移す」の回数。`#0` の再開、NBA の反映の後の評価などで増える） | 1000000 |
| `--max-slot-events=<n>` | 1 つの時刻スロットの中で、どの領域かを問わず実行した事象の数（`--stats=json` が `sched.events.*` として数える事象）。再開、反映、評価のたびに増え、1 つの領域の中でプロセスが起こし合うときにも増える。これは `--max-zero-delay` では数えない（2026-10-19 に追加。承認済み） | 100000000 |
| `--max-steps=<n>` | 1 つのプロセスが、待たずに続けて実行した第 1 層の命令の数（`forever begin end` のように、待ちの無いループで増える） | 1000000000 |
| `--max-scc-iter=<n>` | 第 2 層の静的な島の繰り返し（P2） | P2 で決める |

- `0` は「上限なし」を表す。
- 既定の値の根拠（2026-10-16 に測った）：
  - release の構成の第 1 層は、待ちの無いループで毎秒およそ 3.5 億命令を実行した。dev の構成もほぼ同じで、asan の構成ではおよそ半分の速さだった。`--max-steps` の既定で止まるまでの時間は、release でおよそ 3 秒、asan でおよそ 6 秒になる。
  - `#0` の再開は、100 万回でおよそ 0.04 秒だった。
  - `--max-steps` を大きめにしたのは、`initial` の中で大きなメモリを 1 つのループで初期化する設計（1600 万語なら、およそ 1 億命令）を誤って止めないためである。
  - `--max-slot-events`（2026-10-19 に測った）：起こし合う 2 つのプロセスが既定の値に達するまでの時間は、release でおよそ 7 秒、dev で 8 秒、ubsan で 14 秒、asan で 32 秒だった。既定の値は、事象が 100 万ある時刻スロット（1 つのクロックの辺で 100 万のプロセスが起きる設計など）に対して 100 倍の余裕を取る。
- この既定の値は、M6 で sv-tests と実設計を走らせたときに見直す。変えるときは本書を直し、人の承認を受ける。
- 上限に達したら、終了の理由の行（4 節）の前に、どの上限に達したかを 1 行で標準エラーに書く。形：
  - `kirimasim: error: time did not advance: <n> moves into the Active region in one time slot (--max-zero-delay=<n>)`（reactive の組なら `Reactive`）
  - `kirimasim: error: time did not advance: <n> events ran in one time slot (--max-slot-events=<n>)`
  - `kirimasim: error: time did not advance: process "<プロセスの名前>" ran <n> instructions without waiting (--max-steps=<n>)`
  - プロセスの名前は SIR のプロセスの名前（`top.initial0` など）。
- **最後のプロセスと直前の事象（2026-10-19 に承認）：** 形を変えない 1 行目の後に、その時刻スロットに最後に事象を積んだプロセスと、その時刻スロットの直前の事象を新しい順に最大 32 件、それぞれの領域、プロセス、ソースの位置、何をするかとともに出す。例：

  ```text
  kirimasim: error: time did not advance: 1000000 moves into the Active region in one time slot (--max-zero-delay=1000000)
    last process: top.u_ctl.always0 (always at ctl.sv:12)
    last events (newest first):
      Inactive  top.u_ctl.always0  ctl.sv:15  resume after #0
      Active    top.u_ctl.always0  ctl.sv:14  blocking assignment
  ```

  足した行は字下げし、`: error: ` を含まないので、`:expected_stderr:`（TEST_SUITE.md）は比べない。

### 2.7 資源と壁時計の上限（2026-10-19 に承認）

| オプション | 数えるもの | 既定の値 |
| --- | --- | --- |
| `--max-procs=<n>` | 実行時に作られ（`fork` の子など）、まだ終わっていないプロセス。エラボレーションが作る静的なプロセス（連続代入、ゲート、`always`、`initial` など）は数えない。その数は実行の前に決まり、大きなゲートレベルの設計が既定の値で止まってはならないため | 1000000 |
| `--max-call-depth=<n>` | 1 つのプロセスが同時に入れ子にしている呼び出し。待つ task の呼び出しとそれ以外を合わせて数える（SCHEDULER_SPEC.md の 16.2 節） | 10000 |
| `--max-wall-time=<秒>` | シミュレーションを始めてからの壁時計の秒数（コンパイルは数えない）。整数。理由は wall_time_limit で、コードは time_limit と同じ 4（どちらも実行を打ち切るため） | 無し |

- `--max-procs=0` と `--max-wall-time=0` は、2.6 と同じく「上限なし」を表す。`--max-call-depth=0` は受け付けない（コード 2）。シミュレーションのスレッドのスタックの大きさをこれで決めるため（RUNTIME_ABI.md の 5 節）。
- `--max-procs` はプロセスを作る時点で、`--max-call-depth` は呼び出す時点で、`--max-wall-time` は SIGINT の確認点で確かめる（SCHEDULER_SPEC.md の 16.2 節）。`--max-time-ok` は wall_time_limit には効かない。
- 既定の値の根拠：`--max-call-depth` は、今のスタックで測った深さのまま（RUNTIME_ABI.md の 5 節）。`--max-procs` はまだ測っていない。2.6 のとおり、実設計を走らせたときに見直す。`--max-call-depth` がいくつでも、実行はスレッドのスタックが尽きる前にも止まる（RUNTIME_ABI.md の 5 節。2026-10-21 に追加、承認済み）。
- 段：M7。P1 の計画はこれらのオプションの段を決めておらず、残っている段は M7 だけである。
- 上限に達したら、終了の理由の行（4 節）の前に、どの上限に達したかを 1 行で標準エラーに書く。形：
  - `kirimasim: error: resource limit reached: <n> live processes made at run time (--max-procs=<n>)`
  - `kirimasim: error: wall-clock limit reached: <n> seconds (--max-wall-time=<n>)`
  - 呼び出しの深さは、設計が起こす実行時の誤りの形のまま、文の位置とインスタンスの階層名を含める（SCHEDULER_SPEC.md の 16.2 節）：`<file>:<line>: error: calls nested more than <n> deep (--max-call-depth=<n>) [<階層名> @ <時刻>]`。

## 3. 出力の行き先

- **標準出力：** 設計が出すもの（`$display`、`$write`、`$strobe`、`$monitor` など）だけを書く。テストスイートは標準出力だけを比べる（TEST_SUITE.md）。例外は `--dump-sir` の SIR と、`--stats=json` の JSON（最後に書く）。
- **標準エラー：** kirimasim 自身のメッセージ（診断、版の行、終了の理由の行など）をすべて書く（SYSTEM_TASKS.md の 2 節）。標準エラーに書く前に、標準出力を流す。

## 4. kirimasim 自身のメッセージ

- 形は `kirimasim: <種類>: <内容>`。種類は `error`、`warning`、`note`、`info` のどれか。ソースの位置があるものは、slang の診断と同じ `<ファイル>:<行>:<列>: <種類>: <内容>` の形にする。
- **版の行：** 実行の冒頭に、版、コミット、オプション、シードを 1 行で標準エラーに書く（あとから同じ条件で再現するため）。例：`kirimasim: info: kirimasim 0.1.0 (commit 0123456789ab) seed=0 args: --top=tb a.sv`。`args:` の前に `seed=<n>` を置き、`--seed` が無ければ 0 とする（2026-10-18 に直した、承認済み。オプションがまだ無いのに、例にシードが入っていた。シードは 2026-10-21 に `--seed` とともに足した、承認済み）。標準出力には書かないので、テストスイートで取り除く必要は無い。
- **規格どおりでない初期値（2026-10-19 に承認）：** `--init-values` が `x` でないときは、版の行の次にもう 1 行書く：`kirimasim: note: --init-values=<値>: 4-state variables with no initial value start as <値>, not x (VALUE_SEMANTICS.md 1.4)`。目立つように、版の行に含めず別の行にする。版の行と同じく標準エラーに書き、テストスイートは比べない。
- **終了の理由の行：** 終わるときに、理由と時刻を 1 行で標準エラーに書く。例：`kirimasim: info: finish at 120 ns`。`$finish(0)` と `$stop(0)` のときは書かない（SCHEDULER_SPEC.md の 16.2 節）。
- **時刻の書き方（2026-10-16 に承認）：** 整数と単位を空白 1 つで区切って書く。単位は `s`、`ms`、`us`、`ns`、`ps`、`fs` のうち、その時刻を整数で正確に表せる最も大きいもの（例：精度 1ps で 120000 刻みなら `120 ns`、1500 刻みなら `1500 ps`）。時刻 0 は、設計全体の精度の単位で書く（精度 1ps なら `0 ps`、10ps なら `0 ps`）。丸めないので、表示から刻みの数を正確に戻せる。

## 5. 終了コード

SCHEDULER_SPEC.md の 16.2 節の表と同じ。理由の名前は `--stats=json` にも同じ名前で入れる。

| コード | 理由 | 意味 |
| --- | --- | --- |
| 0 | finish、quiescent、stop | `$finish`、事象が尽きた、`$stop`。`--max-time-ok` 付きの time_limit も 0 |
| 1 | fatal | `$fatal`。`$error` を数えるオプションが有効なら、`$error` が 1 件以上。`--stop=error` 付きの `$stop` も 1（理由は stop のまま。2026-10-19 に承認） |
| 2 | error | コマンドラインや入力の誤り（構文エラー、未対応の構文を含む）、または設計が起こす実行時の誤り（SCHEDULER_SPEC.md の 16.2 節） |
| 3 | internal | kirimasim の内部エラー（SIR の検証器の失敗など）。設計の誤りには使わない |
| 4 | time_limit、wall_time_limit | `--max-time` に達した。`--max-wall-time` が過ぎた（2.7。2026-10-19 に承認） |
| 5 | zero_time_limit | 時刻が進まないまま上限に達した |
| 6 | resource_limit | 資源の上限に達した（2.7。2026-10-19 に承認） |
| 130 | interrupted | SIGINT |

- **未対応の構文：** P1 の途中では、まだ実装していない構文が多い。SIR を作るときに未対応の構文に会ったら、その位置を示す診断を出してコード 2 で終わる（設計仕様書の終了コードの表の 2 のとおり）。黙って無視しない。
- **設計が起こす実行時の誤り（2026-10-18 に追記。承認済み）：** 設計が実行中に起こす誤りは、その文の位置を示す 1 行を標準エラーに出し、コード 2 で終わる（SCHEDULER_SPEC.md の 16.2 節）。コード 3 は kirimasim 自身の不具合だけに使う。

## 6. 未確認・後で書くこと

- `$error` を数えるオプションの名前と既定。
- `--diag-format=json` の項目と、診断の ID の体系（M7。RUNTIME_ABI.md の `kirima_status` と一緒に）。
- **名前の付け方（2026-10-19 に承認。M7 で確定）：** kirimasim 自身の診断には、slang の警告の名前と同じ形で、小文字をハイフンでつないだ `kirima-<語>` の名前を付ける（例：`kirima-zero-time-loop`）。こうすると `-W<名前>`、`-Wno-<名前>`、`-Werror=<名前>`（2.1）が slang の名前と同じように受け取り、接頭辞で slang の診断と分けられる。実行時の誤りと上限は消せない。上限は上げることだけができる。これまでに付けた名前と、それぞれの元の項目は、UNSPECIFIED_BEHAVIOR.md の 7.3 節に挙げる。
