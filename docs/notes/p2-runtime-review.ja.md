# P2 の実行時境界の照合

[English](p2-runtime-review.md)

P1 の基準は `241e6aaf`、今回照合した実装は `1fdd5b35` です。
P3 は、検証済み Design、フレームとスロットの配置記述、任意の解析テーブル、カーネル API を受け取ります。
C の配置、寿命、引数と結果は RUNTIME_ABI 4 節に定めています。ABI 0.5 には版を指定して取得できるサービスが 18 個あります。ABI 1.0 は、生成したモデルを評価した後、P3 で決定します。

## VM からカーネルへの呼び出し

[操作一覧](p2-runtime-calls.txt)は、P1 完了時と今回のどちらかで使われた修飾名 537 個を収録しています。
P1 完了時は 461 操作・1,494 箇所、今回は 517 操作・1,659 箇所です。現在の 517 操作のうち、326 個には C 関数表の対応先があり、22 個はホストの仕事、169 個は公開 C++20 ヘッダーのインライン実装です。操作ごとに対応先または理由、ソースの位置、両時点の件数を記録しました。現在の件数がゼロの行は、以前の直接呼び出しを示します。例えば、以前は VM が初期値を作る際に直接呼んだ `Rng::next` を、今は初期化サービスが呼びます。

集計には Clang が各 `src/vm/*.cpp` の翻訳単位で解決した呼び出し・メンバー参照・宣言参照を使い、VM のヘッダーと取り込まれた実装も調べました。対象はカーネルの関数・メソッド・コンストラクター・変換で、同じ修飾名とファイル・行の組み合わせは 1 回と数えます。オーバーロードは同じ行にまとめています。通常の strobe とファイル向け strobe には別の C エントリーがあります。ファイルを明示した表示は捕捉済みの出力先を使います。Context の用意、物理記憶域、実行ループ、計測はホストが担当します。純粋な値演算、借用する記述子、プロセスの判断は生成モデルからもインライン実装を使えます。ホストとインラインの選択は P3 の生成器を統合するときに再確認します。

## 実行層が共有する判断

各行は [P2 提案 2-d 節](sir-v1-proposal.ja.md)の順です。

| 仕組み | 判断を担当する箇所 | 動作を確認するテスト |
| --- | --- | --- |
| 数値変換・真偽・時刻表記 | カーネルの scalar_value、広幅変換、静的書式 | w4ops、wide、format |
| 記憶域とドライバーの初期化 | SIR のフレーム配置、カーネルの初期化、スロット、信号と強度 | init_values、activation、strength |
| 時刻ゼロの順番と感度 | SIR の processPolicy、sensitiveDuringExecution とカーネルの startTimeZero | ff_expression、clocking、scheduler |
| fork・join・子孫の保持 | カーネルの ForkState、suspendForJoin、frameRelease | C++20 モデル、named-fork |
| Postponed での書き込み | SIR の postponedAccess と効果記述、実体の由来 | verify_matrix、scheduler |
| 遅延・不明値・オーバーフロー | カーネルの delayValue とスケジューラの時刻上限 | 遅延の丸めとオーバーフロー |
| 添字・クリッピング・警告数 | SIR の宣言範囲、カーネルの position、inside と診断集計 | bounds_check、wide |
| 仮引数・活性化・コピーアウト | SIR の引数方向とフレーム配置、カーネルの FrameValue と活性化領域 | vm_frame_value、activation_environment |
| 参照と GC ルート | カーネルの place、reference、container、event。通常の VM フレームはアクセスごとに世代を検証 | scoped_reference、vm_event_gc、C の place モデル |
| コンテナの診断 | カーネルのヒープ操作、heapDiagnostic、objectDiagnostic | heap、class_vm_execution、C の run policy |
| 命令数・呼び出し上限・取消し | カーネルの stepsFit、takeStep、callDepthFull、スタック余裕とチェックポイント | C++20 モデル、ドライバー上限 |
| guard・watch の読み出しと購読 | SIR の monitorReads と effectiveFrame、カーネルの wait・monitor | nba_monitor、implicit_container、monitor |
| monitor と保持値 | 観測・信号サービスとスケジューラの hold・release | C の観測・信号モデル |
| スコープとタスクの disable | カーネルの planDisable、disableActivity、mayUnwind。SIR がスコープと出口の ID を供給 | 別ビルドの C++20 モデル、named-fork と disable |
| 既定値・強度解決 | カーネルの fillDefault、強度グラフ、ネイティブの解決処理 | strength_force_runtime、MOS、C の net モデル |
| 広幅演算 | カーネルの wide、logicalValue、真偽と選択 | wide と valsem の照合 |
| 文字列・実数・不明な条件での選択 | カーネルのスカラー・文字列サービス、selectString、selectReal、logicalValue | real・shortreal、C の execution モデル |
| 参照書き込み・NBA・延期イベント | SIR のオペランド・捕捉記述、カーネルの place、process、event、observation | event_nba_ref_window、vm_nba_event |
| スキャンとメモリファイル | カーネルの IO、presentation、run policy、型付き転送先とメモリ順序 | scan、メモリファイル、C の IO モデル |
| 組み込みメソッド | SIR のメソッドシグネチャ、文字列操作、管理イベントの識別 | 文字列メソッド、managed_event |
| 集合値の表示と書式引数 | SIR の projectPattern、カーネルの formatPattern、formatDisplayOperands、スカラー書式 | pattern_sir、pattern_front、format |
| 警告と乱数状態の所有 | カーネルの RunDiagnostics と乱数関数、SIR の processPolicy | cond_x、init_values、C の scalar・run policy |
| 呼び出し履歴・波形・システム関数 | front がスコープ・波形・ルーチンを記述し、カーネルの formatStackTrace、task、execution、VPI が実行 | stacktrace、C の signal・task・foreign モデル |

