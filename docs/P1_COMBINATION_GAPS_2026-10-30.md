# P1「組合せの不足」の具体例（2026-10-30）

対象：main `51cda3143aed9db377b4ea570f6b8ec0bf8bc513`。
前の残作業説明で挙げた「8. 組合せの不足」を具体化した調査記録です。
クラス、VPI、specify、FST、`%p`／`%v`などの独立した大項目は別に扱います。
仕様・既存期待値・P1の合格条件は変更しません。

**同日追記：** 以下は`51cda314`での測定記録です。その後、E01の
[automatic構造体のevent](notes/automatic-struct-events.ja.md)、A03の
[固定配列メンバー](notes/automatic-struct-array-members.ja.md)、N02の
[固定配列ref経由event-NBA](notes/event-nba-ref-arrays.ja.md)、N03の
[packed親範囲の保持](notes/event-nba-ref-windows.ja.md)、N01の
[スカラーref経由の遅延NBA](notes/delayed-ref-nba.ja.md)と
[固定配列ref・packed選択への遅延NBA](notes/delayed-ref-selections.ja.md)を実装しました。
さらにN04の文字列の文字へのNBA（[SCHEDULER_SPEC 13](spec/SCHEDULER_SPEC.ja.md)）と、
A01のunpacked構造体のref引数（[SIR_SPEC 8](spec/SIR_SPEC.ja.md)。メンバごとの参照）を実装しました。
それぞれに残る制限はリンク先を参照してください。本文と測定JSONの過去の結果は保持します。

## 1. 「組合せの不足」とは

個々には実行できる機能でも、**型・記憶域の寿命・引数の渡し方・予約する更新・実行領域を
組み合わせると、変換や保持の処理がつながっていない**場合があります。

例えば、automaticなスカラーeventと、staticな構造体のeventメンバーは動きます。
一方、automaticな構造体にeventメンバーを置くと未対応になります。
同様に、通常の遅延NBAとref引数はそれぞれ使えますが、`ref static`を介した遅延NBAは未対応です。

以下の分類は作業を見渡すためのものです。同じ原因で複数の形が止まることもあるため、
項目数をそのまま必要なPR数・実装工数・P1必須機能数には換算しません。

## 2. 今回の確認範囲

現在のソースからビルドした`kirimasim 0.0.0 (commit 51cda3143aed)`を使い、
独自に作成した34入力を`parse`と`--dump-sir`で確認しました。
SIR生成に成功した6入力は通常実行も確認しました。

- **25入力：実行用SIRへの変換が未対応。** `parse`は0、`--dump-sir`は2で`unsupported`を報告。
- **5入力：正常に動く対照例。** event、通常の遅延NBA、固定したpacked親、queueのmonitorなど。
- **1入力：波形スコープの不足。** 実行は成功するものの、program／interfaceの信号がVCDに出ない。
- **3入力：フロントエンド段階で拒否。** ネイティブ実装の不足の件数には含めません。

全入力のソース、終了コード、診断、対照例の標準出力は
[測定JSON](measurements/2026-10-30/p1-combination-gaps.json)に保存しています。
本文の`probe`は、そのJSONの`id`です。コード断片の完全なソースも同じJSONで確認できます。

**今回、Icarus 13.0との比較は実施していません。** `parse`成功だけでは、個々の形の規格適合性や
P1必須性を独立に証明できません。以下は現在の実装境界の記録であり、25入力すべてを
新たなP1必須条件とするものではありません。

## 3. event・イベント制御・プロセスの組合せ

### E01：automaticな構造体 × eventメンバー

動く形はautomaticな単独eventと、staticな構造体のeventメンバーです。
不足している形は次の構造体をautomatic task内で作る場合です。

```systemverilog
module top;
  typedef struct { event go; int n; } item_t;
  task automatic check_value();
    item_t item;
    -> item.go;
  endtask
  initial check_value();
endmodule
```

`probe: event_struct_auto`は`unsupported: automatic event variable`。
`event_struct_static_control`と`event_scalar_auto_control`はともに`1`を出力して正常終了しました。

