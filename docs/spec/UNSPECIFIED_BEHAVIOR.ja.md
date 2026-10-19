# UNSPECIFIED_BEHAVIOR：規格が結果を決めていない所での kirimasim の扱い

[English](UNSPECIFIED_BEHAVIOR.md) · 英語版が正本で、これはその日本語版です。食い違ったときは英語版に従います。

優先度：P1。状態：**提案（承認待ち）**。2026-10-19 にこの文書を書いた（提案、承認待ち）。同日、9 節の 4 点で、人が承認済みの決まりを選んだ。J3 と A13 はそのまま、A2 は lint の警告を足し、P4 は引数が与えない範囲をメモリの範囲で補う。同日、A11 と A12 で渡す slang の上限のオプションを決めた（CLI.md 2.1。承認済み）。

この文書は、IEEE 1800-2023 が順序や結果を決めていない所、実装に任せている所、文言どおりに実装すると実用のコードが壊れる所を挙げ、それぞれで kirimasim がどうするかを書きます。規格の本文は写さず、節番号を示します。この文書が頼る決まりの多くは、ほかの詳細仕様書に書いてあります。この文書はそれを参照として示し、ほかに置き場の無いもの（基本の方針（1）、診断の種類（4）、項目ごとの状態（6）、7 と 8 の一覧）だけをここに書きます。

項目の ID（A1〜R5。追加の A14 と A15 を含む）は、2026 年 10 月に行った非公開の調査に合わせたものです。

**行の状態。** 表のどの行にも、次のどれかの値を書きます。

- **in force**：承認済みの詳細仕様書ですでに決まっている。節を示し、決まりはここで繰り返さない。
- **proposed 2026-10-19**：この文書で提案し、まだ承認されていない。人が承認したら `approved <日付>` に書き換える。実装するのは approved の行だけ。
- **later (P2)**、**later (P3)**、**later (P4)**：今ここに書き、細部を決めて実装するのはそのフェーズ。

## 1. 基本の方針

どの項目も、次の 8 つの方針に従います。

1. 規格が決めていることは規格どおりにする。4 値の既定、x の規則、領域の進め方を、速さのために変えない。
2. 規格が順序を決めていない所では、決定的な既定の順序を 1 つ選び、文書に書いて、テストで固定する。版、入力、オプション、シードが同じなら、層、OS、実行の回によらず同じ結果になる（NFR-2）。
3. 文言どおりにすると実用のコードが壊れる所（文の途中でのプロセスの切り替え）では、規格が許す範囲で実務の慣習に合う選択をとり、理由を書く。
4. 既定の順序に頼るコードを見つけるために、順序を乱す競合探索モードを持つ。既定の順序で通っても、競合が無いことの証明にはならない。
5. 他のシミュレータの結果をまねる互換モードは、当面作らない。実行の意味は 1 つにする。例外は `--state=2`（2 値の近似。P2）と `--init-values`（E9）で、どちらも既定ではない。
6. 高速化（第 2 層、2 値版、静的な順序付け、並列化）は観測できる結果を変えない。変えうるものは別のモードにし、`--tier-check` と差分テストで見張る。
7. 時刻の進まない実行と、資源を使いすぎる実行は、必ず上限で止め、どこで起きたかを示す。黙って回り続けることはしない。
8. 規格が決めていない動きに頼っていそうなコードは、エラボレーションの時点の検査（lint）と実行時の検出の 2 段で知らせる。

## 2. 時刻 0 の順序

時刻 0 の手順は SCHEDULER_SPEC.md 6.1 に書きます（proposed 2026-10-19）。項目 B8、B9、B10、D2、J3 はこれに頼ります。static の初期化子（手順の 1）は、スコープの中の宣言の順のままにします（9）。

## 3. 同じ領域の中の既定の順序

- 領域から事象を取り出す順と、プロセスの協調的な実行：SCHEDULER_SPEC.md 4.3（in force）。値の変化がそれを待つプロセスを起こす順：SCHEDULER_SPEC.md 4.3（proposed 2026-10-19）。
- NBA の更新の順と、遅延付きの NBA がその時刻になったときに入る位置：SCHEDULER_SPEC.md 13（前者は in force、後者は proposed 2026-10-19）。
- 静的な島の中の評価の順を第 1 層と第 2 層で共有すること：CODEGEN_SPEC.md の「静的な島」（Static islands）（later (P2)）。

項目 B1、B2、C1、C2、A8 はこれに頼ります。

## 4. 診断の種類

| 種類 | 時点 | 既定の扱い | 例 | 状態 |
| --- | --- | --- | --- | --- |
| lint の警告 | エラボレーション | 出して実行を続ける。`-W` で個別に消すかエラーにできる | 時間制御の無い経路、`casex` | proposed 2026-10-19 |
| エラボレーションのエラー | エラボレーション | 終了コード 2 | `uwire` の多重駆動（slang の診断） | in force（CLI.md 5） |
| 実行時の警告 | 実行時 | 出して実行を続ける。同じ箇所では 1 回だけ出し、終わりに回数を出す | 条件の x、範囲外の添字 | proposed 2026-10-19 |
| 実行時のエラー | 実行時 | 1 行を出し、`final` を実行して終わる（理由 error、コード 2） | Postponed 領域での書き込み、最大の時刻を超える時刻 | in force（SCHEDULER_SPEC.md 16.2） |
| 実行時の致命 | 実行時 | `final` を実行し、理由の終了コードで終わる | 時刻の進まない実行の上限、資源の上限 | zero_time_limit は in force（SCHEDULER_SPEC.md 16.2）。resource_limit と wall_time_limit は proposed 2026-10-19 |

- **名前：** この文書の診断には `kirima-<語>` の形の名前を付ける。小文字で、語はハイフンでつなぐ（例：`kirima-zero-time-loop`）。`-W` が slang の警告の名前と同じように受け取れ、slang の診断と取り違えないためである。名前の付け方と、`-W<名前>`、`-Wno-<名前>`、`-Werror=<名前>` の切り替えは CLI.md 6 に書く（proposed 2026-10-19）。名前は M7 で確定する。実行時のエラーと上限は消せない。上限は上げることだけができる。
- **中身：** ファイルと行、プロセスとブロックの階層名、実行時は SystemVerilog の呼び出しの列。`--diag-format=json`（CLI.md 2.3、M7）も同じものを出す。
- slang が同じ中身の診断を出すもの（B4、E8 など）は slang の診断を使い、二重に出さない。どれを slang が出すかは、M7 で固定した版の slang の診断と照らして確かめる（未確認）。

## 5. 競合探索モード

このモードが乱すものと乱さないものは SCHEDULER_SPEC.md 4.3 と 11 に書きます（proposed 2026-10-19。モード自体は P2）。異なるプロセスの NBA の順は、`--race-nba` のときだけ乱します（P2）。静的な検査として、同じ辺で一方のプロセスがブロッキング代入で書き、別のプロセスが読む変数を、lint の警告 `kirima-race-blocking` で知らせます（B3、B6。proposed 2026-10-19）。

## 6. 項目

列：ID、kirimasim の扱い、出す診断とオプション、規格で確かめる節、フェーズ、状態。in force の行には決まりのある節を書きます。部分ごとに状態の違う項目は、部分ごとに行を分けます。

### 6.1 A：時刻が進まない実行

できるものはエラボレーションで見つけ、実行時は上限で止め、どこで起きたかを報告します。

