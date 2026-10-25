# P1 の対応機能一覧

[English](P1_SUPPORT.md) · 英語版が主で、日本語版はそれに従います。

**改訂（2026-10-24）：** 旧P2→P3・旧P3→P4・旧P4→P5と付け替え、新しいP2（SIR v1）を設けました（tier番号は不変）。[対応表](notes/phase-renumbering.ja.md)。

[承認済みのP1の範囲](spec/P1_SCOPE.ja.md)はIcarus Verilog 13.0以上の実行機能を
目標とし、必要な後続段階の仕組みを前倒しします。目標と実装状況は別で、以下の
未対応項目は引き続き未実装です。[比較の進捗](notes/p1-icarus-progress.ja.md)に
測定した差を記録します。末尾の残作業では、P1に必要な対応と、それを超える後続段階の
予定を分けて示します。

2026-10-25時点の第1層を、固定集約値へのビット問い合わせ、ヒープを使うファイルモニターの生存期間検査、
動的配列・キュー、広い三状態マスク、集約値全体のブロッキング時間制御、
対象の副作用より前の集約値の保持、
繰り返しイベントのブロッキング代入、
整数の`$fread`の書き戻しの訂正、多次元整数メモリのバイナリ入力、
実行時の配列の縮約・整列・min/max、固定配列ポートと要素ごとのnet解決、
遅延付きNBAの値の捕捉、packed範囲内の選択、名前付きイベントの混合待ちを含めてまとめます。
強さ区間のAPIはカーネルの基盤として別に記載し、MOS・スイッチ実行に対応したとは扱いません。
各ビルドのオプションは`kirimasim --help`、コミットは`--version`で確認してください。
[最新の検証と承認待ちの項目](notes/p1-icarus-progress.ja.md#現在の実装とレビュー状況2026-10-25)で、
現在の比較と過去の測定を区別しています。

P1 は進行中です。この一覧の「対応」は、記載した形を SIR に変換してインタプリタで
実行でき、リポジトリにテストがあることを表します。機能同士のすべての組み合わせを
保証するものではありません。`kirimasim parse` が通ることと、実行できることは別です。
[仕様書](spec/SIR_SPEC.ja.md)には今後の実装予定も含まれますが、この一覧は実装済みの
範囲を示します。

最近の機能の実行例は[使い方と制限](P1_FEATURES.ja.md)、ビルド手順と完成条件は
[README](../README.ja.md)を参照してください。

## 言語と実行

| 分野 | 対応する形 | 主な制限 |
| --- | --- | --- |
| フロントエンド | 版を固定し、改変しない slang によるプリプロセス、構文解析、型検査、エラボレーション | slang が受け付けても、kirimasim で実行できるとは限らない |
| 階層 | module、`input`・`output`・`inout`、parameter、`localparam`、`defparam`、generate、インスタンス配列、階層名 | module の `ref` ポート、`inout` ポートへの部分選択の接続は未対応 |
| 共有する宣言 | package、import、`$unit`、interface、modport | virtual interface とクラスは未対応 |
| 構成 | ソース内の config によるトップ選択と束縛。specify 内を含む specparam | ライブラリ対応表のファイル、specify のモジュールパスとタイミングチェックは未対応 |
| 整数値 | 任意の幅の 2 値・4 値、符号付き・符号なしの演算、X/Z の伝播と変換 | 実際にはメモリと実行量の制約を受ける |
| その他の単一値 | `real`、`shortreal`、`realtime`、`string`、名前付きイベント、null の `chandle` | null 以外のハンドルに必要な DPI-C は未対応 |
| 複合型 | packed/unpacked 構造体、packed 共用体、固定長の unpacked 配列、多次元配列、static な構造体配列、unpacked 配列のパラメータ | 動的配列・キューは下記の範囲に対応。連想配列・unpacked 共用体は未対応。配列スライスへの代入には制限がある |
| 列挙型 | 列挙値と `first`、`last`、`num`、`name`、`next`、`prev` | 実行時の `next(n)` / `prev(n)` の移動数は 32 ビット符号なしに変換する。使用例を参照 |
| 式 | 算術、ビット、論理、リダクション、比較、シフト、三項式、連結、反復、キャスト、`inside`、ワイルドカード等価、代入演算子、増減、代入パターン | `matches` によるパターン付きの `if` と三項式は未対応 |
| 選択 | packedのビット・部分範囲・indexed-part選択、packedの次元、固定unpacked配列の添字、構造体メンバー。静的な親範囲内の実行時の書き込み・複合読み取り（NBA・遅延付きNBAを含む） | 親の位置も実行時に変わる入れ子、親の定数範囲自体が格納先をはみ出す実行時の子選択は未対応 |
| ストリーミング | 定数のスライス幅による `>>`・`<<`。固定長配列と構造体の入力。64 ビット超を含む整数値・固定長配列・構造体への分解。ブロッキング代入とNBA、ストリーミング対象への遅延付きNBA | `with`、文字列の代入先、入れ子のストリーミング代入先は未対応。automatic な代入先への NBA は不可 |
| 分岐とループ | `if`、`case`、`casez`、`casex`、`case inside`、`randcase`。`unique`・`unique0`・`priority` の違反報告。`for`（`for (;;)` も）、`while`、`do`、`repeat`、`forever`、`foreach`、`break`、`continue`、`return` | `randsequence` と `wait_order` は未対応 |
| 手続き | `initial`、`always`、`always_comb`、`always_latch`、`always_ff`、`final`、名前付きprogram、anonymous programのサブルーチン | programはReactive・Re-Inactive・Re-NBAとprogramの終了規則を使う。`final` は待てない |
| 代入と遅延 | ブロッキング代入と NBA、対応する代入内遅延、単一値・集約値全体のブロッキング代入内遅延・イベント・repeat、`#`、時間単位・精度と丸め、min/typ/max の選択。実行時に選ぶ固定配列要素・packedのビット/部分範囲・連結、固定配列・unpacked構造体全体への遅延付きNBA | イベント制御付きNBA、ref・文字列文字を通じた遅延付きNBAは未対応。automaticな対象へのNBAは禁止 |
| イベント制御 | 信号の辺、整数の式、`iff`、`wait (式)`、信号・式と混ぜた名前付きイベント、項ごとの名前付きイベントの `iff`、`->`、`->>`、`.triggered` | イベント式・`iff`・wait条件の変数書き込みに関する制約は従来どおり。実行時の添字で選ぶ名前付きイベント、イベント制御付き `->>`、演算した式による `always_ff` のトリガは未対応 |
| 並行実行 | `fork` / `join`、`join_any`、`join_none`、`wait fork`、`disable fork`、親でのfork内宣言と共有static記憶域、対応する名前付きブロックとタスクのdisable | forkの枝が捕捉するautomatic変数、名前付きforkブロックのdisableに未対応の形がある。automatic宣言には通常の型の制限もある |
| サブルーチン | static/automatic な関数とタスク、再帰、既定の引数、`input`・`output`・`inout`・`ref`・`const ref`。固定長配列と unpacked 構造体の引数・戻り値。待つタスク | unpacked 構造体の `ref`、構造体配列の引数、automatic な構造体配列、output/inout に渡す配列スライスは未対応 |
| ネットと解決 | `wire`、`tri`、`wand`、`wor`、`tri0`、`tri1`、`supply0`、`supply1`、`uwire`。複数ドライバと駆動の強さ | `trireg`、利用者定義の nettype、`interconnect` は未対応 |
| 連続ドライバ | ネット宣言の代入を含む連続代入と、立ち上がり・立ち下がり・ターンオフの遅延 | 各ドライバに遅延を適用する `wire #5 n;` のような、代入を伴わない遅延ネット宣言は未対応 |
| プリミティブ | 組み込み論理ゲートとプル、組み合わせ・順序 UDP | MOS・双方向スイッチと、未接続ポートのある UDP インスタンスは未対応 |
| ネットの alias | 互換性のあるネット全体の alias | 部分・連結・配列の alias、互換性のない型、alias やポートで別々に結ばれたグループの結合は未対応 |
| 手続きからの駆動 | 対応する整数型の代入先への `assign` / `deassign`、`force` / `release` | 実数・unpacked 配列全体への指定と、スカラーのポートnetへの force は未対応（定数配列要素は対応） |
| アサーション | 即時の `assert`、`assume`、`cover`。遅延即時アサーション（`#0`、`final`） | 並行アサーションと module 項目として書く遅延アサーションは未対応。遅延アサーションのアクションから、待つ可能性のあるタスクは呼べない |
| スケジューラ | 時刻スロットの全領域と参照アルゴリズム。Observed で保留の報告を確定。Postponed で strobe/monitor を出力 | programの呼び出しとforkは領域と起源を継承。クロッキングブロック・並行アサーションは未対応 |

32 ビットを超える重みの `randcase` は未対応です。

### クラスとstatic初期化の既知の境界

[独自46プログラムの調査](notes/class-execution-investigation.ja.md)では、プロパティを使わない
static関数の1例が動きますが、クラスオブジェクトの実行は未対応です。未使用クラスの
staticプロパティ初期化の副作用を落とす例は、診断なしに終了コード0で終わります。
既知の不具合として扱い、実行できたことをクラス対応の根拠にはしません。
この調査は既存87プローブの比較・承認済みのテストとは別です。
[詳細なクラス契約案](notes/class-runtime-proposal.ja.md)は初期化・dispatch・GC・時間処理を
規定する2026-10-25承認済みの契約で、実装済み機能ではありません。

### UDPの入力数とコンパイルの制限

評価器/転送の固定32入力の上限を外しました。[独自106ソース](notes/wide-udp-inputs.ja.md)は
257入力までを検査し、独立したkernel/SIRで端子、状態順、引数の検証も行います。
直接のkernel/参照検査には65,537入力も使いますが、その入力数のソースコンパイルは未確認です。
密な保持行の表では固定したslangの検査がコンパイル時間上限を超えます。未解決点として追跡し、
診断や依存を緩めていません。

### 集約代入での値の保持

通常代入・NBAは、対象の実行時の添字より先に右辺全体を保持します。コンテナのleafは
後のパターンのメンバーが元を変更する前に捕捉します。実行時に選ぶ固定の部分配列と、
構造体要素の配列メンバーにも、通常代入・NBA・遅延付きNBAの共通の格納を使います。
自動・static functionは固定配列メンバー付き構造体を返せます。明示的なautomaticの
配列メンバー付き構造体変数には既存の制限が残ります。[独自72件と検証](notes/aggregate-value-capture.ja.md)を
参照してください。別の[独自74件](notes/blocking-aggregate-timing.ja.md)で、集約値全体の
ブロッキング遅延・イベント・repeat、活動ごとの型付き保持、異なる範囲の構造体コピーも
検査しています。イベント制御付きNBAは未対応です。

### 繰り返しイベントのブロッキング制御

ブロッキング代入内の`repeat(count) @(...)`は右辺・回数を保持し、待機後に対象を選びます。
0以下・X/Zの回数はイベントを待たず、回数の幅と符号を保ちます。名前付き・混合・式の監視・
項のguardは既存の待機処理を使います。[独自ソース75件](notes/repeated-event-controls.ja.md)は
自動変数、実数・文字列、packed連結、コンテナの値の保持も含みます。固定配列・unpacked
構造体全体にも同じブロッキング制御を適用します。イベント制御付きNBAは未対応です。

### 駆動強さの解決カーネル

三状態ゲートのL/Hマスクは広いベクトルと、64ビット語の途中に来る境界にも対応しました。
別の[独自73件](notes/wide-gate-strength.ja.md)が、測定したIcarusの両版と一致します。
マスク付き・なしのドライバを既存の共有カーネルで解決し、任意のMOS区間の受け渡しとは
区別します。解決対象のpackedネットの遅延付き三状態出力は、値と両sideを所有した1状態で
伝播します。同じ射影の変更・取り消し・同値の予定の期限保持・遅延0・短絡ブロックを
[独自240件と直接3件](notes/delayed-gate-state-execution.ja.md)で検査します。
具体的な期待値移行も別に承認・統合され、ローカルCI全21項目が成功、失敗0・既知2件のみスキップです。

承認済みのVALUE_SEMANTICS 5.2–5.4に従い、共有カーネルで4値への射影前の完全な
強さの区間を取得できます。追加のユニットテスト5本で、ネット11種類の区間ペア158,400組
などの両端点を比較します。4種類の誤実装は従来の値だけの検査を通りますが、新しい
区間のアサーションが検出しました。[APIと根拠](notes/strength-range-foundation.ja.md)を
参照してください。伝播に向けたカーネルの基盤であり、MOS・双方向プリミティブ、強さの書式、
SIR/VMを通る解決後の区間のメタデータは未対応です。
[MOS・スイッチの調査](notes/mos-switch-investigation.ja.md)には、独自28プログラムの
参照での観測と、意味の未解決の点を記録しました。実装済み機能や承認済みの期待値の追加
ではありません。
[実行契約の全体案](notes/mos-switch-proposal.ja.md)は、初期化と完全な強さの時間規則も含め
2026-10-25承認済みで、全体の実装は残作業です。

### fork内の宣言

宣言は子にせず、親で既存の寿命/初期化を使います。static記憶域は共有し、捕捉のない
automatic初期化は入るたびに行います。宣言だけのforkにも対応します。
[独自72ソースと制限](notes/fork-local-declarations.ja.md)で8種類の実行場所と型を検査します。
子が捕捉するautomatic変数は未対応で、拡張した比較にも明示的に含めています。
[記憶域の共有・automatic eventの修正案](notes/automatic-capture-proposal.ja.md)は2026-10-25承認済みで、
automatic event変数も未対応です。

### automatic記憶域の実行境界

Context単位の世代付きscope・共有binding・lease・readonly・typed rootは
[独自14テストとkernel記憶域の全世代検査](notes/automatic-storage-foundation.ja.md)で確認します。
[待機への接続](notes/automatic-storage-observation.ja.md)14、
[SIR表現/検証](notes/automatic-sir-metadata.ja.md)16、
[値の転送](notes/automatic-vm-frame-access.ja.md)7、
[環境構築](notes/automatic-vm-environment.ja.md)21、
[scoped ref](notes/automatic-vm-scoped-refs.ja.md)11テストに加え、
[Interpreterへの接続](notes/automatic-vm-execution.ja.md)で妥当な表現済みhome・scope入退場・
共有fork環境・実際の待機taskのcallback・不変refを実行するようになりました。
独自19実行テストはcall/コピー戻し・再進入・全join・typed root・キャンセル・program終了・
resource失敗を含み、コンパイルした14故障を検出しました。通常フレームとレイアウト/値の境界を
共用し、scopeの値の複製を残しません。
frontは捕捉homeをまだ生成せず、ソースのautomatic fork捕捉は引き続き拒否します。
automatic event構築と所有ref/objectの実提供も未対応です。手組みSIRのactivation命令の実行だけで
これらのソース機能の差分を解消したとは扱いません。

### 固定長配列ポートとネット配列

固定unpacked配列全体のinput/outputと、互換net同士のinoutを実行します。
異なる範囲・方向と二次元も宣言順で対応し、コピーはRHS全体を先に捕捉します。
別名ビュー、要素ごとの強さ解決、疎なドライバ領域、定数要素のforce/release・慣性遅延、
未接続時の既定値・pullを含みます。整数・real・shortreal・stringの対応する値を扱い、
programの起源も保ちます。[実装と358ソースの検証](notes/fixed-array-port-execution.ja.md)を参照してください。
部分配列・refポート、複数ドライバと合流する配列三状態side、明示的配列aliasは残作業です。
二状態net・chandleポートに対する厳格な診断は緩めません。

### programの終了

programのinitialと連続代入はreactive側で実行し、呼び出し・forkは領域と起源programを
継承します。`$exit` は起源のprogramのinitialと子孫を終了し、programのinitialを
起源に持たなければ無視します。各programの全initialが終わると残る子孫を終了し、
initialを持つ全programが終わるとfinalを実行して終了します。空のprogramだけでは
暗黙のfinishを行いません。[programの行列](../tools/gen_program_execution_matrix.py)は
124本、カーネル7本・SIR1本のユニットテストは終了・再利用・メタデータを確認します。
CLIは15組の不正入力と2インスタンスの識別を確認します。クラス・クロッキングブロック・
並行アサーションの対応は含みません。programのスコープのVCD出力は未対応です（WAVEFORM 7）。

## 動的コンテナの共通基盤

[実行仕様の修正案](notes/dynamic-containers-proposal.ja.md)は2026-10-23に承認済みです。
`Context::heap()`に、型付きコンテナの格納、深いコピー、安定した要素セル、上限付き
キューの変更、正確なトレース回収を実装しました。確保失敗は新しいオブジェクトを
巻き戻し、回収した項目は世代を変えて再利用するので、古いトークンが別の値を指しません。
これらはカーネルのAPIで、[従来の15テスト](../tests/unit/heap_test.cpp)、
[変換・スナップショット・記述子・回収・監視の16テスト](../tests/unit/heap_conversion_test.cpp)と
[独立したvalsemのモデル](../src/valsem/include/kirimasim/valsem/containers.h)で検査します。
100回の試行で252万操作が通過し、意図的な6種類の誤りもすべて検出しました。
ヒープの行列はnightlyの参照検査にも含まれます。

コピーは上限が違う同値な入れ子のキュー配置を受け付け、保持する各段でコピー先の
上限を適用し、切り詰めの警告を返します。配置の同値性は登録時に計算し、実行時の
確保やネイティブな再帰は不要です。`Heap::snapshot`は実際の元の型・上限を保ちます。
追加の検査は型・上限・値の432組、9種類の値の入り口、分岐する複合値、2,500段、
コピー・変更・スナップショットの10経路の各段階での確保失敗を含みます。
追加した6種類の変換の誤りを実際のヒープのアサーションですべて検出し、復元後も通過しました。

型付きコンテナ記述子、対象変数への参照、17種類のSIR命令（2026-10-23の基盤で16種類、
2026-10-25から`container.swap.values`を追加）、検証器・テキスト形式は
実装済みです（SIR_SPEC 5.8）。新しい10テストが型・上限の96組、49種類の不正入力、
引数の結び付き、固定配列内のコンテナへの参照を確認します。
VMから17命令を共通ヒープへ接続しました。[独立したSIRの25テスト](../tests/unit/vm_container_test.cpp)が
要素型・上限・格納先の216組、ref呼び出し72組、固定配列の選択、taskの待機をまたぐ
切り離したセル、valsemと比較するpacked書き込み、引数の副作用、不正な添字、警告と
エラーを検査します。空の場合や、同値なref仮引数・固定配列の転送でも実際の型を保ちます。
サイズ異常・ヒープ確保失敗はソース位置付きのError/ResourceLimitへ写し、メソッドの
警告はソース位置ごとに1回返します。意図的なVMの8種類の誤りも検出しました。

領域・スロットの処理完了後に自動回収します。型付きルートはスロットと固定配列、
生存中のプロセス・taskの実行フレーム、切り離したセルへの参照、遅延報告の引数、
NBAの値、保留ドライバ、保持中の値を含みます。通常の数値をハンドルと推測しません。
反復置換・追加削除24組、NBA12組、固定配列8組、遅延報告16組と、ルートだけが変わる
場合を検査し、100回反復してもプールの表が増え続けないことを確認します。
マーキング失敗は全値を保持します。プロセスの呼び出し中、終了要求後、final処理中は回収しません。

ヒープには読み取り・変更フックも追加しました。要素列と個々のセルの値を区別し、
確定した変更を通知します。同値の正規化済みの書き込み、失敗した操作、不正な選択は
変更を通知しません。5テストで書き込み40組、上限、入れ子、確保失敗、所有権と隔離を
確認し、意図的な8種類の誤りも検出しました。スケジューラは監視式の型付き読み取りを
捕捉し、弱いオブジェクト・セルの監視印と共通の変更順序で通知します。
選択・結び付きの変更で付け直し、切り離したセルは固有の識別子を保ちます。
VMのguard/watchの呼び出しと転送したrefも同じ機構を使います。呼び出した関数の
私有コンテナは除外し、グローバルと階層名で直接読む値は依存先に残します。
新しいスケジューラの5テストが再評価、解除・再利用、記録数の上限、所有権、確保失敗を検査し、
VMでも私有・グローバル・階層読み取り16組、直接・関数経由の待機8組、失敗・終了処理4組を
追加しました。意図的な監視の8種類の誤りも検出しました。

動的サイズ配列の要素へのNBAは、セル参照経由を含め規格で禁止されています
（6.21・10.4.2）。直接のソースはslangが拒否し、独自SIRもVMが同じ理由で診断します。
要素のNBAはP1の未実装機能ではありません。適格なstatic変数への値全体のNBAとは区別します。

frontから共通ヒープへ接続し、SystemVerilogから実行できるようにしました。
[独自のソース行列](../tools/gen_dynamic_container_matrix.py)は240件で、8種類の実行場所、
1・4・32・65・129ビット、2値・4値と各機構の組み合わせを検査します。
さらに[連結・スライスの行列](../tools/gen_container_sequence_matrix.py)256件を追加し、
同じ8種類の場所と64ビットも含む6種類の幅を横断します。ソースは合計496件です。
固定・動的・キュー、配列を返す関数、入れ子の実際の上限、値の独立性、保持したref、評価順を検査します。
[CLIの検査](../tests/cli/run_dynamic_container_tests.py)は11件の位置付きエラーを確認します。
既存のソース本体・期待値は変更していません。

- 動的配列の`new[size]`、固定・動的配列による初期化、サイズ変更、`size()`・`delete()`。
  キューのパターン、スカラーの連結、添字、`[$]`・`[$+1]`、delete・insert・push・popと実際の代入先の上限。
- 整数・real・shortreal・string・null chandle・入れ子のコンテナ。
  コンテナを含む固定配列・unpacked構造体。代入、初期化、input・output・inout引数、関数の戻り値は値をコピー。
- 入れ子を含め、同値なキュー型のref引数が実際の上限を保持。待機する呼び出しは移動・切り離したセルを保持。
  メソッドは引数より前に対象の結び付きを、代入は対象の添字より前に右辺の値を捕捉。
- 適格なstatic変数への値全体のNBA・遅延付きNBA、ブロッキング代入内の遅延。
  明示的な`wait`と式の監視は型付きヒープ依存を使い、空のrefを関数へ転送する場合も待機を解除。
- 動的配列・キューへのunpacked連結に、スカラー・入れ子の要素、固定配列とその関数戻り値、
  動的配列・キューを使えます。各項をソース順に1回評価し、実際の代入先で上限を適用します。
- `q[low:high]`の読み取りは、キューを1回評価して境界式より前に値を捕捉します。
  境界は左から1回ずつ評価し、`$`は捕捉した長さを使います。負の下限・過大な上限は切り詰め、
  逆順・X/Z・範囲が重ならない場合は空を返します。広い符号なし境界は符号用の1ビットを確保します。
- 添字を明記した1段以上の動的次元の`foreach`。本体内の待機・continueにも対応。
  第1次元の`$size`・`$left`・`$right`・`$low`・`$high`・`$increment`。

キューのスライスへの代入、コンテナの条件演算、その他の検索メソッド・shuffle、
動的次元と固定次元を混ぜたり添字を省略したりするforeach、実行時・入れ子の次元クエリ、
固定複合型・class・eventの要素は残作業です。コンテナ値に対する暗黙の感度
（`always_comb`・`always_latch`・`always @*`・連続代入）は、静的なトリガー範囲で
ヒープ変更を表せないため位置付きの未対応診断にします。外側のautomatic変数をforkから
使う形は、従来のSIR_SPEC 7.5の制限です。class・DPI・VPIのルートとIcarusとの全面的な
同等性も未完了です。コンテナ全体への対応完了とは扱いません。

### 実行時の配列メソッド

固定配列・動的配列・キューの`sum`・`product`・`and`・`or`・`xor`・`reverse`・
`sort`・`rsort`・`min`・`max`を、共通の型付き変換で実行します。整数の縮約は要素または
`with`式の幅・符号を使い、空でなければ最初の値から始めるため、単独のZも保持します。
承認済みのプロジェクトの規則で、空のproductは1、andは全ビット1、その他は0です。
実行時の呼出し式にも同じ規則を使いますが、slangが先に畳み込むparameterの形は制限が残ります。

整列のキーは整数・real/shortreal・文字列に対応し、同値の順序を保ちます。比較が確実に真の
場合だけ移動し、X/Z/NaNから全順序を作りません。既存の要素refは添字位置に残り、入れ子の
所有値の内部を指すrefはその値と一緒に移動します。純粋なキーを移動前に要素ごと1回評価します。
副作用のあるキーと、純粋性を未確認のユーザー関数呼出しは、位置付きの未対応診断にします。

min/maxは独立した上限なしキューを返し、空入力なら0要素、その他は元の要素をちょうど1個
返します。キーの副作用より前に受信配列の値を保持し、最初も含めキーを1回ずつ評価します。
宣言順で走査し、同値・不明な比較では先の候補を保ちます。名前付き・入れ子の`with`と、
スカラーキーを使う入れ子の動的コンテナ要素も検査しています。固定複合型・class・eventの要素と
未対応の入力表現は残作業です。[独自270件と直接テスト4件](notes/runtime-array-methods.ja.md)で、
実行場所、広い値、実数・文字列・NaNのキー、参照、副作用を検査します。
承認済みの[縮約・整列](notes/array-methods-proposal.ja.md)と
[min/max](notes/array-extrema-proposal.ja.md)の決定には参照実装との違いも記録しています。

## システムタスク・関数・メソッド

用途ごとにまとめます。display/write/strobe/monitor 系は、ファイル出力の形と
`$swrite` を含め、`b`・`o`・`h` の接尾辞のある形にも対応します。

| 用途 | 対応する名前・形 |
| --- | --- |
| コンソール出力 | `$display`、`$write`、`$strobe`、`$monitor`、`$monitoron`、`$monitoroff` |
| 文字列への出力 | `$swrite`、`$sformat`、`$sformatf`。`$sformatf` の別名として `$psprintf` |
| 診断と終了 | `$fatal`、`$error`、`$warning`、`$info`、`$finish`、`$stop`、`$stacktrace`（タスクと関数の形） |
| 時刻 | `$time`、`$stime`、`$realtime`、`$timeunit`、`$timeprecision`、`$timeformat`、`$printtimescale` |
| 変換 | `$signed`、`$unsigned`、`$rtoi`、`$itor`、`$realtobits`、`$bitstoreal`、`$shortrealtobits`、`$bitstoshortreal` |
| 型と配列の問い合わせ | 対応する型への `$bits`、`$typename`、`$isunbounded`、`$dimensions`、`$unpacked_dimensions`、`$left`、`$right`、`$low`、`$high`、`$increment`、`$size` |
| ビットの問い合わせ | `$countbits`、`$countones`、`$onehot`、`$onehot0`、`$isunknown` |
| 整数の数学関数 | `$clog2` |
| 実数の数学関数 | `$ln`、`$log10`、`$exp`、`$sqrt`、`$pow`、`$floor`、`$ceil`、`$sin`、`$cos`、`$tan`、`$asin`、`$acos`、`$atan`、`$atan2`、`$hypot`、`$sinh`、`$cosh`、`$tanh`、`$asinh`、`$acosh`、`$atanh` |
| 文字列メソッド | `len`、`getc`、`putc`、`toupper`、`tolower`、`compare`、`icompare`、`substr`、`atoi`、`atohex`、`atooct`、`atobin`、`atoreal`、`itoa`、`hextoa`、`octtoa`、`bintoa`、`realtoa`。文字の添字アクセス |
| プラスアーグ | `$test$plusargs`、`$value$plusargs`。コマンドラインの `+name` / `+name=value` |
| ファイル管理・出力 | `$fopen`、`$fclose`、`$fflush`、`$fdisplay`、`$fwrite`、`$fstrobe`、`$fmonitor` |
| ファイル入力・位置 | `$fgetc`、`$ungetc`、`$fgets`、`$fscanf`、`$fread`、`$ftell`、`$fseek`、`$rewind`、`$feof`、`$ferror`。STDIN |
| 文字列の入力 | `$sscanf` |
| メモリファイル | アドレス範囲の指定を含む `$readmemb`、`$readmemh`、`$writememb`、`$writememh` |
| 乱数 | `$random`、`$dist_uniform`、`$dist_normal`、`$dist_exponential`、`$dist_poisson`、`$dist_chi_square`、`$dist_t`、`$dist_erlang`。`--seed` を種とするプロセスごとの `$urandom`、`$urandom_range` |
| VCD 制御 | `$dumpfile`、`$dumpvars`、`$dumpoff`、`$dumpon`、`$dumpall`、`$dumplimit`、`$dumpflush` |

### 書式と入力の制限

共有カーネルの[代入パターン表示API](notes/pattern-format-foundation.ja.md)と
[SIR・実行処理](notes/pattern-sir-execution.ja.md)は接続済みです。frontの書式引数の
接続は残り、ソース`%p`は未対応です。クラス識別や連想配列の順序を有効にする変更でもありません。

[固定集約値へのビット問い合わせ](notes/aggregate-bit-queries.ja.md)は、整数の末端要素を持つ
固定配列・構造体・構造体配列を既存のストリーミング処理へ通します（新規ソース136件）。
対応済みのautomatic、配列ref、集約値を返す関数も同じ変換処理を使います。
未対応の集約値の格納形式や、動的コンテナ・文字列のビットストリームを有効にする変更ではありません。

既存queueの要素値を変えたときの`$monitor`再表示は、[実ソースと独自SIRで確認](notes/pattern-sir-execution.ja.md)しています。
[20件の実ソースとschedulerの失敗・生存期間テスト](notes/heap-file-monitor-lifetime.ja.md)で、
heapを使うfile monitor、配列の置き換え、ファイルを閉じる処理、記述子の再利用、コールバックの
終了も確認しています。再入によるコールバック登録と、全ソースコンテキストでの対応は未確認です。

- `$strobe` と `$monitor` の引数から automatic 変数は読めません。
- 出力の変換指定は `%b`、`%o`、`%h` / `%x`、`%d`、`%c`、`%s`、`%e`、`%f`、
  `%g`、`%t`、`%m`、`%l`、`%%`。幅と対応するフラグを指定できます。
  精度と `+`・空白・`#` のフラグは実数変換用です。`%u`・`%z` はホストのバイト順で
  32ビットの生データを下位の語から出力し、幅の上の端数をゼロで埋めます。`%u` は
  X/Zを0にします。通常・ファイル・遅延・文字列への出力は共通処理を使います。
  packed 変数はNULのバイトを保ち、文字列型では取り除きます。`%p`・`%v` は未対応です。
- `$sformat` と `$sformatf` は実行時の文字列・packed 整数の書式に対応します。
  引数の過不足は警告して続けます。packed 書式に X/Z を含む形は未対応です。
  書式の後の文字列リテラルは、パーセント記号を含んでいても値として扱います。
- `$sscanf` と `$fscanf` は実行時の書式に対応します。2・8・10・16 進数、文字・文字列、
  実数の変換、フィールド幅、代入抑止（`*`）、`%%` を使えます。無書式の `%u`・`%z`
  （大文字も可）はホストの32ビット語を整数・列挙型・packedの集まりへ読みます。
  幅の省略と0は代入先の幅、正の幅は転送のビット数です。空白・NULもデータで、
  不足したフィールドは代入しません。入力の `%t`・`%v`・`%m` は未対応です。packed 書式に X/Z があると入力を読まず `-1` を返します。
  出力引数が足りない場合は警告し、その変換の前で止めます。
- `$value$plusargs` は変換指定が 1 つの実行時書式に対応します。`%d`、`%o`、`%h` / `%x`、
  `%b`、`%e`、`%f`、`%g`、`%s` と、その大文字・`%0` の形を使えます。最初の一致を選び、
  一致しなければ出力の値を保ちます。複数の変換、先頭に `%%` のある接頭辞、X/Z を含む
  packed 書式は未対応です。
- `$fread` はスカラー整数の代入先と、整数の多次元固定メモリに対応します。
  整数の代入先ではstart・countの値で入力を制限しませんが、指定した式は左から1回評価します。
  SIR_SPEC 8どおり、整数の実引数をコピー入力時に評価し、正のバイト数を書き戻す時に
  代入先を再評価します。ヒープ要素を直接書いた式はその時点のコンテナから選び、実際のrefは
  既存の参照規則を保ちます。部分入力では未入力ビットのコピー入力値を保ち、0バイトなら
  書き戻しません。[期待値と実装の訂正](notes/fread-copyback-correction.ja.md)も参照してください。
  [153件の行列](../tools/gen_scalar_fread_matrix.py)で8種類の幅、2値・4値、9種類の
  実行場所と、引数・格納先の機構を検査します。
  残る次元が2段以上の整数メモリでは、start/countは線形な位置を表し、数値の小さい添字から
  大きい添字へ、右端の次元を最速で進めます。選択済みの接頭添字は後の引数より前に保持し、
  残り1次元なら従来どおり実際の添字を使います。[156件の行列](notes/multidim-fread-execution.ja.md)で、
  方向・部分入力・回数・initial/automatic/refを検査します。unpacked構造体の一次元メモリは、
  整数の末端メンバと入れ子の構造体を持てます。動的メモリ、構造体の配列・実数メンバ、
  多次元の構造体メモリ、unpacked共用体は未対応です。
- メモリファイル操作は、`ref` で渡した固定メモリ、参照の転送、待機する呼び出し、
  自動変数、配列の行への参照に対応します。writemem は `const ref` も使えます。
  [システムタスク仕様](spec/SYSTEM_TASKS.ja.md)と
  [使い方と制限](P1_FEATURES.ja.md)を参照してください。


## コマンドラインと波形

| 用途 | 実装済みのインターフェース |
| --- | --- |
| 実行・確認 | `kirimasim [options] <files> [+plusargs]`、`kirimasim parse`、`--help`、`--version` |
| ソースの入力 | slang を通した `-f`、`+incdir+`、`+define+`、`--top` |
| 時間の選択 | `--timescale=<unit>/<precision>`、`--mintypmax=min\|typ\|max` |
| シミュレーションの上限 | `--max-time`、`--max-time-ok`、`--max-zero-delay`、`--max-slot-events`、`--max-steps`、`--max-call-depth` |
| 実行中の出力 | `--autoflush`。display・write・strobe・monitor ごとに標準出力をフラッシュ。標準出力宛のファイルタスクも含む。既定は無効 |
| 停止の方針 | `--stop=finish\|ignore\|error`（既定 `finish`）。`ignore` は呼び出し元を継続。`error` は理由 `stop` のまま終了コード 1。`$stop(2)` は継続後の実行終了時にも統計を要求する |
| 実時間の上限 | `--max-wall-time=<秒数>`。整数秒、0 で無効。コンパイルは含めず、`final` を実行して終了コード 4・理由 `wall_time_limit` で終了。`--max-time-ok` は適用しない |
| 乱数の種 | `--seed=<n>` |
| 内部の確認 | `--dump-sir`、`--stats=json` |
| 範囲の診断（`dec94ff` から） | `--bounds-check=off\|warn`（既定は `off`）。不正な packed 選択・固定長配列の添字をソース位置ごとに 1 回警告し、最後にアクセス回数を出す。通常の読み書き・NBA の値は保つ |
| VCD 出力 | `--wave=<file>.vcd`、`--wave-scope=<pattern>[,<pattern>...]`、`--wave-glitches`。4 値の波形。上記の dump タスクからも制御できる |

`--max-scc-iter` は受け付けますが、P3 の静的な島の実装までは効果がありません。
[CLI 仕様](spec/CLI.ja.md)にあるオプションがすべて実装済みというわけではありません。
例えば `--cond-x`、`--init-values`、`--max-procs` は、現在の driver のオプションでは
ありません。各ビルドの `--help` で確認してください。FST 出力は未対応です。
実時間の上限は、イベント実行前と VM の分岐（静的初期化を含む）で検査します。
待っているホスト側の処理を強制的に中断するものではありません。final は期限に止められず完了します。

## 残っている作業と対象範囲

| 状態 | 機能 |
| --- | --- |
| P1に必要な実行機能の残り | 動的コンテナ、クラス、VPI、スイッチレベル、specifyパス、タイミング機能のうちIcarusが対応する範囲。FST、`%p`、解決した強さの`%v`、未対応の`$fread`の形、上記の制限も残る |
| P1のコンテナの接続 | 共通ヒープ・型付きSIR/VM・自動回収・ヒープ依存の待機を接続し、ソース496件で検査済み。言語側の残作業は上記を参照 |
| P2（予定、方針承認済み） | SIR v1のテキスト/パーサ・共通Op schema・kernel境界。P1と並行準備しP1後に凍結。文法/API/参照の詳細はレビュー待ち（[計画](notes/sir-v1-proposal.ja.md)） |
| P3 | 第2層のC++生成、静的な島の解析、RTLのさらなる性能改善 |
| 必要なIcarus対応を超える後続段階の予定 | より広いUVM、制約付き乱数、カバーグループ、並行アサーション・サンプル値の関数、クロッキング、virtual interface、`let`、DPI-Cなど。旧P4/P5の表記を理由にP1で必要な仕組みを後へ回さない |
| 棚卸しする拡張・外部インターフェース | SDF、暗号化IP、古いPLI、ライブラリ対応表、ツール固有の拡張。独自のプローブと互換方針を確認してから対応を判定する |
| 現在の言語の対象外 | VHDL・言語混在・アナログ・混在信号 |

この分類には[承認済みの範囲](spec/P1_SCOPE.ja.md)を適用します。checker、`randsequence`、
利用者定義nettype、`interconnect`、`trireg`なども未対応で、個々の形がP1で必要かは
実測したIcarusの対応範囲に従います。対話的デバッガと独自のPython APIも今後の予定です。

コンパイル型の実行バックエンドはまだありません。この一覧で実行できる言語機能は
第1層で動きます。cocotbを通すPython連携は未実装のVPIに依存し、Icarus対応で必要な
インターフェースはP1に含まれます。完成にはREADMEの条件と機能範囲の検証が必要です。

## 確認の根拠と更新方法

- [シミュレーションスイート](../tests/suite/)：`values`、`proc`、`sched`、`cont`、
  `nets`、`hier` などに分けた実行例と期待値。
- [ユニットテスト](../tests/unit/)：値の意味、スケジューラ、実行時処理の検査。
- [未対応の形の行列](../tools/gen_unsupported_matrix.py)と
  [CLI の入力](../tests/cli/unsupported_matrix/)：拒否する具体的な形とソース位置の診断。
  キーワードの一覧より細かい範囲を確認できます。
- 仕組みごとの行列：[ストリーミング](../tools/gen_stream_matrix.py)、
  [列挙型の移動](../tools/gen_enum_step_matrix.py)、
  [実行時の読み取り書式](../tools/gen_runtime_scan_matrix.py)、
  [文字列の書式](../tools/gen_runtime_string_format_matrix.py)、
  [プラスアーグの書式](../tools/gen_runtime_plusargs_matrix.py)、
  [構造体のメモリ](../tools/gen_fread_struct_matrix.py)、
  [参照引数のメモリ](../tools/gen_ref_memfile_matrix.py)、
  [バイナリ出力](../tests/cli/run_binary_format_tests.py)、
  [範囲の診断](../tools/gen_bounds_check_matrix.py)。
- 実装：[変換処理](../src/front/)、[インタプリタ](../src/vm/)、
  [ランタイム](../src/kernel/)、[driver](../src/driver/main.cpp)。

対応範囲を変えたら、日英の一覧、README の要約、該当するテストを更新してください。
使用例は `P1_FEATURES`、規定する動作は `docs/spec/` に置きます。
仕様書に予定があることだけを、実装済みの根拠にはしません。
