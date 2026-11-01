# P1 母集団の期待出力の監査

[English](oracle_audit.md) · 英語版を正本とし、日本語版と食い違う場合は英語版に従います。

状態：**決定済み**（2026-10-21）。保守者は第 4 節の提案を受け入れ、除外なし、期待出力の変更なし、
README 件数の維持としました。以下の判定は AI（Claude Code）によるもので、人が 1 件ずつ確認したものではありません。

## 1. 目的

期待出力の多くは Icarus Verilog 13.0、同ツールでコンパイルできない残りは Verilator 5.052 で記録されています
（`:oracle:`、kirimasim-tests README）。参照が IEEE 1800-2023 に従わなければ、その差が期待値へ入り、
P1 母集団を通る kirimasim も追従する可能性があります。この監査は `tests/gates/p1_population.txt`
（kirimasim-tests c983cf799b4d の 1466 件）でそれを探し、4.7 など規格が結果を未規定とするテストと分けます。

## 2. 方法

`tools/oracle_audit.py` で母集団全件を Icarus Verilog 13.0（`iverilog -g2012`、`vvp -n`）、
Verilator 5.052（`--binary --timing --x-assign unique --x-initial unique`、実行時に
`+verilator+rand+reset+2`、seed 1・7・42・2026、現行 runner と同じ `+verilator+error+limit+1000000`）、
kirimasim（main、900dbf3）で実行し、1 テスト 1 行の [oracle_audit.csv](oracle_audit.csv) を出します。
実行ファイルだけを使い、Icarus Verilog・Verilator のソースは読んでいません。

```sh
python3 tools/oracle_audit.py --tests-repo <kirimasim-tests at c983cf799b4d> --out <dir> \
    --iverilog-bin <bin of Icarus Verilog 13.0> --verilator <Verilator 5.052> \
    --kirimasim build/release/src/driver/kirimasim
```

次のいずれかを満たすテストを候補とします。

| | 規則 |
| --- | --- |
| a | Verilator で期待値を記録し、期待値・コード・`:fourstate: yes` のいずれかで x/z が関与する |
| b | Icarus と Verilator の両方で実行でき、出力が異なる |
| c | Verilator の出力が seed で異なる |
| d | イベント順に依存し得る構文（`#0`、NBA、`fork`、名前付き event、`$strobe`・`$monitor`、複数手続きが書く変数）を使い、シミュレータで期待値を記録している |

a・d の検出はテキスト走査で、過少より過剰に拾います。CSV に検出結果があります。
a・b・c は x/z、強度、ネット解決、書式、期待値の記録元に依存する全期待行を規格と照合しました。
d は 2 プロセス、または同じプロセスの 2 文が規格上許される別順序で動く箇所をすべて調べ、表示行への影響を確認しました。

判定は、**参照の不具合**（記録元の規格逸脱が期待値にある）、**未規定**（規格が未規定とする選択に表示が依存）、
**テストの誤り**（自身の検査・期待値が規格と矛盾）、**問題なし**、**人の確認が必要**（条項で決定できない）です。

## 3. 結果

参照結果を再現しました。`iverilog@13.0` を挙げる 1238 件はすべて Icarus 13.0 で一致し、
`verilator@5.052` の 226 件は全 4 seed で一致しました。kirimasim は 1451 件成功、15 件未対応です。

| | テスト数 |
| --- | --- |
| Icarus：一致 / コンパイル・実行不可 / 不一致 | 1238 / 223 / 5 |
| Verilator（seed 1）：一致 / ビルド不可 / 不一致 | 1327 / 32 / 107 |
| Verilator の出力が seed で変わる | 33 |
| 両参照で実行でき出力が異なる | 105 |
| 候補 a / b / c / d | 3 / 105 / 33 / 92 |
| 候補合計（重複あり） | 191 |

| 判定 | テスト数 |
| --- | --- |
| 参照の不具合 | 0 |
| 未規定 | 2 |
| テストの誤り | 0 |
| 人の確認が必要 | 0 |
| 問題なし | 189 |

191 件の期待値に、Icarus 13.0・Verilator 5.052 の不具合を記録したものはありませんでした。
両者が違う場合、期待値が規格どおりで他方が逸脱しており、多くは Verilator が x/z・強度を持たないためです。
補助検査として、母集団の期待出力に、テスト自身の検査失敗を表示する行がないことも確認しました。

for 条件と含意演算子 `->` を除く前の旧ツールは d をさらに 34 件抽出しましたが、全件を読んで問題なしと判断しました。

### 3.1 「問題なし」以外のテスト

| テスト | 候補 | 判定 | 節 | 理由 | 規格が許す結果 | kirimasim | 上流 Issue |
| --- | --- | --- | --- | --- | --- | --- | --- |
| `ch4/test_event_delta_cycles` | bcd | 未規定 | 4.7, 9.4.2 | always @(a or b or c) の起動回数を数えます。initial は時間制御なしで a/b/c の 2〜3 個を続けて変えますが、4.7 は文の間での中断を認めるため、変更ごとに起き得ます。期待回数はそこで中断しない場合の値です。4.1 を参照してください。 | counter の 6・7・15・17 行は順に 3〜6、その値+3、8〜12（delayed_counter も等しい）、その値+5。他の全行は固定。 | pass | — |
| `ch9/test_sens_level` | d | 未規定 | 4.7, 9.4.2 | 108 行の a = 0; b = 1; は時間制御なしで、両方を always @(a or b or c) が読みます。4.7 により initial が a = 0 後に中断し、always が走って再待機前に中断することもあり、b = 1 は何も起こさず out3 は 00 のままです。単なる開始順の選択でなく 2 回の中断が必要です。4.1 を参照してください。 | Scenario 6 は PASS（out3=0xbb）または FAIL（out3=0x00）で、件数と最終行も対応する値。他の全行は固定。 | pass | — |

どちらもシミュレータの不具合ではないため、上流 Issue は探していません。両方を母集団に残します（4.1）。

## 4. 決定

保守者は 2026-10-21 に以下を受け入れました。いずれも kirimasim の仕様で承認済みの解釈に従います。

### 4.1 2 文の間での手続きの中断（4.7）

4.7 は、時間制御のない文を 1 イベントとして実行する必要はなく、任意の位置で手続きを中断し、
他プロセスを実行できると定めます。`ch4/test_event_delta_cycles` の always 起動回数は直接それに依存し、
`ch9/test_sens_level` は起きた block 自体も再待機前に中断すると 2 回目の変更を逃します。両方とも**未規定**です。
UNSPECIFIED_BEHAVIOR.md B2 は既にこの点と、待機まではプロセスを切り替えない kirimasim の選択を記録しています。

両方とも**母集団に残します**。期待値は選択した動作の結果で、TEST_SUITE.md は未規定の結果を
kirimasim の選択（`choice`）で判定します。除外は参照の不具合を記録した期待値が対象で、今回はありません。
B2 に依存する他のテストも同じです。level-sensitive always と連続する 2 つの blocking 代入があるテストは
38 件ありますが、個別確認はしておらず、その確認の有無はゲートの結果に影響しません。
新しいリスト版へ移す際に kirimasim-tests で `:nondeterministic: yes` と許される結果を付けることは可能ですが、
この監査とは別の変更です。

### 4.2 %E・%G の指数文字の大小（21.2.1.1）

Table 21-2 は `%e or %E`・`%g or %G` を各 1 つの指定として列挙し、real は C の書式とします。
C の %E・%G は E を書きます。SYSTEM_TASKS.md 3.6 はそのように解釈します（2026-10-17 承認）。
したがって `ch21/test_format_advanced`・`ch21/test_format_float` は**問題なし**です。
期待値 E はこの解釈、Icarus 13.0・kirimasim と一致し、Verilator 5.052 と異なります。
16 進の a〜f を小文字とする SYSTEM_TASKS.md 3.2 の規則には規格本文の裏付けがありませんが、
3 ツールとも小文字なので、対象として挙げたテストはありません。

### 4.3 NaN

5 テスト（`test_math_advanced`、`test_math_hyperbolic_special_values`、`test_math_special_values`、
`test_realtobits_bitstoreal`、`test_system_functions_edge`）は NaN が自身と不等の場合だけ成功します。
規格は NaN に言及しませんが、6.12（real は C double、脚注で IEEE 754 表現）と 20.8.2（C と同じ数学関数）から
VALUE_SEMANTICS.md 7 の解釈に従い、その行は固定と扱います。**問題なし**です。

### 4.4 除外