| ID | 扱い | 診断・オプション | 節 | フェーズ | 状態 |
| --- | --- | --- | --- | --- | --- |
| A1 | `#0` は Inactive（program の中は Re-Inactive）で再開する。1 つの時刻スロットで Active か Reactive の領域に移した回数を数え、上限で止める | 理由 zero_time_limit、コード 5。`--max-zero-delay` | 4.4.2.3、9.4.1 | P1 | in force（SCHEDULER_SPEC.md 8、16.2。CLI.md 2.6） |
| A1 | 上限の報告に、最後のプロセスと直前の事象を足す | CLI.md 2.6 | — | P1 | proposed 2026-10-19 |
| A1 | 時間制御が `#0` だけのループの lint | `kirima-zero-delay-loop` | 4.4.2.3 | P1 | proposed 2026-10-19 |
| A2 | `$finish`、`$fatal`、disable に届かないどの経路にも時間制御の無い `always` には lint の警告を出し、次の行のとおり実行する。時間制御の無い経路が一部だけのものにも lint の警告を出す | `kirima-always-no-wait`、`kirima-always-no-wait-path` | 9.2.2.1 | P1 | approved 2026-10-19（SCHEDULER_SPEC.md 6.2） |
| A2 | 実行時は A14 の上限で止まる | `--max-steps` | 9.2.2.1 | P1 | in force（SCHEDULER_SPEC.md 6.2、16.2） |
| A3 | 感度が空の `always @*` は一度も起動しない | — | 9.4.2.2 | P1 | in force（SCHEDULER_SPEC.md 6.2、9.2） |
| A3 | `always_comb` を勧める警告を出す | `kirima-empty-sensitivity` | 9.4.2.2 | P1 | proposed 2026-10-19 |
| A4 | 常に真、常に偽の `wait` は規格どおり。すぐ進む `wait` は、A14 の上限では待ちに数えない | `--max-steps` | 9.4.3 | P1 | in force（SCHEDULER_SPEC.md 9.3、16.2） |
| A4 | 定数の畳み込みの後に常に真になる、ループの中の `wait` の警告。`wait(0)` は情報だけ | `kirima-const-wait` | 9.4.3 | P1 | proposed 2026-10-19 |
| A5 | 実行時に作られ、生きているプロセスの数の上限 | 理由 resource_limit、コード 6。`--max-procs` | 9.3.2 | P1 | proposed 2026-10-19（SCHEDULER_SPEC.md 16.2。CLI.md 2.7） |
| A6 | 静的な島の強連結成分を、落ち着くまで上限つきで繰り返す | 理由 zero_time_limit、コード 5。`--max-scc-iter` | 4.5、4.7 | P2 | in force（SCHEDULER_SPEC.md 16.2。CLI.md 2.6） |
| A6 | 最後の 2 回で変わった信号を報告する。ビット単位で本当に循環するものの lint | `kirima-comb-not-converged`、`kirima-comb-loop` | 4.5、4.7 | P2 | later (P2) |
| A7 | 依存のグラフをビット範囲で作り、ベクタの別のビットどうしのループを切り離す。実行時に決まる添字は繰り返しで評価する | `kirima-comb-loop-unsplit`（`--report-state-opt` にも出す） | 9.4.2、11.5.3 | P2 | later (P2) |
| A8 | 島の中は各プロセスをレベル順に 1 回ずつ評価し、第 1 層も同じ順を使う | — | 4.7 | P2 | later (P2。CODEGEN_SPEC.md の「静的な島」（Static islands）) |
| A9 | A1 の数に、NBA 領域から Active への戻りを含める | `--max-zero-delay` | 4.5 | P1 | in force（CLI.md 2.6） |
| A9 | 自分が書く変数を待つ `always`（`always @(x) x <= ~x;`）の lint | `kirima-self-trigger` | 4.5 | P1 | proposed 2026-10-19 |
| A10 | ループ変数は規格どおり感度に入る。外すオプションは作らない | — | 9.4.2.2 | P1 | in force（SCHEDULER_SPEC.md 9.2） |
| A10 | モジュールの変数を 2 つ以上の `always @*` のループ変数に使っているときの警告 | `kirima-shared-loop-var` | 9.4.2.2 | P1 | proposed 2026-10-19 |
| A11 | 定数関数の終わらない再帰は slang に任せ、slang の定数の評価の上限をそのまま渡す | slang の `--max-constexpr-depth`、`--max-constexpr-steps` | 13.4.3 | P1 | approved 2026-10-19（CLI.md 2.1） |
| A12 | インスタンスの終わらない再帰と、終わらない generate ブロックは slang に任せ、slang の上限をそのまま渡す（A11 と同じ） | slang の `--max-hierarchy-depth`、`--max-generate-steps` | 27 | P1 | approved 2026-10-19（CLI.md 2.1） |
| A13 | Postponed 領域では何も書かない。禁止の書き込みは実行時のエラー | 実行時のエラー `kirima-readonly-write` | 4.4.2.9 | P1 | in force（SCHEDULER_SPEC.md 2.2、16.2） |
| A13 | `$monitor` や `$strobe` の引数から呼ばれた関数は、自分の変数（引数、automatic と static の変数）に書いてよい。ほかへの書き込みは実行時のエラー | — | 4.4.2.9、21.2.2、21.2.3 | P1 | in force（SCHEDULER_SPEC.md 2.2） |
| A13 | 読み出し専用のコールバックの中の `vpi_put_value` は失敗を返し、`vpi_chk_error` が理由を返す | — | 38 | P4 | later (P4) |
| A13 | `$monitor` や `$strobe` の引数にある副作用のある関数の lint | `kirima-monitor-side-effect` | 21.2.3 | P1 | proposed 2026-10-19 |
| A14 | 1 つのプロセスが待たずに実行した命令（第 1 層）を数え、上限で止める | 理由 zero_time_limit、コード 5。`--max-steps` | 9.4 | P1 | in force（SCHEDULER_SPEC.md 16.2。CLI.md 2.6） |
| A15 | 入れ子の呼び出しの深さに上限を置き、C++ のスタックがあふれる前に止める | 理由 resource_limit、コード 6。`--max-call-depth` | 13.4.2 | P1 | proposed 2026-10-19（SCHEDULER_SPEC.md 16.2。CLI.md 2.7。RUNTIME_ABI.md 5） |

### 6.2 B：競合と実行の順序

順序は 2 と 3 で決まります。それに頼るコードは lint と競合探索で知らせます。規格が許す範囲で他のシミュレータと結果が違うことはあり、それを合わせるモードは作りません。