必要な接続は、構造体のevent leafをautomaticフレームの初期化・型付きroot・captureへ通すことです。
呼出しごとのeventの独立性、forkや待機をまたぐ保持、終了後の回収まで確認する必要があります。
根拠：[構造体フレームの変換](../src/front/lower_struct.cpp)、
[automatic captureの契約](notes/automatic-capture-proposal.md)。

### E02：event値 × 関数戻り値／条件式からの初期化

名前付きeventを直接待つ・triggerする形、対応済みの宣言初期化コピーとinput引数は動きます。
一方、次のようにeventを計算した式からコピーする経路が不足しています。

```systemverilog
function automatic event identity(input event e);
  return e;
endfunction
// event source_eventがある文脈で：
// static event copied = identity(source_event);
// static event selected = choose ? first_event : second_event;
```

`probe: event_return, event_conditional`はともにparseを通過し、
`unsupported: assignment to this kind of target`で停止しました。
関数呼出し・条件式のevent値をコピーする途中で止まるため、今回の結果だけで
関数本体の戻り値処理まで到達したとは扱いません。

関数戻り値の転送、eventの同一性、式の一度だけの評価、GC rootを接続する必要があります。
条件式の未知条件時の扱いなど、既存の限定されたevent契約で未決の意味は実装前の確認事項です。
根拠：[eventコピー処理](../src/front/lower_sub.cpp)、
[式の変換](../src/front/lower_expr.cpp)、[event転送](notes/event-transfers.md)。

### E03：`always_ff` × 計算したエッジ式

通常の信号エッジと、一般のevent controlにおける式の監視には対応しています。
`always_ff @(posedge (a ^ b))`では、式を使う形との接続が不足しています。

`probe: always_ff_expression`は
`unsupported: always_ff with an event control on an expression`。
常に同じevent controlを維持し、式の変化・エッジ・スケジュール領域を検査する必要があります。
根拠：[手続き・感度の変換](../src/front/lower.cpp)、[event control](../src/front/lower_stmt.cpp)。

### E04：`iff`ガード × 変数を書き換える関数

値を読むガードは動きます。ガードで呼ぶ関数が変数を書き換える形は未対応です。

```systemverilog
function automatic bit bump();
  count++;
  return 1;
endfunction
// @(posedge clk iff bump());
```

`probe: guard_writes_value`は`unsupported: iff condition that writes a variable or net`。
評価回数と時点、書き換えによる再通知・再入を決めて検証する必要があります。
これは後述する「ガードへ直接`count++`を書くとSlangが拒否する形」と区別します。
根拠：[ガードの変換](../src/front/lower_stmt.cpp)。

### E05：非ブロッキングevent trigger × event control

通常の`->`、`->>`、遅延付きの対応済みtriggerと、event-NBAは動きます。
`->> @(posedge clk) go;`を使う形は別の未対応経路です。

`probe: event_trigger_control`は`unsupported: ->> with an event control`。
制御への登録、event値の保持、解除・プロセス終了時の清掃まで接続する必要があります。
根拠：[event triggerの変換](../src/front/lower_stmt.cpp)。

### E06：名前付きfork × 名前を指定したdisable

`fork`／`join`と、対応済みの`disable fork`は動きます。
`fork : workers ... join`を`disable workers;`で停止する形は未対応です。

`probe: named_fork_disable`は`unsupported: disable of this block`。
forkの実行単位と名前付きスコープを結び、対象の子プロセス・待機・保留更新を正しく終了させる
必要があります。単なる逐次ブロックのdisableと同じ扱いにはできません。
根拠：[disable処理](../src/front/lower_stmt.cpp)、[capture処理](../src/front/lower_capture.cpp)。

## 4. 予約する代入 × ref・選択対象

### N01：`ref static`引数 × 遅延NBA

通常のstatic変数への`value <= #1 5;`は動き、今回の対照例でも`5`を確認しました。
この対照例のIDは`nba_slot_delay_control`です。
同じstatic変数をref引数経由で予約する形は不足しています。

```systemverilog
module top;
  int value;
  task automatic put(ref static int r);
    r <= #1 5;
  endtask
  initial begin put(value); #2; end
endmodule
```

`probe: nba_ref_delay`は`unsupported: delayed nonblocking assignment through a ref`。
予約時の値・時刻・実際のstatic対象を保持し、taskから戻った後も正しい宛先へ書く接続が必要です。
automatic変数へのNBAの禁止を緩める変更ではありません。
根拠：[refへの書き込み](../src/front/lower_expr.cpp)、[SIRの参照契約](spec/SIR_SPEC.md)。