ありません。参照の不具合を記録した期待値はなく、未規定 2 件も残すため（4.1）、除外リストも作らず、
kirimasim への Issue も開かず、README 件数も変えません。
将来除外が必要な場合、ゲートの実行版はリストのヘッダにある kirimasim-tests 179c55de9c83
（リスト選定は c983cf799b4d）です。外部 main の修正は、TEST_SUITE.md に従い人が承認してリストを更新するまで
ゲートへ届きません。このリポジトリの tests/gates/ の除外リストなら即時反映します。

## 5. 期待出力に影響しないコメントの誤り

| テスト | 行 | 誤り |
| --- | --- | --- |
| `ch5/test_ieee1800_2023_literal_edge` | 182 | `\0` 後の文字を保つかは任意と書いていますが、6.16 は `\0` を除き、残りを保ちます（期待する長さ 10 は正しい）。 |
| `ch20/test_math_edge_cases` | 51 | `$clog2(0)` を処理系定義としますが、20.8.1 は 0 と定めます（検査・期待出力は正しい）。 |
| `ch6/test_x_value_bitwise` | 54 | `4'bxx10 & 4'b1001` を最初は x010 と書きますが、計算過程と検査は x000 です。ヘッダは 6.6.7 を引用しますが、表は 11.4.8 です。 |
| 第 11 章の複数テスト | — | 算術・0 除算に 11.4.2（正しくは 11.4.3）、=== に 11.4.6（11.4.5）、reduction に 11.4.12（11.4.9）を挙げるものがあります。`ch12/test_casex_casez_wildcard` は 12.5.1 の規則に 12.5.2 を表示します。 |

これらは kirimasim-tests の記載で、本提案には含めません。

## 6. 観測した Icarus Verilog・Verilator の逸脱

この監査の出力で観測したもので、最小設計による確認はしていません。期待値に入っているものはありません。
報告用ではなく差分を記録する、kirimasim-tests の docs/simulator-differences.md への候補です。

| 版 | 節 | 観測 | テスト |
| --- | --- | --- | --- |
| Icarus 13.0 | 20.9 | $countbits が内部 assertion で実行を停止します。 | `ch20/test_countones_bits` |
| Icarus 13.0 | 6.16.12, 6.16.9 | 負の int の hextoa がマイナス符号を書きます。 | `ch6/test_string_hextoa` |
| Icarus 13.0 | 21.2.1.2 | field 幅の 16 進出力を、0 でなく空白で埋めます。 | `ch21/test_display_format` |
| Icarus 13.0, Verilator 5.052 | 16.3 | immediate cover の pass 文を実行しません。 | `ch16/test_assert_immed_2` |
| Verilator 5.052 | 6.6.3 | wand・wor・triand・trior がドライバの AND/OR になりません。 | 7 件、例： `ch6/test_net_wand` |
| Verilator 5.052 | 20.5 | $shortrealtobits・$bitstoshortreal が 0 や異なる bit を返します。 | 9 件、例： `ch20/test_shortreal_ieee754` |
| Verilator 5.052 | 20.8.2, 6.12 | NaN が自身と等しいと比較します。 | 4.3 の 5 件 |
| Verilator 5.052 | 4.4.2.3, 4.5 | #0 後、先に起こした連続代入・always_comb が旧値を示します（4.9.1 の該当項目付近）。 | `ch4/test_delta_cycle`, `ch4/test_ieee1800_2023_delta_convergence`, `ch9/test_always_comb_cascade_propagation`, `ch9/test_sensitivity_edge_cases` |
| Verilator 5.052 | 21.2.1.1 | %E・%G が小文字 e を書きます（SYSTEM_TASKS.md 3.6 の解釈と異なる）。 | `ch21/test_format_advanced`, `ch21/test_format_float` |
| Verilator 5.052 | 21.3.4.3 | $sscanf が field 幅を無視し、%d が underscore で止まります。 | `ch21/test_scanf_comprehensive`, `ch21/test_numeric_edge_cases` |
| Verilator 5.052 | 6.16.9 | atoi が符号と先頭空白を読みます。 | `ch6/test_string_atoi`, `ch6/test_string_atoi_2` |
| Verilator 5.052 | 5.7.1 | 32 ビット超のサイズなし十進数が符号 bit を失います。 | `ch6/test_int_types_edge_cases`, `ch11/test_ieee1800_2023_arithmetic_edge` |
| Verilator 5.052 | 11.6.1, 11.6.2 | INT_MIN / -1 が 0 です。 | `ch11/test_ieee1800_2023_arithmetic_edge` |
| Verilator 5.052 | 9.4.2.2 | 読む変数へ書く always @* が DIDNOTCONVERGE で止まります。 | `ch9/test_implicit_event` |
| Verilator 5.052 | 29 | edge-sensitive UDP が別の値を出します。 | `ch29/test_udp_seq` |
| Verilator 5.052 | — | 既知値の行も失敗し、一部は seed で変わります（原因は不明）。 | `ch11/test_case_equality_edge_cases`, `ch11/test_partselect_xz`, `ch11/test_slice_assign`, `ch12/test_control_flow`, `ch20/test_onehot_isunknown`など |

Verilator の括弧なし `$printtimescale` と、Icarus の 1 回だけ評価する `assign` も観測し、既に一覧にあります。

## 7. 全候補