| ID | 扱い | 診断・オプション | 節 | フェーズ | 状態 |
| --- | --- | --- | --- | --- | --- |
| B1 | 各領域は先入れ先出し | — | 4.7 | P1 | in force（SCHEDULER_SPEC.md 4.3） |
| B1 | 値の変化は、それを待つプロセスを、待ち始めた順に起こす | — | 4.7 | P1 | proposed 2026-10-19（SCHEDULER_SPEC.md 4.3） |
| B1 | 競合探索はこれらの順を乱す | `--race-check`、`--race-seed` | 4.7 | P2 | later (P2。SCHEDULER_SPEC.md 11) |
| B2 | 協調的な実行：プロセスは待つか終わるまで切り替えない。書き込みで起きた連続代入は、書いたプロセスの中では実行せず Active に積む | — | 4.7 | P1 | in force（SCHEDULER_SPEC.md 4.3、9.5） |
| B3 | ブロッキング代入とクロックの辺は既定の順序で実行する | — | 4.7、10.4.1 | P1 | in force（SCHEDULER_SPEC.md 4.3） |
| B3 | 同じ辺で、一方のプロセスがブロッキング代入で書き、別のプロセスが読む変数の lint | `kirima-race-blocking` | 4.7、10.4.1 | P1 | proposed 2026-10-19 |
| B4 | `always_comb`、`always_latch`、`always_ff` が書く変数に、ほかのプロセスは書けない（ビット範囲）。slang の診断を使う | slang のエラー | 9.2.2.2、9.2.2.4 | P1 | in force（SCHEDULER_SPEC.md 6.4） |
| B4 | 一般の `always` 2 つが同じビットに書くときの警告 | `kirima-multi-write` | 9.2.2.1 | P1 | proposed 2026-10-19 |
| B5 | 1 つの変数の同じビットへのブロッキング代入と NBA は書いたとおり実行し、警告を出す | `kirima-blk-nblk-mix` | 10.4 | P1 | proposed 2026-10-19 |
| B6 | テストベンチと設計が同じ時刻に駆動するときは既定の順序で実行する。`kirima-race-blocking` はモジュールの `initial` にも働く | `kirima-race-blocking` | 14、24 | P1 | proposed 2026-10-19 |
| B6 | `program` とクロッキングブロックは規格どおり | — | 14、24 | P3 | later (P3) |
| B7 | クロッキングブロックの skew の領域 | — | 14.4、14.13、14.16 | P3 | later (P3。SCHEDULER_SPEC.md の第 4 部) |
| B8 | 宣言の初期値は事象も辺も作らない | — | 6.8 | P1 | in force（SCHEDULER_SPEC.md 6.1） |
| B9 | `always_comb` と `always_latch` は時刻 0 の最後に起動する | — | 9.2.2.2.2 | P1 | in force（SCHEDULER_SPEC.md 6.1） |
| B10 | ネットの初期値：駆動の無いビットは z（またはネットの種類の値）、駆動のあるビットは最初に書くまで x | — | 6.6、10.3 | P1 | in force（VALUE_SEMANTICS.md 5.3、5.4） |
| B11 | `$finish` の後は同じ時刻の事象を実行しない。`final` は実行する | — | 20.2 | P1 | in force（SCHEDULER_SPEC.md 6.2、16.2） |
| B11 | 順序で変わる結果を競合探索が知らせる | `--race-check` | 20.2 | P2 | later (P2) |
| B12 | `always_comb` の中で書く前に読むと、前の値が見える（規格どおり） | — | 9.2.2.2.1 | P1 | in force（SCHEDULER_SPEC.md 6.3） |
| B12 | その警告 | `kirima-comb-read-before-write` | 9.2.2.2.1 | P1 | proposed 2026-10-19 |

B2 の協調的な実行は、第 2 層と将来の並列化の前提でもあります。文の途中で切り替わらないので、生成コードはプロセスを普通の C++ の関数として最適化できます。

### 6.3 C：NBA の細かい意味

| ID | 扱い | 診断・オプション | 節 | フェーズ | 状態 |
| --- | --- | --- | --- | --- | --- |
| C1 | NBA 領域は 1 本の先入れ先出しで、プロセスの中でもプロセスをまたいでも、発行の順に反映する | — | 4.6、10.4.2 | P1 | in force（SCHEDULER_SPEC.md 4.1、4.3、13） |
| C2 | 遅延付きの NBA は、その時刻になると、その時刻スロットで発行された NBA より前に、（満期の時刻、発行の番号）の順で入る | — | 4.5 | P1 | proposed 2026-10-19（SCHEDULER_SPEC.md 13） |
| C3 | 右辺は文を実行した時点で評価する。NBA の行き先もその時点で決める。代入内遅延付きのブロッキング代入の行き先は再開した時点で決める | — | 9.4.5、10.4 | P1 | in force（SCHEDULER_SPEC.md 5、8） |
| C4 | NBA の代入内遅延は取り消さない。連続代入とゲートの遅延は慣性 | — | 10.3.3、10.4.2 | P1 | in force（SCHEDULER_SPEC.md 12、13） |
| C5 | 慣性の遅延は遅延より短いパルスを消す。rise、fall、turn-off の遅延は規格どおり | — | 10.3.3、28.16 | P1 | in force（SCHEDULER_SPEC.md 12） |
| C5 | `min:typ:max` は既定で typ を使い、オプションで選ぶ。別のオプションで、消したパルスを情報として出す | `--mintypmax`、`--report-pulses` | 30.7 | P1 | proposed 2026-10-19（CLI.md 2.2） |
| C6 | `#0` は Inactive で再開する。`a <= #0 b` は同じ時刻の NBA。`a = #0 b` は右辺を評価し、Inactive で再開してから書く | — | 4.4.2.3、9.4.1、9.4.5 | P1 | in force（SCHEDULER_SPEC.md 5、8） |
| C6 | `#0` の書き方ごとの順序の表をテストで固定する | — | 4.4.2.3 | P1 | proposed 2026-10-19（8：region_order） |
| C7 | 組合せのブロックの中の NBA は NBA のまま | — | 9.2.2.2 | P1 | in force（書いたとおり。変える決まりは無い） |
| C7 | その警告 | `kirima-comb-nba` | 9.2.2.2 | P1 | proposed 2026-10-19 |
| C8 | `final` の後は領域を進めないので、`final` の中の NBA は反映しない | — | 9.2.3 | P1 | in force（SCHEDULER_SPEC.md 6.2） |
| C8 | その警告 | `kirima-final-nba` | 9.2.3 | P1 | proposed 2026-10-19 |

### 6.4 D：イベント制御と感度

| ID | 扱い | 診断・オプション | 節 | フェーズ | 状態 |
| --- | --- | --- | --- | --- | --- |
| D1 | 辺は規格の表どおりに、1 つの関数で判定する | — | 9.4.2 | P1 | in force（VALUE_SEMANTICS.md 4.3） |
| D2 | 4 値では、初期値の x から 0 や 1 への変化も辺。辺や事象の待ちで始まるプロセスは、時刻 0 に何かが走る前から待っているので、それを受け取る | — | 9.4.2 | P1 | proposed 2026-10-19（SCHEDULER_SPEC.md 6.1） |
| D2 | `--state=2` では x が無いのでこの辺は起きない。違いとして書く | — | 6.11 | P2 | later (P2) |
| D3 | 時刻の途中の変化は、そのとき待っているプロセスだけが見る。島の最適化でもこれを落とさない | — | 9.4.2、4.7 | P1 | in force（SCHEDULER_SPEC.md 9.1） |
| D3 | 波形は各時刻スロットの終わりの値を記録する。オプションですべての変化を記録する | `--wave-glitches` | 4.7 | P1 | proposed 2026-10-19（CLI.md 2.2） |
| D4 | `always_comb` の感度は、呼ぶ関数、パッケージの変数、階層参照をたどる | — | 9.2.2.2.1 | P1 | in force（SCHEDULER_SPEC.md 6.3） |
| D4 | 静的に決まらない読み出し（ハンドルや仮想インタフェースを通すもの）は感度に入れず、警告を出す | `kirima-comb-dynamic-read` | 9.2.2.2.1 | P3 | later (P3) |
| D5 | 最長の静的な接頭辞：変数の添字の `a[i]` は `a` 全体と `i` を購読し、定数の添字はそのビット範囲を購読する | — | 11.5.3 | P1 | in force（SCHEDULER_SPEC.md 6.3、9.5） |
| D6 | `always @*` と `always_comb` の違い | — | 9.2.2.2.2 | P1 | in force（SCHEDULER_SPEC.md 6.2、6.3、9.2） |
| D7 | `iff` は辺を見つけた時点で評価する。複合式は監視レコードを使う | — | 9.4.2.1、9.4.2.3 | P1 | in force（SCHEDULER_SPEC.md 9.1、9.5） |
| D8 | `triggered` は時刻スロットの終わりまで真 | — | 15.5.1、15.5.3 | P1 | in force（SCHEDULER_SPEC.md 9.4） |
| D8 | 同じ時刻スロットですでに起動したイベントを `@e` で待ち始めたときの実行時の警告（`wait (e.triggered)` を勧める）。その起動で起きたプロセスが待ち直すときは出さない | `kirima-event-missed` | 15.5.1 | P1 | proposed 2026-10-19 |
| D9 | `->>` は NBA 領域で起動する | — | 15.5.2 | P1 | in force（SCHEDULER_SPEC.md 9.4） |
| D9 | 同じ時刻に複数のイベントが起きたときの `wait_order` は、起動した順に従う | — | 15.5.4 | P3 | later (P3) |
| D10 | 規格が禁じる所で仮想インタフェースを感度に使うのはエラー（未確認） | `kirima-vif-in-sensitivity` | 25.9 | P3 | later (P3) |

