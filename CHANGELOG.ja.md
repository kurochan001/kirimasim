# 変更履歴

[English](CHANGELOG.md)

このプロジェクトの主な変更をこの文書に記録します。

形式は [Keep a Changelog](https://keepachangelog.com/en/1.1.0/) に基づき、
版の付け方は [Semantic Versioning](https://semver.org/spec/v2.0.0.html) に従います。

## [Unreleased]（未リリース）

まだリリースはありません。以下の「追加」は 2026-10-18 時点の動作をまとめ、
その後は利用者から見える変更を 1 項目ずつ記載しています。
記載の方針は [CONTRIBUTING](CONTRIBUTING.ja.md) を参照してください。
実装途中の項目は、その版の範囲を保ちます。[P1 対応表](docs/P1_SUPPORT.ja.md) が
現在の対応範囲と残る制限を示します。

### 追加

- `case inside` の項に、関数呼び出しなど副作用のある式を書けるようになりました。項は case の種類が定める順に評価します（12.5.4）。`inside` 演算子の要素に副作用のある式を書くことは、引き続き未対応です。

- `inside` と `case inside` の集合に動的配列とキューを書けるようになりました。固定サイズの配列の中にあってもかまいません。演算子を評価する時点の要素を表します（11.4.13、VALUE_SEMANTICS.md 3.10）。

- `inside` と `case inside` で tolerance の範囲 `[A +/- B]`、`[A +%- B]` が使えるようになりました。範囲は A の型で計算し、実数の幅は切り捨てます（11.4.13、VALUE_SEMANTICS.md 3.10）。

- class プロパティの bit、part、packed メンバーへのブロッキング書き込みが動くようになりました。実行時の位置、複合代入、プロパティの配列の要素も含みます。無効な位置には、ほかの変数と同じく何も書きません（11.5.1、VALUE_SEMANTICS.md 3.11）。プロパティへの NBA は未対応のままです。既存の単体テストの期待値 1 件は、別途承認を受けて移行しました。

- 対応範囲のclass操作を`always_comb`・`always_latch`で実行し、暗黙感度には実引数だけを加えます（[範囲](docs/notes/class-implicit-execution.ja.md)）。旧単体期待値2箇所の移行も、別途承認を得て適用しました。

- 明示的なクロックを持つ`always`・`always_ff`の本体で、対応するclass操作を実行できます。通常のトリガー評価とobject依存の監視の拒否を保ちます（[範囲と検証](docs/notes/class-clocked-execution.ja.md)）。

- VPIからstaticな4値整数net宣言全体へ、point netの即時depositと登録netのforce/releaseを実行できます。HDLとの所有共有と、確定したcallbackの値を保ちます（[範囲と検証](docs/notes/vpi-net-write-execution.ja.md)）。

- 単純なスカラーの状態依存specify `if`／`ifnone`パスを、機能到着時の条件で実行します。4値での有効判定、force中の動作、公開値の観測を保ちます（[範囲と検証](docs/notes/specify-state-execution.ja.md)）。

- 遅延アサーションのラベル指定disableが、対象文の未成熟報告だけを取り消し、兄弟報告・評価済み引数・成熟済み履歴を保持するようになりました。Observed/finalと完了済み所有者も確認しています（[仕様と検証](docs/notes/deferred-label-cancellation.ja.md)）。


- VPIの`vpiObjTypeVal`が読み取り・値変更callback・変数releaseで宣言の型を使い、
  scalarと1ビットのpacked vectorを区別します（[確認](docs/notes/vpi-object-value-formats.ja.md)）。

- VPIの変数releaseが、呼出し元の指定formatで解除後の値を返します（[確認](docs/notes/vpi-release-results.ja.md)）。

- classのtask形式`$cast`を共通の型判定で実行します。失敗時は代入先を変えず設計実行時エラーにします（[範囲](docs/notes/class-task-casts.ja.md)）。

- `--compat=default|vcs|all` を直接指定し、固定版フロントエンドのモードを選べます（[範囲](docs/notes/compatibility-execution.ja.md)）。

- 仕様にあるフロントエンドのエラボレーション上限4種を、コンパイル・parse・SIR出力で
  直接指定できます（[範囲と検査](docs/notes/elaboration-limits-execution.ja.md)）。

- ソースの`%p`・`%0p`で、対応済みの型付きスカラー・固定集約値・動的配列/キューを、
  定数/実行時の書式と合法な遅延表示で扱えるようにしました
  （[範囲](docs/notes/source-pattern-formatting.ja.md)）。

- 対応する静的な部分inoutネットポートは、値の方向付きcarrierや余分な根を作らずに物理identityを使います。固定配列要素・packed部分選択・連結と、wired/pull/supplyの実効kindを含みます（[範囲](docs/notes/partial-inout-port-execution.ja.md)）。

- classオブジェクトと、通常の信号・イベントの感度、待機、monitor、対応するイベント制御付きNBAを併用できます。共有の事前検査で、間接呼び出しやコールバックも含め、オブジェクトに依存する監視は明示的に未対応とします（[範囲](docs/notes/class-independent-observers.ja.md)）。

- 幅・符号の変換や明示キャストを要する整数 input/output ネットポートは、strong の方向付き値 carrier を保ち、配列要素・連結も含めて通常のネット解決を使います（[範囲](docs/notes/converting-port-execution.ja.md)）。

- 静的な部分 input/output ネットポートは、物理ビットごとに wired・pull・supply の実効 kind を統合し、同一性成分ごとに暗黙の根を 1 つ実体化します。未接続の隣接ビットと明示的な物理起源は保ちます（[範囲](docs/notes/port-kind-execution.ja.md)）。

- 対応する input/output ネットポートは、固定配列要素とその静的 packed 選択・連結を通じて物理強度を保ちます。ソースの alias 範囲を物理的な宣言順の序数へ対応付け、他の要素とビットの値を保ちます（[範囲](docs/notes/array-element-port-execution.ja.md)）。

- `always_ff` の式イベント制御は、実行時のビット・配列選択も含め、通常のイベント待機と型付き監視を共有します。ff の書き手・時間制御の制限は保ちます。固定・式の混在項目、iff、初期診断は既存のカーネル monitor 経路を使います（[範囲](docs/notes/always-ff-expression-execution.ja.md)）。
- 名前付き fork の disable は、通常のスコープ ID で、join/join_any/join_none、戻った呼び出し、保持された子孫の子の活動を終了します。無関係な子、予約済み NBA、確定済みレポートは保ち、待機と未確定アサーションは取り消します（[範囲](docs/notes/named-fork-disable-execution.ja.md)）。
- ソースの class ハンドル、コンストラクタ、型付きプロパティ、継承、virtual・直接関数と待機する task、interface ビュー、共変戻り値、浅いコピー、object seed、関数形式の class cast を class SIR へ変換します。固定プロパティ要素、通常の呼び出し転送、class 値のキューも同じ実行経路を使います。依存監視、プロパティ参照・選択書き込み、管理対象 event フィールドは残作業です（[範囲](docs/notes/class-source-execution.ja.md)）。
- VM の class SIR 実行：段階的な構築、フィールドの読み書き、virtual・直接関数と待機する task、コピー、cast、同一性、object seed、弱いハンドル、呼び出し・捕捉・コンテナ・保留代入の型付き root を実行します。この段階ではソースの class 変換、依存監視、管理対象 event フィールド、object 書式は残作業です（[範囲](docs/notes/class-vm-execution.ja.md)）。
- 共通 VM の heap schema 登録と型付き class 値・プロパティ転送を追加しました。保持する対象、転送 pin、class 対応の automatic 記憶域を含みます。記憶域アダプタの段階であり、class 命令の実行とソース変換は未接続です（[範囲](docs/notes/class-vm-storage.ja.md)）。
- 型付き class SIR 記述子、virtual シグネチャ、コンストラクタ段階、プロパティ・値の操作、class 変数参照を追加し、所有権、権限、制御フロー、参照の形を検証します。実行アダプタが接続されるまで class SIR は実行時確保の前に拒否し、この段階のソース class 実行は未対応です（[範囲](docs/notes/class-sir-runtime.ja.md)）。
- 共有カーネルの class object 記憶域を追加しました。型付き同一性、段階的構築、プロパティ・ビット選択、浅いコピー、virtual dispatch、class cast 検査、object RNG 状態、コンテナとの共同追跡、弱参照を提供します。保持された automatic フレームも型付き object フィールドを追跡します。実装途中の記録であり、ソース実行には SIR/front/VM 接続が残ります（[範囲](docs/notes/class-runtime-kernel.ja.md)）。
- ソース警告オプション `-W<name>`、`-Wno-<name>`、`-Werror[=<name>]` を parse・実行の両方で slang に渡し、警告名、重大度の規則、オプション順序を保ちます（CLI.md 2.1）。
- VCD に interface・program のインスタンスを含めます。Icarus 13.0 と同様、種別 `module` のスコープとし、`$dumpvars` はインスタンス階層として数えます（WAVEFORM.md 2）。従来はその変数が出ませんでした。
- stream を含む streaming の代入先（`{<<4{{>>{t}}, g}} = v`）に対応しました。各内側 stream は、packing の逆として、自身に割り当てられたソースを階層ごとに並べ替えます（11.4.14.3）。従来は未対応でした。
- 実行時に選ぶ packed 要素の中で、さらに実行時に選ぶ bit・要素への書き込み（`t[i][j] = v`、3 段も含む）に対応しました。通常・複合代入、即時・遅延・イベント付き NBA を含みます。親の範囲外の添字は書き込みません（7.4.5、11.5.1）。従来は未対応でした。その位置の実行時 part-select（`t[i][k+:4]`）は引き続き未対応です。
- 固定長 unpacked 配列のスライス（`a[l:r]`、`a[b+:w]`、`a[b-:w]`、位置の実行時計算も含む）に対応しました。値、通常・遅延・非ブロッキング代入の対象、比較、input/output/inout 実引数、構造体配列のスライスを含みます（VALUE_SEMANTICS.md 3.13、SIR_SPEC.md 8）。従来は未対応でした。配列の外の要素は既定値を読み、書き込みません。スライスの `ref` 実引数は引き続き未対応です。
- unpacked 構造体の automatic 配列、構造体配列の引数（`input`、`output`、`inout`、`ref`、`const ref`。左境界から対応付け）、それを返す関数に対応しました（6.21、13.5、7.6、SIR_SPEC.md 4.2・8）。従来は未対応でした。構造体配列内の event は引き続き未対応です。
- unpacked 構造体型の `ref`、`const ref`、`ref static` 引数に対応しました。callee は配列・文字列・64 ビット超のメンバも実引数へ即座に読み書きし、`ref static` は NBA も受けます（13.5.2、SIR_SPEC.md 8）。従来は、実行時に選ぶ構造体配列要素のメンバへの `ref`（`t(sa[i].n)`）とともに未対応でした。
- 文字列の文字への遅延付き非ブロッキング代入（`s[i] <= #1 c`）に対応しました。文字列配列要素と `ref static` の文字列も含みます。従来は未対応でした（SCHEDULER_SPEC.md 13）。
- `ref static` が指す static 変数の要素・packed ビットへの遅延なし NBA（`r[i] <= v`、`r[i][p+:8] <= v`）に対応しました。発行したスロットの NBA、program 起源なら Re-NBA で適用します（10.4.2、13.5.2）。従来は遅延形と値全体だけが動き、この形は未対応でした。
- `--init-values=x|zero|one|random` で、未初期化の 4 値整数変数と新しいメモリ要素の既定値を選べます。明示初期化と規格の無効読み取り値は保ちます（CLI.md 2.2）。
- `--report-pulses` は、対応する慣性遅延経路で取り消した保留遷移を、ソース位置・取消し時刻付きで報告します（CLI.md 2.2）。
- `--max-procs=<n>` は実行時に作る未終了の子プロセス数を制限します。超過時は final を実行し、終了コード 6 になります（CLI.md 2.7）。
- 固定・動的・packed が混在する `foreach` 次元と対応する省略を宣言順に実行し、待機中もループ状態を保持します。配列照会の実行時次元は、文書化した固定・動的配列・queue・string に対応します（SIR_SPEC.md 5、SYSTEM_TASKS.md 8）。
- fork の子は、承認済みの実行単位・寿命の契約に従い、対応する捕捉済み automatic 変数を共有し、親終了後も記憶域を保持します（SIR_SPEC.md 7.5、RUNTIME_ABI.md 4）。
- ソースのイベント制御 NBA は、文書化した型の範囲で、捕捉値・対象・制御を発行プロセスとは独立して保持します（SCHEDULER_SPEC.md 8、SIR_SPEC.md 7.3）。
- automatic のスカラー・固定配列 event は、宣言ごとに新しい既定値を持ち、対応する input 引数と宣言コピーで同一性を保ち、実行時の固定配列 event 選択に対応します（SIR_SPEC.md 7.4・8、RUNTIME_ABI.md 6）。
- 非 parameterized class の static プロパティは、共有する型付き記憶域と時刻 0 前の初期化を使います。この段階では object 構築と parameterized static 記憶域は未対応です（SIR_SPEC.md 7.1、VALUE_SEMANTICS.md 1.4）。
- 固定 unpacked 配列全体の input/output ポートと互換な net inout 接続は、宣言順に動きます。要素単位のネット解決と、対応する定数要素の force/release・遅延を含みます（SIR_SPEC.md 3.5、VALUE_SEMANTICS.md 5）。
- 6 種の有向 MOS と 6 種の受動スイッチが、対応する静的端子で完全な強度転送・共有カーネルの時間規則により動きます。この段階では登録 net のソース force と、より広い部分ポート同一性は未対応です（VALUE_SEMANTICS.md 5、SCHEDULER_SPEC.md 14）。

- 一般的な入力オプションを追加しました。include の `-I`、macro の `-D`、library の `-y` は連結・分離した引数を受け、`--libdir=<dir>` と既存の plus 形式も使えます。macro の値省略は `1`、明示的な空値・句読点は保ちます。include と library の探索は区別し、両実行モードで対応します。

- 固定配列・動的配列・queue の実行時 reduction、安定した並べ替え、min/max に対応しました。型付き `with`、入れ子コンテナのキー、保持する要素 ref、副作用のある極値キーの前の snapshot を含みます。直接単体 4 件と独自ソース 270 件で承認済み契約を検査します。副作用のある・純粋性を未検証の並べ替えキーと、未対応の集約表現は制限として残ります。
- 多次元の固定整数メモリへのバイナリ入力を追加しました。数値の昇順、右端の次元が最速、線形の start/count を使います。選択した接頭部の式は後続引数より前に 1 回捕捉し、1 次元の添字規則は変えません。独自のバイトモデル 156 件を追加しました。

- `$countbits`、`$countones`、`$onehot`、`$onehot0`、`$isunknown` は、整数 leaf を持つ対応済みの固定 unpacked 配列・構造体・構造体配列を受けます。既存の streaming 変換とカーネル counter を再利用し、独自 136 件で広幅・2 値・4 値と引数評価を確認します。未対応の記憶域と動的・文字列の bit stream は別扱いです。

- 型付き pattern layout、検証、テキスト、`pattern.format` の実行を共有 renderer へ接続しました。独立した SIR 11 件で、入れ子値、packed field、コンテナ、guard、monitor、資源制限を確認します。ソースの `%p` には front の literal/runtime format bridge が残ります。

- 共有カーネルの代入パターン renderer は、借用する型付き値、入れ子の集約、enum、引用した SV 文字列、与えられたハンドル同一性を扱います。反復的な走査、形・循環・資源の明示エラー、C++20 ヘッダ検査を新規テストで確認します。ソース `%p` は型付き SIR/front 接続まで未対応です。

- UDP の変換・検証・共有カーネル/VM 転送は 32 入力超を受けます。257 入力までの独自ソースと独立した値・SIR 検査で、一致判定、状態順、X/Z、遅延、強度を確認します。dense なエッジ表には固定 slang のコンパイルのスケール制限が残り、時間上限付きの独自再現例を記録しています。警告や依存は緩めていません。

- fork 内の宣言は親の既存の寿命・初期化規則で実行します。static 記憶域は共有し、宣言は子プロセスに数えず、宣言だけの fork は待ちません。独自 72 件で 8 文脈、広幅、全 join 種類、型付き記憶域を確認します。この段階の automatic capture には共有寿命の契約が必要です。旧拒否期待値は別レビューです（6.21、9.3.2）。

- 三状態ゲートは、33〜64 ビットの net で side のオペランドが複数 word を使う場合や短絡制御式も含め、word 境界を越える全幅の L/H side mask を保持します。追加の独自 73 件は測定した両 Icarus 版と一致します。この段階で未対応の解決 net への遅延三状態出力は、後の伝播の項目に記載します（28.6、28.12、VALUE_SEMANTICS 5.8、SIR_SPEC 3.5）。

- 固定配列・unpacked 構造体・構造体配列全体のブロッキング代入内 delay/event/repeat は、待機中に型付きの私有 snapshot を保ち、待機後に実際の対象を評価します。追加の独自 74 件で、8 文脈、広幅 4 値、automatic ref 呼び出し、コンテナ root、異なる宣言範囲を確認します。通常代入・NBA の構造体配列コピーも位置で対応付けます。過去の拒否期待値 6 件は実装と別レビューです（7.6、9.4.5、既存 SCHEDULER_SPEC 8）。

- 実行時に選ぶ固定部分配列全体の代入先と、固定配列メンバを持つ構造体の関数戻り記憶域に対応しました。構造体要素の配列メンバも含みます。この段階では明示的 automatic 集約配列宣言の既存制限は残ります。

- ブロッキング代入内の `repeat(count) @(...)` は count 全体を 1 回保持し、先に評価した RHS を保ち、待機後に対象を選びます。非正・X/Z の count は待機を省きます。独自 75 件で型付き count、8 実行文脈、混在・guard 付き event、スカラー・コンテナ snapshot を確認します。この段階のイベント制御 NBA は未対応です（9.4.5、既存 SCHEDULER_SPEC 8）。

- 共有の強度 resolver は、任意の入力区間と mask したドライバ bit に対し、4 値へ射影する前の符号付き区間全体を公開します。既存の値 API も同じ計算を使います。新規 5 件で 11 net kind の区間対 158,400 組すべてを含め、独立した点列挙器と両端点を比較します。コンパイル済み変異 4 件が従来の値だけのテストでは見逃す誤りを露呈します。この段階の MOS/switch ソース接続と強度の書式は残作業です（VALUE_SEMANTICS 5.2〜5.4）。

- 比較ツールに独自の配列極値プローブ 7 件と `reference_mismatches` の報告欄を追加しました。同じキーでも 7.12.1 に従い単一結果を期待し、参照不一致を Icarus が通る実行不足とは分けます。判定ツールの回帰も追加しました。

- 実行時の unpacked 連結は、スカラー、固定配列、動的配列、queue のオペランドを組み合わせ、入れ子値・戻り配列も扱います。queue の右辺スライスも同じ型付き構築ループを使い、範囲の切り詰め、`$`、空・X/Z 範囲、広い unsigned 境界、境界の副作用前の source snapshot に対応します。追加の独自 256 件で 8 文脈・6 幅を確認します（7.10.1、7.10.4、10.10、既存 SIR_SPEC 5.8 操作）。

- SystemVerilog の動的配列と queue を共有する型付き heap で実行します。構築、深い値コピー、整数・real・string・入れ子要素、queue method、実際の対象へ束縛する ref、切り離された cell、値全体の遅延代入、明示的 heap wait、dynamic foreach、第 1 次元の照会を含みます。独自行列は 8 文脈のソース 240 件と位置付き CLI 診断 11 件を加えます。転送した ref は空変数の束縛依存も保持します。暗黙の heap 感度と残る形は明示的な未対応で、完全な P1 同等性は未達です（7.5、7.6、7.10、10.4.2、13.5、SIR_SPEC 5.8）。

- heap に依存する SIR の監視・待機は、型付き読み取りを捕捉し、添字・構造・束縛変更後に再登録します。共有 scheduler は monitor の通し番号付き通知を予約し、呼び出し先の私有コンテナを除外し、hook の所有者を復元して資源不足を報告します。直接・呼び出し経由の ref、切り離された cell、取消し・再利用、登録上限を検査します。

- 型付き heap の読み取り・変更監視 hook は、コンテナ構造と cell 値を区別します。確定変更の後に通知し、正規化後に等しい書き込み、失敗した操作、無効な選択は通知しません。scheduler/VM の monitor 接続は上の項目に記載します。

- スケジュール領域・スロットが空になった後に型付きコンテナを自動回収し、型付き記憶域、生存する実行単位、切り離された cell ref、遅延 call 引数、scheduler が所有する値を追跡します（RUNTIME_ABI 6）。root だけの変更も回収を起動し、mark 失敗時は全値を保ちます。独自 SIR 入力で確認します。

- 第 1 層は、共有 heap で 16 種の型付きコンテナ SIR 命令を実行します。実際の対象への参照 metadata、安定した要素 ref、snapshot、位置付き method 警告、資源エラーの対応付けを含みます（SIR_SPEC 5.8）。独立した SIR テストで call、wait、packed 書き込み、確保失敗を検査します。型付き root の回収と heap 通知、言語接続は上に記載し、完全なコンテナ対応は未完了です。

- 入れ子のコンテナコピーは、共通 heap コピーでコピー先 queue ごとの上限を適用します。要素書き込み・queue method、切り詰め警告、確保失敗時の巻き戻しも含みます。実際の型の snapshot はコピー元の上限を保ちます（6.22.2、7.5、7.10.5、SIR_SPEC 5.8、RUNTIME_ABI 6）。VM 接続は上に記載します。

- 型付きコンテナ SIR 記述子、受け手の参照、16 命令、検証・テキストを追加しました。明示的 snapshot/copy/assignment と、ref 引数の queue 上限の等価性を含みます（6.22.2、7.5、7.10、13.5.2、SIR_SPEC 5.8）。VM・言語接続は上に記載し、全範囲の対応は未完了です。

- Context が所有する型付きコンテナ heap の基盤を追加しました。安定した要素参照、深いコピー、上限付き queue 操作、確保失敗の巻き戻し、世代検査付き table 再利用による正確な追跡を含みます（7.5、7.10、13.5.2、RUNTIME_ABI 6）。型付き root の公開・回収と言語接続は上に記載します。この基盤だけで動的配列・queue の全範囲を達成したことにはしません。

- 名前付き event を、信号の購読・式監視と同じイベント制御に置き、項目ごとに `iff` を持てます。偽の guard はその項目を保ち、最初の成立項目が待機全体を取り消します。C ABI 0.3 は既存 entry を変えず `wait_mixed` を末尾へ追加します（9.4.2.3、15.5、SIR_SPEC 7.2、SCHEDULER_SPEC 9.5、RUNTIME_ABI 4.2）。

- 静的境界の packed メンバ・part 内の実行時の子選択は、書き込みと複合読み取りで親境界を保ちます。slot、automatic frame、ref、static NBA・遅延 NBA の対象がカーネルの共通 clipping を使い、隣接 bit を保ちます（7.4.5、11.5.1、SIR_SPEC 5.3）。

- streaming の NBA 対象は代入内遅延を受け、通常の streaming・遅延格納と同じ並べ替え、snapshot、対象変換を使います。最後の短い block と実行時 selector も含みます（10.4.2、11.4.14.3、VALUE_SEMANTICS 8）。

- 遅延 NBA は、入れ子の固定配列・real・string メンバも含め、固定配列・unpacked 構造体全体を snapshot します。集約コピーは実行時の要素接頭部を保ち、共通の記憶域走査で各側の宣言順を維持します（7.6、10.4.2、10.9、VALUE_SEMANTICS 3.13）。

- 遅延 NBA は、実行時の固定配列添字、packed bit/part の位置、連結対象を、広幅・部分範囲も含め捕捉します。共有カーネルは task/fork 終了後も更新を保持し、発行 thread に従い NBA/Re-NBA を使います（10.4.2、9.4.5、SIR_SPEC 5.3、SCHEDULER_SPEC 5・13）。

- program は call・fork を通じて Reactive/Re-Inactive/Re-NBA で実行し、名前付き program の同一性と無名 program のサブルーチンを扱います。共有カーネルは program 完了時に子孫を終了し、動的な起源で `$exit` を実装し、initial を持つ全 program の終了後に実行を終えます。空 program と確定済み report call の既存規則は保ちます（24.3、24.3.1、24.6、24.7、SIR_SPEC 7.6、SCHEDULER_SPEC 16.1）。

- `$sscanf` と `$fscanf` は任意整数幅のバイナリ `%u`/`%z` を読みます。実行時 format と automatic/ref の宛先を含み、生の空白と NUL は入力データとして保ち、不完全な field は対象を変えません。幅 0 は承認済みの幅省略規則に従います（SYSTEM_TASKS.md 6.3、SIR_SPEC.md 8）。

- `%u` と `%z` は、任意幅の整数・packed 値も含め、非整形の 2 値・4 値 word を console・file・string format API で出力します。packed 宛先は 0 バイトを保ち、string 宛先は省きます（SYSTEM_TASKS.md 3.11、VALUE_SEMANTICS.md 7.6）。
- memory file task は `ref` 引数経由の固定メモリを受けます。ファイル出力には `const ref` も使え、転送、待機する call、多次元配列の行への参照も含みます（SYSTEM_TASKS.md 7、SIR_SPEC.md 8）。
- `--autoflush` は display/write/strobe/monitor の出力後に stdout を公開します。stdout 向けの file task も含みます（CLI.md 2.2、SYSTEM_TASKS.md 2）。
- `--stop=finish|ignore|error` は `$stop` の正常終了・継続・終了コード 1 を選びます。無視する `$stop(2)` も、最終終了時には統計を報告します（SCHEDULER_SPEC.md 16.2、SYSTEM_TASKS.md 10.2）。
- `--max-wall-time=<seconds>` はコンパイルを除く実時間を制限します。VM の分岐とカーネルのイベント境界で検査し、final を完了して、`--max-time-ok` の有無によらず `wall_time_limit`・コード 4 で終了します（CLI.md 2.7、SCHEDULER_SPEC.md 16.2）。
- `--bounds-check=off|warn` は packed 選択と固定 unpacked 配列の範囲外・X/Z 添字を報告します。広幅、automatic/ref、NBA 対象も含みます。位置ごとに 1 回警告し、終了時に件数を出し、値やイベント件数は変えません（CLI.md 2.2、VALUE_SEMANTICS.md 3.11）。
- `$sformat` と `$sformatf` は string・packed 値・関数結果などの実行時 format を受けます。数値・文字列変換、`%t` の尺度、`%m`/`%l` を共有し、値の不足・余剰は警告して続けます（SYSTEM_TASKS.md 3.13、21.3.3）。
- `$value$plusargs` は、関数結果も含め、呼ぶたびに評価する string・packed format を受けます。最初に一致する plusarg を使い、不一致なら出力とその添字式を評価・変更しません（SYSTEM_TASKS.md 9、SIR_SPEC.md 8、21.6）。
- `$sscanf` と `$fscanf` は string・packed 値の実行時 format を受けます。出力引数の不足は警告し、その field の前で読み取りを止めます。packed format の X/Z は入力せず -1 を返します（SYSTEM_TASKS.md 6.3、21.3.4.3）。
- 64 ビット超の streaming 代入先に対応しました。配列、構造体、automatic 変数、ref 引数を含み、`<<` の逆 block 順と NBA の値捕捉を使います（VALUE_SEMANTICS.md 8、11.4.14.3）。
- enum の `next(n)` と `prev(n)` は副作用のある call も含む実行時 count を受け、2 値・4 値・広幅の基底型で宣言順に巡回します（VALUE_SEMANTICS.md 7.7、6.19.5.3、6.19.5.4）。
- `--mintypmax=min|typ|max` は、実行時評価も含め、設計全体の遅延式を選びます（CLI.md 2.2、SCHEDULER_SPEC.md 12）。
- 条件省略の `for`（`for (;;)` を含む）で、通常のループ制御と時間待機を使えます（SIR_SPEC.md 7.2、12.7.1、12.8）。
- `kirimasim` コマンドで設計のコンパイル・実行（`kirimasim <file>...`）、`kirimasim parse`、`--version` を使えます。入力の `-f`、`+incdir+`、`+define+`、`--top`、`--timescale` と、`$test$plusargs`・`$value$plusargs` 向けの実行時 plusargs を含みます（CLI.md 1・2）。
- 実行の制限として `--max-time`、`--max-time-ok`、時間が進まない実行の `--max-zero-delay`、`--max-steps` を追加しました。それぞれ固有の終了コードとメッセージを持ちます（CLI.md 2.5・2.6・5）。
- 未対応の構文はすべてソース位置付きで報告し、実行前にコード 2 で終了します。無言で省略しません（CLI.md 5）。
- 中間表現のテキストを出す `--dump-sir` と、終了時のイベント counter を出す `--stats=json` を追加しました（SIR_SPEC.md 9、PERFORMANCE.md 3）。
- 実行カーネルの scheduler は IEEE 1800-2023 の全領域と参照アルゴリズムを扱います。Observed の保留 report 確定、Postponed の `$strobe`・`$monitor` を含みます（SCHEDULER_SPEC.md）。第 1 層は SIR を解釈して設計を実行します。
- 設計構造：module、ポート（`input`、`output`、`inout`）、parameter・`defparam`、`generate`、instance 配列、interface・modport、package・`$unit`、階層名、specparam。
- データ型：任意幅の 2 値・4 値整数型、`real`、`shortreal`、`realtime`、`string`、enum、packed/unpacked 構造体、packed union、固定 unpacked 配列（多次元・構造体配列）、unpacked 配列 parameter、名前付き event、`chandle`（null のみ）。
- ネット：`wire`、`tri`、`wand`、`wor`、`tri0`、`tri1`、`supply0`、`supply1`、`uwire`、駆動強度・複数ドライバ解決、連続代入・net 遅延、組み込み gate・pull、UDP、`alias`（VALUE_SEMANTICS.md 5）。
- 手続きと時間：`initial`、`always`、`always_comb`、`always_latch`、`always_ff`、`final`、代入内遅延を持つ通常・非ブロッキング代入、`iff` 付きも含む信号・式の遅延・イベント制御、`wait (expr)`、名前付き event、`fork` と `join`・`join_any`・`join_none`、`wait fork`、`disable fork`、`disable`、`assign`/`deassign`、`force`/`release`。
- 関数・task：static・automatic、再帰、`input`、`output`、`inout`、`ref` 引数、既定引数、unpacked 構造体・配列の引数と戻り値。
- 文・式：`if`、`case`、`casez`、`casex`、`case inside`、違反報告付きの `unique`・`unique0`・`priority`、`break`・`continue`・`return` 付きの全ループ、streaming・`inside`・wildcard equality・代入演算子・増分を含む全演算子、代入 pattern（VALUE_SEMANTICS.md 3）。
- 即時アサーション（`assert`、`assume`、`cover`）と遅延形（`#0`、`final`）。
- システム task・関数：表示・書式（`$display`、`$write`、`$strobe`、`$monitor`、`$sformat`、`$sformatf`、`$swrite`）、`$sscanf`、時刻（`$time`、`$realtime`、`$timeformat`、`$printtimescale`）、終了（`$finish`、`$stop`）、severity（`$fatal`、`$error`、`$warning`、`$info`）、変換、`$bits`、`$typename`、配列照会、`$clog2`・real 数学、bit-vector 関数、`$test$plusargs`、`$value$plusargs`（SYSTEM_TASKS.md）。

- `%d`、`%h`、`%o`、`%b`、`%c`、`%s`、`%t` で、`-`（指定幅または自動幅の左寄せ）と `0`（`%d` の符号後を 0 埋め）の flag を使えます。規格に定義がないため format ごとに 1 回警告します。`+`、空白、`#`、`%e`・`%f`・`%g` 以外の精度は未対応です（SYSTEM_TASKS.md 3.12）。従来の `-` は internal error（コード 3）、`0` は幅の一部として扱っていました。
- ファイル出力：`$fopen`、`$fclose`、`$fflush`、`$fdisplay`、`$fwrite`、`$fstrobe`、`$fmonitor` と b/o/h 版を mcd・fd 向けに使え、`$fgets` で string へ入力できます。`$fclose` は閉じるファイルの `$fstrobe`・`$fmonitor` を終了し、開いていない記述子は位置ごとに 1 回警告します（SYSTEM_TASKS.md 6）。従来は未対応でした。
- ファイル入力：`$fgetc`、`$ungetc`、`$fscanf`、変数・メモリへの `$fread`、`$ftell`、`$fseek`、`$rewind`、`$feof`、`$ferror`、整数への `$fgets` に対応し、STDIN は標準入力を読みます。mcd・閉じたファイルの読み取りは位置ごとに 1 回警告し、書き込み専用ファイルの読み取りは `$ferror` が報告するエラーになります（SYSTEM_TASKS.md 6.3・6.4）。従来は未対応で、STDIN の `$fgets` も入力しませんでした。
- `$stime`、`$timeunit`、`$timeprecision`（引数なし・instance・`$unit`・`$root`）と、`work.<module>` を表示する `%l` に対応しました（SYSTEM_TASKS.md 3.8・5.1・5.2）。従来は未対応でした。
- 1 次元以上のメモリへの `$readmemb`、`$readmemh`、`$writememb`、`$writememh` を、アドレス引数の有無の両方で使えます。範囲外アドレス・word 数不一致は警告し、読めないファイルはコード 2 で終了します（SYSTEM_TASKS.md 7）。従来は未対応でした。
- `$random`、`$dist_uniform`、`$dist_normal`、`$dist_exponential`、`$dist_poisson`、`$dist_chi_square`、`$dist_t`、`$dist_erlang` は、SYSTEM_TASKS.md 12 の説明から実装した IEEE 1800-2023 Annex N の値を返します。正が必要な引数が 0 以下なら `kirima-dist-argument` を警告します。従来は未対応でした。
- 波形：`$dumpfile`、`$dumpvars`、`$dumpoff`、`$dumpon`、`$dumpall`、`$dumplimit`、`$dumpflush` は、各時刻スロット末尾の値を 4 値 VCD へ書きます。`--wave=<file>.vcd`、`--wave-scope`、`--wave-glitches` からも出力できます（WAVEFORM.md）。遅すぎる `$dumpvars`・`$dumpfile` と、`--wave` 使用中の呼び出しは警告して無視します。従来は task が未対応、option が不明でした。FST は未対応です。
- enum 型のメモリへの `$readmemb`・`$readmemh` に対応しました。どの enum 項目の値でもない word はコード 2 で終了し、それより前の word は読み込み済みのままです（SYSTEM_TASKS.md 7.1、21.4.2）。従来は未対応でした。
- unpacked 構造体のメモリへの `$fread` は、各要素のメンバを宣言順に読みます（SYSTEM_TASKS.md 6.3、21.3.4.4）。従来は未対応でした。
- `$stacktrace` を task・関数として使えます。呼び出しのスコープ・位置と、そこへ至る各 call を表示します（SYSTEM_TASKS.md 10.3）。従来は未対応でした。
- `$urandom` と `$urandom_range` は、18.14 に従い seed を与えた各手続き・fork branch の生成器を使います。`--seed` と version 行の seed 表示も追加しました（SYSTEM_TASKS.md 13）。従来は関数が未対応で `--seed` も不明でした。
- `randcase` はプロセスの生成器を使い、重みに従って branch を選びます（SYSTEM_TASKS.md 13.3、18.16）。従来は未対応でした。
- `--max-call-depth=<n>` で 1 プロセスの call の入れ子上限を指定できます。既定は 10000 で、それより大きい指定では simulation thread の stack も比例して増えます（CLI.md 2.7、RUNTIME_ABI.md 5）。
- 固定配列全体のポートに対応しました。net 配列接続は各宣言の範囲を保ちながら要素をポートと共有し、値接続はコピーします。net 配列はドライバ強度を要素ごとに解決します。定数要素の force/release、遅延、未接続の既定値・pull も通ります（7.6、23.3.3、SIR_SPEC.md 3.5、VALUE_SEMANTICS.md 5）。従来は未対応でした。
- 解決 net を駆動する遅延三状態ゲートは、完全な状態を伝播します。4 値が x のままでも L/H side の遅延変化は変更であり、等しい保留状態は期限を保ちます（28.6、28.16、SCHEDULER_SPEC.md 12、VALUE_SEMANTICS.md 5.8）。従来は未対応でした。

### 変更

- 未改変の slang を `6fbb158f` に更新しました。program の parameter を、`defparam` などで外から指せるようになります（slang #2093）（[確認の記録](docs/notes/slang-6fbb158f-update.ja.md)）。
- テストスイートの判定は、`:incdirs:` と `:defines:` を `+incdir+` と `-D` として kirimasim に渡すようになりました（TEST_SUITE.md）。

- 標準が認めるコードを拒んでいた slang のエラー `index-oob`、`string-int-concat`、`string-in-constraint` を、既定で警告にしました（7.4.5、11.4.12.2、18.3）。`-Werror=<名前>` でエラーに戻せます（CLI.md 2.1）。

- テストスイートの判定は、`:expected_exit:` があるテストでだけ終了コードを比べるようになりました。ない場合も、内部エラーとクラッシュは失敗にします（TEST_SUITE.md）。

- P1 の終わり方を、Icarus 13.0 以上の実行範囲を目指す形から、閉じた一覧に変えました。固定した 188 プローブの Icarus 13.0 との比較を含む 4 つの条件で判定します。一覧の外の形は P4 と P5 に移します。どの設計の結果も変わりません（[P1_SCOPE](docs/spec/P1_SCOPE.ja.md#閉じた完了一覧2026-11-07-に承認)、[提案](docs/notes/p1-closed-exit-proposal.ja.md)）。

- 未改変slangを`d02b7cda`へ更新し、#2072の回復処理とstatic関数の戻り値の寿命を
  修正します。追加された配列の互換緩和でも実行用driverの厳密な検査を保ちます
  （[確認](docs/notes/slang-d02b7cda-update.ja.md)）。

- Icarusの`wire real`拡張をP1の必須条件から除きました。確認済みの`real_net`を生の失敗は変えずに
  対象外とし、標準real機能と独立した不具合は既存の扱いを保ちます
  （[決定](docs/notes/p1-wire-real-exception.ja.md)）。

- `atoi`、`atohex`、`atooct`、`atobin` は slang の定数評価と同じく先頭の `-` を 1 つ読み、`"-5"` は従来の 0 から -5 になります（6.16.9、VALUE_SEMANTICS.md 7.6、2026-10-28 承認）。先頭桁より前の underscore も読み、`"_5"` は 5 です。この点は slang と異なるため、front は parameter 以外の変換をどの位置でも実行時に計算し、parameter だけは slang の値を保ちます。従来は `P = "-5"` の `P.atoi()` が代入で -5、`$display` 引数で 0 でした。

- P1 の目標を Icarus Verilog 13.0 以上の実行範囲へ改め、必要な後期フェーズの機構を前倒ししました。バイナリ `%u`/`%z` 入力の意味も承認され、幅 0、生の空白・NUL、部分的 EOF の選択を含みます（P1_SCOPE、SYSTEM_TASKS 6.3）。

- 遅延なしの NBA は保存値を直接使い、スロット末尾まで保持しながら、更新ごとの一時 heap 確保を省きます（SCHEDULER_SPEC.md 5・13、#28）。
- 終了プロセスの回収 queue を再利用し、file monitor がないときはその完了検査を省いて、第 1 層のプロセス起動・再開の負担を減らしました（SCHEDULER_SPEC.md 10.8、SYSTEM_TASKS.md 6.2、#28）。
- ビルド・テストに Python 3.10 以上が必要になりました。テストツールは既にその構文を使っていましたが、CMake は 3.8 を要求していました。
- 設計の実行時エラーはコード 2 と、文の位置の `<file>:<line>: error: <text> [<instance> @ <time>]` 1 行で終了し、`final` も実行します（CLI.md 5、SCHEDULER_SPEC.md 16.2）。対象は、従来無診断または internal error（コード 3）だった Postponed からの書き込み・event、従来 internal error だった関数から外側ブロックへの disable、従来イベントを無言で失った最大時刻超の遅延です。
- call の入れ子超過は、従来の error・コード 2 から resource_limit・コード 6 に変わり、診断は option を示す `calls nested more than <n> deep (--max-call-depth=<n>)` になります（SCHEDULER_SPEC.md 16.2、CLI.md 2.7）。
- 書き込み専用ファイルへの `$ungetc` は、開いている他のファイルと同じく 0 を返します。その後の読み取りは、押し戻した文字を後入れ先出しで返してから EBADF になります（21.3.4.3、SYSTEM_TASKS.md 6.3）。従来は macOS の C library と同じ -1 でした。
- 最後のファイルを閉じた `$fmonitor` は、生成した frame と process を解放します。monitor の開閉を繰り返す設計でも呼び出しごとにメモリが増えず、一定に保たれます（SYSTEM_TASKS.md 6.2、#37）。
- module の default clocking 下の checker は `no inferred clock in this context` で止まらなくなりました。`kirimasim parse` は受理し、実行は clocking block と checker を未対応と報告します（P4、2026-10-24 の改番前は P3、16.14.7、17.4、#24）。
- フェーズを改番しました。新 P2 で SIR v1 と kernel 境界を第 2 層向けに整え、C++ 生成は P3、広い testbench 機能は P4、switch-level・並列実行・埋め込みは P5 へ移します。層番号と P1 に必要な範囲は変えません（docs/notes/phase-renumbering.md）。
- 別プロセスが、呼び出した task や遅延アサーションの action を通じて、`always_comb`・`always_latch`・`always_ff` が書く変数へ書く場合をエラーにしなくなりました。規格が明確でないため意図的に許す slang に従います（slang issue #2040）。通常の変数書き込みとして実行します（SCHEDULER_SPEC.md 6.4、SIR_SPEC.md 3.4）。従来は front がコード 2 で報告しました。

### 修正

- `%t` は、符号付きの引数を符号つきで、64 ビットを超える引数を丸ごと出力するようになりました。これまで負の値は、時間の単位によって異なる大きな符号なしの数（`int t = -1` が 4294967295 や 18446744073709550616）になり、幅の広い値は上位のビットが捨てられていました（SYSTEM_TASKS.md 3.7）。
- `$readmemb` と `$readmemh` のファイルの中のアドレスは、64 ビットを超える数なら範囲の外として扱うようになりました。これまでは下位の 64 ビットだけを使っていたため、`@10000000000000002` は警告なしでアドレス 2 に読み込まれていました（21.4、SYSTEM_TASKS.md 7.1）。
- 継続代入が駆動する変数を VPI から release する（`vpiReleaseFlag`）と、`release` 文と同じく、駆動側の値に戻るようになりました。これまでは、設計が同じ変数を文でも force していない限り、駆動側の値が変わったあとも force した値のままでした（10.6.2、SCHEDULER_SPEC.md 14.3）。

- 各側で関数を呼ぶ（または、上の修正以降はクラスのハンドルを通して読む）条件演算子の連なりで、コンパイルの時間とメモリが段ごとに倍になることがなくなりました。深さ 18 の連なりは 10 分たっても終わりませんでしたが、1 秒未満で終わります（11.4.11）。

- 要素の型が符号付きのキューや動的配列で、packed な要素の一部やビットに代入しても（`logic signed [3:0][7:0] q[$]` に対する `q[0][1] += 1` など）、内部エラーで終わらなくなりました（7.4.5、11.8.1）。

- `%m`、`%l`、`$printtimescale` は、`%` を含む名前（エスケープした識別子）をそのまま出力するようになりました。これまでは名前が書式の一部として読まれ、内部エラーで実行が終わっていました（21.2.1.6、SYSTEM_TASKS.md 3.8）。

- 64 ビットを超える整数のオペランドと real のオペランドを `&&`、`||`、`->`、`<->` でつないだとき、real を 0.0 でなければ真として扱うようになりました。これまでは常に偽として扱っていました（11.4.7、VALUE_SEMANTICS.md 3.5）。

- `&&`、`||`、`->`、`?:` が飛ばすオペランドは、クラスのハンドルを通してメンバーを読む式でも評価しなくなりました。`h` が null のとき、`h != null && h.v == 3` という書き方が「null class handle access」で終わらなくなります（11.3.5、11.4.11、VALUE_SEMANTICS.md 3.1）。

- covergroup の `type_option` を読み書きしたときに kirimasim がクラッシュしなくなりました。未対応であることを診断で伝えます。

- 文字列の `===` と `!==` で内部エラーが出なくなりました。`==` と `!=` と同じように比較します（表 6-9、11.4.5）。

- real または shortreal を含む `===` と `!==`、クラスハンドルの `==?` と `!=?` は、内部エラーではなく診断を出して拒否するようになりました（表 11-1）。

- コンストラクタの引数に既定値があるクラスを継承し、`super.new` の呼び出しを書かないときに、内部エラーで止まらなくなりました。暗黙の呼び出しは既定値を使い、`function new(default)` と `super.new(default)` は展開された引数を渡します（8.7、8.17、13.5.3）。

- `priority case` は、修飾のない case と同じく、最初の一致で項の評価をやめるようになりました。以前は一致の後の項も評価し、その副作用も起きていました（12.5、12.5.3、SCHEDULER_SPEC.md 15.1）。

- 時刻 0 で、継続代入を先に、次にその他の `always`、次に `initial` を開始するようにしました。ソースの書き順にはよりません（SCHEDULER_SPEC.md 6.1）。これまでは、`assign` より前に書いた `initial` が、代入された値ではなく x を読んでいました。

- すべての program の終了や `$exit` で実行が終わるときも、標準エラーに終了の理由の行（`kirimasim: info: finish at <時刻>`）を出します。24.3 と 24.7 の暗黙の `$finish` には引数がないので、レベル 1 を取ります（20.2）。明示的な `$finish(0)` は引き続き何も出しません。標準出力と終了コードは変わりません。

- VPIの`cbNextSimTime`で時刻の数値を無視し、読み取り専用コールバックからも登録できます。形式の検査と、実際の次スロットでの実行は保ちます（[範囲と検証](docs/notes/vpi-next-time-callback.ja.md)）。

- `always @*`が呼ぶ通常関数の本体で、対応するcontainerを読めます。暗黙監視が必要なものとして過剰に拒否せず、実引数だけのトリガーと実際の読み取りの境界を保ちます（[範囲](docs/notes/wildcard-function-dependencies.ja.md)）。

- VPIのpackedビット選択を親宣言と独立した符号なしの型で扱い、選択した1を整数・実数・十進文字列・callbackで正しく返します（[範囲と検証](docs/notes/vpi-packed-bit-values.ja.md)）。

- 警告や互換モードで診断を緩めたソースの書込み競合を、SIR変換前の位置付きコンパイルエラーで拒否し、厳格な実行時のwriter規則を保ちます。

- class SIR の metadata を小さくし、sanitizer ビルドで深い式を変換するときにも従来の native stack 予算を保つようにしました。
- unpacked 構造体型またはその配列の output/inout 引数は、SIR_SPEC.md 8（4.9.7）に従い、戻るときの添字が選ぶ要素へ書き戻します。従来は call 時に添字を決め、callee が変えると古い要素に書き、待機する task は内部 verifier エラーになりました。
- 1 スロット内で起こし合う NBA（`--max-slot-events=0` の場合だけ継続）がメモリを無制限に保持しなくなりました。遅延なし NBA の保留がなくなれば保存値をすぐ解放します（SCHEDULER_SPEC.md 5、2026-10-30 承認）。従来は約 840 MiB/秒増えました。`--stats=json` に `nba.slot_peak` を追加しました。
- 文字列の文字への NBA は、遅延・イベント制御の有無にかかわらず、適用時の文字列へ 1 バイトを書きます（SCHEDULER_SPEC.md 13、2026-10-30 承認）。従来は発行時の文字列全体を書き戻し、`s[0] <= "a"; s[1] <= "b";` は 2 文字目だけを残し、途中の書き込みも戻していました。
- `&&`、`||`、`->`、`?:` は、評価しない operand の call が `inside`・代入 pattern・streaming 連結の中でも、その関数呼び出しを省くようにしました（11.3.5、11.4.11、VALUE_SEMANTICS.md 3.1・3.9）。従来は副作用まで実行していました。
- 先頭桁の前に underscore のある定数 string の `atoi` と、x/z bit を持つ定数の単項 plus は、`inside`・代入 pattern・streaming 連結の中でも実行時の値を使います（SIR_SPEC.md 6）。従来は slang の定数を使いました。
- `priority if` は条件を記述順に、最初の真まで評価します（12.4.2、SCHEDULER_SPEC.md 15.1）。従来は `unique`・`unique0` と同じく全条件を評価し、後続条件の副作用も起こしました。
- SIR verifier は、slot operand の値命令、slot 以外の `load`、64 ビット超または幅との積が wrap する `repl` count、位置が wrap する `partsel` を拒否します（SIR_SPEC.md 5.1・10）。従来は通過し、実行層が buffer 外を読んだり、層ごとに異なる結果を出したりしました。
- 整数から `shortreal` への変換を、最も近い float へ 1 回で丸めます（VALUE_SEMANTICS.md 1.5）。従来は double を経由し、有効桁が 53 ビット超で tie の少し先にある `64'h1000_0010_0000_0001` などが 1 段低くなりました。
- CODEGEN_SPEC.md の Floating point に従い、本体も `-ffp-contract=off` でビルドし、arm64 と x86-64 の `real` の bit を一致させます。従来の arm64 は乗算と加算を融合しました。
- 遅延付き sequential UDP の出力にも、遅延なしと同じく時刻 0 から `initial` 文の値を置き、後の変化だけに遅延を適用します（29.7、VALUE_SEMANTICS.md 5.9）。従来は遅延まで x でした（#26）。
- sequential UDP の instance 配列を実行できるようにしました。従来は全要素が同じ名前を持ち、状態 slot が衝突して internal error になりました（28.3.6）。module instance 配列と同様、各要素を添字付きで命名します。
- 広幅値を持たない設計の第 1 層の速度を復元しました。64 ビット超の対応後は全命令で operand 幅を調べ、bench が 8〜22% 遅くなっていました。経路選択を設計の読み込み時 1 回に移しました（#28）。
- 各命令で行う Postponed からの書き込み検査を稀な分岐と明示し、compiler が通常経路を先に配置するようにしました。bench 3 設計で第 1 層が 4〜9% 高速化しました（#28）。
- 遅延アサーションの action は、実行元の `always_comb`・`always_latch`・`always_ff` の一部ではなく、複数のその block や他プロセスが action の書く対象へ書けます。従来は SIR verifier の internal error でした。別プロセスの遅延 action 経由で block の変数に書く場合は、slang が報告しない書き込み位置のエラーと assertion 位置の note を出します（SCHEDULER_SPEC.md 6.4、#22）。
- 即時 assertion は action block が待つときだけ待ち、遅延 assertion 自体は待ちません。従来は本体に assertion がある `always_ff` を未対応とし、`always_comb`・`always_latch`・`always_ff` が呼ぶ assertion 付き task は SIR verifier の internal error でした（16.3、16.4）。
- 9.2.2 に従い、`always_comb`・`always_latch`・`always_ff` の呼ぶ task が他プロセスも書く変数へ書けます。従来は SIR verifier の internal error でした。別プロセスが task を通じて block の変数へ書く場合は、slang が報告しない書き込み位置のエラーと call 位置の note を出します（SCHEDULER_SPEC.md 6.4、#21）。
- `always_comb` と `always_latch` は、呼んだ task の本体だけが読む変数、または自分や呼んだ関数の assertion action だけが読む変数の変更では再開しなくなりました。task 引数と assertion の式は引き続き感度に含めます（SCHEDULER_SPEC.md 6.3、#21）。
- 1 文だけの `fork`・名前付き block の位置を、文ではなく先頭 token にしました。従来は fork の実行時エラーと `--dump-sir` の fork・子 process・scope 命令がその文の行を示しました（#20）。
- 変数を書く関数を複数の連続代入から呼べます。書き込みは手続き的（6.5）で最後の値を採用し、従来の SIR verifier の internal error を解消しました。この変数は `force` 後の release でも値を保ちます（10.6.2）。従来は強制値を失いました（#19）。
- real の定数遅延は変数と同じく、double に 10 の累乗を掛け、半分を 0 から遠い側へ丸めて精度へ変換します（SCHEDULER_SPEC.md 1）。従来の定数は十進テキストで丸め、1ns/10ps の `#1.005` は 101 step、同値の `real` 変数は 100 step でした（#18）。
- 戻った後の仕事（自身や呼ぶ関数の NBA、`->>`、`fork ... join_none`）を予約する関数を、static 変数の初期値、連続代入、手続き的 `assign`・`force` の右辺、`final` から呼ぶと、13.4.4 に従うコンパイルエラーになります（SCHEDULER_SPEC.md 6.5）。従来は実行、または final で無言の無処理でした（#13）。
- static 変数の初期値が、kirimasim の順序でまだ初期化していない別 instance または自身の instance・package・コンパイル単位の static 変数を読む可能性があると、SCHEDULER_SPEC.md 3 の警告を出します。従来は無言で順序依存でした（#16）。
- `#(-2.0)` などの負の real 定数遅延を、変数と同じく精度へ丸め、step 数を unsigned 64 ビットとして扱います（9.4.1、SCHEDULER_SPEC.md 8）。従来は未対応でした。NaN・無限の定数も未対応でしたが、遅延 0 になります（#12）。
- `` `unconnected_drive pull1``・`pull0` 下の未接続 input は、22.9 に従い net の全 bit に pullup・pulldown を置きます（VALUE_SEMANTICS.md 5.3）。従来は無言で `z` でした。変数 input は型の初期値を保ち、警告します（UNSPECIFIED_BEHAVIOR.md N10、#17）。
- `--top` がない場合、33.5.4 に従いソースの config の design が挙げる cell を top とし、規則で instance を束縛します。従来は config を無視し、どこからも instance 化されない全 module を実行しました。config が複数なら 2 つ目で `--top` を求めるエラーになります（CLI.md 2.1）。
- 自身の駆動値を読む連続代入・gate・UDP、自身が保持する値を読む手続き的 `assign`・`force` は、自身の更新が読み値を変えた場合も再評価します。従来は 1 回で止まり、`assign a = en ? ~a : 1'b0;` が `--max-slot-events` まで反復せず安定していました（SCHEDULER_SPEC.md 5・12、#15）。
- 多数プロセスのコンパイル時間の 2 乗増加を解消しました。SIR verifier の共有書き込み検査（SIR_SPEC.md 10、規則 6）は、全プロセス対の比較から slot ごとの書き込み sort に変わりました。各 1 bit を書く 40000 個の `always` は 1.8 秒から 0.12 秒となり、診断と順序は同じです（#14）。
- 1 プロセスで 10000 を超える call の入れ子は、call 位置で `final` とともに終了します（Changed のとおりコード 6）。従来は internal error（コード 3）で、待機する task の call は無制限、先に stack が尽きる asan などは crash しました。simulation は 512 MiB stack の thread で動きます（RUNTIME_ABI.md 5、#6）。
- SIGINT（Ctrl-C）は次のイベント前または次のループ分岐で停止し、`final` を実行して既存の出力を保ち、コード 130 で終了します。2 度目は即時終了します。従来はすぐ kill され、final と buffer の出力を失いました（SCHEDULER_SPEC.md 16.2、#9）。
- 同じ領域で時間を進めず起こし合うプロセスを、1 スロットの実行イベント上限 `--max-slot-events`（既定 100000000）で止め、`final` とコード 5 で終了します。従来の `--max-zero-delay` と `--max-steps` は数えず、終了しませんでした（CLI.md 2.6、#8）。
- 最大時刻を超える遅延は形式を問わず設計の実行時エラーになります。従来は実行時の step 変換が 64 ビットに入らないと無言で wrap し、定数は未対応の遅延値と報告しました（#7）。
- static 変数の初期化式が呼ぶ関数の unique/priority 違反は、他と同じく時刻 0 で報告します。従来は crash しました（#5）。
- `@(posedge clk or negedge rst_n)` などの複数項目の待機は、起動させなかった項目を残さなくなりました。従来は他の信号が静止している間、clock ごとにメモリが増えました（#4）。
- real の `case` は式と項目を real として比較します。従来は整数へ丸め、1.6 が 2.0 に一致しました。string の `case` は文字列で比較し、従来の bit 数なしの未対応値という診断を解消しました（#10）。
- DPI-C import（`import "DPI-C" ...`）は呼ばれなくても宣言位置で未対応と報告します。従来はすぐ戻る空のサブルーチンとして実行しました（#11）。
- `$finish`・`$stop` の引数 1（既定）・2 は、stderr に `<file>:<line>: $finish at <time>` を報告し、2 では終了時に統計も出します（SYSTEM_TASKS.md 10.2）。従来は無表示でした。
- 要素が複数 bit の packed 配列の範囲選択、例えば `logic [3:0][7:0] a` の `a[2:1]`・`a[i +: 2]` は要素全体を読み書きします。従来は添字を bit 位置と扱いました（#3）。
- 64 ビット超の引数・変数の後にある `ref` 引数を task で待つ場合、実際に指す変数の更新で起きます。従来は別変数を待つか crash しました（#1）。
- operand に call・代入を含む 64 ビット超の条件演算子 `?:` が値全体を保持します。従来は下位 64 ビットより上を失いました（#2）。
- 第 1 引数が string の `$fopen`・`$dumpfile`・`$readmemh` などを、未対応のシステム task・関数として報告します。従来は string method と報告しました。
- 呼び出し前に階層名で参照した関数の static 変数は、その関数の私有のままです。event 式や `$strobe` 引数からの関数 call も書き込めます。従来はソース順により、変数を書く event 式として拒否するか Postponed の書き込みで停止しました。
- 関数の static 変数を階層名で読む式・条件の wait と `$monitor` が、変更で起きるようになりました。従来は起きませんでした。また wait 式の関数が event を trigger しても、その event に待機が依存しなくなりました。従来は無限反復する場合がありました。
- `$sformat`・`$sformatf` は、string parameter など literal 以外の format を表示値として出していました。定数 format は format として読み、引数数の誤りはエラーになります。実行時だけに分かる format は Added の項目に記載します（SYSTEM_TASKS.md 3.13）。
- 別 instance から階層名で呼ぶ function・task（`m1.f()`）を、宣言元 instance のサブルーチンとして扱い、static 変数は 1 回だけ初期化します（SIR_SPEC.md 3.3）。従来は 2 個目のコピーを作り、static も再初期化しました。
- 配列引数に関数の配列戻り値の要素（`sum(g()[1])`）を渡すと、従来は 2 回呼び出しました。input 引数を call 時に 1 回評価するようにしました（SIR_SPEC.md 8）。
- fork の `join`・`join_any`、`wait fork`、block disable 後の再開で、保留の unique/priority 違反や失敗した遅延 assertion を捨てていました。イベント制御・`wait` からの再開、`always_comb` の新しい開始だけで捨てるようにしました（SCHEDULER_SPEC.md 15.4）。
- slang が受理する約 1000 段の文・式の入れ子が macOS のコンパイル中に crash しなくなりました。slang の全処理を、512 KiB の system 既定 stack の thread pool から、driver が寸法を決めた thread へ移しました（RUNTIME_ABI.md 5、#35）。
- driver の 512 MiB thread 以外も含め、どの thread・build でも stack が尽きる前に止まります。残り 1 MiB 以下で設計の call を行うと resource_limit（コード 6）と `<file>:<line>: error: the stack of this thread is nearly used up [<instance> @ <time>]` で終了し、front は `nested too deeply for the stack of this thread` を出します（RUNTIME_ABI.md 5、#38）。front の変換前に深い式を束縛する slang 自身の再帰は未検査です。
- display/file monitor の callback 終了時、frame を解放する前に登録と heap 依存を解除します。終了済み callback を後の cell 変更で再呼び出しせず、他の file monitor は保ちます。新規ソース 20 件と scheduler の寿命・資源失敗検査で、queue・動的値、ファイル閉鎖、記述子再利用、hook 復元を確認します。

- display monitor はコンテナハンドルを置き換えず、既存 queue cell の変更を監視できます。実際の Postponed callback で heap 読み取りを捕捉し、登録の寿命とともに解除します。heap を読む file monitor の新しい組合せは、上記の後続行列と callback の寿命検査で確認します。

- 集約の通常・非ブロッキング代入は、承認済みの評価方針に従い、実行時の対象 selector より前に RHS 全体を捕捉します。コンテナ leaf は後の field の副作用より前に snapshot します。独自 72 ソースで 8 文脈、広幅・2 値・4 値、real/string メンバ、実行時接頭部、深い値を確認します（VALUE_SEMANTICS 3.1・3.13、SIR_SPEC 7.3。IEEE は両側の相対順序を未規定とします）。

- 整数 `$fread` を、承認済み SIR_SPEC 8 の 4.9.7 の copy-in/copyback 解釈へ戻しました。書き込む結果は copyback 時に対象を選び、0 バイトなら書き戻しません。以前の対象を 1 回捕捉する規則と機構の期待値 9 件が誤りでした。期待値修正と実装を別レビューブランチへ分離しています。

- 整数 `$fread` の宛先は任意の start/count を受けますが、入力の制限には使いません（21.3.4.4）。指定した式は承認済みの左から右の順に各 1 回評価します。copy-in/copyback は上記の修正済み SIR_SPEC 8 に従います。独自行列は 9 文脈・8 幅・両値領域の 153 ソースを追加し、既存期待値は保ちます。

- コンテナ cell への NBA は、6.21・10.4.2 の不正な形として位置付きで診断し、接続対応表を訂正しました。これは規格の制限であり、P1 機能の不足ではありません。新規ソースは動的配列・queue の要素全体・bit・part を検査します。対応する static コンテナ変数の値全体への NBA は別扱いです。

- `string.atoreal()` は先頭の `+`・`-` を 1 つ読み、5.7.2 の実数定数を続けて読みます。`realtoa` の負数も読み戻せます（6.16.10、6.16.15、VALUE_SEMANTICS.md 7.6、2026-10-28 承認）。5eae2ecf 以降は先頭符号が 0 となり、P1 母集団 4 件が失敗していました。underscore は除去します。先頭空白・先頭の小数点・後ろに数字がない符号は引き続き 0 です（#57）。
