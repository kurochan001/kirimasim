# 状況の履歴

[English](STATUS_HISTORY.md) · 日英で食い違う場合は英語版を正本とします。

kirimasim の現在の状況は[README](../README.ja.md)にあります。このファイルは、
2026-11-04の更新も含め、以前のREADME・ガイドに載せていた日付付きの記録（実装の区切り、比較プローブの件数、
検証の記録）を保ちます。リンクのパス以外は変えずに移しました。各項目はその時点の版・
日付・件数を保ち、現在の対応範囲を表すものではありません。現在の対応範囲は
[P1 の対応機能一覧](P1_SUPPORT.ja.md)とREADMEの「まだできないこと」を参照してください。
各区切りの詳しい記録は[P1の進捗](notes/p1-icarus-progress.ja.md)に、利用者から見た変更は
[CHANGELOG](../CHANGELOG.ja.md)にあります。

## README の整理（2026-11-01）

[整理前の README 全文](history/README_2026-11-01.ja.md)に、公開 main `b56fae24` の
実装の経緯、過去の比較・ゲート表、性能測定、古い残作業一覧を保存しました。内部の日付と範囲は
過去の記録として保ちます。[検証件数の記録](VALIDATION_COUNTS.ja.md)は測定日を示し、
`counts.json` から維持します。現行 README は、現在の実行の概要・制限・ビルドと実行の入口・
リポジトリのテスト件数を記載します。

この文書変更の検証では、公開 Markdown 344 ファイルが日英172組となり、全組の相互言語リンク、
ローカル Markdown リンク、表の列数が一致しました。CHANGELOG の全168項目と、期待値監査の
191候補の説明を翻訳しています。Issue フォームの YAML 構文、dev ビルド、Tools 全190テストが成功し、
README の counter 例も記載した出力と一致しました。生成器は4文書の出力、再実行時の不変性、
古い出力の検出を確認しました。正式なライセンス本文、実装・既存テスト、counts.json、生の測定証跡は
不変です。ライセンスの読み取り用文書には非公式の日本語参照訳を明示し、正式な英語本文を保ちます。

## 「状況（2026-10-29）」に載せていた区切り