`kirima-event-missed` は、`-> e` と `@e` が同じ時刻に起き、既定の順序で `-> e` が先だった典型的な競合に出します。正しいコードに出ることがあるかは、実設計で確かめます（未確認）。

### 6.5 E：4 値と x

| ID | 扱い | 診断・オプション | 節 | フェーズ | 状態 |
| --- | --- | --- | --- | --- | --- |
| E1 | `if` と `case` の不定の条件は真ではない | — | 12.4、12.5 | P1 | in force（VALUE_SEMANTICS.md 4.1、4.2） |
| E1 | x や z を含んだ条件と case の式を、箇所ごとに 1 回知らせ、終わりに回数を出すオプション | `--cond-x` | 12.4、12.5 | P1 | proposed 2026-10-19（CLI.md 2.2。SIR_SPEC.md の 5.2 節の `cond.x`） |
| E2 | 条件演算子は、条件が不定なら両辺をマージする | — | 11.4.11 | P1 | in force（VALUE_SEMANTICS.md 3.9） |
| E2 | `--cond-x` はその条件も対象にする | `--cond-x` | 11.4.11 | P1 | proposed 2026-10-19（VALUE_SEMANTICS.md 3.9） |
| E3 | 演算子ごとの x の規則は `valsem` が決め、網羅的にテストする | — | 11.4 | P1 | in force（VALUE_SEMANTICS.md 3） |
| E4 | x の発生源は規格どおり。2 値の型の範囲外の読み出しは 0 | — | 7.4.6、11.4.3 | P1 | in force（VALUE_SEMANTICS.md 1.2、3.11） |
| E5 | `--state=2` は近似で、期待値の基準にしない。`--tier-check` は同じ `--state` の実行どうしを比べる | `--state`、`--tier-check` | 6.11 | P2 | later (P2) |
| E6 | `casex` は規格どおり | — | 12.5.1 | P1 | in force（VALUE_SEMANTICS.md 4.2） |
| E6 | `casez` か `case ... inside` を勧める警告 | `kirima-casex` | 12.5.1 | P1 | proposed 2026-10-19 |
| E7 | 強度と解決。`trireg` は未対応 | — | 6.6、28 | P1 | in force（VALUE_SEMANTICS.md 5） |
| E8 | `uwire` の多重駆動はエラー（slang の診断） | — | 6.6.2 | P1 | in force（VALUE_SEMANTICS.md 5.5） |
| E9 | 初期値は既定で規格どおり。オプションで、初期値の無い 4 値の変数とメモリの初期値を選ぶ。既定でない値の実行は、冒頭でそう知らせる | `--init-values` | 6.8 | P1 | proposed 2026-10-19（CLI.md 2.2、4。VALUE_SEMANTICS.md 1.4） |

`--cond-x=warn` と `--init-values=one` を組み合わせると、x の楽観で隠れたリセットの漏れを見つけやすくなります。どちらも既定ではないので、既定の実行の結果と速さは変わりません。

### 6.6 F：式の幅と符号

| ID | 扱い | 診断・オプション | 節 | フェーズ | 状態 |
| --- | --- | --- | --- | --- | --- |
| F1 | 文脈で決まる幅は slang の型付けに従う | slang の警告 | 11.6、11.8 | P1 | in force（VALUE_SEMANTICS.md 2） |
| F1 | 第 2 層は 64 ビットを超える幅も正確に計算する | — | 11.6 | P2 | later (P2) |
| F2 | 符号の混在は slang の型付けに従う | slang の警告 | 11.7、11.8.1 | P1 | in force（VALUE_SEMANTICS.md 2） |
| F3 | シフト量は規格どおり。生成コードは幅以上のシフト量を条件で処理する | — | 11.4.10 | P1 | in force（VALUE_SEMANTICS.md 3.8）。生成コードは later (P2) |
| F4 | 0 での除算と剰余は x。生成コードは INT64_MIN を -1 で割る場合も条件で分ける | — | 11.4.3 | P1 | in force（VALUE_SEMANTICS.md 3.2）。生成コードは later (P2) |
| F5 | べき乗の特例 | — | 11.4.3 | P1 | in force（VALUE_SEMANTICS.md 3.2） |
| F6 | real から整数への変換は最も近い整数に丸め、0.5 は 0 から遠い方 | — | 6.12、6.24.1 | P1 | in force（VALUE_SEMANTICS.md 1.5） |
| F7 | 範囲外の読み出しは x（2 値の型は 0）、書き込みは何もしない。一部が範囲外の部分選択は、範囲内のビットだけ書く | — | 7.4.6、11.5.1 | P1 | in force（VALUE_SEMANTICS.md 3.11） |
| F7 | 範囲外の定数の添字の警告。実行時の範囲外の添字を箇所ごとに 1 回知らせるオプション | `kirima-const-out-of-range`、`--bounds-check` | 11.5.1 | P1 | proposed 2026-10-19（CLI.md 2.2。VALUE_SEMANTICS.md 3.11） |
| F8 | `+:`、`-:` と昇順の範囲は slang に従う | slang の警告 | 11.5.1 | P1 | in force（VALUE_SEMANTICS.md 3.11） |
| F9 | 繰り返しの回数 0 は slang に従う | slang の診断 | 11.4.12.1 | P1 | in force（VALUE_SEMANTICS.md 7.2） |

### 6.7 G：短絡評価と副作用

副作用のある式を評価するかどうか、何回、どの順に評価するかを、最適化で変えません。規格が順序を決めていない所は、左から右にします。

| ID | 扱い | 診断・オプション | 節 | フェーズ | 状態 |
| --- | --- | --- | --- | --- | --- |
| G1 | 左で結果が決まるときは右を評価しない | — | 11.3.5、11.4.7 | P1 | in force（VALUE_SEMANTICS.md 3.1。SIR_SPEC.md 5.2） |
| G1 | 論理演算をビット演算に変えるのは、右辺に副作用も null の参照も無いと証明できたときだけ | — | 11.3.5 | P2 | later (P2。CODEGEN_SPEC.md の「C++ 生成の規則」（Rules of C++ generation）) |
| G2 | 副作用のある式を評価する回数を、最適化で変えない | — | 11.3.5 | P1 | in force（VALUE_SEMANTICS.md 3.1） |
| G2 | 添字や部分選択の中の `++` や副作用のある関数の警告 | `kirima-side-effect-in-index` | 11.3.5 | P1 | proposed 2026-10-19 |
| G3 | 引数は左から右に評価する | — | 11.3.5 | P1 | proposed 2026-10-19（VALUE_SEMANTICS.md 3.1） |
| G3 | C++ は引数の評価の順を決めていないので、生成コードは一時変数で順を固定する | — | 11.3.5 | P2 | later (P2。CODEGEN_SPEC.md の「C++ 生成の規則」（Rules of C++ generation）) |
| G4 | オペランドは左から右に評価する | — | 11.4.2 | P1 | proposed 2026-10-19（VALUE_SEMANTICS.md 3.1） |
| G4 | 1 つの式が同じ変数に 2 回以上書くときの警告 | `kirima-multi-side-effect` | 11.4.2 | P1 | proposed 2026-10-19 |
| G4 | `=` のブロッキング代入は右辺を左辺より先に評価する。`+=` などの代入演算子は左辺を先に評価する | — | 10.4.1、11.4.1 | P1 | approved 2026-10-19（VALUE_SEMANTICS.md 3.1） |

### 6.8 H：タスクと関数の寿命

