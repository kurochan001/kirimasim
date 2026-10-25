# 性能の測定記録

[English](BENCHMARKS.md) · 英語版が主で、日本語版はそれに従います。

第 1 層について、2026-10-23 の macOS（`dec94ff21979`）と、Lyra を追加した
[2026-10-24 の Linux（WSL2）](#linuxwsl2での-4-つのシミュレータ2026-10-24)
（`31160083c77d`）という、別々の測定を記録しています。
[README の要約](../README.ja.md)に対応します。性能の合否基準を変更する文書ではありません。

[P1 の使用例](P1_FEATURES.ja.md)にある実行制御オプション、コンテナの統合、
繰り返しイベント制御、集約値の保持・時間制御、広いゲートマスクの変更は、macOS の測定の後の追加です。
Linux の測定には後の版を使っていますが、以下の macOS と短いテスト群の時間は更新していません。
最新の[機能検証](notes/p1-icarus-progress.ja.md)と
[Icarus 13.0との機能比較](notes/icarus-13-comparison.ja.md)は新しい実装を使いますが、
それ自体が時間・メモリの測定ではありません。

## macOS の測定条件（2026-10-23）

- kirimasim 0.0.0、コミット `dec94ff21979`。release ビルド、`-O3 -DNDEBUG`、
  `-ffp-contract=off`、AppleClang 21.0.0.21000334。実行オプションは既定値で、
  `--bounds-check` は `off`。未コミットの文書・測定ツールの変更は実行バイナリに影響しません。
- Icarus Verilog 14.0 devel、`s20260301-541-gb25510638`。
- Verilator 5.053 devel、`v5.052-349-g2cb54975a`。
- Apple M5 Max、18 コア、128 GiB、macOS 27.0.1 arm64。ベンチマーク前の
  1・5・15 分の負荷平均は 2.37・2.54・3.26。この測定ではベンチマーク、sv-tests、
  自前スイートを順番に実行し、互いを重ねていません。OS の背景処理は止めていません。
- 使用した比較用ツールはローカルにある上記の版です。各プロジェクトの最新版かは未確認です。
  [Linux での 4 つのシミュレータ](#linuxwsl2での-4-つのシミュレータ2026-10-24)の節より前の表は、
  この macOS の測定だけです。その節は別の機械、別の版での別の測定で、数値を混ぜません。

## シミュレーション時間

[tools/bench.py](../tools/bench.py)を使い、各設計・各シミュレータで 1 回のウォームアップ後、
`+n=1` と下記の回数を交互に各 5 回実行しました。指定回数の中央値から `+n=1` の
中央値を引いた秒数です。期待値を変えず、測定した大きい回数の全 60 実行で出力と
終了コードを検査しました。すべて一致しました。

| 設計 | `+n=` | kirimasim | Icarus Verilog | Verilator |
| --- | --- | --- | --- | --- |
| `comb_chain` | 3,000,000 | 5.043 秒 | 7.968 秒 | 0.142 秒 |
| `nba_shift` | 1,000,000 | 11.273 秒 | 18.739 秒 | 0.120 秒 |
| `proc_wait` | 1,000,000 | 3.966 秒 | 5.204 秒 | 1.817 秒 |
| `fork_join` | 4,000,000 | 2.763 秒 | 4.084 秒 | 0.843 秒 |

この 4 設計では、kirimasim は Icarus より約 1.31〜1.66 倍速く、Verilator は
kirimasim より約 2.2〜93.9 倍速い結果でした。4 値インタプリタと 2 値の C++ 生成では
実行方式が異なります。広い実設計での速さや、前の版からの改善率はこの測定だけでは
確認できません。Issue #28 の解決を示すものではありません。

### 起動を含む短い実行（ms）

`+n=1` の 5 回の中央値です。kirimasim の読み込み・エラボレーションも含みます。

| 設計 | kirimasim | Icarus Verilog | Verilator |
| --- | --- | --- | --- |
| `comb_chain` | 8.067 | 6.426 | 3.164 |
| `nba_shift` | 7.714 | 6.266 | 3.107 |
| `proc_wait` | 8.353 | 7.272 | 3.445 |
| `fork_join` | 7.402 | 6.187 | 3.168 |

### 設計のビルド時間（秒、各 1 回）

Icarus は `iverilog -g2012`、Verilator は `--binary --timing` と既定の C++ 設定、
`--build-jobs 0` です。警告オプションを含む呼び出しは生データとスクリプトにあります。
kirimasim は設計の別ビルドを行わず、準備は実行時間に含まれます。
kirimasim 本体のクリーンビルド時間は今回測定していません。

| 設計 | kirimasim | Icarus Verilog | Verilator |
| --- | --- | --- | --- |
| `comb_chain` | — | 0.023 | 1.581 |
| `nba_shift` | — | 0.014 | 1.528 |
| `proc_wait` | — | 0.015 | 1.641 |
| `fork_join` | — | 0.014 | 1.542 |

### 最大 RSS（MiB）

指定回数の各プロセスの最大 RSS を取得し、5 回の中央値を取った値です。
全試行での最大値ではありません。

| 設計 | kirimasim | Icarus Verilog | Verilator |
| --- | --- | --- | --- |
| `comb_chain` | 9.19 | 5.34 | 2.00 |
| `nba_shift` | 9.19 | 5.50 | 2.00 |
| `proc_wait` | 10.09 | 5.86 | 2.16 |
| `fork_join` | 9.19 | 5.33 | 2.06 |

## Linux（WSL2）での 4 つのシミュレータ（2026-10-24）

2026-10-24 の 13:49〜14:22 UTC に、同じ 4 つの設計・回数・期待する結果で、Lyra に
対応させた [tools/bench.py](../tools/bench.py) で測りました。上の macOS の測定とは別の測定で、
機械も、Icarus Verilog と Verilator の版も違います。

- kirimasim 0.0.0、コミット `31160083c77d`、release ビルド（`-O3 -DNDEBUG`）、clang 20.1.8。
- Icarus Verilog 13.0 stable、`v13_0-25-g07dd50d37`。
- Verilator 5.050、`v5.050-60-g3d2421f3b`。生成した C++ は `ccache g++ -Os`（g++ 15.2.0）で
  コンパイルし、mold でリンクしています。
- Lyra は `a2313fdc8707` を `bazel build -c opt` でビルドしました。既定の backend は C++ backend です
  （`--backend` なしで作ったプログラムは、`--backend cpp` で作ったものと同じファイルになります）。
  Lyra の README が主としている LLVM backend は、別の列にしました。どちらも Lyra の既定の `-O 0` で、
  設計ごとに空のキャッシュへ 1 回ビルドしました。設計は clang 20.1.8 でコンパイルされます。
- AMD Ryzen 9 7950X（WSL2 から 32 論理 CPU）、110 GiB、Ubuntu 24.04.4 LTS on WSL2、
  kernel 6.18.33.1、glibc 2.39。測定前の負荷平均は 0.17・0.53・1.68、測定中は 0.17〜1.24 で、
  ベンチマークは順番に実行しました。この負荷の観測だけで、背景処理がなかったとは断定できません。

秒数（本来の回数の中央値から `+n=1` の中央値を引いたもの）。測った 5 列の本来の回数の実行
100 回はすべて終了コード 0 で期待する行を出し、`+n=1` の実行はすべて `n=1` を出しました。

| 設計 | `+n=` | kirimasim | Icarus Verilog | Verilator | Lyra（C++ backend、既定） | Lyra（LLVM backend） |
| --- | --- | --- | --- | --- | --- | --- |
| `comb_chain` | 3,000,000 | 8.167 s | 13.266 s | 0.172 s | 30.139 s | 2.749 s |
| `nba_shift` | 1,000,000 | 20.190 s | 24.315 s | 0.151 s | 143.256 s | 6.059 s |
| `proc_wait` | 1,000,000 | 6.135 s | 5.399 s | 2.182 s | 70.412 s | 5.933 s |
| `fork_join` | 4,000,000 | 4.226 s | 6.129 s | 0.998 s | 15.240 s | 4.689 s |

- ここでは kirimasim は、Icarus Verilog 13.0 より 3 つの設計で約 1.20〜1.62 倍速く、`proc_wait` では
  約 1.14 倍遅くなりました（より新しい Icarus と比べた macOS の測定とは違います）。Verilator は
  kirimasim より約 2.8〜134 倍速く、Lyra の既定の backend は kirimasim より約 3.6〜11.5 倍遅く
  なりました。Lyra の LLVM backend は、`comb_chain` と `nba_shift` で約 3.0〜3.3 倍速く、
  `fork_join` で約 1.11 倍遅く、`proc_wait` では実行のばらつきの範囲内でした。
- `+n=1` の実行は、どのツールも 1〜6 ms でした。別に計ったビルドの時間は、Icarus が
  0.005〜0.024 s、Verilator が温まった ccache で 0.33〜0.34 s、ccache を切って各 1 回で 2.53〜2.63 s、
  Lyra が空のキャッシュで 5.3〜6.4 s（既定）と 0.42〜0.47 s（LLVM backend）です。
- サンプリングした RSS の高水位（本来の回数の 5 回の中央値、MiB）は、kirimasim 21.5〜25.3、Icarus 7.7〜8.2、
  Verilator 3.7〜3.9、Lyra 4.5〜4.8（既定）と 5.3〜5.8（LLVM）です。Linux では、tools/bench.py は
  各プログラムの `VmHWM` を 10 ms 間隔で読みます。終了直前のピークは最後のサンプルより後に
  生じることがあるため、観測できた高水位であり、最終的な最大 RSS を保証する値ではありません。
  子の `ru_maxrss` には起動元の Python の分も入るので、夜間のジョブを含む従来の Linux の
  測定方法では、この実行環境で約 19 MiB を下回りません。この測定方法の変更により、
  従来の Linux の RSS 履歴とは直接比較できません。macOS は引き続き `ru_maxrss` を使います。
- 生データ：[bench.json](measurements/2026-10-24-wsl2/bench.json)。

### Linux の比較を再実行する方法

上記の版とビルド設定の実行ファイルを使います。`KIRIMASIM`、`IVERILOG`、`VERILATOR`、
`LYRA` にそれぞれの実行ファイルのパスを設定してください。`vvp` は `iverilog` と
同じディレクトリに必要です。`LYRA_COMMIT` には Lyra をビルドしたコミットの全桁を指定します
（今回の記録は `a2313fdc870702acf8e382adae0883036f54e3c3`）。ほかの計測と重ねずに実行します。

```sh
python3 tools/bench.py --kirimasim "$KIRIMASIM" \
  --iverilog "$IVERILOG" --verilator "$VERILATOR" \
  --lyra "$LYRA" --lyra-llvm "$LYRA" --lyra-version "$LYRA_COMMIT" \
  --runs 5 --out build/bench-four-simulators
```

`--lyra` は実行ファイルの既定の backend、`--lyra-llvm` は明示的に LLVM を選びます。
今回の Lyra には `--version` がないので、`--lyra-version` で指定したコミットを記録します。
Lyra 本体を `-c opt` でビルドしても、このコマンドで使う設計の既定値 `-O 0` は変わりません。

設計が失敗しても `results.json` と `summary.md` を出力し、1 件でも失敗すれば終了コードは 1 です。
非対応の設計を 0 秒と扱わず、
設計ごとの `stage` と `diagnostics` を確認してください。成功した記録には、ウォームアップ、
小さい回数と本来の回数の各 5 回、出力行、終了コード、タイムアウトの有無を残します。
Linux の `max_rss_mib` はサンプリングした `VmHWM` で、観測できなければ 0 ではなく `null` です。
`bench_report.py` は同じキーを読み、過去の RSS の取得方法を自動で区別しません。
測定方法の変更をまたぐ夜間の RSS 比較は、その違いを踏まえて解釈してください。

公開した JSON は元の試行を保持しています。再実行すれば新しい測定になります。
この記録の再現には固定したシミュレータの版を使い、新しい版で実行した場合はその版の測定として扱います。
公開用の比較には `--n` を使いません。作業量の回数を上書きし、期待するチェックサム全体の検査を省くためです。

## 短いテスト群の時間

[sv-tests のランナー](../tools/svtests.py)と
[自前母集団の計測ツール](../tools/measure_population_times.py)で、各テスト 1 回、
最大 6 本を同時に実行しました。合計は各テストの時間の和で、全体の経過時間では
ありません。起動や準備の影響が大きく、上のシミュレーション速度とは別の値です。

- sv-tests：固定コミット `15490e95fd978ada1c1dc59b7c4b53a7edb23b78` の 344 本。
  今回の判定では kirimasim が 213 本、Icarus が 210 本通過しました。
- 自前の P1 母集団：kirimasim-tests `179c55de9c83` の凍結リスト 1,466 本。
  ランナーは `bc1609dc0055` のものをコピーして固定しました。kirimasim が 1,451 本、
  Icarus が 1,274 本通過しました。合否は既存ランナーで判定し、期待値は変更していません。
- kirimasim の準備は `--dump-sir`、実行は通しの時間との差（負の差は 0）。
  Icarus の準備はコンパイル、実行は生成したプログラムです。自前のランナーは `vvp` に
  渡して起動します。時間には正規化や Python の判定処理を含めていません。
- kirimasim の差分には揺れと SIR の出力コストの影響があります。Verilator の
  テストごとの再ビルド時間は測っていません。合格条件の過去の比較表も更新対象にしていません。

| テストの集まり | シミュレータ | 準備 | 実行 | 合計 |
| --- | --- | --- | --- | --- |
| sv-tests, 344 本 | kirimasim | 7.06 秒 | 0.18 秒 | 7.24 秒 |
|  | Icarus Verilog | 4.79 秒 | 33.52 秒 | 38.31 秒 |
| 自前の P1 母集団, 1,466 本 | kirimasim | 11.65 秒 | 1.15 秒 | 12.80 秒 |
|  | Icarus Verilog | 21.54 秒 | 9.15 秒 | 30.70 秒 |

今回の **2 者がともに通したもの**に限ると、次のとおりです。前の 3 者共通の表とは
対象が異なります。sv-tests の 184 本には拒否を期待する 43 本も含みます。
丸め前に合計を計算するため、表示した準備と実行の和が合計と 0.01 秒ずれる場合があります。

| テストの集まり | シミュレータ | 準備 | 実行 | 合計 |
| --- | --- | --- | --- | --- |
| sv-tests, 184 本 | kirimasim | 1.70 秒 | 0.09 秒 | 1.79 秒 |
|  | Icarus Verilog | 2.51 秒 | 27.22 秒 | 29.73 秒 |
| 自前の P1 母集団, 1,269 本 | kirimasim | 10.06 秒 | 1.00 秒 | 11.06 秒 |
|  | Icarus Verilog | 18.75 秒 | 9.04 秒 | 27.79 秒 |

## 生データと再現

- [Linux の bench.json](measurements/2026-10-24-wsl2/bench.json)：上の列の成功した全実行と
  ビルドのコマンド。再実行方法は上の Linux の節にあります。
- [bench.json](measurements/2026-10-23/bench.json)：各 5 回の経過時間、中央値、RSS、
  版、負荷、実行条件、設計・スクリプト・kirimasim バイナリの SHA-256。
- [svtests.json](measurements/2026-10-23/svtests.json)：各テストの判定と時間。
- [population.json](measurements/2026-10-23/population.json)：各テストの判定と時間、
  使用したランナー 4 ファイルの SHA-256。

以下は、以前の macOS と短いテスト群の測定を再実行するコマンドです。
変数には、それぞれの記録の版の実行ファイル・チェックアウトのパスを設定します。
`TESTS_REPO` は `179c55de9c83` を保持する kirimasim-tests、`RUNNERS_REPO` は
`bc1609dc0055` の kirimasim-tests です。`SV_TESTS` は固定した sv-tests の
UVM サブモジュールを含むチェックアウトです。計測コマンドは重ねずに実行してください。

```sh
cmake --preset release
cmake --build --preset release
python3 tools/check_release_flags.py
python3 tools/bench.py --kirimasim build/release/src/driver/kirimasim \
  --iverilog "$IVERILOG" --verilator "$VERILATOR" --runs 5 --out build/bench
python3 tools/svtests.py --sv-tests "$SV_TESTS" \
  --kirimasim build/release/src/driver/kirimasim --iverilog "$IVERILOG" \
  --jobs 6 --out build/suite-times/svtests
mkdir -p build/population
git -C "$TESTS_REPO" archive 179c55de9c83 > build/population.tar
tar -xf build/population.tar -C build/population
python3 tools/measure_population_times.py --tests-repo build/population \
  --runner-repo "$RUNNERS_REPO" --kirimasim build/release/src/driver/kirimasim \
  --iverilog "$IVERILOG" --jobs 6 --out build/suite-times/population.json
```