`dd2c0188`から、インクルードパスの`-I`、マクロの`-D`、モジュールライブラリの
`-y`・`--libdir=`を使えます。短いオプションは引数の連結形・分離形を受理します。
探索規則、マクロ値、既存のプラス形式との対応は、
[実行例](P1_FEATURES.ja.md#共通の入力オプション)と
[ユーザーマニュアル](USER_MANUAL.ja.md#ファイルリストとインクルードパス)を参照してください。

既知スキップ2件は、既存の期待出力を変えずに実行できるようになりました。
`#1step`でサンプリングする入力専用のクロッキングブロックと、駆動強度を保持する静的な部分ポートに対応します。
[承認済みのクロッキング入力の範囲](notes/clocking-input-1step-proposal.ja.md)と
[実装済み一覧の制限](P1_SUPPORT.ja.md)を参照してください。クロッキングやポートの他の形は残作業で、
P1は未完了です。以下の過去の検証結果は、その時点の日付と件数を保ちます。

slangの固定先を、12.1後の開発コミット[`e2d1021`](notes/slang-e2d1021-update.ja.md)へ更新します。
#2064のclass specifier回復の修正を含み、[local CI全21項目が成功](measurements/2026-10-27/slang-e2d1021-main-validation.json)しました。
既存の期待値は維持しています。母集団の文字列変換4失敗は旧版でも発生し、残作業として記録しています。

[ソースevent-NBA変換](notes/event-nba-source-execution.ja.md)`c4be8ade`を、
移植性/lint修正`9a81a55e`・`b91bd3d7`と合わせてmainへ取り込みます。独自1,018設計が成功し、
コンパイルした6故障をすべて検出しました。2版の固定95プローブは`event_controlled_nba`が新たに通り、
有限の一覧の残差は6件です。古い拒否期待は承認済みの別コミット`16c033a4`で移行しました。
[main統合の検証](measurements/2026-10-27/nba-front-main-integration.json)にpush前の確認を記録します。
P1とソースの残る境界は未完です。

mainへコンテナ条件式`a8bb17cc`とNBA保持値`7bc5fa85`を取り込みました。
[取り込み記録](measurements/2026-10-27/conditional-nba-main-integration.json)で、検証済みの
コード・テストの不変と各commitの期待値保護を確認します。この過去の区切りではソースevent-NBAは未対応でした。

mainに`624123b2`を取り込み、承認済みCLI 2.7の[`--max-procs`](notes/process-limit-execution.ja.md)を
実装しました。共有kernelで実行時に生成された未終了の子プロセスを数え、記録の保持・再利用と区別して、
上限を超える子を登録する前に停止します。
[取り込み記録](measurements/2026-10-27/process-limit-main-integration.json)で検証済みコード・テストの
不変とcommit単位の期待値保護を確認します。全local CIは21/21成功で、P1は未完です。

mainに`9e6a149d`を取り込み、未使用宣言の事前初期化を含む
[非パラメータ化クラスのstaticプロパティ](notes/class-static-execution.ja.md)を追加しました。
[全local CI](measurements/2026-10-27/class-static-validation.json)は21/21項目成功で、
新規CTest568件・コンパイルした7故障の検出も通りました。
[取り込み記録](measurements/2026-10-27/class-static-main-integration.json)で、検証済みの実装・テストが不変であることを確認しています。
クラスオブジェクトとパラメータ付きstatic記憶域は残作業です。

以前のmainの区切りでは`dfddb94b`で[event宣言の初期化コピー](notes/event-initializer-execution.ja.md)を追加し、
人が承認した期待値3件の移行を`2da32daa`に分けました。3コミットを
[`f100cd86`までmainへ取り込みました](measurements/2026-10-27/event-initializer-main-integration.json)。
[全local CI](measurements/2026-10-27/event-initializer-validation.json)は
21/21成功、dev/ASan/UBSanは各6,057/6,059、releaseは6,058/6,060で、
失敗0・既知skip各2件です。母集団・sv-tests・Icarus 13/14のprobeも再実行し、以前の成功を
維持しました。P1は未完了で、以前のmainの区切りは履歴に残します。
[日付付きの記録](notes/p1-icarus-progress.ja.md)は
以前の承認・失敗・公開状態をその時点の履歴として保持します。

`4c69843`で型付きpatternのSIR・検証器・text・実行処理を共有表示APIへ接続しました。
frontの定数・runtime書式引数への接続は残っているため、ソース`%p`は引き続き未対応です。
既存queue要素を変更したときの`$monitor`の再表示も確認しています。
[現在の実装と検証範囲](notes/pattern-sir-execution.ja.md)と
[実行例](P1_FEATURES.ja.md#キュー要素の変更をmonitorで表示する)を参照してください。

共有カーネルのUDP評価を32入力超へ拡張しました。[独自106ソースと独立した値の検査](notes/wide-udp-inputs.ja.md)で、
後方の端子、状態、X/Z、遅延、強度を確認しています。密な表では固定したslangの検査時間が
増える問題が残るため、多入力UDPソース全体の対応は未確認です。診断は緩めていません。

動的配列・キューの実行仕様は2026-10-23に承認されました。共通カーネルには、
深いコピー、安定した要素参照、上限付きキュー、正確なトレース回収、確保失敗時の
巻き戻しを持つ型付きヒープを実装しました。カーネルの31テストで入れ子のコピー432組、
確保失敗時の状態保持、実際の型を保つスナップショットを検査し、252万操作の
モデル比較も通過しています。型付きSIRの記述子・参照・命令と
検証器・テキスト形式も追加しました。独自のSIR入力によるVMの24テストで、実際の
上限を保つref、待機をまたぐ切り離したセル、領域・スロット境界での自動回収を検査します。
型付きスロット、生存中のフレーム、セル参照、遅延報告、スケジューラの保留値を
ルートとして保持します。ヒープに依存する待機、依存先の付け直し、呼び出した関数の
関数ローカルのコンテナの除外も接続しました。frontから構築、深いコピー、要素、キューのメソッド、
実際の上限を保つref、値全体の遅延代入、明示的なヒープ待機、動的foreach、第1次元クエリを
実行できるようにしました。独自のソース行列240件が8種類の実行場所を横断し、CLIでも
11件の位置付き診断を検査します。さらに256件で、実行時のunpacked連結とキューの読み取り
スライスを検査します。広い符号付き・符号なし境界、X/Z、副作用を含む評価順も対象です。
対応は一部で、スライスへの代入、追加の配列メソッド、暗黙のヒープ感度などは残作業です。
[実装状況](P1_SUPPORT.ja.md#動的コンテナの共通基盤)と
[承認済みの契約](notes/dynamic-containers-proposal.ja.md)を参照してください。

mainへ[コンテナの条件式](notes/container-conditional-execution.ja.md)を
追加しました。対応済みの動的配列・キューで`?:`を実行し、不定条件での要素全体の既定値、
snapshot、入れ子の内容比較を扱います。独自351設計が実行場所9種類を横断し、
[local CI](measurements/2026-10-27/container-conditional-validation.json)は21項目すべて成功しました。全面対応ではありません。

mainの[具体的なイベント購読](notes/event-nba-concrete-controls.ja.md)で、固定slot・名前付きイベントの
継続的な購読、全幅の回数、callback元の保持を実装しました。下記の式monitorの区切りは、
この中核を拡張します。SIR/front/VMのソース転送は残作業です。[ローカル検証](measurements/2026-10-27/nba-events-validation.json)では
CI全21項目、新規kernel/SIR-nativeテスト33件とコンパイルした18故障の検出が成功しました。

[式monitorの区切り](notes/event-nba-expression-monitors.ja.md)を`012af2ca`としてmainへ取り込み、
共通評価とheap/activation通知のまとめ方を追加しました。[統合の検証](measurements/2026-10-27/nba-monitors-main-integration.json)に
この区切りのpush前の検査を記録します。その時点ではソースevent-NBAは未対応でした。

[型付きSIR/VMの区切り](notes/event-nba-sir-execution.ja.md)`6f43e387`で、配置表、
verifier/text、新しい発行元callbackを追加し、独自SIRで検査しました。
ローカルの[ソース変換](notes/event-nba-source-execution.ja.md)で値保持の順序とcallbackを検査し、local CI全21項目が成功しました。

mainの[NBA保持値の土台](notes/event-nba-payload-foundation.ja.md)で、共有kernelに
値の所有と保留中の発行順を追加し、独立した単体24件で検査します。上の実装は
slot・名前付き購読とcallback元の保持を追加し、上記のmainの区切りで式monitorを追加しました。ソースの
`nba.event`転送は、これらの過去のkernelの区切りでは残作業でした。
[最終配置のlocal CI](measurements/2026-10-27/nba-payload-validation.json)は21項目すべて成功しました。

[共通の強度の解決処理](notes/strength-range-foundation.ja.md)は、値への射影前の
完全な区間を保持できるようにし、全区間ペアで検査しました。
[方向付きMOSのソース実行](notes/source-mos-execution.ja.md)は通常driverを取り込み、
制御を別に捕捉します。部分対応であり、登録 net の force、部分・非結合ポート、完全な強度の
書式は残作業です。[ソースの受動実行](notes/source-passive-execution.ja.md)では、静的な
bit・要素・連結の対応とプリミティブ配列を含む双方向 6 種を共有網へ接続しました。
独立した不定網でも公開は一括のままです。独自143設計が通り、古いtran拒否期待は
[別途承認した移行](notes/passive-expectation-migration.ja.md)を`77c9d188`で適用しました。
全ソース入力は維持しています。

広い三状態ゲートでも、ワード境界をまたぐL/Hマスクの両半分を保持します。新規73ソースは
Icarus 13.0・開発版とすべて一致しました。[対応範囲と検証](notes/wide-gate-strength.ja.md)を
参照してください。sideだけが変わる遅延の時刻も、[意味の修正案](notes/delayed-gate-side-proposal.ja.md)が2026-10-25承認済みです。

集約値の通常代入・NBAでは、代入先の副作用より前に右辺全体を保持するよう修正しました。
コンテナの値は後のメンバーの式より前に捕捉し、実行時に選ぶ固定の部分配列と
構造体配列のメンバーにも同じ経路を使います。[値の保持と検証](notes/aggregate-value-capture.ja.md)を
参照してください。

固定配列・unpacked構造体全体のブロッキング代入内遅延・イベント・repeatにも対応しました。
型付きの非公開フレームで右辺を待機中も保持し、実際の対象は待機後に選びます。
別の独自74件で、これらの形と宣言範囲が異なる構造体配列のコピーを検査します。
[実装と期待値の別レビュー](notes/blocking-aggregate-timing.ja.md)を参照してください。

ブロッキング代入内の`repeat(count) @(...)`は、右辺と完全な幅の回数を保持し、
待機後に代入先を選べるようになりました。0以下・X/Zなら待機を省きます。
独自ソース75件が実行場所8種類とスカラー・コンテナの値の保持を検査します。
イベントNBAのソース変換は上記の別段階で検査します。[実行と検証](notes/repeated-event-controls.ja.md)を
参照してください。

fork内の宣言は、親での初期化と共有static記憶域に対応し、子プロセスには数えません。
宣言だけなら待機も出しません。[対応範囲と独自72ソース](notes/fork-local-declarations.ja.md)で
全join種類を検査します。[ソースからのautomatic共有捕捉](notes/automatic-front-capture.ja.md)
は新規186設計で通り、過去の未対応3テストは承認済みの別コミット`11096dc3`で移行しました。
[型付きSIR event実行](notes/event-vm-boundaries.ja.md)と既存のregion/slot境界での回収も
接続しました。承認済みの[automatic eventの命令境界回収](notes/instruction-event-execution.ja.md)で、
物理行を増やす前に、native field、SSA領域、転送全体のpinを公開します。初期化子のない
automatic scalar/固定配列eventの宣言は、固有のobjectを生成し、子孫の保持も守ります。
新規ソース176件、検証済みSIR32設計、コンパイルした12故障の検出が通りました。
[過去の拒否期待値1件の移行](notes/automatic-event-expectation-migration.ja.md)は承認後に`4ba6b63d`で適用しました。
eventの値・仮引数・structの対応はまだ部分的です。

[実行時indexでの固定配列event選択](notes/indexed-event-execution.ja.md)を型付き値へ接続し、
名前付き/混合待機・遅延対象・`.triggered`を独自160ソースで検査しました。
旧CLI入力の未対応診断6行は[別途承認した移行](notes/indexed-event-expectation-migration.ja.md)を
`28f5c972`で適用済みです。別件の[tran期待値移行](notes/passive-expectation-migration.ja.md)も
承認後に`77c9d188`で適用しました。以前に失敗した2検査も
[main統合の検証](notes/main-integration-2026-10-26.ja.md)に含めます。

固定配列ポートは宣言ごとの範囲を保って要素を共有・コピーし、ネット配列の強度を
要素ごとに解決します。定数要素のforce/release、遅延、未接続の既定値・pullも含め、
[独自358ケースと対応範囲](notes/fixed-array-port-execution.ja.md)を記録しました。
既存87プローブのIcarus成功・native未成功は各参照版で24→22件になりました。
P1全体は引き続き未完了です。

## 「P1 の合格条件」に載せていたプローブの件数と調査

進捗は[実装済み一覧](P1_SUPPORT.ja.md)と
[比較記録](notes/p1-icarus-progress.ja.md)で管理します。
一覧は独自87プローブです。固定配列ポートの区切りでは、Icarus `13.0 (stable) (v13_0)`に
対する未対応が22本、参照結果の不一致が4本です。`14.0 (devel) (s20260301-411-gca6b8d376)`では
22本と7本です。その後のautomatic捕捉で過去の13.0合格との差分は21本に減り、
新しい14.0実行も差分21本・不一致7本です。このevent VM段階で13.0は再実行していません。向きのあるMOSの実行が入った後、
2026-10-26に両方で測り直すと差分は14本、参照の不一致は13.0で4本、14.0で7本です。
その後、同日に測定した[ソース受動スイッチの区切り](notes/source-passive-execution.ja.md)では、
差分は**両参照とも8本**へ減り、参照の不一致4/7本は不変です。この安定版のコンパイラと
ランタイムはいずれも`13.0 (stable) (v13_0-dirty)`を返し、実測は
[13.0の記録](measurements/2026-10-26/icarus13-source-passive-probes.json)と
[開発版の記録](measurements/2026-10-26/icarus14-source-passive-probes.json)に保ちます。
後続の[automatic eventの区切り](notes/instruction-event-execution.ja.md)では、両方の実行ファイルで
全87件を再計測し、差分は**両参照とも7本**、参照の不一致は4/7本でした
（[13.0](measurements/2026-10-26/icarus13-instruction-events-probes.json)、
[14.0](measurements/2026-10-26/icarus14-instruction-events-probes.json)）。
比較対象の版を分け、[結果と条件](notes/icarus-13-comparison.ja.md)を記録しました。
全面的な同等性は未確認です。プローブがすべて通っても全機能の対応を証明できず、
仕組みやインターフェースを確認するごとに棚卸しを広げます。
多入力UDPの区切りでは以前の84件の状態を維持し、新たに棚卸しした多入力UDPの2本が両参照とnativeで通りました。
追加した密な表の網羅性検査はnativeのsubprocess上限に達します。同じ拡張後の一覧では、
保持した旧baselineが27/33件、多入力UDPの区切りが25/31件で、後の配列・入力の区切りでは24/24件、固定配列ポート対応後は22/22件です。
[automatic記憶域の共有・eventの修正案](notes/automatic-capture-proposal.ja.md)に
独自32プログラムを記録し、2026-10-25承認済みです。共有字句捕捉は実装済みで、
[automaticスカラー/固定配列eventの生成](notes/instruction-event-execution.ja.md)も実装しました。
過去の拒否期待値1件の別移行は承認後に`4ba6b63d`で適用しました。
[配列ポート・解決の修正案](notes/fixed-array-ports-proposal.ja.md)にも独自59本を記録し、
違法な接続を未対応機能と分けています。

[クラスの独自46プログラムによる調査](notes/class-execution-investigation.ja.md)も別に記録しました。
過去のstatic初期化の不一致を含みます。[非パラメータ化クラスのstatic記憶域](notes/class-static-execution.ja.md)でこの欠落を修正しました。オブジェクト実行は引き続きP1の残作業です。
[クラス実行の詳細契約案](notes/class-runtime-proposal.ja.md)は独自14本の追加観測を含み、2026-10-25承認済みです。

[MOS・スイッチの調査](notes/mos-switch-investigation.ja.md)には、
独自28プログラムと過去の参照実行56件の観測を、この一覧と別に記録しています。
[有向MOS](notes/source-mos-execution.ja.md)と[受動スイッチ](notes/source-passive-execution.ja.md)は、
記載した静的端子では実行できます。登録済みネットへのソースforce、部分ポートの同一性の拡張、
完全な強度書式は残作業です。

整数への`$fread`はstart・countの指定に対応し、値の引数を1回ずつ評価します。
選択したinoutの代入先は、承認済み規則どおりコピー入力時と、正のバイト数の書き戻し時に
評価します。先の1回捕捉の実装は[期待値・実装を分離した訂正](notes/fread-copyback-correction.ja.md)で
修正しています。従来の同じ61プローブで差分が23本から22本へ減りました。
承認済みの[多次元メモリ入力](notes/multidim-fread-execution.ja.md)を実装し、独自156件で
検査しています。配列の実行時の縮約・安定な整列・min/maxも[共通の型付き変換](notes/runtime-array-methods.ja.md)
で動き、独自270件と直接テスト4件を追加しました。同値でもmin/maxは元の要素を最大1個返し、
Icarusが重複して返す観測は参照側の不一致として保ちます。副作用のある整列キー・純粋性未確認の
ユーザー関数キーと、未対応の複合型の入力表現には制限が残ります。

残る6件の[実行契約](notes/p1-execution-contracts-2026-10-25.ja.md)は2026-10-25に承認済みです。
最初に[遅延付き三状態ゲートの完全な状態の伝播](notes/delayed-gate-state-execution.ja.md)を実装し、
独自240ソースと直接3件で検査しました。具体的な期待値のみの移行も別に承認・統合済みです。
[ローカルCI全21項目が成功](measurements/2026-10-25/delayed-side-integration-validation.json)し、
dev/ASan/UBSanは4,179件中4,177件、releaseは4,180件中4,178件成功、失敗0・既知2件のみ
スキップです。他の承認済みP1機構の実装は残作業です。

## 「テストの結果」に載せていた検証の記録

過去の`615afa0`（2026-10-25、macOS arm64）の全local CIは、次の件数でした。
21項目すべてが通り、dev・ASan・UBSanは各4,652件中4,650件成功、既知の2件のみスキップ、
releaseは4,653件中4,651件成功、同じ2件のみスキップです
（[記録](measurements/2026-10-25/activation-environment-final-validation.json)）。

以前の[main取り込み](measurements/2026-10-27/conditional-nba-main-integration.json)は、
コンテナ条件式`a8bb17cc`とNBA保持値`7bc5fa85`を含みます。README再生成後のdev再確認を含め、
local CI全21項目を確認しました。
dev/ASan/UBSanは各7,013/7,015、releaseは7,014/7,016、失敗0・既知skip各2件です。
拡張した独自95プローブの差は両参照とも7本、参照不一致は13.0 stableが4本・14.0 develが7本です。
[最新の検証と制限](notes/p1-icarus-progress.ja.md#現在の実装とレビュー状況2026-10-27)と
[版ごとの個別記録](notes/icarus-13-comparison.ja.md)を参照してください。
以下の外部sv-tests・母集団の件数と性能測定は、記載した
コミット・測定日の値を保っています。

## 「テストのやり方」に載せていたローカルのファジング実行

夜間は1ワードの演算、スケジューラ、コンテナのモデルとの突き合わせを通常の100倍で実行し、1〜260ビットの値は同じ倍率の別ステップで検査します。従来の値の2ステップは2026-10-22にローカルで通過し、別のヒープの252万操作は2026-10-23に通過しました。どちらもP1の条件の連続した晩数には加えません。

<a id="guide-checkpoints-before-2026-11-04"></a>

## 2026-11-04更新前のガイドの区切り

以下は、2026-11-04の更新で現行README・FAQ・検証ガイドから移した記録です。版・日付・母集団は
変更していません。保存した本文中の「最新」は当時の区切りを指します。現在の公開実装とCIは、
[main統合記録](notes/main-integration-2026-11-04.ja.md)にあります。

2026-11-03の[inside集合要素修正の取り込み](notes/inside-member-integration.ja.md)に、既存ブランチからの反映、全local CI、外部クロスの入力保持を記録しています。修正した2件と、[承認済みのラベル取消し仕様](notes/deferred-label-cancellation-proposal.ja.md)、[固定版エラボレータのconfig不一致](notes/config-reset-dependency-gap.ja.md)を区別します。作業ツリーの検査であり、正式Nightlyや固定母集団のゲートを更新したものではありません。

承認済みの[遅延アサーションのラベル取消し](notes/deferred-label-cancellation.ja.md)を、最新upstream main `7654825d`にローカル変更として実装しました。入力を保持した外部クロス2,785件は**2,252 pass・529 unsupported・configの4 fail**で、遅延報告31件はすべてPASSです。[仕様](notes/deferred-label-cancellation-proposal.ja.md)どおり、兄弟報告・評価済み副作用・成熟済み報告を保持します。正式Nightlyや連続実行のゲートとは別のローカル観測です。

**承認後の統合チェックポイント（2026-11-01）：** class監視・部分inoutの既存期待値3件を承認どおり移行し、dev全体11,299件が除外なしで成功しました。固定176プローブのIcarus13.0比較は差3件・別分類の参照不一致25件です。[統合記録](notes/p1-approved-class-inout-integration.ja.md)を参照してください。以下の以前の測定日・母集団は保持します。

**以前の記録（2026-11-01）：** main `4fb5f6b1`のlocal_ciは全21項目が成功しました
（dev/ASan/UBSanは各10,205件、releaseは10,206件）。
[145プローブのIcarus 13比較](measurements/2026-11-01/icarus13-array-element-ports.json)の不足は5件で、
新たに棚卸ししたsupplyの2形を含みます。従来131件のソース・期待値・nativeの状態は同じです。
[ポートの記録](notes/array-element-port-execution.ja.md)は、従来の参照不一致4件と、
新しい任意の非collapseによる内側の値の相違11件を分けます。
これは記録した実行で、sv-tests・母集団・14.0・7晩のfuzzを新たに実行したものではありません。
ガイドのコマンド例には、それぞれ確認した範囲を付けています。


続く[実効ネット種別の測定](measurements/2026-11-01/icarus13-port-effective-kinds.json)は
実装 `afebe49f` と同じ固定145ソース・期待値を使います。内側supplyの2件も成功し、
参照側が通る不足は3件です。参照側の不一致15件は別に記録しています。今回の実装の
検証と別途承認した既存期待値移行は[実行の記録](notes/port-kind-execution.ja.md)を参照してください。
測定時の手元の実装 `afebe49f` は上流 `d0865eb6` へrebaseし、`67a93af0` になりました。
ポート生成器とテストは同じで、上流のSIRテキストの修正も出力を保ちます。


最新の[156プローブ比較](measurements/2026-11-01/icarus13-converting-ports.json)は従来145件を
変えず、変換ポート11件を加えます。固定した実装前の不足8件は実装後3件になり、任意collapseに
よる新たな相違6件は別に保ちます。[変換の検証](notes/converting-port-execution.ja.md)を参照してください。


最新の[171件](measurements/2026-11-01/icarus13-partial-inout-ports.json)は部分inoutの15件を加え、
実装前の不足14件が実装後3件になりました。wiredの読み出しの相違4件は参照側の原因を未確認として
別に保ち、従来のソース・期待値・nativeの状態は変えていません。
[部分inoutの検証](notes/partial-inout-port-execution.ja.md)を参照してください。
