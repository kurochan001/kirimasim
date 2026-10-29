# kirimasim 開発者ガイド

[English](DEVELOPMENT.md) · 日英で食い違う場合は英語版を正本とします。

合意した機能の実装や不具合修正を行う保守担当者向けのガイドです。
[CONTRIBUTING](../CONTRIBUTING.ja.md)と[AGENTS.md](../AGENTS.md)の規則を補います。
外部からの参加は現在、仕様提案とIssueが対象です。PRは無効で、権限を持つ共同開発者が
`main`へpushします。このガイドでその方針を変えるものではありません。

実例は、ソース`5e8ab597`での既存の`$countones`・`$countbits`の実行経路を辿ります。
確認日は**2026-10-29**です。機能の実装箇所と根拠の探し方を示します。
システム関数の名前自体を追加する場合は、固定した未改変slangでの対応も必要です。

## 開発用の作業環境を準備する

必要な環境は[ユーザーマニュアル](USER_MANUAL.ja.md#ビルドとバージョンの確認)を参照してください。
依存の追加や新しいツールの導入は事前に確認します。リポジトリのルートから実行します。

```sh
cmake --preset dev -DKIRIMASIM_WERROR=ON
cmake --build --preset dev
build/dev/src/driver/kirimasim --version
ctest --preset dev -N
```

`-N`はテストを実行せず一覧を表示します。構成ごとにビルド先は別です。
現在の実行ファイルはSIRを直接解釈し、第2層のC++生成はP3の計画です。
ライブラリの境界は[アーキテクチャ](ARCHITECTURE.ja.md)、古い段階名は
[段階の対応表](notes/phase-renumbering.ja.md)を参照してください。

## 契約と独自の最小再現例から始める

[docs/spec](spec/)で対象の節と承認状態を確認します。ビットクエリなら、
[システムタスク8節](spec/SYSTEM_TASKS.ja.md#8-変換と問い合わせ)と
[値の意味](spec/VALUE_SEMANTICS.ja.md)の表現から読みます。規格の節番号、引数と結果の型、
幅、X/Z、評価順、副作用、寿命、エラーの結果を記録してください。

必要な動作が詳細仕様と食い違う場合は、実装の前に仕様修正を提案し、承認を受けます。
他シミュレータの出力だけで言語の規則を決めず、規格、承認済みの契約、独立した参照実装を
使います。CONTRIBUTINGのクリーンルーム規則に従い、Icarus・Verilatorの実装コードを
読んだり、他プロジェクトの再現コードをコピーしたりしません。slangは未改変で使い、
必要なフロントエンドの修正は上流へ報告します。

独自の[ビットクエリの例](examples/development/bit_queries.sv)は、X/Zを含む8ビットの値と、
語の境界を越える65ビットの値を使います。実行時に代入し、`$fatal`で結果を検査して、
次を表示します。

```text
ones=3 unknown=2 wide=2
```

フロントエンドを検査し、変換結果を確認してから実行します。

```sh
build/dev/src/driver/kirimasim parse --top=bit_queries \
  docs/examples/development/bit_queries.sv
build/dev/src/driver/kirimasim --dump-sir --top=bit_queries \
  docs/examples/development/bit_queries.sv > bit_queries.sir
rg -n '\$countbits' bit_queries.sir
build/dev/src/driver/kirimasim --top=bit_queries \
  docs/examples/development/bit_queries.sv
```

各コマンドの終了コードは0です。ダンプは診断形式で、コンパイル済みモデルとして
読み込む形式ではありません。この例は文書用データで、テストスイートへの自動登録はしません。

## 既存の関数1つを実装全体で辿る

### 1. フロントエンドとSIRへの変換

[front.cpp](../src/front/front.cpp)は固定版フロントエンドを設定して実行します。
[lower_expr.cpp](../src/front/lower_expr.cpp)で、`$countbits`・`$countones`・
`$onehot`・`$onehot0`・`$isunknown`を扱う分岐を探してください。

```sh
rg -n 'countones|countbits' src/front/lower_expr.cpp
```

この分岐は、対応する引数をビットストリームへ変換し、`$countones(value)`を、制御ビット1を
渡す`$countbits`という名前の`Syscall`へ変換します。`$isunknown`にはXとZを渡し、
one-hotのクエリは個数との比較も行います。明示した制御値は最下位ビットを使います。
型付きの変換で結果の幅と状態を決めます。近くの定数評価の経路も確認してください。
定数だけの例では実行時の実装の不具合を見逃す場合があります。

必要な順序で引数を1回だけ捕捉します。対応するソースの形を追加するときは、
static/automatic、呼び出し、ref、集約値、待機を個別に考えます。未対応の形には、
推測した変換ではなく位置付き診断が必要です。フロントエンドでの受理だけでは不十分です。

### 2. SIRの境界

演算と型付き引数は[design.h](../src/sir/include/kirimasim/sir/design.h)に定義します。
[検証器](../src/sir/verify.cpp)が表現を検査し、[テキスト出力](../src/sir/text.cpp)と
[types.cpp](../src/sir/types.cpp)の演算名で内容を確認できます。

この関数群は`Op::Syscall`を再利用し、ソースの名前ごとに新しいopcodeを必要としません。
表現全体の検証だけでは、各syscallの実行時の意味までは保証できません。新しい演算、
引数形式、メタデータが必要なら、合意したSIRの契約、検証器、テキスト形式とテストを
合わせて直します。この境界では、コンパイラが出す正しい形と、不正な形の両方を検査します。

### 3. VMの実行分岐

[vm.cpp](../src/vm/vm.cpp)で、`$countbits`から`SysTask::CountBits`への名前の対応と、
実行するcaseを辿ります。VMは数える0・1・X・Zの分類を集め、引数の幅で狭幅・広幅の
カーネル関数へ振り分け、結果を命令の型付き一時値へ格納します。

VMは型付きSIRの実行とフレーム管理を担当し、共通の値の意味はカーネルの値演算に置きます。
状態を持つタスクでは、所有するContext、スケジューラの領域、終了、停止・再開、後処理も
辿ってください。信号の値は型付き引数で指定し、実行中にソース名から探索しません。

### 4. カーネルと独立した参照実装

狭幅の高速実装は[w4ops.h](../src/kernel/include/kirimasim/kernel/w4ops.h)の
`countBits4`です。広幅は[wide.cpp](../src/kernel/wide.cpp)の`wide::countBits`で、
[wide.h](../src/kernel/include/kirimasim/kernel/wide.h)に宣言します。
独立した意味の参照実装は[value.cpp](../src/valsem/value.cpp)の`valsem::countBits`です。

参照実装は単純で、高速実装から独立したものにします。語の境界に揃わない幅の余りビットも
検査し、X/Zの区別を保ってください。`kernel`をslangやSIRへ依存させたり、アルゴリズムを
共有するために製品のカーネルを`valsem`へリンクしたりしません。両者の比較はテストで行います。
許される依存の向きは[アーキテクチャ](ARCHITECTURE.ja.md#ライブラリ)にあります。

### 5. 複数の境界での検証

[W4Ops.CountBits](../tests/unit/w4ops_test.cpp)は、生成した値と幅、分類の全16通りのマスクを
使い、狭幅の高速実装を独立した参照実装と比較します。既存の
[集約値のソース例](../tests/suite/values/aggregate_bit_query_record_fork_two_65.sv)は、
fork内の広幅集約値について、フロントエンドの変換と実行も検査します。
確かめる性質が異なり、片方だけではすべてのソースの形を証明できません。

これらの既存例を選択して実行します。

```sh
ctest --preset dev -N \
  -R '^(W4Ops[.]CountBits|suite/values/aggregate_bit_query_record_fork_two_65)$'
ctest --preset dev --output-on-failure \
  -R '^(W4Ops[.]CountBits|suite/values/aggregate_bit_query_record_fork_two_65)$'
```

選択は2件になるはずです。追加したテストも、実際に登録した名前で実行してください。
選択が0件では、新しい機能を検証したことにはなりません。

## 承認済みの拡張を実装する

前述の経路を参考に、変更が必要な境界を決めます。

1. 独自の最小入力を書き、必要な結果とその根拠を示します。合法だが未対応の入力と、
   言語として不正な入力を区別します。
2. 値の意味が変わる場合は独立した参照実装を定義・拡張します。
   実行層で共有する動作には、型付きのカーネル機能を選びます。
3. 既存の型付き演算で表せない場合にSIRを拡張し、新しい不変条件の拒否テストを追加します。
4. カーネルの関数と参照実装との比較テストを作り、必要なVMの分岐やフレーム処理を実装します。
   評価と記憶域の寿命を守ります。
5. フロントエンドを接続し、定数と実行時、必要な幅・型、X/Z、副作用のある引数を
   ソースレベルで検査します。
6. 該当する診断、資源、終了時の動作も確認します。状態を持つタスクには、順序、繰り返し、
   後処理、独立したContextのテストも必要です。
7. [P1対応機能](P1_SUPPORT.ja.md)、関係するユーザー・リファレンスの節、実行例を日英で更新します。
   利用者から見える動作の変更には、CONTRIBUTINGに従ってCHANGELOGも記載します。

すべての変更で全ライブラリを触る必要はありません。既に検査した関数を使う新しい変換なら、
カーネルの演算追加が不要な場合もあります。承認した契約と依存関係の範囲で実装してください。

## 期待値を弱めずにテストを追加する

検査する境界に合わせて配置します。

- **値の関数：** `tests/unit/`で独立した参照実装と比較し、狭幅・広幅、符号、X/Zを扱います。
- **SIR・VM：** `tests/unit/`で正しい・不正な型付き形式、実行、寿命を検査します。
- **ソース言語：** `tests/suite/`へ新しい`.sv`を置き、正しいsv-tests互換のメタデータと、
  独立した根拠を持つ期待値を付けます。
- **driverの動作：** `tests/cli/`で引数、stdout/stderr、終了コードを検査し、
  新しいrunnerを対応するCMakeへ登録します。
- **Contextの分離・処理回数：** その性質が変わる場合は、`tests/context/`や
  `tests/perf_counts/`の該当する契約を検査します。

スイートのファイルはCMakeの構成時に検出します。`:top_module:`・`:type: simulation`・
`:expected_stdout:`と、適用する期待値の根拠、節番号、状態、レビュー情報は
[テストスイート仕様](spec/TEST_SUITE.ja.md)に従って記載します。拒否する場合は
`:should_fail_because:`と、正確な位置・文面の`:expected_error:`を付け、終了コード2と
意図した診断の両方を検査します。ヘッダの指定に依存する前に
[run_suite_test.py](../tools/run_suite_test.py)を確認してください。
各デザインは分離した一時ディレクトリで実行します。新しいユニットテストのソースは
[tests/unit/CMakeLists.txt](../tests/unit/CMakeLists.txt)へ追加します。

既存の期待値とテスト本体は保護されています。仕様と既存の期待値が食い違う場合は、
先に根拠を示してレビューを受けます。期待値の修正は、理由と規格の節番号を持つ
承認済みの独立したコミットにし、`src/`の変更から分けます。新しいテストと新しい期待値は
実装に添えられます。通すためにアサーションを弱めたり、スキップを増やしたりしません。
[CONTRIBUTING](../CONTRIBUTING.ja.md)の期待値変更の規則も参照してください。

## 検証して変更を公開できる形にする

開発中はビルドと対象を絞ったテストを実行します。必要に応じてサニタイザーとContextの
テストを行い、PerfCountの差分を確認します。pushできる段階では、リポジトリが定める
ローカルCIを実行します。

```sh
CTEST_PARALLEL_LEVEL=4 python3 tools/local_ci.py
```

並列数は環境に合わせて選んでください。既定ではdev・release・ASan・UBSanの構成・ビルド・
テスト、releaseのフラグ、プロジェクトの規則、未pushコミット、READMEの件数、clang-format・
clang-tidyを検査します。`build/local-ci/`の要約とログを確認し、実際に実行した範囲を報告します。
最小対応コンパイラとnightlyの検証は別であり、ローカルの要約からその結果は判断できません。

新しいテストで件数が変わる場合は、CONTRIBUTINGの手順とrunnerのdevのJUnit出力を使って更新し、
生成したREADMEの差分も確認します。PerfCountの回帰を通すために期待値を直さず、
[保守手順](../tests/perf_counts/README.ja.md)に従ってください。

本体はC++23で、カーネル公開ヘッダと生成コードはC++20です。公開ヘッダを増やしたら
[kernel_headers_cxx20.cpp](../tests/unit/kernel_headers_cxx20.cpp)にも追加します。
生成コードでは例外とRTTIを使いません。禁止機能と大域状態の規則を守り、実行時の状態は
Contextが所有し、エラーを例外としてカーネルの境界から外へ出しません。

コミットは1つの目的ごとにまとめ、英語の先頭行は72文字以内にします。作者のメールは
GitHubのnoreplyを使い、AIが手伝った場合は必要な`Assisted-by`トレーラーを付けます。
実際のモデルと版を示し、確認できない版は未確認と記載してください。
AIの`Co-Authored-By`や`Signed-off-by`は付けません。承認されたpushの前に、意図した差分と
上流の状態、未pushの各コミットの期待値変更を確認し、ローカルCIの要約を添えます。
非公開の設計、個人のパス、規格本文の引き写しをリポジトリへ入れないでください。