| ID | 扱い | 診断・オプション | 節 | フェーズ | 状態 |
| --- | --- | --- | --- | --- | --- |
| H1 | static と automatic は規格どおり（slang が判定する） | — | 6.21、13.3.2 | P1 | in force（SIR_SPEC.md 3.3） |
| H1 | 前の呼び出しが待っている間に static の task がもう一度呼ばれたとき、task ごとに 1 回の実行時の警告 | `kirima-static-task-reentry` | 13.3.2 | P1 | proposed 2026-10-19 |
| H2 | `fork` の中のループ変数は規格どおり | — | 9.3.2 | P1 | in force（SCHEDULER_SPEC.md 10.2） |
| H2 | `join_none` や `join_any` の本体が、親が変えるループ変数を automatic の写しを通さずに読むときの警告 | `kirima-fork-loop-var` | 9.3.2 | P1 | proposed 2026-10-19 |
| H3 | 子が参照する親のフレームは、子が終わるまで残す（フレームを GC の根にする） | — | 6.21、9.3.2 | P3 | later (P3。RUNTIME_ABI.md 8) |
| H4 | 待ちの間に、キューや動的配列の要素が消えたり移ったりした `ref` を通す読み書きは、実行時のエラー（未確認） | `kirima-ref-stale` | 13.5.2 | P3 | later (P3) |
| H5 | 関数の中の時間制御はエラー。互換のための例外は無い | slang の診断 | 13.4 | P1 | in force（slang） |
| H6 | disable で終わった task は output と inout の引数を書き戻さない | — | 9.6.2 | P1 | in force（SCHEDULER_SPEC.md 10.4） |

### 6.9 I：プロセス制御と終了

| ID | 扱い | 診断・オプション | 節 | フェーズ | 状態 |
| --- | --- | --- | --- | --- | --- |
| I1 | 名前で task を disable すると、その実行中のすべての呼び出しを終える | — | 9.6.2 | P1 | in force（SCHEDULER_SPEC.md 10.4） |
| I2 | kill したプロセスを購読、監視レコード、イベントの待ちから外す | — | 9.7 | P1 | in force（SCHEDULER_SPEC.md 10.7） |
| I2 | semaphore と mailbox の待ちからも外す | — | 15.3、15.4 | P3 | later (P3。SCHEDULER_SPEC.md 10.7) |
| I3 | `wait fork` は直接の子を待つ。`disable fork` はすべての子孫を終える | — | 9.6.1、9.6.3 | P1 | in force（SCHEDULER_SPEC.md 10.3、10.5） |
| I4 | `suspend` と `resume` | — | 9.7 | P3 | in force（SCHEDULER_SPEC.md 10.6）。実装は P3 |
| I5 | `final` は、2 回目の SIGINT と内部エラーを除くすべての終わり方で実行する | — | 9.2.3 | P1 | in force（SCHEDULER_SPEC.md 16.2） |
| I6 | 終了コードに、資源の上限の 6 を加える | 理由 resource_limit | 20.2 | P1 | proposed 2026-10-19（SCHEDULER_SPEC.md 16.2。CLI.md 5） |
| I6 | `$finish` の引数は出すものだけを変える。`$stop` は行を出してコード 0 で終わる | — | 20.2 | P1 | in force（SCHEDULER_SPEC.md 16.2。SYSTEM_TASKS.md 10.2） |
| I6 | `$stop` で何をするかをオプションで選ぶ | `--stop` | 20.2 | P1 | proposed 2026-10-19（SCHEDULER_SPEC.md 16.2。CLI.md 2.2） |
| I7 | 対話のモードは持たない | — | — | P1 | in force（SCHEDULER_SPEC.md 16.2） |
| I8 | シミュレーションの時刻の上限とは別に、壁時計の上限を持つ | 理由 wall_time_limit、コード 4。`--max-wall-time` | — | P1 | proposed 2026-10-19（SCHEDULER_SPEC.md 16.2。CLI.md 2.7） |

### 6.10 J：クラスとメモリ

| ID | 扱い | 診断・オプション | 節 | フェーズ | 状態 |
| --- | --- | --- | --- | --- | --- |
| J1 | 正確なマーク・スイープの GC | `--stats=json` に GC の回数と停止の時間 | 8.29 | P3 | later (P3。RUNTIME_ABI.md 6) |
| J2 | null の参照は実行時のエラー（理由 error、コード 2）で、SystemVerilog の呼び出しの列を出す | `kirima-null-deref` | 8.4 | P3 | 理由とコードは in force（SCHEDULER_SPEC.md 16.2）。残りは later (P3) |
| J3 | static の初期化子は、スコープの中の宣言の順に評価し、パッケージとコンパイル単位をソースの順で先にする。ほかのスコープの static 変数を読む初期値には警告を出す | `kirima-static-init-order` | 6.8、8.9、26.3 | P1 | in force（SCHEDULER_SPEC.md 3、6.1。SIR_SPEC.md 7.1）。名前は approved 2026-10-19 |
| J4 | コンストラクタの中の仮想メソッドの呼び出しがどのメソッドに届くかは、規格で確かめて固定する（未確認） | slang の診断 | 8.15 | P3 | later (P3) |
| J5 | パラメータ付きクラスの特殊化は slang に従う。static メンバは特殊化ごと | — | 8.25 | P3 | later (P3) |
| J6 | `weak_reference` は、印付けの後に参照先が生き残っていなければ null にする | — | 節は未確認 | P3 | later (P3。RUNTIME_ABI.md 6) |
| J7 | string と整数の変換は規格どおり | — | 6.16 | P1 | in force（VALUE_SEMANTICS.md 7.4、7.6） |
| J7 | VPI で string に書くときは遅延のモードに従う | — | 6.16、38 | P4 | later (P4) |

### 6.11 K：乱数と制約

同じシードなら、版をまたいでも同じ結果にします。他のシミュレータと同じ列を出すのは、規格が計算を決めている `$random` と `$dist_*` だけです。

| ID | 扱い | 診断・オプション | 節 | フェーズ | 状態 |
| --- | --- | --- | --- | --- | --- |
| K1 | シードの配り方（全体のシード、プロセスとオブジェクトを作る順、階層名）を RANDOMIZE_SPEC.md で固定し、版をまたいで変えない。変えるときは主版を上げる | — | 18.14、18.14.1 | P3 | later (P3) |
| K1 | 実行の冒頭にシードを書く | — | 18.14 | P1 | in force（CLI.md 4） |
| K2 | プロセスとオブジェクトごとに乱数の状態を持つ | — | 18.14.2、18.14.3 | P3 | later (P3) |
| K3 | `$random` と `$dist_*` は附属書 N のアルゴリズムの結果を出す | — | 20.15、附属書 N | P1 | in force（VALUE_SEMANTICS.md 8。M7 で SYSTEM_TASKS.md の乱数の節に書く） |
| K3 | `$urandom` は kirimasim のアルゴリズムで、版をまたいで固定する | — | 18.13 | P1 | proposed 2026-10-19 |
| K4 | 同じシードなら同じ解。`randc` は周期の中で重複しない。未対応の制約はエラー | `kirima-constraint-unsupported` | 18.4.2、18.5 | P3 | later (P3) |
| K5 | 矛盾すると `randomize()` は 0 を返し、変数を変えない（未確認）。関わった制約を示す | `kirima-randomize-failed`、`--randomize-fail` | 18.6.3 | P3 | later (P3) |
| K6 | DPI-C から使える種の関数を、svdpi.h の拡張として用意する（名前は未決） | `kirima_dpi_seed()` | — | P3 | later (P3) |

### 6.12 L：アサーション

