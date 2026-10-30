# P1 の対応機能一覧

**2026-10-29追記：** 静的なmodule・interface内の名前付き・入力専用クロッキングブロックに対応しました。
既定または明示した`#1step`、整数型のクロッキング変数、静的なパック配列の選択、`@(cb)`を扱います。
各時刻スロットで一度だけ入力値を保存し、Observedでまとめて公開します。他のスキュー、output/inout、
より広いクロッキング機能は未対応です（[承認済みの実行契約](notes/clocking-input-1step-proposal.ja.md)）。
同じ種別の`wire`同士または`tri`同士の静的なinput/outputのビット選択・部分選択・連結では、
共有のネット解決が必要な場合に各ドライバの駆動強度を保持します。異種ネット、forceの対象、遅延付き宣言、
アンパック配列・動的な部分接続、部分inoutポートは残作業です。以前のスキップ2件は、既存の期待出力のまま成功します。

[English](P1_SUPPORT.md) · 英語版が主で、日本語版はそれに従います。

**改訂（2026-10-24）：** 旧P2→P3・旧P3→P4・旧P4→P5と付け替え、新しいP2（SIR v1）を設けました（tier番号は不変）。[対応表](notes/phase-renumbering.ja.md)。

[承認済みのP1の範囲](spec/P1_SCOPE.ja.md)はIcarus Verilog 13.0以上の実行機能を
目標とし、必要な後続段階の仕組みを前倒しします。目標と実装状況は別で、以下の
未対応項目は引き続き未実装です。[比較の進捗](notes/p1-icarus-progress.ja.md)に
測定した差を記録します。末尾の残作業では、P1に必要な対応と、それを超える後続段階の
予定を分けて示します。

[ソースevent-NBA変換](notes/event-nba-source-execution.ja.md)`c4be8ade`の独自1,018設計を、
mainの修正`9a81a55e`・`b91bd3d7`と合わせます。過去の拒否期待は承認済みの別コミット`16c033a4`で移行しました。
[main統合の記録](measurements/2026-10-27/nba-front-main-integration.json)にpush前の検証を記録し、
過去の測定は当時の範囲を維持します。

mainのコンテナ条件式`a8bb17cc`とNBA保持値`7bc5fa85`を含みます
（[取り込み記録](measurements/2026-10-27/conditional-nba-main-integration.json)）。その区切りではソースevent-NBAは残作業でした。