### N02：event-NBA × ref固定配列の要素

直接のstatic配列要素へのevent-NBAは対応済みです。
`ref static int r[2]`を介した`r[1] <= @(posedge clk) 5;`は未対応です。

`probe: nba_ref_event_array`は`unsupported: event-NBA to an array element through a ref`。
refが指す配列と予約時の添字を、event待機中も保持する必要があります。

### N03：event-NBA × ref先の入れ子packed選択

`ref static logic [1:0][7:0] r`を介し、`r[0][i] <= @(posedge clk) 1;`とする形です。
単純なref先と、対応済みのbit選択だけでは、この親の範囲を持つ選択を表せません。

`probe: nba_ref_event_window`は
`unsupported: event-NBA to a nested packed selection through a ref`。
親の幅・位置・境界と子の位置を捕捉し、書いてよいビットだけを予約する必要があります。
N02・N03の根拠：[event-NBAの対象捕捉](../src/front/lower_nba.cpp)、
[event-NBA契約](notes/event-nba-proposal.md)。

### N04：文字列の文字選択 × 遅延NBA

通常の文字列・文字への代入は動きます。
staticな文字列に対する`text[i] <= #1 8'h41;`は未対応です。

`probe: nba_string_char_delay`は
`unsupported: delayed nonblocking assignment to a string character`。
予約中に文字列の値・長さ・添字が変わる場合の宛先と更新方法を、既存の文字列の意味に接続する
必要があります。根拠：[文字選択への書き込み](../src/front/lower_expr.cpp)。

## 5. 固定の複合型 × automatic・引数・copyback

### A01：unpacked構造体 × ref引数

固定構造体の値や対応済みのinput／output／inout転送、スカラー・固定配列のrefは使えます。
`typedef struct { int n; } item_t;`を`task put(ref item_t r);`へ渡す形は未対応です。

`probe: struct_ref`は`argument 'r' of an array of structures or a ref structure`を報告。
構造体全体のrefをleafへ割り当て、待機・転送・const制約も含め実際の対象を保持する必要があります。
根拠：[サブルーチン引数の変換](../src/front/lower_sub.cpp)。

### A02：構造体配列 × automatic記憶域／配列引数

staticな構造体配列の対応済み処理は動きます。
automatic taskの`item_t items[2];`と、`input item_t items[2]`には不足があります。

`probe: struct_array_auto`は`automatic unpacked array in a structure or of structures`、
`struct_array_input`は`argument 'r' of an array of structures or a ref structure`。
leaf配列のautomaticな記憶域、引数のコピー、呼出しごとの独立性を接続する必要があります。
関数が構造体配列を返す形もソースに明示的な拒否がありますが、今回の実行確認には含めていません。
根拠：[構造体記憶域](../src/front/lower_struct.cpp)、[引数・戻り値](../src/front/lower_sub.cpp)。

### A03：構造体の固定配列メンバー × automatic記憶域

次の型をautomatic task内のローカル変数として使う場合です。

```systemverilog
typedef struct { int words[2]; } item_t;
// task automatic内：item_t item; item.words[0] = 1;
```

`probe: struct_array_member_auto`は
`unsupported: automatic unpacked array in a structure or of structures`。
構造体のscalar leafだけでなく、配列leafのフレーム上の形と初期化・コピーを扱う必要があります。
根拠：[構造体フレーム](../src/front/lower_struct.cpp)。

### A04：固定配列スライス × output／inoutの書き戻し

配列スライスの対応済みの読み取りや、配列全体の引数は使えます。
部分配列をoutput実引数とする形は未対応です。

```systemverilog
module top;
  int values[4];
  task put(output int r[2]); r = '{1, 2}; endtask
  initial put(values[1:2]);
endmodule
```

`probe: array_slice_copyback`は`unsupported: an output or inout array argument of this actual`。
今回の自作入力はoutputのみで、inoutは同じ制限として対応表・ソースに記載される範囲です。
実引数の評価時点と、戻るときの対象範囲・宣言方向・他要素の保持を接続する必要があります。
根拠：[配列実引数の変換](../src/front/lower_sub.cpp)、[SIRのcopyback](spec/SIR_SPEC.md)。