| ID | 扱い | 診断・オプション | 節 | フェーズ | 状態 |
| --- | --- | --- | --- | --- | --- |
| L1 | Preponed の標本は時刻スロットの始めの値 | — | 16.5.1、4.4.2.1 | P3 | later (P3) |
| L2 | `disable iff` を評価する時点は規格で確かめる（未確認） | — | 16.12 | P3 | later (P3) |
| L3 | 遅延即時アサーションと unique、priority は、保留の列と flush point を使う | — | 16.4、12.4.2.1、12.5.3 | P1 | in force（SCHEDULER_SPEC.md 15） |
| L4 | 終わりに未完了の試行は、既定では失敗に数えず、件数を出す | `--assert-incomplete` | 16 | P3 | later (P3) |
| L5 | action block は Reactive で実行する。始まる前の `$past` の値は規格で確かめる（未確認） | — | 16.9.3、16.14 | P3 | later (P3) |
| L6 | vacuous な成功を、成功と分けて数える | `kirima-never-match` | 16.13、16.14 | P3 | later (P3) |

### 6.13 M：VPI と DPI

| ID | 扱い | 診断・オプション | 節 | フェーズ | 状態 |
| --- | --- | --- | --- | --- | --- |
| M1 | `cbReadWriteSynch` は Post-NBA で呼び、そこでの書き込みは Active 側をもう一度回す。`cbReadOnlySynch` は書き込みを許さない | — | 4.10、38 | P4 | in force（SCHEDULER_SPEC.md 17.2、2.2）。実装は P4 |
| M2 | `vpi_put_value` の遅延のモード | — | 38 | P4 | later (P4) |
| M3 | コールバックの中の書き込みで起きたコールバックは積み、今のコールバックが戻ってから呼ぶ | — | 38 | P4 | later (P4) |
| M4 | `context` でない import から export や `svGetScope` を呼ぶのは実行時のエラー。`pure` の呼び出しも省かない | `kirima-dpi-not-context` | 35.5.3、附属書 H | P1 | proposed 2026-10-19（DPI-C の import と同じ M7） |
| M5 | ファイバーが入るまで、DPI-C の task の中の時間待ちは実行時のエラー | `kirima-dpi-task-wait` | 35.9 | P1 | proposed 2026-10-19。ファイバーは later (P3) |
| M6 | export の中の `$finish`、4 値の引数と開いた配列が有効な期間 | — | 附属書 H、20.2 | P3 | later (P3。RUNTIME_ABI.md 8) |

### 6.14 N：階層と接続

| ID | 扱い | 診断・オプション | 節 | フェーズ | 状態 |
| --- | --- | --- | --- | --- | --- |
| N1 | 違いを観測できない接続だけ、ポートを畳み込む | — | 23.3.3 | P1 | in force（SCHEDULER_SPEC.md 5。SIR_SPEC.md 3.5） |
| N1 | `--report-state-opt` に、畳み込んだ数と、畳み込まなかった理由を出す | `--report-state-opt` | 23.3.3 | P2 | later (P2) |
| N2 | `force` と `release` は規格どおり | — | 10.6.2 | P1 | in force（SCHEDULER_SPEC.md 14） |
| N3 | 暗黙のネットは `` `default_nettype `` に従う | slang の警告 | 6.10、22.8 | P1 | in force（slang） |
| N4 | 階層参照と上方参照の名前は slang が解決する | — | 23.6、23.8 | P1 | in force（slang。SIR_SPEC.md 11） |
| N5 | `defparam` は slang に任せる | slang の診断 | 23.10 | P1 | in force（slang） |
| N6 | 名前の無い generate のブロックは slang が付ける名前（`genblk`）を使い、VPI と波形でも使う | — | 27.6、23.11 | P1 | in force（SIR_SPEC.md 1） |
| N7 | `inout` の接続は解決の節点 | — | 23.3.3、28 | P1 | in force（VALUE_SEMANTICS.md 5.3）。スイッチは later (P4) |
| N8 | 仮想インタフェースを通す modport の方向の違反で、エラボレーションで見つからないものは実行時の警告（未確認） | `kirima-modport-violation` | 25.5、25.9 | P3 | later (P3) |

### 6.15 O：タイムスケールと時刻

| ID | 扱い | 診断・オプション | 節 | フェーズ | 状態 |
| --- | --- | --- | --- | --- | --- |
| O1 | timescale の混在と欠落。`--timescale` が既定を決める | slang の診断 | 3.14、22.7 | P1 | in force（SCHEDULER_SPEC.md 1。CLI.md 2.1） |
| O2 | 遅延は規格どおりに丸める | — | 3.14.3 | P1 | in force（SCHEDULER_SPEC.md 1） |
| O2 | 丸めで 0 になる定数の遅延の警告。式の遅延が 0 になるときは、箇所ごとに 1 回の実行時の警告 | `kirima-delay-rounded-zero` | 3.14.3 | P1 | proposed 2026-10-19 |
| O3 | 最大の時刻を超える時刻は実行時のエラー（理由 error、コード 2） | `kirima-time-overflow` | 20.3 | P1 | in force（SCHEDULER_SPEC.md 8、16.2） |
| O4 | x、z、負の遅延 | — | 9.4.1 | P1 | in force（SCHEDULER_SPEC.md 8） |
| O4 | x や z を含む遅延と、負の遅延について、箇所ごとに 1 回の実行時の警告 | `kirima-delay-unknown`、`kirima-delay-negative` | 9.4.1 | P1 | proposed 2026-10-19 |
| O5 | `#1step` は最小の精度の 1 単位 | — | 14.4 | P1 | in force（SCHEDULER_SPEC.md 1） |

### 6.16 P：ファイルと表示

| ID | 扱い | 診断・オプション | 節 | フェーズ | 状態 |
| --- | --- | --- | --- | --- | --- |
| P1 | `$display` の書式は規格の表どおり | — | 21.2.1 | P1 | in force（SYSTEM_TASKS.md 3） |
| P2 | `$monitor` は値が変わったときに Postponed で出す。有効なのは最後に呼んだ 1 つだけ | — | 21.2.3 | P1 | in force（SYSTEM_TASKS.md 4） |
| P3 | SystemVerilog の出力と DPI-C の `printf` は 1 つの標準出力を共有し、順を保つ | — | 21.3 | P1 | in force（SYSTEM_TASKS.md 2） |
| P3 | 出力を書き出す時点と、表示のタスクのたびに書き出すオプション | `--autoflush` | 21.3 | P1 | proposed 2026-10-19（SYSTEM_TASKS.md 2。CLI.md 2.2） |
| P4 | `$readmemh` は、ファイルが与えない語を変えない | — | 21.4 | P1 | in force（SYSTEM_TASKS.md 7.1） |
| P4 | ファイルの中のアドレスが範囲の外ならエラーにして読み込みをやめる。終わりのアドレスが無ければ範囲はメモリの最も大きいアドレスで終わり、開始のアドレスも無ければ最も小さいアドレスから始まる | — | 21.4 | P1 | in force（SYSTEM_TASKS.md 7.1）。終わりのアドレスが無いときの範囲は approved 2026-10-19 |
| P4 | 範囲や語の数が合わないときの実行時の警告 | `kirima-readmem-range`、`kirima-readmem-short` | 21.4 | P1 | proposed 2026-10-19 |
| P5 | `$dumpvars` は build で残したスロットだけを出せる。残していないものを求められたら一覧を出す | `kirima-dump-not-preserved` | 21.7 | P2 | later (P2) |

### 6.17 Q：決定性と並列化