2026-10-27時点のmainの第1層です。process上限の実装`624123b2`を含みます
（[取り込み記録](measurements/2026-10-27/process-limit-main-integration.json)）。クラスstatic記憶域の実装`9e6a149d`を含みます
（[取り込み記録](measurements/2026-10-27/class-static-main-integration.json)）。
mainの基準`ca72b91f`には[event初期化コピー](notes/event-initializer-execution.ja.md)が入り、
mainで[非パラメータ化クラスのstaticプロパティ](notes/class-static-execution.ja.md)を追加します。
既存の範囲は、固定集約値へのビット問い合わせ、ヒープを使うファイルモニターの生存期間検査、
動的配列・キュー、広い三状態マスク、集約値全体のブロッキング時間制御、
対象の副作用より前の集約値の保持、
繰り返しイベントのブロッキング代入、
整数の`$fread`の書き戻しの訂正、多次元整数メモリのバイナリ入力、
実行時の配列の縮約・整列・min/max、固定配列ポートと要素ごとのnet解決、
遅延付きNBAの値の捕捉、packed範囲内の選択、名前付きイベントの混合待ちを含めてまとめます。
強度区間のAPIと[方向付きMOSのソースの部分対応](notes/source-mos-execution.ja.md)は
[ソースの受動スイッチ](notes/source-passive-execution.ja.md)とともに別に記載します。
登録ネットのforce、上記以外の部分ポートの同一性、完全な強度の書式は残作業です。
各ビルドのオプションは`kirimasim --help`、コミットは`--version`で確認してください。
[最新の検証と制限](notes/p1-icarus-progress.ja.md#現在の実装とレビュー状況2026-10-27)で、
現在の比較と過去の測定を区別しています。

P1 は進行中です。この一覧の「対応」は、記載した形を SIR に変換してインタプリタで
実行でき、リポジトリにテストがあることを表します。機能同士のすべての組み合わせを
保証するものではありません。`kirimasim parse` が通ることと、実行できることは別です。
[仕様書](spec/SIR_SPEC.ja.md)には今後の実装予定も含まれますが、この一覧は実装済みの
範囲を示します。

最近の機能の実行例は[使い方と制限](P1_FEATURES.ja.md)、ビルド手順と完成条件は
[README](../README.ja.md)を参照してください。

公開済みの実装は main の [`304e1aec`](notes/automatic-event-main-integration.ja.md)です。
automatic scalar/固定配列 event の既定値、task/fork の保持範囲、公開済み命令境界での event 回収を含みます。

`string.atoreal()`は再び先頭の符号を1つ読みます（VALUE_SEMANTICS.md 7.6、2026-10-28に承認）。
`13fe2b65`と[依存更新の比較](notes/slang-e2d1021-update.ja.md)で失敗していた母集団の4件は通ります。
`atoi`、`atohex`、`atooct`、`atobin`はslangと同じく先頭のマイナスを読みます（VALUE_SEMANTICS.md 7.6、2026-10-28に改訂）。
負の文字列に0を期待していた母集団の6件は、kirimasim-tests `41085fe3`から両方の読み方を受け入れます。
以下の文字列メソッドの対応は検証した部分集合です。

## 言語と実行

| 分野 | 対応する形 | 主な制限 |
| --- | --- | --- |
| フロントエンド | 版を固定し、改変しない slang によるプリプロセス、構文解析、型検査、エラボレーション | slang が受け付けても、kirimasim で実行できるとは限らない |
| 階層 | module、`input`・`output`・`inout`、parameter、`localparam`、`defparam`、generate、インスタンス配列、階層名 | module の `ref` ポート、`inout` ポートへの部分選択の接続は未対応 |
| 共有する宣言 | package、import、`$unit`、interface、modport | virtual interface とクラスオブジェクトは未対応。非パラメータ化staticプロパティは下記参照 |
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
| 代入と遅延 | ブロッキング代入と NBA、対応する代入内遅延、単一値・集約値全体のブロッキング代入内遅延・イベント・repeat、`#`、時間単位・精度と丸め、min/typ/max の選択。実行時に選ぶ固定配列要素・packedのビット/部分範囲・連結、固定配列・unpacked構造体全体への遅延付きNBA。値/回数/対象を保持するソースevent-NBA、信号/名前付き/式の制御とguard | event-NBAのref配列/入れ子ref窓、managed-event値の形は未対応または未確認。ref・文字列文字を通じた遅延付きNBAは未対応。automaticな対象へのNBAは禁止 |
| イベント制御 | 信号エッジ、整数の式、`iff`、`wait (式)`、信号・式と混ぜた名前付きイベント、項ごとの名前付きイベントの `iff`、`->`、`->>`、`.triggered`、実行時indexでの固定配列event選択、automatic scalar/固定配列eventの既定値・入力引数・宣言初期化コピー | イベント式・`iff`・wait条件の変数書き込みに関する制約は従来どおり。イベント制御付き `->>`、演算した式による `always_ff` のトリガは未対応。scalar eventの戻り値/条件式とautomatic structのevent fieldは未対応で、output/inoutの再関連付けは追加しない |
| 並行実行 | `fork` / `join`、`join_any`、`join_none`、`wait fork`、`disable fork`、親でのfork内宣言と共有static記憶域、字句的なautomatic共有捕捉、対応する名前付きブロックとタスクのdisable | 捕捉は下記の型付き部分集合に対応。objectフィールドと名前付きforkのdisableは残作業。automatic eventの制限は下記を参照 |
| サブルーチン | static/automatic な関数とタスク、再帰、既定の引数、`input`・`output`・`inout`・`ref`・`const ref`。固定長配列と unpacked 構造体の引数・戻り値。待つタスク | unpacked 構造体の `ref`、構造体配列の引数、automatic な構造体配列、output/inout に渡す配列スライスは未対応 |
| ネットと解決 | `wire`、`tri`、`wand`、`wor`、`tri0`、`tri1`、`supply0`、`supply1`、`uwire`。複数ドライバと駆動強度 | `trireg`、利用者定義の nettype、`interconnect` は未対応 |
| 連続ドライバ | ネット宣言の代入を含む連続代入と、立ち上がり・立ち下がり・ターンオフの遅延 | 各ドライバに遅延を適用する `wire #5 n;` のような、代入を伴わない遅延ネット宣言は未対応 |
| プリミティブ | 組み込み論理ゲートとプル、対応する固定端子での方向付きMOS全6種と受動スイッチ全6種、組み合わせ・順序 UDP | スイッチは部分対応：登録ネットのforce、より広い部分ポート、完全な強度の書式は残作業。未接続ポートのあるUDPは未対応 |
| ネットの alias | 互換性のあるネット全体の alias | 部分・連結・配列の alias、互換性のない型、alias やポートで別々に結ばれたグループの結合は未対応 |
| 手続きからの駆動 | 対応する整数型の代入先への `assign` / `deassign`、`force` / `release` | 実数・unpacked 配列全体への指定と、スカラーのポートnetへの force は未対応（定数配列要素は対応） |
| アサーション | 即時の `assert`、`assume`、`cover`。遅延即時アサーション（`#0`、`final`） | 並行アサーションと module 項目として書く遅延アサーションは未対応。遅延アサーションのアクションから、待つ可能性のあるタスクは呼べない |
| スケジューラ | タイムスロット内の全イベントリージョンと参照アルゴリズム。Observedで保留中の報告を確定。Postponedでstrobe/monitorを出力。入力専用の`#1step`クロッキング | programの呼び出しとforkは領域と起点を継承。より広いクロッキング機能・並行アサーションは未対応 |

32 ビットを超える重みの `randcase` は未対応です。

mainの[NBA保持値の土台](notes/event-nba-payload-foundation.ja.md)で、kernelによる値の所有、
後からのready化、共通の保留発行順を追加します。独立した単体24件の成功は、ソースからのイベント制御付き
NBAの対応ではありません。mainの[具体的な購読の中核](notes/event-nba-concrete-controls.ja.md)で、
固定slot・名前付きイベントの持続する購読、全幅の回数、callback元の保持を追加しました。
mainの[式monitorの区切り](notes/event-nba-expression-monitors.ja.md)で、共通評価・
動的依存先の保持・heap/activation通知のまとめ方を追加しました。この区切りは下記の型付きSIR/VM対応より前です。

[型付きSIR/VMの区切り](notes/event-nba-sir-execution.ja.md)`6f43e387`で、独自SIR用の型付き命令・
配置表、verifier/text、新しいcallback転送を追加します。ソースevent-NBAの対応ではなく、
その区切りではfront変換と実ソース行列は残作業でした。[ソース段階](notes/event-nba-source-execution.ja.md)で値保持の順序とcallbackを検査します。

### クラスのstaticプロパティと残るオブジェクトの境界

[非パラメータ化クラスのstaticプロパティ](notes/class-static-execution.ja.md)は、参照のない宣言も
含め、通常の型付きslotと時刻0前の初期化を使います。修飾名・別名・継承名・隠蔽した再宣言と
staticメソッドも、通常のstatic変数と同じ変換経路を使います。packedの2/4値、real/shortreal、
string、event、固定配列、対応するunpacked構造体、整数要素のqueue/動的配列に対応します。
待つtask・NBA・感度・各方向の引数は、それぞれの型の既存の制限を引き継ぎます。

パラメータ付きクラスのstatic記憶域は、初期化の副作用を持つ未使用の特殊化も含め、明示的に
未対応と診断します。プロパティのないgenericのstaticメソッドは利用できます。
オブジェクト生成・handle・field・dispatch・RNG・寿命/GCは未完です。
[承認済みのクラス契約](notes/class-runtime-proposal.ja.md)全体の実装ではありません。
[46プログラムの調査](notes/class-execution-investigation.ja.md)は修正前の日付付き観測として残します。
有限の検査でクラス全体の対応を認定するものではありません。

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
検査しています。イベントNBAのソース段階は上記を参照してください。

### 繰り返しイベントのブロッキング制御

ブロッキング代入内の`repeat(count) @(...)`は右辺・回数を保持し、待機後に対象を選びます。
0以下・X/Zの回数はイベントを待たず、回数の幅と符号を保ちます。名前付き・混合・式の監視・
項のguardは既存の待機処理を使います。[独自ソース75件](notes/repeated-event-controls.ja.md)は
自動変数、実数・文字列、packed連結、コンテナの値の保持も含みます。固定配列・unpacked
構造体全体にも同じブロッキング制御を適用します。イベントNBAのソース段階は上記を参照してください。

### 駆動強度の解決カーネル

三状態ゲートのL/Hマスクは広いベクトルと、64ビットワードの途中に来る境界にも対応しました。
別の[独自73件](notes/wide-gate-strength.ja.md)が、測定したIcarusの両版と一致します。
マスク付き・なしのドライバを既存の共有カーネルで解決し、任意のMOS区間の受け渡しとは
区別します。解決対象のpackedネットの遅延付き三状態出力は、値と両sideを所有した1状態で
伝播します。同じ射影の変更・取り消し・同値の予定の期限保持・遅延0・短絡ブロックを
[独自240件と直接3件](notes/delayed-gate-state-execution.ja.md)で検査します。
具体的な期待値移行も別に承認・統合され、ローカルCI全21項目が成功、失敗0・既知2件のみスキップです。

承認済みのVALUE_SEMANTICS 5.2–5.4に従い、共有カーネルで4値への射影前の完全な
強度の区間を取得できます。追加のユニットテスト5本で、ネット11種類の区間ペア158,400組
などの両端点を比較します。4種類の誤実装は従来の値だけの検査を通りますが、新しい
区間のアサーションが検出しました。[APIと根拠](notes/strength-range-foundation.ja.md)を
参照してください。この解決処理は、後述の完全な区間の公開、SIR・VMでの転送、ソースのMOS・
受動スイッチ実行の基盤です。完全な強度書式と、登録済みネットへのソースforceは未対応です。
[MOS・スイッチの調査](notes/mos-switch-investigation.ja.md)には、独自28プログラムの
参照での観測と、意味の未解決の点を記録しました。実装済み機能や承認済みの期待値の追加
ではありません。
[実行契約の全体案](notes/mos-switch-proposal.ja.md)は、初期化と完全な強度の時間規則も含め
2026-10-25承認済みで、全体の実装は残作業です。

[2026-10-30のforce基盤](notes/strength-force-foundation.ja.md)では、ネイティブのノード単位の
一括force更新、仮想ドライバの伝播、現在の基底ドライバからの同期的な解除を追加しました。
独立した値の参照実装とスケジューラのテストで共有カーネルを検査します。ソース・SIRとの接続と、
既存の拒否期待値の別PRでの移行は残作業で、登録ネットへのソースforceは引き続き未対応です。

[MOSの値演算の基盤](notes/mos-strength-operations.ja.md)で、MOS全6種類と、符号付きの恒等/
非抵抗性/抵抗性の写像を追加しました。新規10テストで全120区間と11,520の制御/素子の組を確認し、
コンパイルした9故障を検出しました。C++20のカーネル/値の意味論の部品です。
[型付きstrength SSAの実行](notes/strength-ssa-execution.ja.md)でmake/MOS/射影をVMへ接続し、
13テスト・手書きSIR 2,034設計・コンパイルした9故障を検査しました。全local CIの21確認も通ります。
[完全なnet状態の公開](notes/strength-store-publication.ja.md)で、各Contextに公開区間を持ち、
全区間/全値をcallbackより先に更新します。新規19公開/幅テストとコンパイルした16故障を検査し、
全local CIの21確認も通りました。[物理originの実行](notes/strength-driver-execution.ja.md)で、
安定したdriver表、スカラ単位の慣性更新、Active処理の集約と独立nodeの解決をこのpublisherへ接続しました。
新規16テストとコンパイルした13故障で完全区間と時間処理を検査しました。全local CIの21確認も通ります。
[宣言した網のSIR実行](notes/strength-network-execution.ja.md)で、検証した静的node/origin、捕捉したload、
スカラのdrive/delay、分離した強度感度を、VMから同じkernel APIへ接続しました。
新規19テストで手書きSIR 13,465実行を含み、コンパイルした16故障も検出しました。全local CIの21確認も通りました。
[方向付きMOSのソース実行](notes/source-mos-execution.ja.md)で、front変換、所有する
通常driverの取り込み、制御の独立した捕捉を接続しました。新規154ソースと境界6件が通り、
コンパイルした11故障を検出しました。Icarus 13.0とは113設計が完全一致し、
添字・初期状態・遷移の差41件を記録しています。古い nmos CLI 拒否期待の移行は main に別途
適用済みです（`6f301460`）。登録netのforce/release、部分・非結合portの
同一性、完全な強度の書式は未完成です。
[受動網のカーネル実行](notes/passive-network-execution.ja.md)で、物理originの経路、
不確定edgeのworld、スカラー導通の時間処理を実際のscheduler/publisherへ追加しました。
新規19テスト、参照14,445状態、コンパイルした28故障を確認しています。
[受動辺の SIR 実行](notes/passive-sir-execution.ja.md)で、検証した辺の表と所有するスカラーの
制御・遅延命令を tier 1 に接続しました。新規 7 テストは独立した参照設計 4,560 件を含みます。
[ソースの受動実行](notes/source-passive-execution.ja.md)で、静的な選択・連結と 1/9/65/129 bit
配列を含む全 6 種を変換します。独自 143 設計が通り、コンパイルした新規・既存故障 9 件を
検出しました。独立した網でも公開は一括のままです。Icarus 13.0 は 135 設計をコンパイルでき、
110 設計が一致し、ベクトル制御・初期状態・混在 wired の差を記録しています。
87プローブの差分は両参照で8件に減りました。古いtran拒否期待は別途承認後に
`77c9d188`で移行し、全ソース入力を保っています。完全なスイッチ対応とP1は未完成です。
[main統合の検証](notes/main-integration-2026-10-26.ja.md)も参照してください。

### fork内の宣言

宣言は子にせず、親で既存の寿命/初期化を使います。static記憶域は共有し、捕捉のない
automatic初期化は入るたびに行います。宣言だけのforkにも対応します。
[独自72ソースと制限](notes/fork-local-declarations.ja.md)で8種類の実行場所と型を検査します。
後述のソース捕捉対応でautomatic変数を共有します。[automatic scalar/固定配列eventの
既定値](notes/instruction-event-execution.ja.md)は、宣言実行ごとに固有objectを作り、子孫の
活動まで保持します。[scalar/固定配列のevent入力](notes/event-input-execution.ja.md)でもobjectを
共有します。[宣言初期化](notes/event-initializer-execution.ja.md)でもnullと固定配列patternを含め
同じobjectを共有します。automatic unpacked structのevent fieldは未対応です。

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
frontは[ソースの共有字句捕捉](notes/automatic-front-capture.ja.md)を生成し、新規186設計と
全フィールド・寿命の確認器が通り、実際にコンパイルした5故障も検出しました。
その捕捉の区切りではautomatic forkプローブが通り、Icarus 13.0合格・native未成功差分は21件でした。
[現在の比較](notes/icarus-13-comparison.ja.md)は両参照とも7件です。
旧未対応3テストは承認済みの別コミット`11096dc3`で移行し、ソース入力は変更していません。
[管理eventのカーネル段階](notes/managed-event-registry.ja.md)で世代付き同一性・実行時pin・
保持スコープのevent rootを提供し、20テストと32ビット全世代を検査しました。
[実行時所有の段階](notes/managed-event-runtime.ja.md)で、型付きのカーネル待機・trigger・monitor操作と
保留操作のpin自動所有を追加し、新規23テストと実際にコンパイルした16故障の検出を確認しました。
[型付きフィールドのprovider](notes/event-activation-roots.ja.md)と
[VMでのSIR event実行](notes/event-vm-boundaries.ja.md)で、型付き記憶域・待機/trigger・
実際の対象読み取りの捕捉を、既存のregion/slot回収境界へ接続しました。
通常/捕捉配列と入力/戻り値の受け渡しを手組みSIRで検査しています。
後続の[automatic eventの区切り](notes/instruction-event-execution.ja.md)で、scalar/固定配列の
宣言時の固有object、現在のevent SIRに必要なnative/SSA/転送の完全公開、物理行を増やす前の
回収を実装しました。VM12テストのSIR32設計、新規生成ソース176件、コンパイルした12故障の
検出が通りました。コンテナheapの回収時点とfinalの除外は保ち、
[過去の拒否期待値1件の移行](notes/automatic-event-expectation-migration.ja.md)は承認後に`4ba6b63d`で適用しました。
[ソースのevent入力引数](notes/event-input-execution.ja.md)を型付き転送へ接続し、
待機する呼び出しと遅延アクションのsnapshotでも共有します。[宣言初期化](notes/event-initializer-execution.ja.md)も
scalarと固定配列patternで同じ型付きコピーを使います。automatic structのevent fieldは未対応で、
object/refのインターフェースとevent値全体の対応も部分的です。SSAは型付きbodyワード数に
応じて保守的に保持し、最終使用直後の生存解析ではありません。制限と計測は実装ノートを参照してください。

ソースの[実行時indexによる固定配列event選択](notes/indexed-event-execution.ja.md)は、
独自160設計と厳格な方針境界16件が通り、コンパイルした4故障も検出しました。
load済みの名前付き/混合待機・遅延対象・明示的な`.triggered`を含み、妥当な定数対象は
既存dumpを保ちます。旧CLI入力は1byteも変えず、[別途承認した移行](notes/indexed-event-expectation-migration.ja.md)で
受理項目1個を追加し、古い診断6行だけを除きました。その以前の区切りでは全CLI14件とlocal CI全21確認が通り、
dev/ASan/UBSanは5,139/5,141、releaseは5,140/5,142で、各失敗0・既知スキップ2件でした。
その後の受動スイッチ実装では、上に記した別件の古いtran拒否期待2か所が失敗しています。

### 固定長配列ポートとネット配列

固定unpacked配列全体のinput/outputと、互換net同士のinoutを実行します。
異なる範囲・方向と二次元も宣言順で対応し、コピーはRHS全体を先に捕捉します。
別名ビュー、要素ごとの強度解決、疎なドライバ領域、定数要素のforce/release・慣性遅延、
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
CLIは15組の不正入力と2インスタンスの識別を確認します。クラスオブジェクト・クロッキングブロック・
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
関数ローカルのコンテナは除外し、グローバルと階層名で直接読む値は依存先に残します。
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
- 添字を明記した動的次元と、固定／動的次元が混在する`foreach`。動的配列・キューを
  要素とする固定配列、その後の整数のpacked次元を、宣言の左端から右端へ反復します。
  合法な固定・末尾次元の省略と、省略した動的次元の後に固定次元だけを反復する形にも対応。
  待機中もループ状態を保ち、`break`はforeach全体を終了、`continue`は最内の名前付き次元を
  進めます。[実行例](P1_FEATURES.ja.md#混在次元のforeach)を参照してください。
  配列クエリの次元には実行時の式も指定できます。固定次元、要素の次元が固定された動的配列・
  キュー、文字列に対応します。[実行例と残る境界](P1_FEATURES.ja.md#実行時の配列クエリの次元)を参照してください。

mainの[コンテナ条件式](notes/container-conditional-execution.ja.md)では、対応済みの
動的配列・キュー表現の`?:`を実行します。既知条件は片側だけ、不定条件は左から両側を捕捉し、
長さが異なれば空、不一致・不定な等価の要素は型の既定値にします。入れ子は内容で比較し、結果の
記憶域は独立します。独自351設計が実行場所9種類、ref、関数、遅延NBA、明示的waitを検査します。
未対応の要素表現や固定/動的オペランドを混ぜる形は未対応または未確認です。
固定の非パック配列の条件式も、入れ子の配列や非パック構造体を含む外側の要素全体を比較します。
異なる要素はビットごとに統合せず、Table 7-1の型の既定値を取ります
（IEEE 1800-2023 11.4.11、7.4.5）。`int`なら0、`integer`ならXです（6.11.2）。
構造体の既定値は、入れ子の配列・構造体も含めメンバーの宣言初期値を保ちます（7.2.2）。
回帰テストは2値・4値の整数、広幅、real、string、メンバーの既定値、副作用、
左右のオペランドを独立して捕捉することを検査します。

キューのスライスへの代入、その他の検索メソッド・shuffle、内側に可変サイズの次元がある型への実行時次元クエリ、
固定複合型・class・eventの要素は残作業です。固定unpacked要素を持つ動的コンテナは
未対応です。先行次元を省略した後の名前付き動的foreach次元は固定版slangが拒否し、
ループ変数も引き続き読み取り専用です。コンテナ値に対する暗黙の感度
（`always_comb`・`always_latch`・`always @*`・連続代入）は、静的なトリガー範囲で
ヒープ変更を表せないため位置付きの未対応診断にします。外側のautomatic変数をforkから
使う形は、従来のSIR_SPEC 7.5の制限です。class・DPI・VPIのルートとIcarusとの全面的な
同等性も未完了です。コンテナ全体への対応完了とは扱いません。

不正なキュー添字への書き込みは格納値を変えず、`--bounds-check=off`でも
必須の警告をソース位置ごとに1回出します。packed選択や、転送・待機を経たrefからの
書き込みも対象です。読み取りやrefの束縛だけでは警告せず、切り離された有効な要素refには
引き続き書き込めます。キューの現在の要素数と同じ添字への書き込みは、警告なしで末尾へ
追加します。[CLI回帰](../tests/cli/run_queue_write_warning_tests.py)で、bounds-checkの
各モードにおける値・診断・回数カウンタを確認します。

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
実行場所、広幅値、実数・文字列・NaNのキー、参照、副作用を検査します。
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
| プラス引数 | `$test$plusargs`、`$value$plusargs`。コマンドラインの `+name` / `+name=value` |
| ファイル管理・出力 | `$fopen`、`$fclose`、`$fflush`、`$fdisplay`、`$fwrite`、`$fstrobe`、`$fmonitor` |
| ファイル入力・位置 | `$fgetc`、`$ungetc`、`$fgets`、`$fscanf`、`$fread`、`$ftell`、`$fseek`、`$rewind`、`$feof`、`$ferror`。STDIN |
| 文字列の入力 | `$sscanf` |
| メモリファイル | アドレス範囲の指定を含む `$readmemb`、`$readmemh`、`$writememb`、`$writememh` |
| 乱数 | `$random`、`$dist_uniform`、`$dist_normal`、`$dist_exponential`、`$dist_poisson`、`$dist_chi_square`、`$dist_t`、`$dist_erlang`。`--seed` を種とするプロセスごとの `$urandom`、`$urandom_range` |
| VCD 制御 | `$dumpfile`、`$dumpvars`、`$dumpoff`、`$dumpon`、`$dumpall`、`$dumplimit`、`$dumpflush` |

`a`・`ab`・`a+`・`a+b`・`ab+`の追記出力では、EOFを越えたシークや
押し戻した入力の読み取り後も、`$ftell`・相対`$fseek`・後続の読み取りが
書き込んだデータの末尾に合うように位置を保ちます。連続した書き込みは
バッファリングを維持します。[新規の位置回帰テスト](../tests/suite/proc/file_append_positions.sv)で
通常の更新モードとの違いも確認し、[FIFOテスト](../tests/cli/run_append_fifo_tests.py)で
シークできないファイルへの出力も保ちます。

### 書式と入力の制限

共有カーネルの[代入パターン表示API](notes/pattern-format-foundation.ja.md)と
[SIR・実行処理](notes/pattern-sir-execution.ja.md)は接続済みです。frontの書式引数の
接続は残り、ソース`%p`は未対応です。クラス識別や連想配列の順序を有効にする変更でもありません。

[固定集約値へのビット問い合わせ](notes/aggregate-bit-queries.ja.md)は、整数の末端要素を持つ
固定配列・構造体・構造体配列を既存のストリーミング処理へ通します（新規ソース136件）。
対応済みのautomatic、配列ref、集約値を返す関数も同じ変換処理を使います。
未対応の集約値の格納形式や、動的コンテナ・文字列のビットストリームを有効にする変更ではありません。

`parse`・通常コンパイル・`--dump-sir`は、動的サイズの型を返す関数呼び出しを含む
`$bits`をエラーにします（IEEE 1800-2023 20.6.2）。
[共通のfront検査](notes/bits-query-validation.md)は、定数化された宣言、次元、generate条件、
サイズキャストも調べます。固定サイズの戻り値への問い合わせは式を実行せず、
動的変数や固定ハンドル値を引数に持つ呼び出しにも従来の意味を適用します。

既存queueの要素値を変えたときの`$monitor`再表示は、[実ソースと独自SIRで確認](notes/pattern-sir-execution.ja.md)しています。
[20件の実ソースとschedulerの失敗・生存期間テスト](notes/heap-file-monitor-lifetime.ja.md)で、
heapを使うfile monitor、配列の置き換え、ファイルを閉じる処理、記述子の再利用、コールバックの
終了も確認しています。再入によるコールバック登録と、全ソースコンテキストでの対応は未確認です。

- `$strobe` と `$monitor` の引数から automatic 変数は読めません。
- 出力の変換指定は `%b`、`%o`、`%h` / `%x`、`%d`、`%c`、`%s`、`%e`、`%f`、
  `%g`、`%t`、`%m`、`%l`、`%%`。幅と対応するフラグを指定できます。
  精度と `+`・空白・`#` のフラグは実数変換用です。`%u`・`%z` はホストのバイト順で
  32ビットの生データを下位のワードから出力し、幅の上の端数をゼロで埋めます。`%u` は
  X/Zを0にします。通常・ファイル・遅延・文字列への出力は共通処理を使います。
  packed 変数はNULのバイトを保ち、文字列型では取り除きます。`%p`・`%v` は未対応です。
- `$sformat` と `$sformatf` は実行時の文字列・packed 整数の書式に対応します。
  引数の過不足は警告して続けます。packed 書式に X/Z を含む形は未対応です。
  書式の後の文字列リテラルは、パーセント記号を含んでいても値として扱います。
- `$sscanf` と `$fscanf` は実行時の書式に対応します。2・8・10・16 進数、文字・文字列、
  実数の変換、フィールド幅、代入抑止（`*`）、`%%` を使えます。無書式の `%u`・`%z`
  （大文字も可）はホストの32ビットワードを整数・列挙型・packedの集まりへ読みます。
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

オプションの書式、既定値、実行の終了結果は、
[リファレンスマニュアル](REFERENCE_MANUAL.ja.md#コマンドラインオプション)を参照してください。

| 用途 | 実装済みのインターフェース |
| --- | --- |
| 実行・確認 | `kirimasim [options] <files> [+plusargs]`、`kirimasim parse`、`--help`、`--version` |
| ソースとトップ | slangを通した`-f <file>`、`--top=<name>`・`--top <name>` |
| インクルードパス | `-I<dir>`・`-I <dir>`、`+incdir+<dir>`。ローカル探索の後にユーザー指定順で探索 |
| マクロ | `-DNAME[=value]`・`-D NAME[=value]`、`+define+NAME[=value]`。値の省略は`1`、明示した空の値は空のまま、最初の事前定義を保持 |
| モジュールライブラリ | `-y<dir>`・`-y <dir>`、`--libdir=<dir>`、`+libext+<ext>`。includeの探索とは別 |
| 時間の選択 | `--timescale=<unit>/<precision>`、`--mintypmax=min\|typ\|max` |
| 実行時に生成された未終了の子プロセス | `--max-procs=<n>`。既定1000000、0で無効。実行時に生成された未終了の子プロセスを数え、静的プロセス・終了後の保持記録は除外。拒否した子は開始せず、finalを実行し`resource_limit`・終了コード6（[検証](notes/process-limit-execution.ja.md)） |
| シミュレーションの上限 | `--max-time`、`--max-time-ok`、`--max-zero-delay`、`--max-slot-events`、`--max-steps`、`--max-call-depth` |
| 実行中の出力 | `--autoflush`。display・write・strobe・monitor ごとに標準出力をフラッシュ。標準出力宛のファイルタスクも含む。既定は無効 |
| 停止の方針 | `--stop=finish\|ignore\|error`（既定 `finish`）。`ignore` は呼び出し元を継続。`error` は理由 `stop` のまま終了コード 1。`$stop(2)` は継続後の実行終了時にも統計を要求する |
| 実時間の上限 | `--max-wall-time=<秒数>`。整数秒、0 で無効。コンパイルは含めず、`final` を実行して終了コード 4・理由 `wall_time_limit` で終了。`--max-time-ok` は適用しない |
| 乱数シード | `--seed=<n>` |
| ソース変数の初期値 | `--init-values=x\|zero\|one\|random`（既定`x`）。初期値のない4値整数変数と新規メモリ要素の既定値を選ぶ。明示初期値と不正な読み出しの標準値は保つ |
| 内部の確認 | `--dump-sir`、`--stats=json` |
| 範囲の診断（`dec94ff` から） | `--bounds-check=off\|warn`（既定は `off`）。不正な packed 選択・固定長配列の添字をソース位置ごとに 1 回警告し、最後にアクセス回数を出す。通常の読み書き・NBA の値は保つ |
| 除去されたパルスの診断 | `--report-pulses`（既定off）。慣性遅延が取り消した保留中の変化を、ソース位置と取消時刻付きの注記として毎回報告。値・スケジューリング・カウンタは保つ |
| 条件の診断 | `--cond-x=off\|warn\|error`（既定は `off`）。`if`・`?:`の条件と`case`の選択式に含むX/Zを検出。`warn`はソース位置ごとに1回警告し、評価回数を集計。`error`は設計の実行時エラーにする。値と分岐結果は保つ |
| VCD 出力 | `--wave=<file>.vcd`、`--wave-scope=<pattern>[,<pattern>...]`、`--wave-glitches`。4 値の波形。上記の dump タスクからも制御できる |

共通の入力形式は、両実行モードを対象とする`cli/common_input`の93項目で検査しています。
ファイルリストのパス、引用符、マクロ値、ライブラリ探索は、
[実行例](P1_FEATURES.ja.md#共通の入力オプション)と
[マニュアル](USER_MANUAL.ja.md#ライブラリモジュールの探索)を参照してください。

`--max-scc-iter` は受け付けますが、P3 の静的アイランドの実装までは効果がありません。
[CLI 仕様](spec/CLI.ja.md)にあるオプションがすべて実装済みというわけではありません。
各ビルドの `--help` で確認してください。`--init-values`の自動変数の寿命と
独立したシード付き乱数については[初期値の実行例](P1_FEATURES.ja.md#ソース変数の初期値)を参照してください。
FST 出力は未対応です。
実時間の上限は、イベント実行前と VM の分岐（静的初期化を含む）で検査します。
待っているホスト側の処理を強制的に中断するものではありません。final は期限に止められず完了します。

## 残っている作業と対象範囲

| 状態 | 機能 |
| --- | --- |
| P1に必要な実行機能の残り | 動的コンテナ、クラス、VPI、スイッチレベル、specifyパス、タイミング機能のうちIcarusが対応する範囲。FST、`%p`、解決した強度の`%v`、未対応の`$fread`の形、上記の制限も残る |
| P1のコンテナの接続 | 共通ヒープ・型付きSIR/VM・自動回収・ヒープ依存の待機を接続し、ソース496件で検査済み。言語側の残作業は上記を参照 |
| P2（予定、方針承認済み） | SIR v1のテキスト/パーサ・共通Op schema・kernel境界。P1と並行準備しP1後に凍結。文法/API/参照の詳細はレビュー待ち（[計画](notes/sir-v1-proposal.ja.md)） |
| P3 | 第2層のC++生成、静的アイランドの解析、RTLのさらなる性能改善 |
| 必要なIcarus対応を超える後続段階の予定 | より広いUVM、制約付きランダム化、カバーグループ、並行アサーション・サンプル値の関数、クロッキング、virtual interface、`let`、DPI-Cなど。旧P4/P5の表記を理由にP1で必要な仕組みを後へ回さない |
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
  [プラス引数の書式](../tools/gen_runtime_plusargs_matrix.py)、
  [構造体のメモリ](../tools/gen_fread_struct_matrix.py)、
  [参照引数のメモリ](../tools/gen_ref_memfile_matrix.py)、
  [バイナリ出力](../tests/cli/run_binary_format_tests.py)、
  [範囲の診断](../tools/gen_bounds_check_matrix.py)。
- 実装：[変換処理](../src/front/)、[インタプリタ](../src/vm/)、
  [ランタイム](../src/kernel/)、[driver](../src/driver/main.cpp)。

対応範囲を変えたら、日英の一覧、README の要約、該当するテストを更新してください。
使用例は `P1_FEATURES`、規定する動作は `docs/spec/` に置きます。
仕様書に予定があることだけを、実装済みの根拠にはしません。
