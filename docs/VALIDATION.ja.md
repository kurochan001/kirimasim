# 検証・再現ガイド

[English](VALIDATION.md) · 日英で食い違う場合は英語版を正本とします。

各検証ツールが何を確かめるか、実行の再現方法、結果の読み方を説明します。
コマンドとローカル環境での小規模な実行例は、ソース`2020305e`で**2026-10-30**に確認しました。
P1は進行中です。合意した条件は[テストスイート](spec/TEST_SUITE.ja.md)と
[P1の範囲](spec/P1_SCOPE.ja.md)が正本で、このガイドで合格条件を追加しません。

個別の失敗の調査は[不具合調査ガイド](DEBUGGING.ja.md)、実装とテストの追加は
[開発者ガイド](DEVELOPMENT.ja.md)を参照してください。

## 根拠の種類を分ける

- **CTest・ローカルCI：** リポジトリのunit、ソース、CLI、Context、カウンタ、ツールの
  テストと、ビルド・静的な規則を検査します。成功しても、言語全体や外部比較の条件を
  達成したとは判断できません。
- **`valsem`・参照スケジューラとの比較：** 対象の値や処理列について、高速実装を
  独立したモデルと比較します。固定シードのローカルテストと、必要な連続nightlyは別の根拠です。
- **sv-testsのシミュレーション比較：** 固定した手書きのシミュレーションテスト集合の合格数を、
  LinuxとmacOSで個別に比較します。
- **固定P1母集団：** 承認済みの固定一覧の90%以上が独自スイートの判定を通る条件です。
  未対応の項目も分母に残します。
- **機能プローブ：** 記載した期待値に照らした、有限の機構ごとの比較です。
  差分がなくても全面的な同等性を証明しません。
- **ミューテーションテスト：** 意図した誤りをコンパイルして、テストが検出するか調べます。
  適用位置の検査だけでは、誤りの検出は確かめられません。
- **PerfCount：** 処理回数の回帰を検出します。経過時間とメモリの測定は
  [性能測定](BENCHMARKS.ja.md)を参照してください。

CTestの1項目が多数の設計の組み合わせを実行する場合や、構成ごとに件数が違う場合が
あります。テスト件数の合計を言語機能の種類数とは扱いません。固定の数値条件の母集団から
ある機構が除外されていても、Icarusを対象とした広い対応範囲の要件は引き続き適用します。

## 再現できる情報を記録する

