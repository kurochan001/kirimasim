# 不具合調査ガイド

[English](DEBUGGING.md) · 日英で食い違う場合は英語版を正本とします。

失敗するデザインの最小化、失敗する段階の特定、SIRの確認、報告に必要な情報の収集を
説明します。実行例はソース`2020305e`で**2026-10-30**に確認しました。
意図的に未対応になる例と、上限で停止するループを含む、現在の実装の説明です。
オプションの定義は[リファレンスマニュアル](REFERENCE_MANUAL.ja.md)、
テスト結果の判定は[検証・再現ガイド](VALIDATION.ja.md)を参照してください。

## 変更前に元の失敗を記録する

正確なコマンド、順序付きのソース、トップ、include、マクロ、データファイル、
作業ディレクトリ、構成、実行ファイルの版を保存します。`--seed`、`--init-values`、
時間精度、警告のモード、資源上限も記録してください。作業ツリーの版だけでは、
古い実行ファイルを識別できません。

[開発用ビルド](USER_MANUAL.ja.md#ビルドとバージョンの確認)の後、リポジトリのルートで実行します。

```sh
mkdir -p build/debugging-guide
git rev-parse HEAD > build/debugging-guide/source-revision.txt
git status --short > build/debugging-guide/worktree.txt
build/dev/src/driver/kirimasim --version > build/debugging-guide/version.txt
```

複数の試行を残す場合は新しい出力先を選びます。変更前にstdout、stderr、実際の終了コードを
保存してください。起動表示と診断は通常stderrに出ます。また、終了コード0でも、要求した
波形ファイルを開けた証明にはなりません。

## 失敗する段階を特定する

既存の[ビットクエリの例](examples/development/bit_queries.sv)は、処理の流れを確認する
小さな成功例です。

```sh
build/dev/src/driver/kirimasim parse --top=bit_queries \
  docs/examples/development/bit_queries.sv \
  > build/debugging-guide/parse.out 2> build/debugging-guide/parse.err
build/dev/src/driver/kirimasim --dump-sir --top=bit_queries \
  docs/examples/development/bit_queries.sv \
  > build/debugging-guide/model.sir 2> build/debugging-guide/lower.err
build/dev/src/driver/kirimasim --top=bit_queries --seed=0 --init-values=x \
  --max-time=10ns --max-wall-time=5 --stats=json \
  --wave=build/debugging-guide/values.vcd \
  docs/examples/development/bit_queries.sv \
  > build/debugging-guide/run.out 2> build/debugging-guide/run.err
```

すべて終了コード0で、デザインの出力は`ones=3 unknown=2 wide=2`です。
最後の実行では、その後にJSON行が付きます。失敗するデザインは段階ごとに確認します。

- **引数やファイルで失敗する：** 実際の引数形式と作業ディレクトリを確認します。
  include、モジュールライブラリ、実行時データのパスは別の役割です。
  [移行ガイド](PORTING.ja.md#入力指定を明示的に移す)も参照してください。
- **`parse`で失敗する：** 前処理、型、エラボレーション、config、フロントエンドの検証を
  調べます。クエリの検証などkirimasim自身の検査も含むため、parse失敗だけで
  slangの不具合とは判断できません。
- **`parse`は通るがSIR生成で失敗する：** 最初の位置付き未対応・変換診断と
  [P1対応機能](P1_SUPPORT.ja.md)を確認します。フロントエンドの受理だけでは実行対応を示せません。
- **検証器で失敗する：** 内部表現の問題としてソース、コマンド、診断を保存します。
  `--dump-sir`が失敗した場合、有効なダンプではなく空ファイルが残ることもあります。
- **SIR生成は通るが実行に失敗・不一致がある：** 型付き命令の実行、スケジューリング、
  寿命、入力、終了理由を調べます。

終了コード2には入力、未対応ソース、設計の実行時エラーがあり、3は内部の失敗です。
`$fatal`や`--stop=error`は1になります。上限にはそれぞれ別のコードがあります。
コードだけではなく、[終了コードの一覧](REFERENCE_MANUAL.ja.md#終了と終了コード)と
位置付きのメッセージ、失敗した段階を合わせて読んでください。

### フロントエンドで受理されても実行は未対応の場合がある

独自の[オブジェクトの例](examples/debugging/unsupported_object.sv)は、確認した範囲では
意図的に実行できない例です。以下は個別に実行してください。

```sh
build/dev/src/driver/kirimasim parse --top=unsupported_object \
  docs/examples/debugging/unsupported_object.sv
```

parseの終了コードは0です。次のコマンドは終了コード**2**を期待します。

```sh
build/dev/src/driver/kirimasim --dump-sir --top=unsupported_object \
  docs/examples/debugging/unsupported_object.sv \
  > build/debugging-guide/unsupported.sir 2> build/debugging-guide/unsupported.err
```

診断は未対応の`sample_box`の記憶域と`NewClass`を示します。ファイルが存在しても、
変換成功の証拠にはなりません。これらは文書用の例で、スイートへ自動登録するテストでは
ありません。意図した失敗を、シミュレーション成功とは数えないでください。

## 推測せずにSIRを確認する

成功したダンプを検索します。

```sh
rg -n '^(precision|source|slot|process)|syscall.*\$countbits' \
  build/debugging-guide/model.sir
```

確認したformat 0の例には、次のような項目があります。

```text
slot %s1 "bit_queries.wide" : i4<65> var inst=0
%t5 = syscall s2<32> "$countbits", %t4, const i4<1> 1'h1
```

宣言した幅・状態、変数のslot、一時値、制御の流れ、ソース位置のコメントを確認します。
この例では`$countones`を、制御値1を渡す`$countbits`へ変換しています。
元の名前がそのままダンプに出るとは限りません。位置のコメントのファイル番号はsource表で
解決します。slot・一時値・processの番号は局所的な診断用IDで、変わる場合があります。
`precision`は全体のスケジューリング精度で、`sim.end_time`はそのtickで表します。

ダンプは安定した交換形式ではなく、コンパイル済みモデルとして実行する入力でもありません。
具体的な実装経路は[開発者ガイド](DEVELOPMENT.ja.md#既存の関数1つを実装全体で辿る)を参照してください。

stdout全体をJSONとして扱わず、最後の統計行を読みます。

```sh
python3 - <<'PY'
import json
from pathlib import Path
lines = Path('build/debugging-guide/run.out').read_text().splitlines()
stats = json.loads(lines[-1])
print(stats['sim.end_reason'], stats['sim.end_time'])
PY
```

この例は`finish 1000`です。精度1 psで1 nsを表します。コンパイルに失敗した場合は、
実行時統計やfinalの後処理が得られるとは限りません。

## 値と条件を調べる

算術演算の不具合と判断する前に、型の幅、符号、変換、初期化、ドライバの解決を確認します。
既定では初期値のない通常の4値変数はXで始まり、駆動のない通常のnetはZになり得ます。
10進表示でビットの状態が分からない場合は`%b`・`%h`を使ってください。

[X条件の例](examples/debugging/x_condition.sv)で確認できます。

```sh
build/dev/src/driver/kirimasim --top=x_condition --init-values=x --seed=0 \
  --cond-x=warn docs/examples/debugging/x_condition.sv
```

終了コードは0で、`ready=x`、`branch=false`を出します。警告は`if`の位置を示します。
不定な条件に対する想定どおりの扱いであり、カーネルの不具合の証明ではありません。
`--cond-x=warn`は値と分岐を保ち、通常実行の`error`は到達した報告を設計の実行時エラーにします。
ほかの初期値モードでは実験の前提が変わります。リセット不足を隠すモードへ変えるのではなく、
指定も記録してください。

対応するpacked選択・固定配列のアクセスには`--bounds-check=warn`を使い、
位置ごとの回数も読みます。すべてのコンテナ操作を検査するものではありません。
不正なキュー書き込みには、この指定と独立した必須警告もあります。
集約値やrefでは、値・選択式をいつ捕捉したか、待機中もどの記憶域が生きているかを確認します。
最小化しても、幅、X/Z、寿命の性質を保ってください。

## 時刻・見えない遷移・上限を調べる

ソースの時間単位と全体の精度を合わせます。期待する観測がActive、NBAの後、Postponedの
どこなのかを決めます。モジュールとprogramも別の領域集合を使います。
`$display`と`$strobe`は観測する地点が違います。新しい独自のプローブで違いを観測できますが、
既存テストの観測方法を書き換えると、レースを隠したり期待値の意味を変えたりする場合があります。

VCDは通常、タイムスロットで落ち着いた値を記録します。`--wave-glitches`は対象となる途中の
遷移を見せますが、スコープ選択や未対応のデータで信号が省かれる場合もあります。
`--report-pulses`は、対応する慣性遅延の経路で取り消した遷移を位置付きの注記で報告します。
未対応のタイミング機能を追加する指定ではありません。[波形仕様](spec/WAVEFORM.ja.md)と
[パルスの例](P1_FEATURES.ja.md#除去されたパルスの報告)も参照してください。

[ゼロ時刻の例](examples/debugging/zero_time.sv)には意図的に待機がありません。
次のコマンドの期待する終了コードは**5**です。

```sh
build/dev/src/driver/kirimasim --top=zero_time --max-steps=1000 \
  --max-wall-time=5 --stats=json docs/examples/debugging/zero_time.sv \
  > build/debugging-guide/zero-time.out 2> build/debugging-guide/zero-time.err
```

`final reached`を出し、最終JSONの理由は`zero_time_limit`、終了時刻は0です。
診断には待機せずに動いたプロセスが出ます。`--max-steps`はプロセスごとの連続命令数で、
ほかの処理やfinalも含む総数`vm.instrs`の上限ではありません。
時刻を進めないループは、シミュレーション時刻の上限だけでは止められません。

上限を増やす前に、報告した上限の意味を調べます。シミュレーション時刻と実時間の上限は
別の目的で使います。実時間は安全な地点での検査であり、止まったホスト処理の強制中断では
ありません。資源上限の終了コード6では、未終了の子や呼び出し・スタックの枯渇も確認します。
`--max-time-ok`が変えるのは意図した時刻境界の終了コードであり、記録する理由や
結果の正しさではありません。

## 担当する部分を探し、ホスト側の失敗を調べる

根拠に応じて境界を辿ります。

- [driver](../src/driver/main.cpp)：引数、モード、起動表示、終了コードへの対応。
- [front](../src/front/front.cpp)、[クエリ検査](../src/front/query_checks.cpp)、
  [式の変換](../src/front/lower_expr.cpp)：フロントエンドの設定とSIRの生成。
- [SIR検証器](../src/sir/verify.cpp)と[テキスト出力](../src/sir/text.cpp)：
  表現の正しさと、その診断用の表示。
- [VM](../src/vm/vm.cpp)：型付き命令の実行とフレーム。
- [スケジューラ](../src/kernel/sim.cpp)、[store](../src/kernel/store.cpp)、
  [heap](../src/kernel/heap.cpp)：領域、値、寿命。
- [値の参照実装](../src/valsem/value.cpp)、
  [参照スケジューラのテスト](../tests/unit/scheduler_test.cpp)：独立した比較。

これらは調査の出発点であり、終了コードだけから決めた原因ではありません。
未対応の形、設計のアサーション、資源上限、ホストのクラッシュを区別します。
サニタイザーの診断と最初の関係するスタックトレースを保存してください。
ビルドの前提を準備済みの場合、成功例をサニタイザーで再現する手順は次のとおりです。

```sh
cmake --preset asan
cmake --build --preset asan
build/asan/src/driver/kirimasim --version
build/asan/src/driver/kirimasim --top=bit_queries \
  docs/examples/development/bit_queries.sv
cmake --preset ubsan
cmake --build --preset ubsan
build/ubsan/src/driver/kirimasim --version
build/ubsan/src/driver/kirimasim --top=bit_queries \
  docs/examples/development/bit_queries.sv
```

その構成の古い実行ファイルは、現在のソースの根拠にはなりません。ホストのシグナルは、
Pythonのsubprocessでは負の終了値、シェルではシグナルの終了状態として見える場合があります。
どの方法で結果を得たかも残してください。新しい調査ツールの導入は事前に確認します。
外部フロントエンドの問題とする場合も、独立した最小例と、slangの動作とkirimasim自身の
検査を分ける根拠が必要です。

## 調べる問いを保って最小化・報告する

関係ないロジックを段階的に除き、変更のたびに失敗が残るか確認します。
原因を引き起こすクロック、精度、入力順、指定、シード、副作用、ref・automaticの寿命、
観測する領域を保ちます。新しく独自の再現例を書いて作業してください。
既存のテスト本体と期待値は[CONTRIBUTING](../CONTRIBUTING.ja.md)の規則で保護されています。

[不具合報告フォーム](../.github/ISSUE_TEMPLATE/bug_report.md)に、最小の独自ソースと
相対配置、正確なコマンド、ソース・実行ファイルの版、ビルド環境、stdout/stderr、
終了コード、期待する動作と根拠を添えます。失敗が必要な自己検査には`$fatal`を使ってください。
`$error`だけではkirimasimの終了コードを失敗にはしません。

観測した事実、原因の推定、未確認の作業を分けます。日付付きの
[失敗分析](kirimasim_failure_analysis_2026-10-28.md)と
[トリアージ台帳](kirimasim_failure_triage_2026-10-28.md)は過去の例です。
現在の問題として扱う前に、報告する版で再現してください。再実行の範囲と根拠の収集は
[検証・再現ガイド](VALIDATION.ja.md)を参照します。公開する報告に、非公開設計、
個人のパス、規格本文のコピー、商用シミュレータの出力を含めないでください。
