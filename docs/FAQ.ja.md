# よくある質問（FAQ）

[English](FAQ.md) · [文書案内](README.ja.md) · [プロジェクトREADME](../README.ja.md)

**2026-11-01更新、対象実効ネット種別の実装 `afebe49f`。** 実装済みの範囲を説明します。
詳しい制限は[対応機能一覧](P1_SUPPORT.ja.md)、用語は[用語集](GLOSSARY.ja.md)を参照してください。
過去のベンチマークと外部母集団の結果は、記録した版と測定日のままです。

- [実行の開始と確認](#実行の開始と確認)
- [対応範囲と比較結果](#対応範囲と比較結果)
- [値・スケジューリング・寿命](#値スケジューリング寿命)
- [波形・上限・性能](#波形上限性能)
- [開発と貢献](#開発と貢献)

<a id="start-and-inspect-a-run"></a>

## 実行の開始と確認

<a id="can-i-use-kirimasim-on-my-project-now"></a>

### 今すぐ自分の設計で使えますか？

開発の初期で、P1は未完了です。RTLや簡単なテストベンチをSIRのインタプリタで動かし、
classオブジェクトなどの新しい機能にも対応範囲があります。[ユーザーマニュアル](USER_MANUAL.ja.md)から始め、
型・寿命・実行場所の組み合わせを[対応機能一覧](P1_SUPPORT.ja.md)で確認してください。
仕様書に予定として書かれたインターフェースが、実装済みとは限りません。

<a id="what-should-i-record-when-a-result-needs-reproducing"></a>

### 再現のために何を残せばよいですか？

入力の順序、top、作業ディレクトリ、指定、seed、stdout、stderr、終了コードを残します。
実行ファイルが古い場合もあるため、`git rev-parse HEAD`と実行ファイルの`--version`の両方を保存します。
[検証ガイド](VALIDATION.ja.md)と[不具合調査](DEBUGGING.ja.md)に手順と独自の最小例があります。

<a id="why-can-parse-succeed-while-simulation-reports-unsupported"></a>

### parse が通っても、実行時に未対応になるのはなぜですか？

`parse`はslangの前処理・構文解析・型検査・エラボレーションを行います。
通常実行では、さらにSIRへの変換・検証・対応済みruntimeでの実行が必要です。
`--dump-sir`は変換と検証をしてテキストを出し、シミュレーションはしません。
完全な実行ファイルの保存でもありません。[3つの実行形式](REFERENCE_MANUAL.ja.md#起動形式と表記)を参照してください。

<a id="why-does-it-stop-at-an-unsupported-construct-instead-of-skipping-it"></a>

### 対応していない構文を、飛ばさずに止まるのはなぜですか？

飛ばすと、正しく見える誤った結果を出すおそれがあるためです。通常の未対応ソースは、
デザインを動かす前に位置付きの診断と終了2で報告します。入力の不正・未対応・実行時の
デザインエラーが終了2を共有するため、診断文も残してください。
内部の失敗（終了3）は別の報告対象です。[終了コード](REFERENCE_MANUAL.ja.md#終了と終了コード)で区別します。

<a id="can-i-pass-every-slang-option-including---compat"></a>

### --compat など、slang のすべての指定を渡せますか？

いいえ。driverが公開するのは[一覧にある入力指定](REFERENCE_MANUAL.ja.md#入力オプション)です。
`-W<name>`・`-Wno-<name>`・`-Werror[=<name>]`はフロントエンドの警告方針を指定順で渡しますが、
実行時エラーや資源上限を無効にはしません。`--compat`とエラボレーション上限の直接指定は未実装です。
単体のslangや将来の仕様書の記載から、driverの対応を推測しないでください。

<a id="why-is-a-header-module-or-data-file-not-found"></a>

### ヘッダ・module・データファイルが見つからないのはなぜですか？

includeの探索先（`-I`・`+incdir+`）、moduleライブラリ（`-y`・`--libdir`）、実行時のファイル名は別のものです。
`-f`内のパスと`$fopen`・`$readmemh`の名前は、ファイルリスト自身の配置先ではなく、
作業ディレクトリが基準です。[入力の探索](USER_MANUAL.ja.md#入力と実行モードの選択)を参照してください。

<a id="where-do-design-output-statistics-and-diagnostics-go"></a>

### デザイン出力・統計・診断はどこへ出ますか？

デザイン出力と最後の`--stats=json`の要約はstdout、診断・infoはstderrへ出ます。
統計を有効にすると、stdoutは一つのJSON文書とも、デザインの文章だけとも限りません。
出力を分けて保存し、最後の要約の行を解析してください。必要な場合はdriverがJSONの前に改行を挿入します。

```sh
build/dev/src/driver/kirimasim --stats=json demo.sv > run.out 2> diagnostics.log
tail -n 1 run.out
```

[出力と統計](USER_MANUAL.ja.md#出力と統計の取得)に詳しい説明があります。

<a id="coverage-and-comparison-results"></a>

## 対応範囲と比較結果

<a id="is-p1-complete-now-that-the-numerical-gates-pass"></a>

### 数値の条件を満たしたら P1 は完了ですか？

いいえ。承認済みの値とスケジューリングの規則を保ち、Icarus Verilog 13.0以上の実行範囲を満たす必要があります。
有限のプローブ、固定1,466件の母集団、sv-testsの件数、連続したfuzzは別の根拠です。
[P1の条件](../README.ja.md#p1-の合格条件)と[残作業](P1_SUPPORT.ja.md#残っている作業と対象範囲)を確認してください。

<a id="why-do-the-comparison-counts-change-from-95-to-131-to-145"></a>

### 比較件数が 95・131・145 と変わるのはなぜですか？

測定日と対象が違うプローブ一覧です。11月1日のポート棚卸しは、従来131件のソースhash・期待値のバイト列・
nativeの状態を保って14件を追加しました。新しい配列要素の不足を解消し、内側supplyの不足2件を明らかにしました。
母集団を広げて不足が増えても、それだけでは後退とは判断できません。
[145件の記録](measurements/2026-11-01/icarus13-array-element-ports.json)と
[実装前の比較・解釈](notes/array-element-port-execution.ja.md)を参照してください。
古い数値の表には測定日を付けており、この機能の段階では14.0・母集団・sv-testsは再測定していません。

<a id="what-are-the-five-gaps-in-the-latest-finite-inventory"></a>

<a id="what-are-the-three-gaps-in-the-latest-finite-inventory"></a>

### 最新の有限の一覧に残る 3 件は何ですか？

`specify_path`・`dense_udp_coverage`とIcarusの拡張`real_net`です。
[同じ固定145プローブ](measurements/2026-11-01/icarus13-port-effective-kinds.json)で、
[実効ネット種別の対応](notes/port-kind-execution.ja.md)後は内側supplyの2件も成功しました。密なUDPの例は、プローブのフロントエンド実行上限15秒に達します。
UDPという構文全体が未対応という意味でも、実行値が不一致という意味でもありません。
必要なspecify/VPIの実行には詳細契約と実装が残り、この有限の一覧がP1の残作業全体でもありません。

<a id="my-design-prints-something-different-from-icarus-verilog-or-verilator-which-is-right"></a>

### Icarus Verilog や Verilator と違う値を表示します。どちらが正しいですか？

記載した期待値を保ち、規格と承認済みの契約を確認します。[参照側の不一致](GLOSSARY.ja.md#reference-mismatch)と
[参照側が通る不足](GLOSSARY.ja.md#passing-reference-gap)は別です。順序や任意のポートcollapseによっては、
複数の結果が合法です。最新の一覧は、従来の参照不一致4件と、新しい任意の非collapseの内側の値の相違11件を
分けて記録し、参照側の出力を写して期待値を変えていません。[検証ガイド](VALIDATION.ja.md)を参照してください。

<a id="how-does-it-differ-from-icarus-verilog-and-verilator"></a>

### Icarus Verilog や Verilator と何が違いますか？

kirimasimは承認済みの4値と全リージョンのスケジューリングを、共有カーネルで実行することを目指します。
現在は第1層がSIRを解釈し、C++を生成する第2層はP3の予定です。他の処理系は比較の根拠であり、期待値の正本ではありません。
機能と速さの比較は[測定記録](BENCHMARKS.ja.md)に書いた版・入力・指定の範囲で読み、
すべての設計や他ツールの現在の版についての一般的な優劣とは扱いません。

<a id="how-does-it-differ-from-lyra"></a>

### Lyra と何が違いますか？

[記録したLinux比較](../README.md#on-linux-development-versions-with-lyra)の対象はLyra `63922502ff0a`と、
そこに記載したkirimasimの版です。backend、ビルドできた設計、時間は、その版の観測として読んでください。
今回の文書更新ではLyraの現在の版や機能を再測定していません。コマンドの移行は[移行ガイド](PORTING.ja.md)を参照してください。
外部ツールの例も、それぞれ根拠と制限を記載しています。

<a id="do-you-check-against-commercial-simulators"></a>

### 商用シミュレータでも確認していますか？

本プロジェクトでは、商用SystemVerilogシミュレータのライセンスを保有していません。
商用製品では確認しておらず、今後も本プロジェクトでは実行・比較を行わない方針です。
第三者が得た商用シミュレータの出力・診断も、テスト期待値の作成や実装判断へ流用しません。
結果を非公開にするだけで利用条件を満たすとは判断しません。期待値はIEEE 1800-2023と
承認済みのプロジェクト仕様に基づき、独立した参照モデルと、文書に記録した比較環境で確認します。

公開契約の例として、[Synopsys Cloud Service Agreement 2024](https://www.synopsys.com/cloud/service-agreement.html)の
§9.3.2・§9.3.5・§9.3.7には、リバースエンジニアリング、競合製品の開発・改良への利用、
ベンチマーク目的の利用・結果開示に関する禁止事項があります。2026-11-01には、ログインや
製品ライセンス認証なしで本文を閲覧できる公開ページでした。公開契約文の閲覧と製品ライセンスの
保有は別で、この例示は、本プロジェクトが当該契約を締結していることや、当該契約が
本プロジェクトへ適用されることを示すものではありません。

適用条件は製品・契約・法域によって異なり、通常の動作確認が常に禁止される、または
リバースエンジニアリングに当たるという断定ではありません。商用製品を利用しないのは、
このプロジェクトの運用方針です。商用製品での実行可否や期待値との一致は未確認です。
[投稿方針](#may-i-include-commercial-simulator-output-or-diagnostics-in-an-issue)も参照してください。

<a id="does-it-support-class-objects-or-only-static-properties"></a>

### class は static プロパティだけでなく、オブジェクトも動きますか？

両方に対応する範囲があります。オブジェクトは型付きフィールド、段階的なコンストラクタ、継承、
直接/仮想メソッド、待つtask、浅いコピー、受け手の保持に対応します。非パラメータ化staticは共有記憶域です。
propertyのref・部分書き込み・NBA対象、観測と書式、managed-eventフィールド、パラメータ付きstatic記憶域は境界を保ちます。
[実行例](P1_FEATURES.ja.md#class-objects-and-virtual-dispatch)と[厳密な範囲](notes/class-source-execution.ja.md)を参照してください。

<a id="does-it-run-uvm"></a>

### UVM は動きますか？

まだです。classやコンテナの例が動いても、すべての組み合わせ、制約、同期プリミティブ、外部インターフェースが
使えるわけではありません。必要なIcarus対応範囲はP1の作業で、より広いUVMと制約付き乱数化は後の予定です。
UVMはclass名の特別扱いをせず、通常のSystemVerilogソースとして動かす方針です。

<a id="does-it-support-vpi-dpi-c-or-cocotb"></a>

### VPI・DPI-C・cocotb は使えますか？

実行できる統合はまだありません。Icarusが動かすVPIの必要範囲はP1で、DPI-Cとより広いインターフェースは後の予定です。
ヘッダの準備やAPIの記述は実行対応とは別です。cocotbの統合にも未完のVPIが必要です。
[未実装のインターフェース](REFERENCE_MANUAL.ja.md#未実装のインターフェース)を参照してください。

<a id="is-slang-modified"></a>

### slang は改変していますか？

いいえ。構文解析とエラボレーションは、[README](../README.ja.md#状況2026-11-01)が示す固定版を改変せず使います。
フロントエンドのソース処理の上限とカーネルの実行上限は別なので、切り分けには`parse`も使ってください。
上流の新しい版で例が通っても、リポジトリの固定版が通る証拠にはなりません。実際の版を残してください。

<a id="values-scheduling-and-lifetime"></a>

## 値・スケジューリング・寿命

<a id="which-version-of-the-standard-does-it-follow"></a>

### 規格のどの版に従っていますか？

IEEE 1800-2023を目標とし、slangを`--std=1800-2023`で動かします。公開の版切り替え指定はありません。
フロントエンドの診断は既定で厳格に保ち、レビューした選択は[未規定の動作](spec/UNSPECIFIED_BEHAVIOR.ja.md)に記録します。
これは契約であり、規格の全機能の実装済みという主張ではありません。

<a id="is-there-a-two-state-mode-for-speed"></a>

### 速くするために 2 値モードにできますか？

4値のlogicを一律に2値シミュレーションへ変える指定はありません。宣言した型は区別し、
`logic`はX/Zを保持でき、`bit`は保持できません。予定する第2層の解析でも、結果を保てる所だけを2値の記憶域にします。
[値の型](spec/VALUE_SEMANTICS.ja.md#12-4-値の型と-2-値の型)を参照してください。

<a id="can-i-replace-unknown-initial-values-without-changing-explicit-initializers"></a>

### 明示的な初期化を保ち、不定の初期値だけを変えられますか？

`--init-values=x|zero|one|random`で、対象となる未初期化の4値変数を選べます。
明示初期化、net解決、すべての型の既定値を置き換えるものではありません。randomの再現には`--seed`も残します。
初期値の変更でリセット不足が見えなくなる場合があるため、指定を記録してください。
[初期値の範囲](P1_FEATURES.ja.md#ソース変数の初期値)に詳しい説明があります。

<a id="why-does-display-after-a-nonblocking-assignment-show-the-earlier-value"></a>

### NBA の直後の display が以前の値を出すのはなぜですか？

NBAは更新を予約するので、発行元の次の文は更新より先に動く場合があります。programの処理は通常のNBAではなくRe-NBAを使います。
意図した更新の後になる観測点を選んでください。`#0`は「すべてのNBAの後」という意味ではありません。
[用語](GLOSSARY.ja.md#スケジューリング)と[Reactive側の例](P1_FEATURES.ja.md#programとreactive側の実行)を参照してください。

<a id="can-always_ff-wait-on-a-computed-expression-or-runtime-index"></a>

### always_ff で計算した式や実行時の添字を待てますか？

[明示的な式のイベント制御の範囲](P1_FEATURES.ja.md#expression-events-in-always_ff)で対応します。
型付きwatchを使い、整数の式・実行時選択・固定項目との混在を扱い、単一writerとタイミングの制限も保ちます。
最初の基準値の登録だけでは本体を動かしません。class/eventの観測との組み合わせや、本体の追加の待機には制限が残ります。

<a id="does-changing-an-iff-condition-wake-the-process-by-itself"></a>

### iff の条件が変わるだけで再開しますか？

いいえ。`iff`はイベントが起きたときに評価する条件で、`wait (condition)`の代わりではありません。
式のedge/変化とguardは別です。イベント・guardで使う関数にも、外側への書き込みに関する制限が適用されます。
[watchの動作](notes/always-ff-expression-execution.ja.md)を参照してください。

<a id="what-does-disabling-a-named-fork-cancel"></a>

### 名前付き fork の disable は何を止めますか？

`disable workers`は対象の名前付きparallel scopeと、その保持された子の活動を止めます。
`join_none`や呼び出しから戻った後に残る子も対象です。無関係の活動、予約済みNBA、確定済みの報告は保ち、
対象の待機を取り消します。同じ名前の綴りを持つ全processを一つの対象にする意味ではありません。
[例と制限](P1_FEATURES.ja.md#disable-of-a-named-fork)を参照してください。

<a id="are-copying-a-class-handle-and-copying-an-array-the-same-operation"></a>

### class のハンドルと配列は、同じようにコピーされますか？

違います。ハンドルの代入は同じオブジェクトを参照し、浅いオブジェクトコピーでは新しいオブジェクトの内部にある
ハンドルの参照先を保ちます。対応済みの動的配列・キューの値代入は、型の規則に従って所有する値をコピーします。
`ref`は値のスナップショットではなく、呼び出し元の記憶域を参照します。[記憶域と寿命](GLOSSARY.ja.md#記憶域オブジェクト寿命)を参照してください。

<a id="why-can-differently-numbered-fixed-array-ports-share-elements"></a>

### 添字の数値が違う固定配列ポートを共有できるのはなぜですか？

互換な次元は宣言順に対応し、数値が逆向きでも左端同士を合わせます。各宣言のindexやqueryのviewは保持します。
対象となる静的な要素・packed部分・連結のinput/outputは物理的な強度も保ちます。
[wired・pull・supplyの実効種別をbitごとに扱います](notes/port-kind-execution.ja.md)。
明示的な部分inout、uwire、変換、forceの観測には制限が残ります。
[二つのポートの形](P1_FEATURES.ja.md#固定配列ポート)を参照してください。

<a id="waveforms-limits-and-performance"></a>

## 波形・上限・性能

<a id="can-it-write-waveforms"></a>

### 波形を書き出せますか？

4値のVCDを、デザインのdump taskか`--wave=<file>.vcd`で書けます。
FSTの依存は準備済みですが出力バックエンドは未実装で、名前の変更で形式が有効になるわけではありません。
[波形指定](REFERENCE_MANUAL.ja.md#波形のオプション)を参照してください。

<a id="why-is-a-signal-or-transition-missing-from-vcd"></a>

### VCD に信号や遷移がないのはなぜですか？

scopeと深さ、`--wave-scope`、dump on/off、値が変わったかを確認します。通常出力は時刻slot内の確定した変化をまとめ、
`--wave-glitches`は対応する途中の変化も残します。unpacked配列やclass/objectの観測には別の未対応の境界があります。
型の実行対応が波形対応も意味するとは限らないため、[VCDの制限](USER_MANUAL.ja.md#vcd波形の記録)を確認してください。

<a id="how-do-i-stop-a-run-that-does-not-end"></a>

### 終わらない実行を止めるには？

`--max-time`はデザイン時刻の上限（終了4、`--max-time-ok`なら0）、`--max-wall-time`はコンパイルを除く
シミュレーションの実時間上限（終了4）です。時刻が進まない処理はzero-time上限で止めます（終了5）。
Ctrl-Cは正常な停止を要求します（130）。`--max-time-ok`は実時間・zero-time・資源・デザインエラーを免除しません。
[実行上限](USER_MANUAL.ja.md#実行とリソースの上限)を参照してください。

<a id="why-can-a-process-limit-be-reached-with-few-initial-blocks"></a>

### initial が少ないのに process の上限に達するのはなぜですか？

`--max-procs`はソースのinitialだけではなく、登録待ち・待機中を含む未終了のruntimeの子を数えます。
終了した子は、子孫のために管理記録を保持していても容量を返します。メモリ、stack、命令回数の上限とは別です。
[process上限の例](P1_FEATURES.ja.md#未終了のruntime-processの上限)を参照してください。

<a id="does-exit-0-prove-my-self-check-passed"></a>

### 終了 0 なら自己検査も成功したと言えますか？

いいえ。通常のfinish、quiescent、許可した時刻上限でも0になり、判定器には期待出力の検査も必要です。
このシミュレータでは`$error`だけでOSの終了コードは非0になりません。失敗させる自己検査には`$fatal`などを適切に使い、
stderrも残してください。[終了の規則](REFERENCE_MANUAL.ja.md#終了と終了コード)を参照してください。

<a id="how-fast-is-it"></a>

### どのくらい速いですか？

日付付きの[ベンチマーク](BENCHMARKS.ja.md)を参照してください。設計規模、フロントエンド、実行、出力、
ビルド構成、ツールの版によって経過時間は変わります。`--stats=json`とPerfCountは処理回数であり、実時間ではありません。
C++を生成する第2層はP3の予定で、現在の実行経路ではありません。

<a id="does-it-use-more-than-one-thread"></a>

### 複数スレッドで実行しますか？

一つのシミュレーションは一つのスレッドで動きます。独立したContextはそれぞれ状態を所有し、同時に動かせます。
一つのデザインの内部並列化は後の作業です。Contextの同時実行テストが、一つのデザインの並列実行を示すわけではありません。
[構成](ARCHITECTURE.ja.md)を参照してください。

<a id="which-platforms-does-it-run-on"></a>

### どの環境で動きますか？

LinuxとmacOSを対象にしています。[ビルド手順](USER_MANUAL.ja.md#ビルドとバージョンの確認)と
コミット済みのCI/nightlyが、対応するコンパイラと構成を示します。Windowsは未確認です。
一回の測定に使ったホストを、対応環境全体の一覧とは扱いません。

<a id="development-and-contributions"></a>

## 開発と貢献

<a id="does-checking-mutation-anchors-prove-the-tests-catch-mistakes"></a>

### 変異の適用位置を調べれば、誤りの検出も証明できますか？

いいえ。`tools/mutants.py --check`は置換を適用できることだけを確認します。
検出には、選んだ変異をそれぞれコンパイルし、テストが失敗することを確かめて、ソースとビルドを復元する必要があります。
選んだテストとログを保ち、[検証ガイド](VALIDATION.ja.md)に従って適用可否と検出を区別します。

<a id="is-the-code-written-by-ai"></a>

### コードは AI が書いているのですか？

大部分はClaude Code・Codex・OpenCodeなどのAIツールが生成しています。コミットの`Assisted-by`にツールとモデルを記録します。
仕様、期待値、レビューの判断は人が担い、[AI方針](spec/AI_POLICY.ja.md)で境界を定めます。
テストの`ai`という記録は、人のレビュー完了を意味しません。

<a id="may-i-include-commercial-simulator-output-or-diagnostics-in-an-issue"></a>

### Issueに商用シミュレータの出力・診断を載せてもよいですか？

受け付けません。Issue本文・コメント・Discussionsに加え、ログ、画像、波形、添付ファイル、
外部リンクも対象です。匿名化した抜粋・要約や第三者の実行結果も持ち込まないでください。
報告のために商用製品を実行したり、他の投稿者へ実行を求めたりすることも避けてください。

期待する動作は、IEEE 1800-2023の条項番号と自分の言葉による説明、自作の最小例で示します。
実行証跡はkirimasimか、文書に記録した本プロジェクトの比較環境の結果に限り、版・コマンドと
ソースのコミットを記録します。規格本文、他人のコード、非公開情報も転載しないでください。
該当内容を見つけても引用・要約・取り込みは行わず、投稿者または管理者による対応を求めます。

4種類すべての[Issueフォーム](../.github/ISSUE_TEMPLATE/)に投稿前の案内と必須確認欄を置きます。
一般投稿者向けの空欄Issueは引き続き無効です。ただし内容の自動検査ではなく、管理者等の空欄投稿、
API・CLIからの投稿、投稿後のコメントや添付を完全に防げるものではありません。
[GitHubのテンプレート設定](https://docs.github.com/en/communities/using-templates-to-encourage-useful-issues-and-pull-requests/configuring-issue-templates-for-your-repository)も参照してください。

<a id="how-can-i-help"></a>

### どうすれば手伝えますか？

独自の最小再現、不具合、期待値の懸念、仕様案は[Issues](https://github.com/kurochan001/kirimasim/issues)、
質問やアイデアは[Discussions](https://github.com/kurochan001/kirimasim/discussions)で受け付けます。
プルリクエストは現在停止しています。再現できる入力、関連する節や契約、版、投稿方針で認める出力を添え、
[投稿方針](#may-i-include-commercial-simulator-output-or-diagnostics-in-an-issue)に従い、非公開の設計情報は避けてください。
[貢献の案内](../CONTRIBUTING.ja.md)を参照してください。