VM は自身のレジスターとフレームの添字を変換し、値、型付きオペランド、コールバックを受け渡します。保存した Design は論理宣言、ソース位置、クロック、トリガーと捕捉範囲、宣言型、波形情報を保持します。物理オフセットと最適化の解析結果は再計算します。解析キーには正準 Design、生成元、配置、パス・オプションが含まれます。情報がない場合とキーが異なる場合は不明として扱います。

## ソースから SIR への対応

| フロントの入力と担当 | 保存する表現・境界 |
| --- | --- |
| 展開済み定義・名前・階層（lower、lower_sub） | インスタンス・スコープ・名前・スロット・サブルーチン・フレーム・所有者の行。実行時は ID を使い、未解決または未対応の構文はフロントで診断 |
| 型付き式・選択・集合値・ストリーミング（lower_expr、lower_select、lower_struct、lower_stream） | 型付き SSA、定数、範囲、並べ替え。平坦化する集合値にも検証済みの操作が必要とする宣言次元や宣言型を保持 |
| 代入パターンと表示（lower_pattern、lower_system） | 構築は値演算に展開。表示は PatternLayout、format.pattern、型付きオペランド、メンバー順、列挙値、タグ |
| 文・制御・寿命（lower_stmt、lower_capture、lower_nba） | 基本ブロック、型付き継続、所有・捕捉、イベント制御、名前付きスコープ |
| 配列・コンテナ・クラス・インターフェイス（lower_array、lower_container、lower_class、lower_interface） | 型表・クラス表、割り当て・参照命令、対応するインターフェイス機能の明示的な束縛 |
| ネット・ポート・スイッチ・クロック・サンプル（lower_net_ports、lower_strength、lower_module_path、lower_clocking、lower_sampled） | 物理ソース・ドライバー範囲、強度エッジ、モジュールパス、クロック行、サンプル用スロット |
| システムタスク・ファイル・乱数列・波形・VPI（lower_system、lower_scan、lower_randsequence、lower_dump、lower_foreign） | 検証するシグネチャと型付き引数、展開した制御フロー、波形行、外部呼び出し箇所 |

[形式契約の一覧](../spec/sir-v1-contract.txt)に全レコード、バリアント、順序付きフィールド、命令、システムルーチンを収録しています。ソース側の対応範囲と診断は [P1 対応表](../P1_SUPPORT.ja.md)を参照してください。以後の機能は版を付けて拡張できます。