[開発用ビルド](USER_MANUAL.ja.md#ビルドとバージョンの確認)の後、リポジトリのルートで
実行します。複数の試行を残す場合は、新しい出力先を選んでください。

```sh
mkdir -p build/validation-guide
git rev-parse HEAD > build/validation-guide/source-revision.txt
git status --short > build/validation-guide/worktree.txt
build/dev/src/driver/kirimasim --version > build/validation-guide/version.txt
```

実行ファイルが作業ツリーより古い場合もあるため、両方の版を保存します。構成、コンパイラ、
OS、順序付きの入力、作業ディレクトリ、指定、シード、対象テスト、比較相手と母集団の版も
記録してください。stdout、stderr、runnerの終了コードを残します。シェルで途中の失敗を
保持しないパイプラインでは、末尾の`tee`が失敗を隠す場合があります。
公開する前にパスと入力内容を確認してください。

## ローカルテストとCIを実行する

対象を絞ったときは実際にテストが選ばれることを確認してから実行します。

```sh
ctest --preset dev -N \
  -R '^(W4Ops[.]CountBits|suite/values/aggregate_bit_query_record_fork_two_65)$'
ctest --preset dev --output-on-failure \
  -R '^(W4Ops[.]CountBits|suite/values/aggregate_bit_query_record_fork_two_65)$'
```

確認した版では2件が選ばれます。push前にリポジトリが求める全検査は次のとおりです。

```sh
CTEST_PARALLEL_LEVEL=4 python3 tools/local_ci.py
```

並列数は環境に合わせます。既定ではdev・release・ASan・UBSanの構成・ビルド・テスト、
releaseのフラグ、プロジェクトの規則、未pushコミット、READMEの件数、clang-format・
clang-tidyを検査します。要約と`build/local-ci/`のログを確認してください。
devのJUnitは`ctest-dev.xml`です。構成を選んだり検査を省いたりした場合は、その範囲を
報告します。最小対応コンパイラ、nightly、外部比較は別の結果が必要です。

### 合格・失敗・未対応・スキップを読む

独自スイートの判定は[run_suite_test.py](../tools/run_suite_test.py)が行います。
拒否を期待するソースは、デザインの終了コード2と必要な診断によってテスト合格になります。
逆にデザインが終了コード0でも、期待するstdout、stderrなどの条件を満たすとは限りません。

Nightlyの`slang-main`ジョブは、明示的に
`-DKIRIMASIM_SUITE_DIAGNOSTICS=slang-main`で構成します。これにより判定ツールへ
`--diagnostic-profile=slang-main`を渡し、`proc/random_seed_not_a_variable`だけに
slang [PR #2076](https://github.com/MikePopoloski/slang/pull/2076)以降の診断を指定します。
終了コード2と、ファイル名・行・列・診断文の一致は引き続き必須です。他のテストは
ヘッダをそのまま使います。既定の`pinned`プロファイルでは既存の期待値を維持します。
この追従検査は、固定版で検証するP1ゲートとは区別します。

未対応の構文は、実行成功ではありません。`tests/suite/`では、未対応による終了コード2を
スキップにできるのは、承認済みの[一覧](../tests/suite/skips.txt)にある場合だけで、それ以外は
失敗です。スキップのharness終了コードは77です。一覧内のテストが通り始めたら、レビューして
一覧から外す必要があります。外部の独自スイートには、母集団用の別の未対応の判定があります。

アサーションとメタデータの内容も確認してください。期待出力や有効な自己検査のないテストでは、
値の意味に関する根拠が限られます。通すためにスキップを増やしたり、本体や期待出力を
弱めたりせず、[期待値の変更規則](../CONTRIBUTING.ja.md)に従います。

## 参照モデルと実行順序のテストを再現する

[fuzz.h](../tests/unit/fuzz.h)のユニットテスト用の指定は、シミュレータの`--seed`とは
別です。小さな再現例は次のとおりです。

```sh
KIRIMA_FUZZ_SCALE=1 KIRIMA_FUZZ_SEED=20261030 \
  build/dev/tests/unit/kirimasim_unit_tests --gtest_filter=W4Ops.CountBits
```

元の環境変数の値と、失敗時に表示するテストごとのシードを両方記録します。
渡したシードはテスト固有の定数と混ぜるため、表示した混合後のシードを環境変数へ
戻すと別の入力になります。`KIRIMA_FUZZ_SCALE=0`は無効化ではなく、最低1倍を使います。

[nightly](../.github/workflows/nightly.yml)ではUTCの日付のシードでファジングの試行数を100倍に設定します。
releaseビルドの後、ある日付の2つの参照比較グループを再現する指定は次のとおりです。

```sh
KIRIMA_FUZZ_SCALE=100 KIRIMA_FUZZ_SEED=20261030 \
  build/release/tests/unit/kirimasim_unit_tests \
  --gtest_filter='W4Ops.*:Valsem.*:Scheduler.*:Heap.*'
KIRIMA_FUZZ_SCALE=100 KIRIMA_FUZZ_SEED=20261030 \
  build/release/tests/unit/kirimasim_unit_tests --gtest_filter='Wide.*'
```

調べる実行の記録にある日付・シード・版を使ってください。ローカル実行の1回の成功では、
7回連続のnightly成功を示せません。P1判定では、日付ごとの結果と実際の選択を保存します。
1プロセスでのshuffle・反復テストは、テスト間に残る状態を調べます。
devビルドの後、3回反復する実行順序の確認例は次のとおりです。

```sh
build/dev/tests/unit/kirimasim_unit_tests \
  --gtest_shuffle --gtest_repeat=3 --gtest_random_seed=30
```

失敗を再現するときは、30と対象をそのジョブの記録に合わせます。GoogleTestのシードが
決めるのは実行順であり、値演算のファジング入力やSystemVerilogの乱数列ではありません。
workflowのサニタイザージョブも参照してください。

## 外部母集団を比較する

以下には準備済みのチェックアウトと実行ファイルが必要です。例の`checkouts/...`は、
自分で準備した配置先へ置き換えます。採用した版を確認し、ツールの導入や依存追加は事前に
確認してください。Icarus・Verilatorの実装ソースを読んだり、外部生成テストを取り込んだり
しません。

### sv-tests

[テストスイート仕様](spec/TEST_SUITE.ja.md)で採用した母集団とIcarusの版を使います。
母集団の固定先は[svtests.py](../tools/svtests.py)の`PINNED`にも記載しています。

```sh
SVTESTS_CHECKOUT=checkouts/sv-tests
SVTESTS_IVERILOG=checkouts/icarus/bin/iverilog
git -C "$SVTESTS_CHECKOUT" rev-parse HEAD
"$SVTESTS_IVERILOG" -V
python3 tools/svtests.py --sv-tests "$SVTESTS_CHECKOUT" \
  --kirimasim build/release/src/driver/kirimasim \
  --iverilog "$SVTESTS_IVERILOG" --jobs 4 \
  --out build/validation-guide/svtests --gate
```

`results.json`と`summary.md`を生成します。意図した全母集団で両エンジンが動いたことを
確認してください。`--gate`が合格数を比較するのはkirimasimとIcarusを両方指定した場合です。
固定先と違う場合も、現実装は警告するだけで実行を止めません。対象を絞った結果や、版が
異なる結果は調査であり、正式なゲートではありません。必要なCIの各OSで測定し、
片方の結果からもう片方の達成を判断しません。

判定はsv-testsの期待失敗と`:assert:`の規則で、独自スイートのstdout期待値とは別です。
特にkirimasimの未対応による拒否は、期待失敗の合格にはなりません。外部生成の母集団は除外します。
`--parse-only --baseline tests/gates/svtests_parse.txt`は別の母集団を測って低下を警告する指定で、
simulationのゲートではなく、その警告でrunnerの終了コードが失敗になるわけでもありません。

### 固定した独自P1母集団

[p1_population.txt](../tests/gates/p1_population.txt)の先頭行にある版の母集団と、
承認した期待値を使います。runner自身はチェックアウトの版を強制しません。

```sh
P1_TESTS_CHECKOUT=checkouts/kirimasim-tests
git -C "$P1_TESTS_CHECKOUT" rev-parse HEAD
python3 tools/p1_population.py run --tests-repo "$P1_TESTS_CHECKOUT" \
  --kirimasim build/release/src/driver/kirimasim \
  --list tests/gates/p1_population.txt --jobs 4 \
  > build/validation-guide/p1-population.out
```

合格・失敗・未対応を別に読みます。合格率は、全固定一覧を分母にした合格数で、未対応を
除外しません。90%未満なら終了コード1で、終了コード0でも全件合格ではありません。
`select`は母集団の案を生成する指定です。検証中に承認済み一覧を上書きしないでください。
一覧や期待値の変更には、合意したレビューの手順が必要です。

## 有限の機能プローブを比較する

導入済みのIcarusコンパイラとランタイムで、独自プローブを1つ選び、両者の版と判定した
出力を保存する小さな実行例です。

```sh
python3 tools/icarus_feature_probe.py \
  --kirimasim build/dev/src/driver/kirimasim --iverilog iverilog --vvp vvp \
  --only wide_integral --require-parity \
  --output build/validation-guide/wide-integral.json
```

レポートの`probes`・`gaps`・`reference_mismatches`を確認します。`--require-parity`なしでは、
差分があっても終了コード0になり得ます。指定しても拒否するのは、Icarusが合格したのに
kirimasimが合格しないプローブだけで、Icarusが全期待値に一致したことまでは示しません。
`--extended`も有限の機構の追加で、全組み合わせではありません。レポート内の標準機能、
プロジェクトの選択、Icarus拡張の分類を区別してください。

## 負の対照と処理回数を確認する

ミューテーションの適用位置を読むだけの検査は次のとおりです。

```sh
python3 tools/mutants.py --check
```

コンパイルや誤りの注入は行わず、置き換える位置を検査します。現在の`--check`は、
`--only`があっても全一覧を検査します。実際のミューテーションは再ビルドと`src/`の
一時的な変更を伴うため、専用のクリーンなチェックアウトで、CONTRIBUTINGとnightlyの
手順に従って実行します。そのdev構成を準備した後、対象を選んだ誤り注入の実行例は次のとおりです。

```sh
python3 tools/mutants.py --preset dev --only countbits
```

`--only`は大小文字を区別した名前の部分一致です。`--shard 1/8`は一覧の一部分だけを選ぶため、
一覧全体の根拠には全区分の結果が必要です。runnerは未変更時の合格を確認し、各ソースを戻します。
コンパイルした誤りは、テストの失敗や新しく未対応になったスキップで検出します。
`DOES NOT APPLY`・`DOES NOT BUILD`・`SURVIVED`は問題であり、対照の成功ではありません。
選択が0件でないことも確認してください。

処理回数の検査は次のとおりです。

```sh
ctest --preset dev -L PerfCount --output-on-failure
```

既存予算より10%以上多い回数、期待したキーの不足、未知のキー、文字列カウンタの違いは
失敗になります。予算変更を提案する前に[PerfCountの保守手順](../tests/perf_counts/README.ja.md)を
確認し、回帰を隠すために期待値を上げないでください。回数の変化、時間の測定、
正しさの結果は、それぞれ別の知見です。

## 範囲を明記して結果を共有する

検証した問い、ソース・実行ファイル・母集団の版、コマンド、環境、対象、シード、
合格・失敗・未対応・スキップの件数、終了コード、成果物の場所を示します。
再現した事実、原因の推定、未確認の作業を分けます。push時はローカルCIの要約を添え、
実行していない検査を成功と報告しません。

本書のローカル環境での小規模なテスト選択、ミューテーションの適用位置検査、単一の機能プローブは、
コマンドの確認です。外部ゲート全体、実際の誤り注入、7晩のnightlyを新たに測定した報告では
ありません。日付付きの[P1監査](P1_CRITERIA_GAPS_2026-10-28.md)と生データは元の範囲で読み、
現在の完了判定には必要な新しい根拠を揃えてください。READMEの生成件数は、
CONTRIBUTINGと[readme_counts.py](../tools/readme_counts.py)に従い、対象を絞らない完全な
測定記録をレビューしてから扱います。
