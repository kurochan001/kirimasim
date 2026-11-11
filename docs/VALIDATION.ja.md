# 検証・再現ガイド

[English](VALIDATION.md) · 日英で食い違う場合は英語版を正本とします。

公開済みの実装`91139559`に合わせて**2026-11-04**に更新しました。全local CIの21項目が
成功し、dev・ASan・UBSanは各12,012件、Releaseは12,013件が通りました。同じコミットの
[GitHub CI](https://github.com/kurochan001/kirimasim/actions/runs/41725997482)も成功しています。
[統合記録](notes/main-integration-2026-11-04.ja.md)では、この結果と機能プローブ・外部gateの根拠を区別します。

現行の[180プローブ](measurements/2026-11-04/icarus13-class-clocked-after.json)は、P1評価対象179件、
依存を確認した任意の`wire real`除外1件、必須範囲の差1件、参照不一致25件です。dense UDPの時間切れは
独立した失敗のままです。標準の`real`機能は既存の範囲で扱います。固定母集団・sv-tests・nightly fuzz・
性能測定は、それぞれの測定日を保ち、今回の文書更新で測り直したものではありません。

各検査が何を確かめるかと再現方法を説明し、新しい合格条件は追加しません。合意した条件は
[テストスイート](spec/TEST_SUITE.ja.md)と[P1の範囲](spec/P1_SCOPE.ja.md)が正本です。
以前のガイドの区切りは[状況の履歴](STATUS_HISTORY.ja.md#guide-checkpoints-before-2026-11-04)にあります。
小規模なコマンド例は`91139559`で確認しました。fuzz・変異の実行・外部測定のコマンドは手順の説明で、新しい測定結果ではありません。
個別の失敗は[不具合調査](DEBUGGING.ja.md)、実装の追加は[開発者ガイド](DEVELOPMENT.ja.md)を参照してください。

## 根拠の種類を分ける

[テスト件数](GLOSSARY.ja.md#test-count)は特定の検証対象を数えたもので、SystemVerilog言語全体の実装率を表すものではありません。

| 検証の種類 | 答えられる問い | 結論の範囲 |
| --- | --- | --- |
| CTest・ローカルCI | unit・ソース・CLI・Context・カウンタ・ツールのテスト、ビルド、静的な規則に通るか？ | 言語全体の網羅や外部比較の条件の達成は示さない |
| `valsem`・参照スケジューラ | 対象の値・処理列で、高速実装が独立したモデルと一致するか？ | 固定シードのローカル実行は、必要な連続nightlyの代わりにはならない |
| sv-testsのシミュレーション比較 | 固定した手書きのシミュレーション集合で、Linux・macOSそれぞれ何件通るか？ | 対象の固定母集団と記録したツールの版に適用 |
| 固定P1母集団 | 承認済みの固定一覧の90%以上が独自スイートの判定を通るか？ | 未対応も分母に残し、より広い対応範囲の要件も維持 |
| 機能プローブ | 有限の機構ごとの入力が、記載した期待値と一致するか？ | 差分がなくても全面的な同等性は示さない |
| ミューテーションテスト | 意図した誤りをコンパイルして、テストで検出できるか？ | 適用位置の検査だけでは検出を確かめられない |
| PerfCount | 処理回数が回帰していないか？ | 経過時間・メモリには別の[性能測定](BENCHMARKS.ja.md)が必要 |

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
ヘッダをそのまま使います。slangの固定版がこの変更を含むd02b7cdaに進んでからは、
既定の`pinned`の構成でも、このテスト1件だけは同じ上流の診断で判定します
（`tests/suite/CMakeLists.txt`）。ヘッダは以前の診断のままで、登録済みの期待として残しています。
既定のプロファイルのほかの期待値は変えていません。
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

## ビルドをまたいでlibFuzzerの入力を保持する

`fuzzSetting()`の失敗時表示では、`mixed_seed`と元の`KIRIMA_FUZZ_SEED`の文字列を区別し、
解釈後のscaleと、実行中のユニットテストバイナリ・正確なフィルターを含むシェル用の再現コマンドを
出します。未設定は未設定として再現し、0や従来の寛容な解析を使う文字列も元の意味を保ちます。
seedの混合式、固定CIの乱数列、日付seed・100倍のNightly、P1の7連続Nightly条件は変更していません。
バイナリのハッシュとチェックアウトの版も一緒に残してください。

Nightlyは既存の`kirimasim_fuzz_compile`用の**入力ファイル**を`build/fuzz-new`へ、版をまたいで復元します。
過去の判定は使いません。新しくビルドしたバイナリで各ファイルを1回ずつ再評価してから、従来の30分の
変異探索を行います。再評価も変異探索と同様に報告のみで、固定スイートのCTestは、OSの引数長上限を避けるため64件ずつ全入力を実行します。
各バッチの正確なargvは`fuzz-corpus-replay.log`へ、前後のスナップショットには全入力の指紋・現在の
バイナリ指紋・版・ソース変更有無を残します。`fuzz-corpus`と別の`fuzz-findings` artifactで、
後のビルドがpassしても元の観測を保持します。キャッシュは追い出されることがあるため、調査に必要な
artifactは別途ダウンロードして保存してください。

Linuxでダウンロードしたcorpusを再評価する場合は、現在のfuzz presetをビルドした後に実行します。

```sh
mkdir -p build/fuzz-findings
python3 tools/fuzz_corpus_snapshot.py \
  --binary build/fuzz/tests/fuzz/kirimasim_fuzz_compile \
  --corpus build/fuzz-new --output build/corpus-replay-before.json -- \
  find build/fuzz-new -type f -print0
find build/fuzz-new -type f -print0 | \
  xargs -0 -r -n 128 -t build/fuzz/tests/fuzz/kirimasim_fuzz_compile \
    -timeout=30 -rss_limit_mb=4096 -artifact_prefix=build/fuzz-findings/ \
    > build/corpus-replay.log 2>&1
```

`-r`はGNU xargsの指定です。macOSでは、対象ファイルを明示してバイナリへ渡し、空リストを避けます。
ディレクトリを渡すと変異探索、ファイルを渡すと各1回の再実行になります。コード変更後は現在のバイナリで
再評価し、過去の成功を流用しません。元のクラッシュ入力とログを残し、規格・フロントエンド／参照・runner・
内部実装を切り分けて、確認した機構のunit testや既存matrixへ追加します。合格のために期待値を緩めません。

外部SystemVerilog入力の探索と同じ刺激での他シミュレータ確認は、
[kirimasim-tests](https://github.com/kurochan001/kirimasim-tests)の既存ランダムrunnerとmanifestで行います。
ランダム証拠は固定P1母集団や指紋付きクロスのカバレッジと分けます。本体では内部のvalsem・scheduler比較と
libFuzzerを維持し、これらのゲートに外部探索用の重複省略を適用しません。

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

生の`probes`・`gaps`・`reference_mismatches`に加え、`p1_scope`・`p1_out_of_scope`・
`p1_required_gaps`・`independent_failures`を確認します。`gaps`には参照が通る生の差をすべて保ちます。
P1のgateは必須範囲の差とkirimasimの独立した不具合を使い、`--require-parity`はどちらでも1を返します。
指定なしでは差があっても0になり得ます。`reference_failures`は参照のクラッシュ・上限を別記し、
kirimasimの不具合や同等性の証拠とは扱いません。

`wire real`例外は確認済みの名前とソースhashが両方一致した場合だけ適用します。
総数・評価対象・対象外の件数と、対象外の名前・理由を分けて示します。
未対応は失敗・未サポートのままで、PASSへ変えません。任意ケースの終了コード2は、別の`--dump-sir`実行で
標準出力なしの位置付きコンパイルエラーと確認できたときだけ許容します。実行時エラー・クラッシュ・
タイムアウト・出力上限・内部エラーは失敗です。標準のreal変数・演算・合法な単一連続代入・
`nettype real`は既存の範囲を保ちます。

`--extended`も有限の機構の追加で、全組み合わせではありません。gateはIcarusの全期待値一致や
P1の完了を示しません。[例外の監査](notes/p1-wire-real-exception.ja.md)も参照してください。

## 負の対照と処理回数を確認する

[負の対照](GLOSSARY.ja.md#negative-control)は意図的な誤りをテストが検出できるかを確かめ、[PerfCount](GLOSSARY.ja.md#perfcount)は処理回数の変化を確かめます。

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

`--only`は大小文字を区別した名前の部分一致です。`--shard 1/32`は一覧の一部分だけを選ぶため、
一覧全体の根拠には全区分の結果が必要です。1,126変異を8分割した全ジョブが6時間の上限に達したため、
Nightlyは32分割（その時点で最大36変異ずつ）にしています。実行の分割だけを変え、一覧や検出規則は維持します。
ホスト上での完走は、変更後の実行で確認する必要があります。
runnerは未変更時の合格を確認し、各ソースを戻します。
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
ありません。日付付きの[P1監査](history/P1_CRITERIA_GAPS_2026-10-28.ja.md)と生データは元の範囲で読み、
現在の完了判定には必要な新しい根拠を揃えてください。READMEの生成件数は、
README 件数と[日時付きの検証記録](VALIDATION_COUNTS.ja.md)は、CONTRIBUTINGと[readme_counts.py](../tools/readme_counts.py)に従い、対象を絞らない完全な
測定記録をレビューしてから扱います。