## 6. 選択・streamingの組合せ

### S01：位置が動くpacked親 × 実行時の子選択

`values[1][bit_index] = 1;`のように親を固定すると動き、対照例は`0800`を出力しました。
この対照例のIDは`packed_parent_fixed_control`です。
親も実行時に選ぶ形は未対応です。

```systemverilog
module top;
  logic [1:0][7:0] values;
  int row = 1, bit_index = 3;
  initial begin
    values = '0;
    values[row][bit_index] = 1;
  end
endmodule
```

`probe: packed_parent_runtime`は`unsupported: a runtime select of this parent packed range`。
親・子の位置を一度ずつ評価し、子が親の範囲外を選んでも隣の要素へ書かないよう、
位置と境界の両方を保持する必要があります。根拠：[packed書き込み対象](../src/front/lower_expr.cpp)。

### S02：streaming × `with`／文字列の宛先／入れ子の宛先

固定配列・構造体の対応済みstreamingと遅延NBAは実装済みです。残る代表例は次の3形です。

- `probe: stream_with`：`{>>{values with [1:2]}}`。`streaming with a with clause`で拒否。
- `probe: stream_string_target`：`{>>{text}} = 16'h4142;`。`unpacking this stream`で拒否。
- `probe: stream_nested_target`：`{>>{{>>{value}}}} = 8'h41;`。`unpacking into this target`で拒否。

選択した要素数・順序、文字列長、入れ子のビット並べ替え、代入先ごとの切り詰めを
共通の変換・書き込みへ接続する必要があります。
根拠：[streamingの変換](../src/front/lower_expr.cpp)、[SIRのstreaming契約](spec/SIR_SPEC.md)。

## 7. ファイル・波形・監視の組合せ

### F01：`$fread` × 動的メモリ／複雑な構造体メモリ

整数スカラー、多次元の固定整数メモリ、整数leafを持つ対応済みの1次元構造体メモリは動きます。
残る代表例は次の4形です。

- `probe: fread_dynamic`：`byte values[]`への読み込み。動的メモリの宛先経路が未対応。
- `probe: fread_struct_array_member`：構造体が`byte values[2]`を持つメモリ。配列メンバーが未対応。
- `probe: fread_struct_real_member`：構造体が`real value`を持つメモリ。realメンバーが未対応。
- `probe: fread_struct_multidim`：構造体の`rows[2][2]`。構造体メモリの多次元化が未対応。

4入力ともparse成功後に、`$fread into a memory of this kind`または
`$fread into a structure with a member of type ...`で拒否されました。
今回の入力はファイルを開く処理へ到達していません。
要素の巡回順、start／count、部分入力、入力前の値の保持、書き戻し対象の捕捉を統合する必要があります。
根拠：[入力の対象・レイアウト](../src/front/lower_expr.cpp)、[システムタスク契約](spec/SYSTEM_TASKS.md)。

### F02：program／interface内部の変数 × VCDスコープ

moduleのVCD出力は動きます。今回、top moduleの中にinterfaceとprogramをインスタンス化し、
それぞれの内部変数を更新して`$dumpvars(0, top)`を実行しました。

`probe: vcd_program_interface`はparse・SIR生成・通常実行のすべてで成功しましたが、
生成VCDには`$scope module top`と`module_bit`だけが現れ、
interfaceの`bus_bit`とprogramの`program_bit`は現れませんでした。
終了コードだけでは分からない不足です。

階層の巡回、スコープ種別、対応する信号の登録を波形経路へ接続する必要があります。
根拠：[dump階層の構築](../src/front/lower_dump.cpp)、[波形仕様](spec/WAVEFORM.md)。

### H01：動的コンテナの値 × 暗黙の感度

動的配列・queueの明示的なwaitと、対応済みのheap依存監視は動きます。
`always_comb result = values.size();`のような暗黙の感度には不足があります。

`probe: heap_implicit_sensitivity`は
`unsupported: implicit sensitivity to dynamic container values`。
staticなスロット範囲だけでなく、コンテナの変更・置換・要素変更を感度へ反映する接続が必要です。
`always_latch`、`always @*`、連続代入も対応表上の同じ制限で、今回は`always_comb`だけを実行確認しました。
この項目は前の大分類「4. 動的コンテナの残り」と重複するため、二重に残作業数へ加えません。
根拠：[暗黙の感度の検査](../src/front/lower.cpp)、[コンテナの対応範囲](P1_SUPPORT.md)。

