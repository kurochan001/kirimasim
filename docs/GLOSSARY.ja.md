# 用語集

[English](GLOSSARY.md) · [文書案内](README.ja.md) · [プロジェクトREADME](../README.ja.md)

[プロジェクトREADME](../README.ja.md)と[各ドキュメント](README.ja.md)で使う用語です。

**2026-11-02更新。** 対応の有無は[P1対応機能](P1_SUPPORT.ja.md)、操作は[FAQ](FAQ.ja.md)を参照してください。
用語の説明は、未実装のインターフェースを有効にするものではありません。

- [構成](#構成)・[値](#値)・[記憶域と寿命](#記憶域オブジェクト寿命)・[スケジューリング](#スケジューリング)・[検証](#テストと仕事の進め方)
- 最近の実装で使う語：[型付きwatch](#typed-watch)、[iff](#iff)、[alias view](#alias-view)、[物理ドライバの出所](#physical-origin)、[参照側が通る不足](#passing-reference-gap)

## 構成

| 用語 | 意味 |
| --- | --- |
| slang | kirimasim が改変せずに、1 つのコミットに固定して使う SystemVerilog のフロントエンドのライブラリ（構文の解析、エラボレーション、型の検査）。[互換プロファイル](notes/compatibility-execution.ja.md)は明示して選ぶ |
| <a id="elaboration"></a>エラボレーション | ソースコードからインスタンス階層を展開し、パラメータや型を確定する処理。slang が行い、実行時とは別の[フロントエンドの評価上限](REFERENCE_MANUAL.ja.md#フロントエンドのエラボレーション上限)を使う |
| `front` | slang を呼び、その結果を SIR に変える kirimasim のライブラリ |
| <a id="lowering"></a>ローワリング（lowering） | slang がエラボレーションした設計を、SIR の命令に変えること |
| <a id="sir"></a>SIR | kirimasim の中間表現。ストレージスロットと型付き命令からなり、すべての実行層で共通の表現を使う（[SIR_SPEC.ja.md](spec/SIR_SPEC.ja.md)） |
| `valsem` | 値の意味論の参照実装。整数のビットごとの演算と、独立したコンテナのセルのモデル。演算子、エッジ、case の一致の意味はここで決め、速い実装はすべてこれと突き合わせて確かめる |
| カーネル（kernel） | すべての層が共有する実行カーネル。スケジューラ、値の演算、VCD/FST出力、型付きヒープAPI。型付きSIR・VMでのヒープ実行、自動回収、ヒープ依存の待機は実装済み。SystemVerilogのコンテナには未対応の形が残り、VPIは[最初の範囲](notes/vpi-execution.ja.md)と[net全体の書込み](notes/vpi-net-write-execution.ja.md)を接続済み、DPI-Cは未実装。利用者の設計と一緒にリンクするので、slang にも LLVM にも依存しない |
| <a id="execution-tier"></a>第 1 層、VM | 第 1 層のインタプリタ（P1）。今は SIR を直接解釈する。SIR をレジスタ方式のバイトコードに変換して実行するのは後で |
| 第 2 層 | SIR から C++ を生成して前もってビルドし、速く実行する層（P3） |
| <a id="context"></a>Context | 1 つのシミュレーションの実行時の状態を持つオブジェクト。独立した Context を同時に動かせる |
| `analysis` | 第 2 層のための解析（P3）。X 到達解析（x が来ないところを見つけ、2 値のまま持てるようにする）、静的アイランド、レベル化（組合せ回路を依存の順に並べる）、可観測性（VPI や force から見えるので畳み込んではいけないもの） |
| 静的アイランド | 第 2 層が、速さのために 1 つのまとまりとしてコンパイルする予定の、設計の一部（P3。CODEGEN_SPEC.md に最初の規則を書いている） |
| スロット | 静的な変数、ネット、その配列のための、カーネルの格納領域の場所 |
| フレーム、フレームの変数 | プロセスやサブルーチンの呼び出しの、automatic な変数の格納領域 |
| W4 | カーネルの64ビット4値ワード。a と b の2つのビットプレーンで表現し、0 は a=0 b=0、1 は a=1 b=0、z は a=0 b=1、x は a=1 b=1 |
| チャンク | 64 ビットを超える値を分けた、1 つずつの W4。下位から並べる |
| P0〜P5 | プロジェクトの段階（[READMEの「段階」](../README.ja.md#段階)） |
| <a id="ast"></a>AST（抽象構文木） | 構文を構造として表したもの。frontはソース文字列を書き直さず、slangの構文と意味の情報を使う。 |
| <a id="sir-verifier"></a>SIR検証器 | 型付きの引数、所有、制御フロー、書き込み制約などを実行前に検査する。parseの成功だけでは、この検査は行われない。 |
| <a id="format-version"></a>formatVersion | SIRテキスト形式の版。現在は0。既存のdumpは部分的で、完全な可搬シミュレーション保存形式ではない。SIR v1はP2の作業。 |
| <a id="abi"></a>ABI | 部品間のバイナリ呼び出し・データの境界。文書のC ABIは現在0.3で、予定するABI 1.0は現在のインターフェースではない。 |
| <a id="numeric-id"></a>数値ID | slot・process・型などの行を指す識別子。実行は数値の同一性を使い、名前は診断や観測のために残す。 |


## 値

| 用語 | 意味 |
| --- | --- |
| <a id="four-state"></a>4 値、2 値 | ビットが 0・1・x・z を取る型（`logic`、`integer`）と、0・1 だけの型（`bit`）。2 値の変数に入れると x と z は 0 になる |
| x、z | それぞれ不定値とハイインピーダンスを表す論理値 |
| 信号強度（strength） | ネットを駆動するドライバの強度（supply、strong、pull、weak など）。複数のドライバが同じネットに接続される場合の値の解決に使う |
| <a id="effective-net-kind"></a>実効ネット種別 | 対象となるポート接続を統合した後のネット種別。表23-1に従い物理bitごとに決め、隣接bitとは異なる種別を持てる |
| <a id="implicit-origin"></a>暗黙の出所 | ネット宣言によるpull・supplyの寄与。identityで接続したbitの組は根を1つ共有し、tranで結ぶ独立ネットはそれぞれの根を保つ |
| <a id="resolution"></a>ネットの解決（resolution） | すべてのドライバの駆動値と強度から、ネットの値を決定する処理（[VALUE_SEMANTICS.ja.md](spec/VALUE_SEMANTICS.ja.md) の 5 節） |
| ワイヤード論理 | 強度を考慮した後、同等の競合にAND/ORの規則を使うネット（wand/wor）。単純に強度を捨てて全入力を論理演算するものではない（VALUE_SEMANTICS 5.4） |
| サイドスロット | ゲート出力に付随する2値の補助スロット。各ビットの1側・0側の駆動情報を保持し、ネットの解決に使う |
| UDP | 利用者定義のプリミティブ。真理値表で書くゲート（`primitive`） |
| 上書きのプロセス | 手続き的連続代入（`assign`、`force`）を保つプロセス |
| <a id="variable-net"></a>変数とネット | 変数は値を保持し、ネットはドライバの寄与から値を決める。logicは値の型、wireはネットの種類を表す。 |
| <a id="packed-unpacked"></a>packedとunpacked | packed次元は一つの整数の値を作り、unpacked次元は別々に添字で選ぶ要素を並べる。対応する形と代入の規則は異なる。 |
| <a id="array-extent"></a>配列の要素数（extent） | 一つの次元の要素数。宣言の数値範囲や昇順・降順とは別で、要素数が等しくてもソースの添字が等しいとは限らない。 |
| <a id="declaration-order"></a>宣言順 | 互換な固定配列ポートでは、宣言の左端同士を対応付ける。記憶域では最後の次元が最も速く変わる。 |
| <a id="alias-view"></a>aliasのviewと物理slot | 宣言ごとの範囲を保ちながら記憶域を共有する表現。アクセス時に、そのviewを共通の物理要素へ対応付ける。 |
| <a id="element-ordinal"></a>要素の序数（ordinal） | 宣言順での0から始まる位置で、ソースの添字そのものではない。複数のunpacked添字を検査して一つの物理位置へ変換する。 |
| <a id="port-collapse"></a>ポートのcollapse | 適格な接続ネットの物理ドライバを共に解決すること。ネット全体の共有や対応済みの静的bitの接続でも、ソースの名前とviewは残す。 |
| <a id="directed-carrier"></a>暗黙の連続代入・carrier | 変数・式・collapseしない形で使う、一方向のポート接続。整数の幅・符号の変更や明示castは、接続先へ通常のstrongで入る。解決済みの値を読むことと、個々の物理ドライバを取り込むことは異なる。 |
| <a id="physical-origin"></a>物理ドライバの出所（origin） | 独立した一つの駆動の寄与。同じ出所から複数の受動経路で届いても、別々の独立ドライバは増えない。 |
| <a id="identity-edge"></a>identity edge | 物理的な駆動強度を保つbitの接続。強度を弱めるスイッチと違い、出所からの強度を減衰させない。 |
| <a id="strength-interval"></a>強度の区間 | 0/1/X/Zへ写す前の、可能な強度の点の範囲。片側の状態L/Hも通常の4値として読むとXになる。 |
| 状態依存のスカラーパス | 機能値の到着時に条件を読む、対応範囲のspecify `if`・`ifnone`。LSBが1/X/Zなら有効、0なら無効。`ifnone`は宣言グループの条件がすべて無効のときだけ使う（[範囲](notes/specify-state-execution.ja.md)） |
| クロック本体とトリガー評価器 | 明示クロックは通常のtrigger・guard・watch式を監視し、本体では対応するclass操作を実行できる。プロパティ変更だけでは本体を起動しない（[範囲](notes/class-clocked-execution.ja.md)） |
| <a id="module-path"></a>module path | specifyで入力ポートから遅延した公開出力までを結ぶ時間の接続。モジュールの機能計算とは別に扱う。最初のスカラー範囲では、元の物理ドライバを保つ。 |
| <a id="path-deadline"></a>pathの期限 | 入力イベントの絶対時刻と選択した遷移遅延の和。機能到着との大きい方で公開する。直列の別セルは、公開到着を自分の入力イベントとして使う。 |
| <a id="inertial-delay"></a>慣性遅延 | 後の入力変化によって、遅延中のドライバ更新が置き換えられたり取り消されたりする遅延。--report-pulsesは値の規則を変えず、除去した遷移を報告する。 |


## 記憶域・オブジェクト・寿命

| 用語 | 意味 |
| --- | --- |
| <a id="static-automatic"></a>staticとautomaticの寿命 | static記憶域は呼び出し間で共有し、automatic記憶域はactivationに属する。値が2値か4値かとは別の性質。 |
| <a id="activation"></a>activation（呼び出しの活動） | 実行中のprocessやサブルーチンの一回の呼び出しと、そのautomatic記憶域の寿命・同一性。保持された子やrefが、戻った呼び出しより長く生きる場合もある。 |
| <a id="lexical-capture"></a>字句scopeの共有捕捉 | 子が、外側のautomatic記憶域へのアクセスを保持すること。無関係な値のコピーを作るのではなく、型ごとの対応範囲に従って共有する。 |
| <a id="ref"></a>refとconst ref | 呼び出し元の記憶域を、型・範囲・寿命・書き込みの可否とともに参照する。const refはその参照経由の書き込みを禁じるもので、値のスナップショットではない。 |
| <a id="class-handle"></a>classハンドルとオブジェクトの同一性 | オブジェクトを指す型付きの参照。ハンドルのコピーは同じオブジェクトを指し、nullの代入はそのハンドルを外すだけで、ほかの参照は消さない。 |
| <a id="constructor"></a>コンストラクタ | newで作ったオブジェクトを、classと基底classの初期化順で初期化するもの。ソース実行は記載した段階的な構築に対応する。 |
| <a id="class-static"></a>staticプロパティとinstanceプロパティ | staticプロパティは宣言元classの共有記憶域を持ち、instanceプロパティは各オブジェクトに属する。パラメータ付きstatic記憶域は未対応。 |
| <a id="virtual-dispatch"></a>仮想メソッドの呼び分けと受け手 | 仮想呼び出しが、受け手の実際のclassに応じてoverrideを選ぶこと。待つメソッドでは受け手を保持し、そのオブジェクトの寿命を保つ。 |
| <a id="shallow-copy"></a>オブジェクトの浅いコピー | 新しいオブジェクトへフィールドをコピーしても、内部のオブジェクトハンドルは元の参照先を保つ。ハンドルだけのコピーとも異なる。 |
| <a id="gc-root"></a>GCのルートと保持する所有者 | 管理対象の記憶域へ到達できるように保つ、生きた値・frame・保留操作・実行時の明示的な保持。ソース名だけではなく到達可能性で回収を決める。 |
| <a id="safepoint"></a>回収できる安全点（safepoint） | 管理対象の値とルートの公開が完了し、その記憶域の契約に従って回収できると定めた実行境界。 |
| <a id="generation-token"></a>世代付きトークン | 行の再利用後に古い参照が別の対象へ化けないよう、世代も含めて識別する管理用の値。 |
| <a id="dynamic-container"></a>動的コンテナと要素のセル | 動的配列・キューは、コピーと参照の規則を持つ型付き要素を保持する。このcontainerはデータの格納であり、文の実行場所を意味する用法とは別。 |

## スケジューリング

| 用語 | 意味 |
| --- | --- |
| <a id="time-slot"></a>タイムスロット（time slot） | 同一シミュレーション時刻で処理するイベント全体 |
| <a id="region"></a>イベントリージョン（region） | タイムスロット内の17の実行領域。規格のスケジューリング順序に従って処理する（Active、Inactive、NBA、Observed、Reactive、Postponedなど。[SCHEDULER_SPEC.ja.md](spec/SCHEDULER_SPEC.ja.md) の 2 節） |
| Active側・Reactive側のリージョン群 | 設計コードを実行するリージョン群（Active、Inactive、NBA）と、program・checkerのコードを実行するリージョン群（Reactive、Re-Inactive、Re-NBA） |
| <a id="nba"></a>NBA | ノンブロッキング代入（`<=`）。更新は後でNBA（programではRe-NBA）リージョンで起きる |
| イベント購読（subscription） | スロットの指定ビット範囲の変化を待機するための登録 |
| 監視レコード | 式が読むものが変わったときに、その式を評価し直すカーネルの記録（`wait (...)`、式のイベント制御） |
| 保留中の報告、flush point | unique と priority の違反と遅延即時アサーションは、Observedリージョンまで報告を保留する。それより先にプロセスがイベント制御から再開したら捨てる（その点が flush point） |
| 遅延即時アサーション | `assert #0` と `assert final`。タイムスロットが落ち着いてから動作を実行する |
| static-initプロセス | 時刻0のプロシージャ実行前に、static変数を初期化するプロセス |
| quiescent | イベントが無くなって実行が終わったこと |
| zero-time limit | シミュレーション時刻が進まないまま実行回数の上限に達したことによる停止（`--max-steps`、`--max-zero-delay`、`--max-slot-events`） |
| プロセスレコードの回収 | 終了したプロセスの管理レコードを解放する処理（[SCHEDULER_SPEC.ja.md](spec/SCHEDULER_SPEC.ja.md) の 10.8 節） |
| <a id="blocking-nba"></a>ブロッキング代入とノンブロッキング代入 | ブロッキング代入は代入が完了してから次へ進み、NBAは更新を予約して発行元が先へ進む。タイミング制御には捕捉と寿命の規則もある。 |
| <a id="typed-watch"></a>型付きwatch | 借りるprocessのframeで整数の式を評価し、関連する依存の変化で監視する仕組み。対応済みのalways_ffを含む式のイベント制御で使う。 |
| <a id="arming"></a>待機の登録（arming） | 購読を登録し、比較の基準値を評価すること。対応済みalways_ffの式のイベントではinitialの動作より前に行い、ffの本体は実行しない。 |
| <a id="iff"></a>iffのguard | イベントが起きたときに評価する条件。guardが真になるだけでは、waitの条件のように再開しない。 |
| <a id="snapshot"></a>スナップショット・捕捉 | 文の規則が定める評価時点で保持した値や対象。その後に右辺や選択式が変わっても、捕捉済みのNBAの内容は変わらない。 |
| <a id="event-nba"></a>イベント制御付きNBA | 保持した更新が発行元とは独立してイベントを待つNBA。発行元のブロッキングなイベント待ちではない。 |
| <a id="program-origin"></a>programの出自 | 呼び出しやforkを通じて保つ、発行元のprogramと評価群。programの処理にはReactive/Re-NBAとprogram終了の規則を使う。 |
| <a id="named-fork"></a>名前付きforkの活動 | scopeとactivationの同一性で識別する、名前付きparallel scopeと子の活動。親がscopeを抜けた後も子孫との関連を保持できる。 |
| <a id="simulation-wall-time"></a>シミュレーション時刻と実時間 | デザインの時刻は予約されたイベントで進み、実時間は実行に経過した時間。二つの上限は別のものを測る。 |


## テストと仕事の進め方

| 用語 | 意味 |
| --- | --- |
| 未対応（unsupported） | kirimasimがまだ実装していない構文。ソース位置付きの診断と終了コード2で停止し、別のセマンティクスに置き換えて実行しないことを目指す |
| 機能別テストマトリクス | 対象機能の宣言位置、アクセス方法、識別子表記、実行コンテキストの組み合わせを網羅する生成テスト。不具合の再現条件だけでなく、関連する条件も検証する |
| テストジェネレータ | テストマトリクスと期待値を、定義した規則に従って生成するスクリプト `tools/gen_*.py`。`--check` でコミット済みファイルとの一致を確認する |
| 実行コンテキスト | 文の実行場所。モジュール内のプロシージャ、タスク、関数、fork分岐、interface、generate、packageなど。同じ文のセマンティクスは実行コンテキストによって変わらない |
| <a id="negative-control"></a>変異・negative control | 意図的な誤りを一つずつ入れ、コンパイルして選んだテストが検出するか調べる。適用位置の検査だけでは検出の証明にならない。ソースとビルドを復元し、対象範囲を記録する。`tools/mutants.py` で検証する |
| <a id="perfcount"></a>PerfCount | 時間ではなくイベントの回数で測る回帰テスト。期待値は `tests/perf_counts/expected.json` |
| `--stats=json` | その回数を出すオプション |
| <a id="oracle"></a>オラクル（oracle） | テスト期待値の根拠（ヘッダの `:oracle:`）。規格、または比較検証に使ったシミュレータ名を示す |
| `:spec_check:` | テストを規格と照らし合わせたのが誰か。`ai`（AIによる確認）か `human`（人） |
| P1 の母集団 | P1 の合格条件で数える、自前のテストスイートの 1466 本の固定した一覧 |
| 必須テスト | 通過率とは別に、落としてはいけないテストの決まった一覧（TEST_SUITE.md）。P1 では、P4 が要る 1 本を除いてすべて通ることが合格条件 |
| sv-tests | CHIPS Alliance が公開している SystemVerilog のテスト集 |
| Icarus Verilog、Verilator | 比べるためと証拠として動かす、ほかのオープンソースのシミュレータ。正解としては扱わない |
| <a id="vpi"></a>VPI | Cのpluginがtask/関数・reflection・callbackを通して接続する規格のインターフェース。[最初の実行範囲](notes/vpi-execution.ja.md)は対応済み。全API・cocotb対応ではない |
| DPI-C | SystemVerilogとCの関数・taskの呼出し境界。未実装で、必要なIcarus対応の範囲はP1 |
| net全体の書込み権限 | 公開された物理net宣言全体への、検証済みの数値の権限。netフラグやbit handleだけでは書込みを許さない（[範囲](notes/vpi-net-write-execution.ja.md)） |
| netのdeposit | 物理driverを変更しないpoint netの一時的な公開値。driverの確定寄与が実際に変わると解除し、持続するforceとは異なる |
| VPI handleと借用buffer | handleは所有するContextの生存中だけ有効。返された値・文字列bufferは関連APIの次の呼出しや再入までの借用で、保持にはコピーが必要 |
| FST | `--wave=<file>.fst` で選ぶコンパクトなバイナリ波形形式。native の4値・実数・event出力はVCDと対象選択・時刻を共有（[証拠](notes/fst-execution.ja.md)） |
| 規格で確認、本書の決まり、解釈、未確認 | 仕様書の注記（英語版では checked、rule of this document、interpretation、not confirmed）。「規格で確認」は規格の節を読んで確かめたこと、「本書の決まり」は規格が実装に任せた点を kirimasim として決めたこと、「解釈」は規格の文面が 2 通りに読める点の読み方、「未確認」はまだ確かめていないこと（確かめ方を添える） |
| <a id="feature-probe"></a>機能プローブ | 記載した期待値、ソースhash、処理系ごとの結果を持つ小さな独自設計。有限の形の検査であり、言語全体の同等性は証明しない。 |
| <a id="passing-reference-gap"></a>参照側が通る不足 | 参照側が記載した期待値に合い、kirimasimが合わないプローブ。件数には母集団とソースの版も添える。 |
| <a id="reference-mismatch"></a>参照側の不一致 | 参照側の観測値が記載した期待値と違うこと。任意のcollapseや未規定の順序では複数の結果が合法な場合もあり、不一致だけで正誤は決まらない。 |
| <a id="extension"></a>拡張と互換モード | 厳格な規格の契約の外の動作と、それを受け入れる明示的な方針。参照ツールの機能が自動的にこちらの既定にはならず、--compatも現在は未公開。 |
| <a id="expectation-migration"></a>期待値の移行 | 実装から分けてレビューする、既存テストの分類や出力の変更。以前の未対応入力が実行できても、元のソースを保持できる。 |
| <a id="local-ci"></a>ローカルCIとnightly | ローカルCIは選んだ構成のビルド・テスト・静的な規則を検査する。nightlyは設定された長時間/外部検査も行う。各実行の範囲と結果を確認する。 |
| <a id="source-binary-version"></a>ソースと実行ファイルの版 | 作業ツリーより実行ファイルが古い場合がある。再現にはgit rev-parse HEADとkirimasim --versionの両方を残す。 |
| <a id="test-count"></a>テスト件数と対応範囲 | CTestの項目、生成ソース、外部の固定母集団、機能の仕組みは別の単位。合格数の増加だけではP1の全条件を満たしたことにならない。 |
| 捕捉した強度区間 / 由来 | 4値への射影前の符号付き両端点で、選択したnet引数と同時点で捕捉するもの。実行時の`%v`は型付きの情報を使い、名前検索や`%b`の値から再構成しません（[範囲](notes/strength-format-execution.ja.md)）。 |