| ID | 扱い | 診断・オプション | 節 | フェーズ | 状態 |
| --- | --- | --- | --- | --- | --- |
| Q1 | 並列に走らせるのは副作用の無い静的な島だけ。`$display`、DPI-C、乱数、`$finish` を含むプロセスは、主のスレッドで既定の順に実行する | — | 4.5 | P4 | later (P4。RUNTIME_ABI.md 8) |
| Q2 | 結果に影響する所で、順序の決まらないコンテナを走査しない | — | — | P1 | in force（CLAUDE.md。SIR_SPEC.md 1） |
| Q2 | 同じ入力から同じ C++ を作る（NFR-6）。CI で 2 回作って比べる | — | — | P2 | later (P2。CODEGEN_SPEC.md の「C++ 生成の規則」（Rules of C++ generation）) |
| Q3 | カーネルと生成コードは `-ffast-math` なしで、積和を融合しないよう `-ffp-contract=off` でビルドする。数学の関数は libm に任せる（NFR-2 の例外） | `tools/check_release_flags.py` | 6.12 | P1 | proposed 2026-10-19（CLAUDE.md。CODEGEN_SPEC.md の「浮動小数点」（Floating point）） |

### 6.18 R：そのほかと UVM

| ID | 扱い | 診断・オプション | 節 | フェーズ | 状態 |
| --- | --- | --- | --- | --- | --- |
| R1 | `always_latch` は規格どおり | — | 9.2.2.3 | P1 | in force（SCHEDULER_SPEC.md 6.2） |
| R1 | `always_comb` がラッチになるときと、`always_latch` がラッチにならないときの警告 | `kirima-latch-inferred`、`kirima-no-latch` | 9.2.2.3 | P1 | proposed 2026-10-19 |
| R2 | `assign` と `deassign` は `force` の下の層 | — | 10.6.1 | P1 | in force（SCHEDULER_SPEC.md 14） |
| R3 | 前処理の式は slang に任せる | slang の診断 | 22.6 | P1 | in force（slang） |
| R4 | UVM の規格外の依存：kirimasim 自身の DPI-C 関数、名前の表と `force` を通す `uvm_hdl_*`。`$psprintf` は `$sformatf` として受け付け、警告する | `kirima-nonstandard` | — | P3 | later (P3) |
| R5 | 言語の版の混在は slang に任せる（`` `begin_keywords ``） | — | 22.14 | P1 | in force（CLI.md 2.1） |

`uvm_hdl_deposit` で書いた値は、`vpiNoDelay` の書き込みと同じようにプロセスを起こします（M2。later (P4)）。

## 7. オプション、終了の理由、診断の一覧

### 7.1 オプション

各オプションの意味、既定、段階は CLI.md にあります。どれも既定の実行の結果を変えません。

| オプション | 項目 | 置き場 | 状態 |
| --- | --- | --- | --- |
| `--max-scc-iter`、`--max-zero-delay`、`--max-slot-events`、`--max-steps` | A1、A6、A9、A14 | CLI.md 2.6 | in force |
| `--max-procs`、`--max-call-depth`、`--max-wall-time` | A5、A15、I8 | CLI.md 2.7 | proposed 2026-10-19 |
| `--stop`、`--init-values`、`--cond-x`、`--bounds-check`、`--autoflush`、`--mintypmax`、`--report-pulses`、`--wave-glitches` | I6、E9、E1、E2、F7、P3、C5、D3 | CLI.md 2.2 | proposed 2026-10-19 |
| `--max-hierarchy-depth`、`--max-generate-steps`、`--max-constexpr-depth`、`--max-constexpr-steps`（slang のもの） | A11、A12 | CLI.md 2.1 | approved 2026-10-19 |
| `--race-nba`、`--randomize-fail`、`--assert-incomplete` | B1、K5、L4 | CLI.md 2.3（P2 以降のオプション） | proposed 2026-10-19（P2 以降） |

### 7.2 終了の理由

理由と終了コードは SCHEDULER_SPEC.md 16.2 と CLI.md 5 にあります。

| 理由 | コード | 項目 | 状態 |
| --- | --- | --- | --- |
| resource_limit | 6 | A5、A15 | proposed 2026-10-19 |
| wall_time_limit | 4 | I8 | proposed 2026-10-19 |
| fatal、error、zero_time_limit | 1、2、5 | A1、A6、A9、A13、A14、J2、O3 | in force |

### 7.3 診断

名前は 4 のとおり。状態はそれぞれの 6 の行に従い、名前そのものは M7 で確定します。

| 種類 | 名前（項目） |
| --- | --- |
| エラボレーションのエラー | `kirima-vif-in-sensitivity`（D10）、`kirima-constraint-unsupported`（K4）。`always_comb` などの複数の書き手と `uwire` の多重駆動は slang の診断（B4、E8） |
| lint の警告 | `kirima-zero-delay-loop`（A1）、`kirima-always-no-wait`、`kirima-always-no-wait-path`（A2）、`kirima-empty-sensitivity`（A3）、`kirima-const-wait`（A4）、`kirima-comb-loop`（A6）、`kirima-self-trigger`（A9）、`kirima-shared-loop-var`（A10）、`kirima-monitor-side-effect`（A13）、`kirima-race-blocking`（B3、B6）、`kirima-multi-write`（B4）、`kirima-blk-nblk-mix`（B5）、`kirima-comb-read-before-write`（B12）、`kirima-comb-nba`（C7）、`kirima-final-nba`（C8）、`kirima-comb-dynamic-read`（D4）、`kirima-casex`（E6）、`kirima-const-out-of-range`（F7）、`kirima-side-effect-in-index`（G2）、`kirima-multi-side-effect`（G4）、`kirima-fork-loop-var`（H2）、`kirima-static-init-order`（J3）、`kirima-never-match`（L6）、`kirima-delay-rounded-zero`（O2）、`kirima-latch-inferred`、`kirima-no-latch`（R1）、`kirima-nonstandard`（R4） |
| 情報 | `kirima-comb-loop-unsplit`（A7）、`wait(0)` の `kirima-const-wait`（A4）、消したパルス（C5） |
| 実行時の警告 | `kirima-event-missed`（D8）、`kirima-cond-x`（E1、E2）、`kirima-out-of-range`（F7）、`kirima-static-task-reentry`（H1）、`kirima-randomize-failed`（K5）、`kirima-modport-violation`（N8）、`kirima-delay-unknown`、`kirima-delay-negative`（O4）、`kirima-readmem-range`、`kirima-readmem-short`（P4）、`kirima-dump-not-preserved`（P5） |
| 実行時のエラー | `kirima-readonly-write`（A13）、`kirima-ref-stale`（H4）、`kirima-dpi-not-context`（M4）、`kirima-dpi-task-wait`（M5）、`kirima-null-deref`（J2）、`kirima-time-overflow`（O3） |
| 実行時の致命 | `kirima-zero-time-loop`（A1、A9、A14）、`kirima-comb-not-converged`（A6）、`kirima-process-limit`（A5）、`kirima-call-depth-limit`（A15） |

## 8. テストの群

この文書の選択は、どれもテストスイートのテストで固定します。規格が結果を決めていない所では、oracle の `choice` で kirimasim の選択だと示します（TEST_SUITE.md、proposed 2026-10-19）。上限と診断は `:expected_exit:` と `:expected_stderr:` で確かめ、ID が決まったら（M7）`--diag-format=json` の出力で確かめます。これらの群を作る生成器は、TEST_SUITE.md の「What is added during P1」に挙げます。

| 群 | 項目 | 期待値の出どころ | 中身 | フェーズ |
| --- | --- | --- | --- | --- |
| zero_time | A1、A2、A4、A5、A6、A9、A14、A15 | spec（終了の理由と診断） | `forever #0;`、`forever begin end`、反転する組合せのループ、自分を起こす NBA、終わらない `join_none`、終わらない再帰 | P1 |
| time0_order | B8、B9、B10、D2、J3 | choice | `initial` と `always` をどちらの順にも書いたリセットの負の辺、遅延付きの連続代入の初期値、互いに頼る static の初期化子 | P1 |
| region_order | B1、B2、B11、C1、C2、C6 | choice、spec | 同じ時刻の複数の NBA、`#0` の書き方ごとの順序、同じ時刻の `$finish`、文の途中で切り替えないこと | P1 |
| race_detect | B3、B6、D7、D8 | spec（`--race-check` が競合を報告すること） | ブロッキング代入の競合、同じ時刻の `->` と `@` | P2 |
| sensitivity | A10、B12、D4、D5、D6 | spec | 関数の中の読み出し、パッケージの変数、添字、書く前の読み出し、`always @*` との違い | P1 |
| delay | C3、C4、C5、O2、O4 | spec | 代入内遅延の添字、慣性と取り消しの無いこと、丸めで 0、x の遅延 | P1 |
| x_semantics | E1〜E4、E6、F3〜F7 | spec、iverilog | 条件の x、演算子ごとの x、範囲外、べき乗、実数の丸め | P1 |
| side_effect | G1〜G4 | spec、choice | 短絡評価と null、添字の中の `++`、引数の順 | P1 |
| lifetime | H1〜H6、I1〜I4 | spec | static の task の同時の呼び出し、`fork` のループ変数、disable、kill の後始末、`wait fork` | P1、P3 |
| random | K1〜K5 | spec、choice | 同じシードで同じ列、乱数の安定性、附属書 N と照らした `$random`、矛盾した制約 | P1、P3 |
| assertion | L1〜L6 | spec | 標本の時点、`disable iff`、未完了の試行、vacuous な成功 | P3 |
| vpi_dpi | M1〜M6、J7 | cocotb のテスト、spec | コールバックの時点、遅延のモード、再入、`context` | P1、P4 |
| output | P1〜P5 | iverilog、spec | 書式、`$monitor`、出力の順、`$readmemh` | P1 |
| determinism | Q1〜Q3 | CI の検査 | スレッド数を変えた実行、2 回の生成の比較、浮動小数点のフラグ | P0、P4 |