| テスト | 候補 | 判定 | 節 | 理由 | kirimasim |
| --- | --- | --- | --- | --- | --- |
| `ch10/test_assign_edge_cases` | d | 問題なし | 4.9.1, 4.4.2.2, 4.4.2.3, 4.5, 4.6 | a・b・sel を書く手続きは 1 つだけです。26〜27 行の連続代入は #1 後、または Test 8 では `#0`（117 行）後に読みます。 | pass |
| `ch10/test_assign_nba` | d | 問題なし | 4.6, 4.4.2.3, 10.4.2, 9.4.1 | `a` を読み書きする手続きは 1 つだけです（`always #5 clk` は誰も待たない clock を反転します）。 | pass |
| `ch10/test_assign_proc` | b | 問題なし | 10.6, 10.6.1, 4.9.2 | 手続き的連続代入は RHS の operand 変更時に再評価するため、Test 4b の wide_out は data_in（0xDEBC）に追従します。Icarus 13.0 は 0x55BC を保ちます。 | pass |
| `ch10/test_assignment_forms` | d | 問題なし | 10.3.2, 10.4.2, 4.4.2.4, 6.8 | `a`/`b` を書く手続きは 1 つだけです。`a = 50;`（45 行）は t=2、`always_ff`（29〜31 行）が sample する edge は t=5・15 なので、書き込みと edge は重なりません。 | pass |
| `ch10/test_blocking_in_clocked_always` | d | 問題なし | 4.4.2.4, 4.5, 10.4.2, 9.4.2 | 両 `always @(posedge clk)`（44・70 行）は同じ edge で順序未規定で起きますが、共有するのは NBA の対象だけです。`resetn <= 1`（73 行）を 48 行が読み、`reg_a`/`reg_b`（49〜54 行）を 78・90〜103 行が読みます。 | pass |
| `ch10/test_concat_lhs_assign` | d | 問題なし | 6.8, 9.4.2, 10.4.2 | NBA は `{nba_high, nba_low} <= 16'hDEAD`（97 行）だけです。同じプロセスが次の `@(posedge clk)`（98〜99 行）、1 周期後に読むため、前の時刻の NBA は実行済みです。clk は宣言初期化の 0（6.8）から始まり、時刻 0 に posedge はありません。 | pass |
| `ch10/test_continuous_assign_2` | d | 問題なし | 4.9.1, 10.3.3, 4.4.2.3, 4.6 | Test 10 以外は operand 変更の `#1` 後に wire を読みます。Test 10 の `assign #1 delayed_out = a` は t=9 より前に新値を取れず（10.3.3）、どの順序でも検査結果は同じです。 | pass |
| `ch10/test_ct_assignment_complex` | d | 問題なし | 10.4.2, 4.4.2.4, 10.6.2, 6.21 | clock（18 行）以外は 1 プロセスです。各 NBA（135・145・155〜158・169〜171・184 行）は posedge で発行し、10 単位後の次の posedge、NBA 領域が実行済みの別時刻で読みます。 | pass |
| `ch10/test_edge_delta_cycle_chain` | d | 問題なし | 4.9.1, 10.3.2, 10.3.3, 4.4.2.3, 4.5 | `assign mid = source; assign #0 dest = mid;` は時刻 0（4.9.1）と source の変更ごとに評価し、#0 の更新は同じスロット内で終わります。 | pass |
| `ch10/test_force_deep_generate_scope` | d | 問題なし | 10.6.2, 4.6 | 複数の書き手は連続代入と 1 つの initial の force/release です。force は連続代入より優先し（10.6.2）、全検査は対象の force/release の少なくとも `#1` 後です。 | pass |
| `ch10/test_nba` | d | 問題なし | 4.6, 10.4.2 | 手続きは 1 つです。NBA の交換と複数 NBA を `#1` 後に検査し、NBA は実行順に適用します（4.6 b）。 | pass |
| `ch10/test_nba_assignment` | d | 問題なし | 10.4.2, 4.4.2.4 | test プロセスと clock が 1 つずつです。各 NBA は次の `@(posedge clk)`（10 単位後）の後に読み、68〜69・82〜83 行の pipeline 読み取りは意図的に旧値を取ります（10.4.2）。 | pass |
| `ch10/test_nba_assignments` | d | 問題なし | 4.6, 10.4.2, 9.4.2 | 1 つの initial が全代入・読み取りを行います。clock プロセスは宣言初期化された `clk`（22 行）だけを反転するため、時刻 0 の edge はありません。 | pass |
| `ch10/test_nba_basic` | d | 問題なし | 4.6, 10.4.2 | initial は 1 つです。各 NBA は次の `@(posedge clk)`、10 時間単位後に読みます。 | pass |
| `ch10/test_nba_basic_2` | d | 問題なし | 6.8, 9.4.2, 10.4.2, 4.6 | clock 以外では initial だけが a・b を扱います。NBA と同じ時刻での読み取りは旧値（10.4.2）、後の検査は NBA の 1 周期後です。 | pass |
| `ch10/test_nba_complex_lhs` | d | 問題なし | 10.4.2, 4.4.2.4 | test プロセスと clock が 1 つずつです。bit・part・member への各 NBA 群は、後の時刻の次の `@(posedge clk)` 後に読みます。 | pass |
| `ch10/test_nba_complex_lhs_extended` | d | 問題なし | 4.6, 10.4.2 | 1 プロセスがすべての NBA 対象を書き、それぞれを次の `@(posedge clk)` 後にだけ読みます。 | pass |
| `ch10/test_nba_edge_cases` | d | 問題なし | 4.7, 10.4.2, 9.4.2 | 競合はありますが出力へ影響しません。68〜69 行のループと 116〜117 行（`@(posedge clk); a = 8'h55;`）で、initial は 28〜29 行の always が sample する同じ posedge に a を blocking で書きます。always は旧値・新値のどちらも取り得ます（4.7）。全ケースで検査（69・118 行）の前にもう 1 つ posedge があります。 | pass |
| `ch10/test_nba_eval` | d | 問題なし | 4.6, 10.4.2 | 手続きは 1 つで他プロセスはありません。各 NBA 群の後に `#1` を待って検査し、同じ block の NBA は実行順に適用します（4.6 b）。 | pass |
| `ch10/test_nba_nested_array` | d | 問題なし | 10.4.2, 4.4.2.4 | clock なしの 1 プロセスです。各 NBA 群の後に `#1` を待つため、読み取り前に、以前のスロットの NBA 領域で全更新が適用済みです。 | pass |
| `ch10/test_nba_ordering` | d | 問題なし | 4.6, 10.4.2, 4.4.2.4 | 逐次プロセスは 1 つです。`a <= 8'hAA` 直後の読み取り（38〜40 行）は旧値 0 を見る必要があります（10.4.2）。 | pass |
| `ch10/test_nba_part_select` | d | 問題なし | 10.4.2, 4.6 | 1 プロセスです。重ならない part-select の NBA は #1 後、NBA 領域が実行済みの時点で読みます。 | pass |
| `ch10/test_nba_select` | d | 問題なし | 4.6, 10.4.2 | 手続きは 1 つです。bit・part-select の NBA は別々の bit を扱い、`#1` 後に検査します。 | pass |
| `ch10/test_nba_verification` | d | 問題なし | 4.7, 10.4.2, 10.3.2 | 36 行は実際の競合です。t=3 の posedge に `i.d = 1;` と `always_ff @(posedge i.clk) i.q <= i.d;`（18 行）の sample が重なり、順序で q は 0/1 になります。しかしその edge は出力に影響しません。検査は 2 edge 後の t=7 で、その前の t=5 は t=3 に書いた d=1 を競合なく sample するため、i.q と local_q（assign、29 行）は両順序で 1 です。 | pass |
| `ch10/test_nonblocking_assign` | d | 問題なし | 4.6, 10.4.2 | 逐次プロセスは 1 つです。各 NBA は同じ時刻（111〜113 行のように旧値）または後の `@(posedge clk)` 後に読みます。 | pass |
| `ch10/test_nonblocking_assignment` | d | 問題なし | 10.4.2, 4.6 | 1 プロセスです。NBA の RHS は同じプロセス内の NBA 前の値を読み、検査は 1 posedge 後です。 | pass |
| `ch10/test_stmt_nonblocking` | d | 問題なし | 6.8, 9.4.2, 10.4.2, 4.6 | 走査では a〜d を 2 手続きが書きますが、initial（23 行）は t=0、`always @(posedge clk)` の初回は t=5 で、書き込みには非ゼロの時間差があります。 | pass |
| `ch11/test_bitwise_corner_cases` | bc | 問題なし | 11.4.8 (Tables 11-11 to 11-14) | 1 & x と 0 \| x は x、0 & x は 0 で、z は x と扱います。2 値の Verilator は x/z を持たず、その位置に既知値を出します。 | pass |
| `ch11/test_case_equality` | bc | 問題なし | 11.4.5 | === と !== は x・z も値として比較し、常に 0/1 を返します。2 値の Verilator は x/z を持たず、その位置に既知値を出します。 | pass |
| `ch11/test_case_equality_edge_cases` | b | 問題なし | 11.4.5 | x === x、z === z、同じ x/z bit の pattern は 1、x !== z も 1 です。2 値の Verilator は x/z を持たず、その位置に既知値を出します。 既知値だけの行も失敗します（原因は未確認）。 | pass |
| `ch11/test_comparison_corner_cases` | b | 問題なし | 11.4.4, 11.4.5 | === は x/z を厳密に比較し、x/z を含む ==・< は x です。2 値の Verilator は x/z を持たず、その位置に既知値を出します。 | pass |
| `ch11/test_comparison_ops` | bc | 問題なし | 11.4.4, 11.4.5 | Verilator が異なる 2 行は x 値の比較です。2 値の Verilator は x/z を持たず、その位置に既知値を出します。 | pass |
| `ch11/test_div_zero` | b | 問題なし | 11.4.3, 21.2.1.3 | 0 による除算・剰余は全 bit が x です。2 値の Verilator は x/z を持たず、その位置に既知値を出します。 | pass |
| `ch11/test_expression_edge_cases` | b | 問題なし | 11.4.3, 11.4.5 | 0 による除算・剰余は全 bit が x です。2 値の Verilator は x/z を持たず、その位置に既知値を出します。 | pass |
| `ch11/test_generate_bitselect_literal` | d | 問題なし | 6.20, 4.7 | 出力は ram_model の initial だけで、エラボレーション時の定数 localparam を検査して t=0 に `$finish` します。 | pass |
| `ch11/test_ieee1800_2023_arithmetic_edge` | b | 問題なし | 11.4.3, 11.6.1 (Table 11-21), 11.6.2, 5.7.1 | 0 除算は x です。INT_MIN / -1 は 32 ビットで計算して桁上がりを捨て、INT_MIN になります。2147483648 は符号込み 33 ビットが必要で負ではありません。2 値の Verilator 5.052 は INT_MIN / -1 にも 0 を返します。 | pass |
| `ch11/test_implication_edge_cases` | d | 問題なし | 11.4.7, 6.8 | initial が 1 つで event はありません。テキスト走査が見つけた `->` は、event trigger でなく含意演算子です。 | pass |
| `ch11/test_mixed_xz` | bc | 問題なし | 11.4.8, 11.4.3, 11.4.5, 11.4.9, 21.2.1.3 | x/z と既知 bit が混在すると、bitwise・算術・reduction の規則に従い bit ごとに x が生じます。2 値の Verilator は x/z を持たず、その位置に既知値を出します。 | pass |
| `ch11/test_operator_edge_cases_2` | b | 問題なし | 11.4.5 | 同じ x pattern の === は 1 です。2 値の Verilator は x/z を持たず、その位置に既知値を出します。 | pass |
| `ch11/test_operator_precedence` | b | 問題なし | 11.4.5 | 同じ x pattern の === は 1 です。2 値の Verilator は x/z を持たず、その位置に既知値を出します。 | pass |
| `ch11/test_partselect_xz` | bcd | 問題なし | 11.5.1, 21.2.1.3 | vector 外の part-select の bit は x を読みます。2 値の Verilator は x/z を持たず、その位置に既知値を出します。 既知値の行も seed で変わります。イベント順序は出力行へ影響しません。 | pass |
| `ch11/test_reduction_corner_cases` | bc | 問題なし | 11.4.9 (Tables 11-16 to 11-18) | 全 bit が x/z の reduction は x、既知の 1 は \|、既知の 0 は & の結果を決定します。2 値の Verilator は x/z を持たず、その位置に既知値を出します。 | pass |
| `ch11/test_reduction_xz_propagation` | bc | 問題なし | 11.4.9 | x bit を含み、結果を決める bit がない reduction は x です。2 値の Verilator は x/z を持たず、その位置に既知値を出します。 | pass |
| `ch11/test_slice_assign` | bc | 問題なし | 11.5.1 | part-select の読み書きは既知値で、1 つの x/z 書き込みも未変更 bit だけで検査します。Verilator 5.052 は wide_reg への書き込みを失います（原因は未確認）。 | pass |
| `ch11/test_streaming_nba_complex` | d | 問題なし | 9.4.2, 10.4.2, 11.4.14.3 | clk は t=0 の `initial clk = 0` と t=5 以後の always が書きます。x->0 は posedge でなく、streaming NBA は 10 単位後の次の posedge で検査します。 | pass |
| `ch11/test_ternary` | b | 問題なし | 11.4.11 (Table 11-20), 21.2.1.3 | x 条件では両 operand を bit ごとに結合し、同じ bit は保ち、違う bit は x になります。0xFF と 0xAA は XX と表示します。2 値の Verilator は x/z を持たず、その位置に既知値を出します。 | pass |
| `ch11/test_value_ops_edge_cases` | bc | 問題なし | 11.4.5, 11.4.8, 11.8.2 | 真理値表に従い、x と 1 が出会う bit の bitwise 結果は x です。2 値の Verilator は x/z を持たず、その位置に既知値を出します。 | pass |
| `ch11/test_x_propagation` | bc | 問題なし | 11.4.3, 11.4.8, 11.4.9, 11.4.11 | 出力の各 x 値は演算子の表に従います。2 値の Verilator は x/z を持たず、その位置に既知値を出します。 | pass |
| `ch11/test_x_z_propagation` | bc | 問題なし | 11.4.3, 11.4.5, 11.4.8, 11.4.11, 12.5.1 | x == x は x、=== は既知、演算子は z を x と扱い、casez の x は無視する bit ではありません。2 値の Verilator は x/z を持たず、その位置に既知値を出します。 | pass |
| `ch11/test_x_z_propagation_detailed` | bc | 問題なし | 11.4.3, 11.4.5, 11.4.8, 12.4 | 12 検査すべてが 4 値で成立します。2 値の Verilator は x/z を持たず、その位置に既知値を出します。 | pass |
| `ch11/test_z_propagation` | bc | 問題なし | 11.4.8, 11.4.10, 11.4.11, 11.5.1 | 演算子は z を x と扱い、shift は z bit をそのまま移します。2 値の Verilator は x/z を持たず、その位置に既知値を出します。 | pass |
| `ch12/test_case_statement` | c | 問題なし | 12.5, 12.5.1 | casex は式・項目の x/z bit を無視します。2 値の Verilator は x/z を持たず、その位置に既知値を出します。 | pass |
| `ch12/test_casex_casez_wildcard` | bc | 問題なし | 12.5.1 | casez 式の z は無視するため、10z1 は最初の項目 1001 に一致します。2 値の Verilator は x/z を持たず、その位置に既知値を出します。 | pass |
| `ch12/test_control_flow` | b | 問題なし | 12.4, 12.5, 6.11.2 | int は x/z を保持しないため、4'b10x1 を格納すると 9 になります。2 値の Verilator は x/z を持たず、その位置に既知値を出します。 既知値の行も失敗します（原因は未確認）。 | pass |
| `ch12/test_statement_case` | bc | 問題なし | 12.5, 12.5.1 | 4'b1?00 は 1z00 で、casez の規則に従い項目が一致します。2 値の Verilator は x/z を持たず、その位置に既知値を出します。 | pass |
| `ch12/test_stmt_casex` | c | 問題なし | 12.5, 12.5.1 | casex の x/z は無視しますが、11x0 と 0000 は bit 3 が異なるため一致しません。2 値の Verilator は x/z を持たず、その位置に既知値を出します。 | pass |
| `ch12/test_stmt_casez` | bc | 問題なし | 12.5.1 | casez 式の z bit、casex 式の x bit は無視します。2 値の Verilator は x/z を持たず、その位置に既知値を出します。 | pass |
| `ch15/test_event_named` | d | 問題なし | 15.5.2, 9.4.2, 4.6 | 5 つの待機側 initial は t=0 に `@(evt...)` へ達します。trigger（73 行の `-> evt1` など）は t>=1 なので、全待機者が待機済みです（15.5.2）。 | pass |
| `ch15/test_event_ordering` | d | 問題なし | 15.5.1, 9.4.2, 4.4.2.2, 4.4.2.3, 4.5 | `clk = 1;`（59 行）は P1 を Active に起こします。P1 の `-> ev1` は既に `@(ev1)` で待つ P2 を解放し（15.5.1）、ev2・ev3 へ続きます。全段階が t=5 の Active イベントです。 | pass |
| `ch15/test_event_triggered` | d | 問題なし | 15.5.3 | 1 プロセスが自分の event を trigger・検査します。`-> e1;`（32 行）後の e1.triggered はその時刻の間ずっと真です（15.5.3）。e2 は 51 行前が偽、後が真、e3 は一度も trigger しません。 | pass |
| `ch16/test_comb_stable_multiinstance` | d | 問題なし | 16.9.3, 16.5.1, 10.4.2, 9.4.2 | testbench は `@(posedge clk); #1;` 後の t=edge+1 だけで sig0・sig1 を変え、clock edge では変えません。 | unsupported |
| `ch20/test_bit_query_functions_2` | b | 問題なし | 20.9 | x/z bit があると $isunknown は 1 です。2 値の Verilator は x/z を持たず、その位置に既知値を出します。 | pass |
| `ch20/test_bitop_edge_cases` | b | 問題なし | 20.9 | $onehot と $onehot0 は 1 の bit だけを数え、x は 1 ではありません。2 値の Verilator は x/z を持たず、その位置に既知値を出します。 | pass |
| `ch20/test_bitop_functions` | b | 問題なし | 20.9 | x/z bit があると $isunknown は 1 です。2 値の Verilator は x/z を持たず、その位置に既知値を出します。 | pass |
| `ch20/test_bitop_functions_2` | bc | 問題なし | 20.9 | 各関数は直前に代入した値を読み、x/z で $isunknown は 1 です。2 値の Verilator は x/z を持たず、その位置に既知値を出します。 既知値の行も seed で変わります。 | pass |
| `ch20/test_bitop_system_tasks` | b | 問題なし | 20.9 | x/z bit があると $isunknown は 1 です。2 値の Verilator は x/z を持たず、その位置に既知値を出します。 | pass |
| `ch20/test_comb_sysfunc_edge` | b | 問題なし | 20.9 | x と z の両方で $isunknown は 1 です。2 値の Verilator は x/z を持たず、その位置に既知値を出します。 | pass |
| `ch20/test_comb_system_functions` | b | 問題なし | 20.9 | x bit があると $isunknown は 1 です。2 値の Verilator は x/z を持たず、その位置に既知値を出します。 | pass |
| `ch20/test_countones_2` | b | 問題なし | 20.9 | x/z bit があると $isunknown は 1 です。2 値の Verilator は x/z を持たず、その位置に既知値を出します。 | pass |
| `ch20/test_countones_bits` | b | 問題なし | 20.9 | $countbits は control bit のどれかと等しい bit を数え、4・4・8 です。Icarus 13.0 は最初の $countbits で内部 assertion により終了します（コード 134）。 | pass |
| `ch20/test_isunknown` | b | 問題なし | 20.9 | x/z bit があると $isunknown は 1 です。2 値の Verilator は x/z を持たず、その位置に既知値を出します。 | pass |
| `ch20/test_isunknown_2` | b | 問題なし | 20.9 | 1 bit でも x/z なら $isunknown は 1、既知値なら 0 です。2 値の Verilator は x/z を持たず、その位置に既知値を出します。 | pass |
| `ch20/test_isunknown_3` | b | 問題なし | 20.9 | 両値とも x bit を含むため、$isunknown は 1 です。2 値の Verilator は x/z を持たず、その位置に既知値を出します。 | pass |
| `ch20/test_isunknown_basic` | b | 問題なし | 20.9 | 全 x・一部 x の両方で $isunknown は 1 です。2 値の Verilator は x/z を持たず、その位置に既知値を出します。 | pass |
| `ch20/test_isunknown_func` | b | 問題なし | 20.9 | x/z bit で $isunknown は 1、既知値で 0 です。2 値の Verilator は x/z を持たず、その位置に既知値を出します。 | pass |
| `ch20/test_math_advanced` | b | 問題なし | 20.8.2, 6.12, 11.4.5 | 定義域外の数学関数は C と同じく NaN（20.8.2）を返し、C double と同じく自身と不等になります（6.12 と脚注）。Verilator 5.052 は nan を表示しても v != v を偽とします。4.3 を参照してください。 | pass |
| `ch20/test_math_edge_cases` | b | 問題なし | 20.8.1, 20.9 | x/z で $isunknown は 1、$clog2(0) は 0 です。2 値の Verilator は x/z を持たず、その位置に既知値を出します。 51 行のコメントは誤りです（第 5 節）。 | pass |
| `ch20/test_math_hyperbolic_special_values` | b | 問題なし | 20.8.2, 6.12, 11.4.5 | 定義域外の数学関数は C と同じく NaN（20.8.2）を返し、C double と同じく自身と不等になります（6.12 と脚注）。Verilator 5.052 は nan を表示しても v != v を偽とします。4.3 を参照してください。 | pass |
| `ch20/test_math_special_values` | b | 問題なし | 20.5, 20.8.2, 6.12, 11.4.5 | 変換は IEEE 754 単精度の bit pattern を返す必要があります（20.5）。Verilator 5.052 は $shortrealtobits・$bitstoshortreal で 0 や別の bit を返します。定義域外の数学関数は C と同じく NaN（20.8.2）を返し、C double と同じく自身と不等です（6.12 と脚注）。Verilator 5.052 は nan を表示しても v != v を偽とします。4.3 を参照してください。 | pass |
| `ch20/test_onehot_isunknown` | b | 問題なし | 20.9 | $onehot・$onehot0 は 1 bit を数え、xxxx0000・0000zzzz の $isunknown は 1 です。2 値の Verilator は x/z を持たず、その位置に既知値を出します。 既知値の $onehot も失敗します（原因は未確認）。 | pass |
| `ch20/test_printtimescale` | b | 問題なし | 20.4.2 | 規格は設計要素名を括弧で囲む行の書式を定めます。Verilator 5.052 は括弧を省きます（既存の差分一覧に記載済み）。 | pass |
| `ch20/test_real_bits_simple` | b | 問題なし | 20.5 | 変換は IEEE 754 単精度の bit pattern を返す必要があります（20.5）。Verilator 5.052 は $shortrealtobits・$bitstoshortreal で 0 や別の bit を返します。 | pass |
| `ch20/test_real_literal_type` | b | 問題なし | 20.5, 5.7.2 | 変換は IEEE 754 単精度の bit pattern を返す必要があります（20.5）。Verilator 5.052 は $shortrealtobits・$bitstoshortreal で 0 や別の bit を返します。 | pass |
| `ch20/test_realtobits_bitstoreal` | b | 問題なし | 20.5, 6.12, 11.4.5 | 変換は IEEE 754 単精度の bit pattern を返す必要があります（20.5）。Verilator 5.052 は $shortrealtobits・$bitstoshortreal で 0 や別の bit を返します。 | pass |
| `ch20/test_realtobits_corner_cases` | b | 問題なし | 20.5, 21.2.1.2 | 変換は IEEE 754 単精度の bit pattern を返す必要があります（20.5）。Verilator 5.052 は $shortrealtobits・$bitstoshortreal で 0 や別の bit を返します。 | pass |
| `ch20/test_shortreal_ieee754` | b | 問題なし | 20.5 | 変換は IEEE 754 単精度の bit pattern を返す必要があります（20.5）。Verilator 5.052 は $shortrealtobits・$bitstoshortreal で 0 や別の bit を返します。 | pass |
| `ch20/test_system_bitstoshortreal` | b | 問題なし | 20.5 | 変換は IEEE 754 単精度の bit pattern を返す必要があります（20.5）。Verilator 5.052 は $shortrealtobits・$bitstoshortreal で 0 や別の bit を返します。 | pass |
| `ch20/test_system_funcs_bits` | b | 問題なし | 20.9, 5.7.1 | literal の X 桁は x bit なので $isunknown は 1 です。2 値の Verilator は x/z を持たず、その位置に既知値を出します。 | pass |
| `ch20/test_system_function_calls` | b | 問題なし | 20.9 | x bit があると $isunknown は 1 です。2 値の Verilator は x/z を持たず、その位置に既知値を出します。 | pass |
| `ch20/test_system_functions_edge` | b | 問題なし | 20.9, 20.8.2, 6.12, 11.4.5 | x/z の $isunknown は 1 です。2 値の Verilator は x/z を持たず、その位置に既知値を出します。 定義域外の数学関数は C と同じく NaN（20.8.2）を返し、C double と同じく自身と不等です（6.12 と脚注）。Verilator 5.052 は nan を表示しても v != v を偽とします。4.3 を参照してください。 | pass |
| `ch20/test_system_isunknown` | b | 問題なし | 20.9 | x/z bit があると $isunknown は 1 です。2 値の Verilator は x/z を持たず、その位置に既知値を出します。 | pass |
| `ch20/test_system_shortrealtobits` | b | 問題なし | 20.5 | 変換は IEEE 754 単精度の bit pattern を返す必要があります（20.5）。Verilator 5.052 は $shortrealtobits・$bitstoshortreal で 0 や別の bit を返します。 | pass |
| `ch20/test_type_conversion` | b | 問題なし | 20.5 | 変換は IEEE 754 単精度の bit pattern を返す必要があります（20.5）。Verilator 5.052 は $shortrealtobits・$bitstoshortreal で 0 や別の bit を返します。 | pass |
| `ch21/test_display_format` | b | 問題なし | 21.2.1.2 | 値より広い field 幅の 16 進表示は左を 0 埋めし、8'hAB の %4h は 00ab です。Icarus 13.0 は空白で埋めます。:oracle: は spec のみです。 | pass |
| `ch21/test_format_advanced` | b | 問題なし | 21.2.1.1 (Table 21-2) | 8・16 行は %E・%G で real を表示します。real 書式は C と同じで、大文字 E を使います。kirimasim の SYSTEM_TASKS.md 3.6 は 21.2.1.1 をそのように解釈します。Icarus 13.0・kirimasim は E、Verilator 5.052 は e です。4.2 を参照してください。 | pass |
| `ch21/test_format_binary` | bc | 問題なし | 21.2.1.3 | %b は各 bit を 0/1/x/z で出します。2 値の Verilator は x/z を持たず、その位置に既知値を出します。 既知値の行も seed で変わります。 | pass |
| `ch21/test_format_float` | b | 問題なし | 21.2.1.1 (Table 21-2) | 64〜65・82〜83 行は test_format_advanced と同じく %E の結果を大文字 E と比較します。Verilator 5.052 は e を出します。4.2 を参照してください。 | pass |
| `ch21/test_format_octal` | bc | 問題なし | 21.2.1.3 | 8 進桁の 3 bit が全 x なら x、全 z なら z、混在なら X または Z です。2 値の Verilator は x/z を持たず、その位置に既知値を出します。 | pass |
| `ch21/test_monitor_strobe` | d | 問題なし | 21.2.2, 21.2.3, 4.4.2.9, 20.3.1, 20.4.3 | counter の書き込みは clock edge と重なりません。$display は Active、$monitor・$strobe は Postponed（4.4.2.9）で実行し、strobe の前に monitor を止めるため、Postponed の 2 行が重なりません。 | pass |
| `ch21/test_numeric_edge_cases` | b | 問題なし | 21.3.4.3 (Table 21-7) | $sscanf の %d は数字と underscore を読み、1_000_000 は 1000000 です。Verilator 5.052 は underscore で止まります。 | pass |
| `ch21/test_scanf_comprehensive` | b | 問題なし | 21.3.4.3 | field 幅は変換が読む文字数を制限し、%4d%4d は 1234・5678 を読みます。Verilator 5.052 は幅を無視します。 | pass |
| `ch21/test_scanf_functions` | b | 問題なし | 21.3.4.3 (Table 21-7), 21.2.1.3 | h field は x・X も桁として読み、0xCAFE の x も x 桁なので 000xcafe と表示します。2 値の Verilator は x/z を持たず、その位置に既知値を出します。 | pass |
| `ch23/test_config_block` | d | 問題なし | 9.4.2, 4.7 | `dut1.clk = 1'b1; dut2.clk = 1'b0;`（42〜43 行）が時刻 0 に always_ff を起こすかは未規定です（4.7）。ただし書く data は表示せず、表示する clock は `#1` 後に読みます。 | pass |
| `ch23/test_config_eval` | d | 問題なし | 9.4.2, 10.4.2, 4.9.6 | t=0（33 行）の clk の x->0 は negedge であり、always_ff と待機 initial は開始順によらず posedge を見ません。 | pass |
| `ch23/test_inst_array_edge_cases` | bd | 問題なし | 6.8 (Table 6-7) | q は初期値のない logic vector で、書くまで x です。2 値の Verilator は x/z を持たず、その位置に既知値を出します。 イベント順序は出力行へ影響しません。 | pass |
| `ch23/test_instantiate_edge_cases` | d | 問題なし | 4.7, 10.4.2, 10.3.2 | 132 行には実際の競合があります。t=25 の posedge に `data_in = 8'hAA;` と always_ff（21〜23 行）の sample が重なり、data_out は 55/AA です。しかし 135 行の検査は t=40、競合なく AA を sample した t=35 後なので、Chain = aa で固定です。他の入力書き込みは t=2、negedge、edge 以外で行い、各読み取りは次の negedge か `#1` を待ちます。 | pass |
| `ch23/test_module_parameter_corner` | d | 問題なし | 4.7, 10.4.2, 23.3.3.2, 4.5 | 各 task は起こされた posedge で dataN_in を blocking 代入するので、最初の sample は旧・新どちらの値も取り得ます（4.7）。検査は 2 edge 待ち、2 つ目は安定した入力を sample します。 | pass |
| `ch23/test_module_ports` | d | 問題なし | 4.7, 9.4.2, 10.4.2, 4.9.6 | 時刻 0 に競合があります。`reset = 1`（40 行）は x->1 の posedge で、94 行の always は既に待機していれば捕捉します（4.7）。どちらでも t=10（47 行）の clk posedge は t=5 からの reset=0 により data_out を 0 にし、検査は t=15 です。その後は入力を edge の少なくとも 5 単位前に変更し、検査は 5 単位後です。 | pass |
| `ch23/test_port_edge_cases` | b | 問題なし | 6.6.1, 23.3.3.3 | net の唯一のドライバが z なので net は z です。2 値の Verilator は x/z を持たず、その位置に既知値を出します。 | pass |
| `ch23/test_port_output` | d | 問題なし | 4.9.1, 4.9.6, 9.4.2, 10.4.2 | 各組合せ検査は入力変更の `#1` 後に出力を読みます。 | pass |
| `ch23/test_x_propagation_child` | bc | 問題なし | 23.3.3.3, 6.8 | 未接続の input net は z で、その先の logic は x です。2 値の Verilator は x/z を持たず、その位置に既知値を出します。 | pass |
| `ch25/test_interface_basic_2` | d | 問題なし | 9.4.2, 10.4.2, 4.7 | main initial が bus.clk の posedge 自体を作って `#5` 待ちます。sample する data は別時刻に書き、NBA の結果は 5 単位後に読みます。 | pass |
| `ch25/test_interface_modport_corner` | d | 問題なし | 4.5, 4.4.2.4, 10.4.2, 9.4.2, 23.3.3 | reset は t=25（164 行）の `rst_n <= 1` で解除します。その edge に `assign bus_if.clk = clk` 経由で起きる master・slave の always は、NBA より先の Active なので rst_n=0 を読みます（4.5）。 | unsupported |
| `ch25/test_interface_parameter_corner` | d | 問題なし | 4.4.2.2, 4.4.2.4, 4.5, 4.9.1, 10.4.2 | testbench は全 bus 信号を NBA で駆動します。posedge で Active の `assign busN.clk = clk` に起こされるメモリは、プロセス順によらずその時刻の NBA 前の値を sample します。 | pass |
| `ch25/test_modport_edge_cases_2` | d | 問題なし | 9.4.2, 10.4.2, 4.4.2.3, 4.6 | testbench が blocking の `intf.clk = 1` で全 edge を作り、次の読み書きまで `#1` を待ちます。 | pass |
| `ch27/test_generate` | d | 問題なし | 4.4.2.2, 4.9.1, 9.2.1 | 生成された 4 つの initial は、t=0 に data の別々の要素を書きます。 | pass |
| `ch27/test_generate_cond` | d | 問題なし | 27.5 | 走査は `initial use_narrow = ...` を 3 つ見つけますが、1 つの if-generate の選択肢であり、DATA_WIDTH=32 では medium_config（76 行）だけを展開します。 | pass |
| `ch27/test_generate_cond_2` | d | 問題なし | 27.5, 4.7 | MODE=1 では gen_mode1 だけを生成し、data の書き手は t=0 の `initial data = 16'h1111` だけです。 | pass |
| `ch27/test_generate_corner_2` | d | 問題なし | 4.7, 10.4.2, 4.9.1 | 77・91・147 行の stage_in への blocking 書き込みは stage-0 always が sample する posedge 直後です（4.7）。ただし検査前の次の posedge まで stage_in は変わりません。 | pass |
| `ch27/test_generate_edge_cases` | d | 問題なし | 27.4, 27.5, 6.8 | 複数の書き手は if-generate・case-generate の分岐で、各 1 つだけを展開します。生成した initial は別々の要素を書き、全読み取りは `#1` 後です。 | pass |
| `ch27/test_generate_if` | d | 問題なし | 4.7, 10.4.2, 10.3.2 | 89〜90 行は実際の競合です。t=25 の posedge に `a = 50; b = 100;` と always_ff（24〜26 行）の sample が重なります。順序に依存する a_reg/b_reg は以後表示しません。t=25 の sum は NBA 前の値（10・20）を使い、どちらでも 30 です。sum_pipe は t=25 の Active で読み、t=15 の NBA の 30 を保持します。 | pass |
| `ch29/test_udp_seq` | b | 問題なし | 29.5, 29.6, 29.7 | sequential UDP は初期値から表に従います。Verilator 5.052 は edge-sensitive UDP に別の出力を返します（原因は未確認）。 | pass |
| `ch4/test_array_element_noop_nba_keeps_override_order` | d | 問題なし | 4.6, 10.4.2 | 1 プロセスが `arr[0] <= 8'h5a; arr[0] <= arr[1];`（40〜41 行）を発行します。4.6 b) と 10.4.2 は実行順の適用を要求し、arr[0] は 34 です。 | pass |
| `ch4/test_delta_cycle` | bd | 問題なし | 4.4.2.3, 4.5, 4.9.1, 10.4.2 | #0 は Active が空になってから再開するため（4.4.2.3、4.5）、先に起こされた連続代入・always_comb は実行済みです。Verilator 5.052 は古い値を示します。イベント順序は出力行へ影響しません。 | pass |
| `ch4/test_delta_cycles` | d | 問題なし | 4.4.2.3, 4.6, 20.3.1, 20.4.3 | 手続きが 1 つで他プロセスはなく、`#0` が順序付ける相手はいません。 | pass |
| `ch4/test_delta_scheduler` | d | 問題なし | 9.2.2.2, 4.4.2.3, 10.4.2 | 値の検査（38・74 行）は a 変更の `#1` 後で、always_comb の連鎖は安定済みです。always_comb は時刻 0 にも 1 回走るので開始順序も影響しません（9.2.2.2）。 | pass |
| `ch4/test_edge_event_regions` | d | 問題なし | 4.4.2.3, 4.4.2.4, 4.5, 21.2.2 | 1 プロセスです。`a <= 1`（23 行）の後の `#0`（32 行）はプロセスを Inactive に置きます。 | pass |
| `ch4/test_event_delta_cycles` | bcd | 未規定 | 4.7, 9.4.2 | always @(a or b or c) の起動回数を数えます。initial は時間制御なしで a/b/c の 2〜3 個を続けて変えますが、4.7 は文の間での中断を認めるため、変更ごとに起き得ます。期待回数はそこで中断しない場合の値です。4.1 を参照してください。 | pass |
| `ch4/test_ieee1800_2023_delta_convergence` | bcd | 問題なし | 4.4.2.3, 4.5, 4.9.1 | #0 は Active が空になってから再開するため（4.4.2.3、4.5）、先に起こされた連続代入・always_comb は実行済みです。Verilator 5.052 は古い値を示し、seed でも変わります。イベント順序は出力行へ影響しません。 | pass |
| `ch4/test_nba_after_zero_delay` | bcd | 問題なし | 6.8 (Table 6-7), 4.4.2.3, 10.4.2 | 変数は初期値がなく x で、#0 後も NBA は未適用です。2 値の Verilator は x/z を持たず、その位置に既知値を出します。 イベント順序は出力行へ影響しません。 | pass |
| `ch4/test_nba_cross_module` | d | 問題なし | 4.5, 10.4.2, 23.3.3.2, 9.4.2 | 3 つの `@(posedge clk)` は各 edge に順序未規定で起きますが、共有状態はすべて NBA で書くため、98 行の表示は常に edge 前の値です（10.4.2）。 | pass |
| `ch4/test_nba_cross_module_timing` | d | 問題なし | 4.4.2.2, 4.4.2.4, 4.9.6, 10.4.2, 9.4.2 | 両 always は同じ posedge に起きますが、相互に渡す値はすべて NBA で書くため、どちらの順でも前の edge の値を読みます（10.4.2）。 | pass |
| `ch4/test_nba_port_propagation` | d | 問題なし | 6.8, 10.4.2, 4.9.6, 4.9.1 | 走査では valid を 2 手続きが書きますが、initial は t=0、always の最初の NBA は t=5 で、時刻が異なります。 | pass |
| `ch5/test_attrib_module` | b | 問題なし | 6.8 (Table 6-7) | sub.data は書かれない logic 変数なので x です。2 値の Verilator は x/z を持たず、その位置に既知値を出します。 | pass |
| `ch5/test_ieee1800_2023_literal_edge` | b | 問題なし | 5.7.1, 6.16 | x/z literal はその bit を保ち、途中の \0 は除かれるため文字列の長さは 10 です。2 値の Verilator は x/z を持たず、その位置に既知値を出します。 182 行のコメントは誤りです（第 5 節）。 | pass |
| `ch5/test_lexical_numbers` | b | 問題なし | 5.7.1, 20.9 | 8'b1010_xxxx は x bit を持つため $isunknown は 1 です。2 値の Verilator は x/z を持たず、その位置に既知値を出します。 | pass |
| `ch6/test_core_types` | b | 問題なし | 6.11.2, 20.9 | logic 変数は 1'bx を保つため $isunknown は 1 です。2 値の Verilator は x/z を持たず、その位置に既知値を出します。 | pass |
| `ch6/test_edge_wor_wand` | bd | 問題なし | 6.6.3 (Tables 6-3, 6-4), 4.9.1 | wand・triand はドライバの AND、wor・trior は OR を取ります（Table 6-3・6-4）。Verilator 5.052 は 1 ドライバの値を出します。イベント順序は出力行へ影響しません。 | pass |
| `ch6/test_generate_string_localparam` | d | 問題なし | 9.2.1, 4.6 | NAME は localparam であり、手続きが書く変数ではなく定数です。 | pass |
| `ch6/test_ieee_compliance_dimensions_twostate` | bc | 問題なし | 6.11.2, 6.24.1 | x/z bit の 2 値型への代入・cast は 0 になります。Verilator 5.052 は --x-initial unique でランダムな bit にします。 | pass |
| `ch6/test_int_types_edge_cases` | b | 問題なし | 5.7.1 | 1000000000000 は 32 ビット超なので、literal は符号 bit を含め値に必要な幅を持ちます。Verilator 5.052 は負数にします。 | pass |
| `ch6/test_integer_edge_cases` | bc | 問題なし | 6.8 (Table 6-7), 6.11 | integer の初期値は x です。2 値の Verilator は x/z を持たず、その位置に既知値を出します。 | pass |
| `ch6/test_integer_types` | bc | 問題なし | 6.11.2, 20.9 | logic・integer は x を保つため $isunknown は 1 です。2 値の Verilator は x/z を持たず、その位置に既知値を出します。 | pass |
| `ch6/test_net_logic` | b | 問題なし | 6.3.1, 6.11.2 | 4 値 vector は 2'b01 と 2'bxz を保ちます。2 値の Verilator は x/z を持たず、その位置に既知値を出します。 | pass |
| `ch6/test_net_resolution` | b | 問題なし | 6.6.1 (Table 6-2), 6.6.3 (Tables 6-3, 6-4) | wand・triand はドライバの AND、wor・trior は OR を取ります（Table 6-3・6-4）。Verilator 5.052 は 1 ドライバの値を出します。ドライバが z の wire は z です。 | pass |
| `ch6/test_net_resolution_multi` | b | 問題なし | 6.6.1 (Table 6-2), 6.6.3, 28.10 | wire の strong 1 と strong 0 の 2 ドライバは x になります。2 値の Verilator は x/z を持たず、その位置に既知値を出します。 | pass |
| `ch6/test_net_triand` | b | 問題なし | 6.6.3 (Table 6-3) | wand・triand はドライバの AND、wor・trior は OR を取ります（Table 6-3・6-4）。Verilator 5.052 は 1 ドライバの値を出します。 | pass |
| `ch6/test_net_trior` | b | 問題なし | 6.6.3 (Table 6-4) | wand・triand はドライバの AND、wor・trior は OR を取ります（Table 6-3・6-4）。Verilator 5.052 は 1 ドライバの値を出します。 | pass |
| `ch6/test_net_types_resolution` | b | 問題なし | 6.6.3 (Tables 6-3, 6-4) | wand・triand はドライバの AND、wor・trior は OR を取ります（Table 6-3・6-4）。Verilator 5.052 は 1 ドライバの値を出します。 | pass |
| `ch6/test_net_wand` | b | 問題なし | 6.6.3 (Table 6-3) | wand・triand はドライバの AND、wor・trior は OR を取ります（Table 6-3・6-4）。Verilator 5.052 は 1 ドライバの値を出します。 | pass |
| `ch6/test_net_wor` | b | 問題なし | 6.6.3 (Table 6-4) | wand・triand はドライバの AND、wor・trior は OR を取ります（Table 6-3・6-4）。Verilator 5.052 は 1 ドライバの値を出します。 | pass |
| `ch6/test_parameter_edge_cases` | d | 問題なし | 4.7, 9.4.2, 10.4.2 | 実際の競合はありますが結果を表示しません。複数の din* 書き込みが posedge に重なるため（例：146 行、t=25 の `din16 = 16'h0000`）、always_ff は旧値・新値を取り得ます。表示する dout* は書き込み後 2 つ目の posedge の Active で読み、入力が安定していた書き込み後最初の posedge の NBA を保持します。134 行の `din8 = 8'h00` は既存値なので変更を起こしません。 | pass |
| `ch6/test_reg_edge_cases` | bc | 問題なし | 6.8 (Table 6-7), 6.11 | reg は x で始まり、x/z を保持します。2 値の Verilator は x/z を持たず、その位置に既知値を出します。 | pass |
| `ch6/test_string_atoi` | b | 問題なし | 6.16.9 | atoi は先頭の数字・underscore を読み、符号は読まないため、- または空白で始まる string は 0 です。Verilator 5.052 は C の atoi のように読みます。 | pass |
| `ch6/test_string_atoi_2` | b | 問題なし | 6.16.9 | atoi は先頭の数字・underscore を読み、符号は読まないため、'-9876' と '   42' は 0 です。Verilator 5.052 は C の atoi のように読みます。 | pass |
| `ch6/test_string_hextoa` | b | 問題なし | 6.16.12, 6.16.9 | hextoa は符号を読まない atohex の逆なので、32'hDEADBEEF は deadbeef です（テストは大小どちらも受理）。Icarus 13.0 は -21524111 です。 | pass |
| `ch6/test_string_methods_corner` | a | 問題なし | 6.16 | テキスト走査だけで抽出しました。X は文字列の文字（putc は XBCDE）で、x/z 値は関与しません。 | pass |
| `ch6/test_string_methods_extended` | a | 問題なし | 6.16 | テキスト走査だけで抽出しました。X は文字列の文字（AXC）で、x/z 値は関与しません。 | pass |
| `ch6/test_tri_edge_cases` | b | 問題なし | 6.6.1, 6.6.5 | 唯一のドライバが z の tri net は z です。2 値の Verilator は x/z を持たず、その位置に既知値を出します。 | pass |
| `ch6/test_value_logic` | bc | 問題なし | 6.3.1 | logic は 1'bx・1'bz を保つため、=== の検査は成立します。2 値の Verilator は x/z を持たず、その位置に既知値を出します。 | pass |
| `ch6/test_wire_edge_cases` | b | 問題なし | 6.6.1 (Table 6-2) | z の 2 ドライバは z です。2 値の Verilator は x/z を持たず、その位置に既知値を出します。 | pass |
| `ch6/test_x_value_bitwise` | bc | 問題なし | 11.4.8 (Tables 11-11, 11-12) | 4'bxx10 & 4'b1001 は x000 で、他の行も同じ表に従います。2 値の Verilator は x/z を持たず、その位置に既知値を出します。 | pass |
| `ch7/test_array_nba_part_select_index` | d | 問題なし | 4.7, 9.4.2, 10.4.2, 4.5 | 2 か所の順序が未規定ですが出力に影響しません。時刻 0 の `rst_n = 0`（61 行、x->0 の negedge）は開始順により always_ff が捕捉するか変わります（4.7）。どちらも t=5 の posedge に rst_n=0 でポインタを reset します。また t=15 の `rst_n = 1`（66 行）は always_ff が sample する posedge に blocking で書きます。 | pass |
| `ch7/test_array_reduction` | a | 問題なし | 7.12.3 | テキスト走査だけで抽出しました。x は sum(x * 2) の反復子名で、x/z 値は関与しません。 | pass |
| `ch7/test_assignment_forms` | d | 問題なし | 4.7, 10.4.2, 10.6.2, 4.6 | `ff_in = 8'h77`（218 行）は always_ff が sample する posedge に書きます（4.7）。検査は 2 posedge 後なので ff_out はどちらも 77 です。他の検査は posedge または `#1` 後です。 | pass |
| `ch9/test_always_blocks` | d | 問題なし | 6.8, 9.4.2, 9.2.2.2, 10.4.2 | 宣言初期化はプロセス開始前（6.8）なので時刻 0 の edge はなく、45 行は t=1 に counter=0 を出します。 | pass |
| `ch9/test_always_comb_cascade_propagation` | bd | 問題なし | 9.2.2.2, 4.4.2.3, 4.5 | #0 は Active が空になってから再開するため（4.4.2.3、4.5）、先に起こされた連続代入・always_comb は実行済みです。Verilator 5.052 は古い値を示します。イベント順序は出力行へ影響しません。 | pass |
| `ch9/test_always_comb_latch` | bcd | 問題なし | 6.8 (Table 6-7), 9.2.2.3, 12.4 | 検査前に latch 条件は真にならず、latch_out は初期値 x を保ちます。2 値の Verilator は x/z を持たず、その位置に既知値を出します。 イベント順序は出力行へ影響しません。 | pass |
| `ch9/test_always_comb_latch_ff` | d | 問題なし | 9.2.2.2.2, 9.4.2, 10.4.2, 4.6 | rst_n は negedge（77 行、t=20）に解除し、その後の 5 posedge（25..65）が counter を増やすため、t=70 の negedge で 5 を読みます。 | pass |
| `ch9/test_always_ff_async` | d | 問題なし | 4.7, 9.4.2, 10.4.2 | t=0 の clk x->0、rst_n/set_n x->1 は always_ff のどの edge list にも一致せず、開始順は影響しません。reset は t=1 に適用します。 | pass |
| `ch9/test_always_ff_corner` | bd | 問題なし | 9.2.2.4, 6.8 | en=0 の間 q_enable は書かれず、まだ x です。2 値の Verilator は x/z を持たず、その位置に既知値を出します。 イベント順序は出力行へ影響しません。 | pass |
| `ch9/test_always_ff_edge_cases` | d | 問題なし | 4.6, 9.4.2, 10.4.2 | clk・rst_n の edge を作るのは initial だけです。各テストで rst_n・d・load を、edge を作る文より前に設定します。例：`rst_n = 1; d = 8'hAA; clk = 1;`（65〜67 行）。 | pass |
| `ch9/test_always_ff_simple` | d | 問題なし | 10.4.2, 4.6 | data_in は各 `clk = 1` の 1 時間単位前に変わり、data_out は 2 単位後に読みます。 | pass |
| `ch9/test_always_latch_edge_cases` | bc | 問題なし | 9.2.2.3, 6.8 | en=0 の間 q は代入されず x を保ちます。2 値の Verilator は x/z を持たず、その位置に既知値を出します。 | pass |
| `ch9/test_assign_delay_edge_cases` | d | 問題なし | 9.4.1, 9.4.5, 9.4.2, 4.6 | data・t1・t2 を読み書きするのは initial だけです。clock プロセスは clk を反転するだけです。 | pass |
| `ch9/test_clocked_value_timing` | d | 問題なし | 10.4.2, 4.4.2.4, 9.4.2, 20.4.3 | value は edge の `#1` 後（t=6・36）、その NBA 領域の実行後に読むため、1・4 です。 | pass |
| `ch9/test_disable_wait` | d | 問題なし | 4.6, 9.6.2, 9.4.3 | 1 プロセスです。名前付き event done_event は宣言するだけで trigger も wait もしません。 | pass |
| `ch9/test_edge_detection` | d | 問題なし | 6.8, 9.4.2, 10.4.2 | counter は edge-triggered always だけが書き、edge の `#1` 後（t=1・6・11・56・66）に読みます。 | pass |
| `ch9/test_event_ctrl` | d | 問題なし | 9.4.2, 4.7 | 同時刻の組は t=0 の `clk = 0`（21 行、x->0）と main initial の `@(posedge clk)` 到達（30 行）だけです。x->0 は posedge でなく、両順序で最初の起動は t=5 です。 | pass |
| `ch9/test_implicit_event` | b | 問題なし | 9.4.2.2, 9.4.2 | block は trigger_count を読み書きしますが、動作中は待機していないため自身の書き込みで起きません。Verilator 5.052 は DIDNOTCONVERGE で止まります。 | pass |
| `ch9/test_initial_block` | d | 問題なし | 6.8, 4.5, 4.7 | 書き手は t=0（v1）・2（v2）・3（v3）、読み手は t=1・4 なので、別プロセスの読み書きが同じ時刻に重なりません。 | pass |
| `ch9/test_initial_edge_cases` | d | 問題なし | 9.2.1, 6.8, 6.21, 4.6 | 走査は a を複数手続きが書くと判定しました。3 つの小さな initial が t=0 に a/b/c を書き、main initial は `#1` 後（40 行）に読みます。 | pass |
| `ch9/test_initial_final` | d | 問題なし | 4.7, 9.2.1, 9.2.3 | t=0 の 2 initial は異なる変数を書くため順序は影響せず、main initial は `#1` 後（45 行）に読みます。 | pass |
| `ch9/test_multi_sensitivity_list` | d | 問題なし | 6.8, 4.7, 9.4.2, 10.4.2 | 宣言初期化（17〜53 行）は手続き開始前でイベントを生じないため（6.8）、Test 1 は q=0・counter=0 を読みます。 | pass |
| `ch9/test_scope_nba` | d | 問題なし | 6.8, 10.4.2, 4.6 | 1 プロセスです。block スコープの static 変数は時刻 0 前に初期化します（6.8）。 | pass |
| `ch9/test_sens_level` | d | 未規定 | 4.7, 9.4.2 | 108 行の a = 0; b = 1; は時間制御なしで、両方を always @(a or b or c) が読みます。4.7 により initial が a = 0 後に中断し、always が走って再待機前に中断することもあり、b = 1 は何も起こさず out3 は 00 のままです。単なる開始順の選択でなく 2 回の中断が必要です。4.1 を参照してください。 | pass |
| `ch9/test_sens_multi` | d | 問題なし | 9.4.2, 4.7, 10.4.2 | t=0 の手続き開始順は未規定ですが、初回の書き込みは rst_n の x->1 と clk・sig1〜sig3 の x->0 で、edge list に一致しません。comb_out は後の 101 行の変更で a/b/c 全体から再計算します。 | pass |
| `ch9/test_sensitivity_edge_cases` | bd | 問題なし | 9.2.2.2, 4.4.2.3, 4.5 | a = 1; b = 2; #0; a = 3; b = 4; の後、always_comb は再実行して 7 になります。#0 が initial を再開する前に実行・再待機済みだからです。Verilator 5.052 は 3 です。イベント順序は出力行へ影響しません。 | pass |
| `ch9/test_sequential_logic` | d | 問題なし | 9.4.2, 10.4.2 | initial は rst_n・data_in を negedge（54 行 t=10、77 行 t=50、89 行 t=60）だけで変え、検査も negedge です。 | pass |
| `ch9/test_stmt_disable_2` | d | 問題なし | 9.6.2, 4.6 | 手続きは 1 つです。名前付き event done_event（17 行）は宣言するだけで trigger も wait もしません。 | pass |
| `ch9/test_timing_edge_cases` | d | 問題なし | 4.7, 9.4.1, 9.4.5 | main block だけが data を読み書きし、`#0`（33・44〜46 行）と代入内遅延（70 行）は単純な逐次動作です。 | pass |