### U01：heapを読むfile monitor × 全コンテキスト／再入登録（未確認）

queue要素の変更をmonitorが追う基本形、file monitorの閉鎖・記述子再利用・callback終了などは
既に検査済みです。今回の`postponed_heap_control`も、同じqueue要素の変更に対して`1`と`2`を出力しました。

一方、callbackからの再入登録や、全ソースコンテキストでの組合せ網羅は対応表で**未確認**です。
未実装・不具合が今回見つかったという意味ではありません。
登録中の増減、ファイル終了、置換されたheap値、callbackの所有者と清掃を組み合わせて確認する必要があります。
根拠：[file monitorの寿命検査](notes/heap-file-monitor-lifetime.md)、[対応表](P1_SUPPORT.md)。

## 8. 実装不足の件数に含めないもの

以下の3入力は、今回の厳格なフロントエンド設定でparseから拒否されました。
ネイティブ変換を有効にするために、これらの診断を緩める扱いにはしません。

- `automatic_nba_illegal_control`：automaticローカルへのNBA。
  `nonblocking assignment to automatic variable ... is not allowed`。
  N01では、これを避けるために実際のstatic対象を受ける`ref static`を使いました。
- `strobe_auto`：`$strobe`からautomatic変数を直接参照。
  `automatic variable ... cannot be traced`。対応表上の制限ですが、今回の環境ではSlang段階の診断です。
- `packed_parent_outside`：宣言範囲外の定数packed添字。
  `cannot refer to element 2 ... [-Windex-oob]`がエラーになります。
  これは厳格診断の観測であり、「定数の範囲外選択は常に規格上不正」という一般化はしません。

同様に、ガードへ直接`count++`を書く試行はSlangが拒否しました。
E04で確認したのは、関数の内部で変数を書き換える別の形です。

## 9. 既に完了した範囲と、この記録の使い方

今回の不足と区別すべき実装済みの範囲には、混在する固定・動的次元の`foreach`、
対応済みの実行時次元クエリ、automatic scalar／固定配列event、基本event-NBA、
固定aggregateの遅延NBA、heapを使う基本monitorなどがあります。
直近の強度付きforceはカーネル基盤までで、ソース接続は別の大分類「5. ネット・スイッチの残り」です。

この記録を次の実装へ使う際は、対象の形を1つ選び、次を確認します。

1. [P1_SCOPE](spec/P1_SCOPE.md)に沿う必須範囲か、比較・仕様確認が必要な候補かを確定する。
2. 既存の意味と受理・拒否期待値を確認し、仕様変更や期待値移行が必要なら先に分離する。
3. 単独の成功例と組合せの失敗例を残し、値・評価回数・時刻・領域・寿命を確認する。
4. 対応後は、この日付付きの観測を保存したまま、新しい版の対応表と回帰を更新する。

## 10. 保存した入力の再現方法

例として、E01の完全な入力をJSONから取り出します。リポジトリのルートで実行します。

```sh
mkdir -p build/combination-audit
python3 - <<'PY'
import json
from pathlib import Path
data = json.loads(Path('docs/measurements/2026-10-30/p1-combination-gaps.json').read_text())
probe = next(item for item in data['probes'] if item['id'] == 'event_struct_auto')
Path('build/combination-audit/probe.sv').write_text(probe['source'] + '\n')
PY
build/dev/src/driver/kirimasim parse build/combination-audit/probe.sv
build/dev/src/driver/kirimasim --dump-sir build/combination-audit/probe.sv
```

記録した版では、前者は0、後者は2です。診断の位置は入力を整形すると変わります。
他のprobeへ切り替えるときは`id`を変更し、`parse`と`sir`の保存結果を確認してください。
波形例では作業ディレクトリへ`probe.vcd`が作られるため、独立した作業ディレクトリを使います。
将来の実装で成功へ変わることはあり得ます。この資料は自動登録されたテストスイートではなく、
現在の8,311件へ追加した合格・不合格件数でもありません。