## 9. 最初の提案との違い

この文書の最初の提案と承認済みの詳細仕様書は、次の 4 点で違っていました。2026-10-19 に、人がそれぞれ下のとおり選びました。

1. **static の初期化子を評価する順（J3。SCHEDULER_SPEC.md 6.1 の手順 1）。** 最初の提案は依存の順（依存が無ければ宣言の順）で、循環があれば警告を出して宣言の順にするものだった。承認済みの決まりは、スコープの中は宣言の順で、パッケージとコンパイル単位をソースの順で先にする（SCHEDULER_SPEC.md 3、6.1。SIR_SPEC.md 7.1）。規格はこれらの間の順を決めておらず（6.8、10.5）、パッケージをそれを読むスコープより先にコンパイルすることを求める（26.3）。**2026-10-19 に人が選んだもの：承認済みの決まり。** ほかのシミュレータの扱いも分かれている（Icarus Verilog 14.0 (devel) は宣言の順、Verilator 5.052 は依存の順）。そのため大事なのは、ほかのスコープの static 変数を読む初期値に、承認済みの決まりが出す警告（`kirima-static-init-order`）である。
2. **どの経路にも時間制御の無い `always`（A2）。** 最初の提案はエラボレーションのエラーにするものだった。承認済みの決まりは、実行して `--max-steps` で止める（SCHEDULER_SPEC.md 6.2、16.2）。規格は、そのような `always` が同じ時刻で回り続けると述べるが、違反とはしていない（9.2.2.1）。**2026-10-19 に人が選んだもの：承認済みの決まりに lint の警告を足す。** 規格が許すので、拒まない（方針 1）。これまでどおり実行して `--max-steps` で止め（方針 7）、エラボレーションの時点で lint の警告 `kirima-always-no-wait` で知らせる（方針 8）。
3. **`$monitor` や `$strobe` の引数から呼ばれた関数の書き込み（A13）。** 最初の提案はどの書き込みもエラーにするものだった。承認済みの解釈では、その関数は自分の引数と、automatic と static の変数に書ける（SCHEDULER_SPEC.md 2.2）。規格は Postponed 領域でネットや変数に書くことを禁じる（4.4.2.9）。**2026-10-19 に人が選んだもの：承認済みの解釈。** どの書き込みもエラーにすると、自分の変数で計算する関数を呼べなくなる。`$monitor` と `$strobe` は引数に関数の呼び出しを許している（21.2.2、21.2.3）。副作用のある関数は lint の警告 `kirima-monitor-side-effect` で知らせる。
4. **`$readmemb` と `$readmemh` の範囲外のアドレス（P4）。** 最初の提案は無視するものだった。承認済みの決まりは、引数で与えた範囲の外にあるファイルの中のアドレスをエラーにし、読むのをやめる（SYSTEM_TASKS.md 7.1）。範囲を与えないときにメモリの宣言の範囲の外にあるアドレスは、どちらも決めていなかった。**2026-10-19 に人が選んだもの：承認済みの決まりに、引数が与えない範囲をメモリの範囲で補うことを足す。** 終わりのアドレスが無ければ範囲はメモリの最も大きいアドレスで終わり、開始のアドレスも無ければ最も小さいアドレスから始まる。そのため、メモリの外のアドレスも同じエラーになる（SYSTEM_TASKS.md 7.1）。Icarus Verilog 14.0 (devel) も同じ扱いである。Verilator 5.052 は、メモリを越えるアドレスや語で実行を終える。

## 10. 未確認

- 7.3 の診断のうち、固定した版の slang がすでに出すもの（4）。確かめ方：M7 で slang の診断を一覧にして照らす。
- 正しいコードに `kirima-event-missed` が出ることがあるか（D8）。確かめ方：P3 で実設計と UVM のテストベンチを走らせる。
- 6 で「未確認」とした節（D10、H4、J4、J6、K5、L2、L5、N8）。確かめ方：各行に挙げた節を読む。
- 実設計での上限の既定（`--max-procs` と、CLI.md 2.6 と 2.7 のほかの上限）。確かめ方：M6 以降に Ibex、CVA6、UVM の例を走らせる（CLI.md 2.6）。
- 時刻 0 に `always` を `initial` より先にすることで、実設計や UVM のテストベンチが困らないか。確かめ方：同じ実行。
- 遅延 0 の `vpiInertialDelay` を積む所（M2）。確かめ方：P4 で 38 章と cocotb のテスト。
- 互換モードが求められることがあるか（方針 5）。確かめ方：P4 で利用者の要望を見る。

## 11. 参考資料

公開のページだけを挙げます。規格の本文は写さず、節番号だけを示します。

- Accellera、[IEEE 1800-2023 が IEEE GET Program で入手できることの案内](https://accellera.org/news/press-releases/394-accellera-announces-ieee-1800-2023-standard-available-through-ieee-get-program)
- Verilator：[Warnings](https://verilator.org/guide/latest/warnings.html)、[Revision History](https://verilator.org/guide/latest/changes.html)、[Input Languages](https://verilator.org/guide/latest/languages.html)、[Issue #1538（UVM 対応）](https://github.com/verilator/verilator/issues/1538)
- Icarus Verilog：[iverilog のマニュアル](https://manpages.ubuntu.com/manpages/noble/man1/iverilog.1.html)、[vvp のフラグ](https://steveicarus.github.io/iverilog/usage/vvp_flags.html)
- cocotb：[Timing Model](https://docs.cocotb.org/en/stable/timing_model.html)、[Simulator Support](https://docs.cocotb.org/en/stable/simulator_support.html)、Issue [#448](https://github.com/cocotb/cocotb/issues/448)、[#2178](https://github.com/cocotb/cocotb/issues/2178)、[#4082](https://github.com/cocotb/cocotb/issues/4082)
- slang：[Command Line Reference](https://sv-lang.com/command-line-ref.html)
- A. Lööw、[The Simulation Semantics of Synthesisable Verilog](https://arxiv.org/abs/2502.19348)（arXiv 2502.19348）
