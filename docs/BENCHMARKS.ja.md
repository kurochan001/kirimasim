# 性能の測定記録

[English](BENCHMARKS.md) · 英語版が主で、日本語版はそれに従います。

第 1 層について、2026-10-23 の macOS（`dec94ff21979`）と、Icarus Verilog・Verilator・Lyra を
開発中の最新の版にして 11 個の設計で測った [2026-10-25 の Linux（WSL2）](#linuxwsl2で開発版の最新2026-10-25)
（`b66834254c65`）という、別々の測定を記録しています。
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
  [Linux で開発版の最新](#linuxwsl2で開発版の最新2026-10-25)の節より前の表は、
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

## Linux（WSL2）で開発版の最新（2026-10-25）

2026-10-25 の 10:55〜11:42 UTC に、上の 4 つの設計と、別の仕組みを測るために書いた 7 つの設計の、
合わせて 11 個を、同じ方法で、各ツールをその日の開発中の最新のコミットにして、
[tools/bench.py](../tools/bench.py) で測りました。上の macOS の測定とは別の測定で、機械も、ツールの版も違います。

- kirimasim 0.0.0、コミット `b66834254c65`（main）、release ビルド（`-O3 -DNDEBUG`）、clang 20.1.8。
- Icarus Verilog 14.0 devel、`2b8d42dc088f`（master）。`tools/build_icarus.sh` と同じ手順でビルドしました。
- Verilator 5.053 devel、`1838b6ed035c`（master）。configure の既定でビルドしました。生成した C++ は
  `ccache g++ -Os`（g++ 15.2.0）でコンパイルし、mold でリンクしています。
- Lyra は `63922502ff0a`（main）を `bazel build -c opt` でビルドしました。既定の backend は C++ backend です
  （`--backend` なしで作ったプログラムは、`--backend cpp` で作ったものと同じファイルになります）。
  Lyra の README が主としている LLVM backend は、別の列にしました。どちらも Lyra の既定の `-O 0` で、
  設計ごとに空のキャッシュへ 1 回ビルドしました。設計は clang 20.1.8 でコンパイルされます。
- AMD Ryzen 9 7950X（WSL2 から 32 論理 CPU）、110 GiB、Ubuntu 24.04.4 LTS on WSL2、kernel 6.18.33.1、
  glibc 2.39。1 分の負荷平均が 1.2 を下回り、ほかのビルドやテストが動いていない状態が続いてから始め、
  測定中は 0.22〜1.15 でした。ベンチマークは順番に実行しましたが、この負荷の観測だけで背景処理が
  なかったことは確かめられません。

新しい 7 つの設計は [bench/](../bench/) にあり、それぞれ 1 行のチェックサムを出します。足したときに、
その回数で kirimasim、Icarus Verilog、Verilator が同じ行を出しました。X や Z に左右されないので、
2 値の Verilator とも一致します。

| 設計 | 内容 | `+n=` |
| --- | --- | --- |
| `wide_arith` | 256 ビットの加算、128 ビットの積、シフト、比較を 1 つのループで | 1,500,000 |
| `mem_rw` | 1024 ワードのメモリーを、クロックの立ち上がりごとに番地を変えて読み書き | 4,000,000 |
| `func_call` | `inout` と `output` の引数を持つタスクが、4 段再帰する関数を呼ぶ | 1,500,000 |
| `string_fmt` | `$sformatf`、`substr`、連結と、全文字のハッシュ | 500,000 |
| `dyn_queue` | 動的配列のまわりで、キューに入れて取り出す | 4,000,000 |
| `gate_udp` | 32 個の xor ゲートと、組み合わせ回路の UDP 32 個 | 400,000 |
| `casez_struct` | `casez` で packed 構造体へ振り分け、レジスターファイルを更新 | 5,000,000 |

秒数（本来の回数の中央値から `+n=1` の中央値を引いたもの）。ビルドできた本来の回数の実行 265 回は
すべて終了コード 0 で期待する行を出し、`+n=1` の実行はすべて `n=1` を出しました。Lyra はゲートと UDP に
まだ対応していないので、`gate_udp` のプログラムを作れませんでした。

| 設計 | `+n=` | kirimasim | Icarus Verilog | Verilator | Lyra（C++ backend、既定） | Lyra（LLVM backend） |
| --- | --- | --- | --- | --- | --- | --- |
| `comb_chain` | 3,000,000 | 9.249 s | 9.796 s | 0.223 s | 31.915 s | 2.878 s |
| `nba_shift` | 1,000,000 | 25.486 s | 26.585 s | 0.196 s | 140.711 s | 5.900 s |
| `proc_wait` | 1,000,000 | 6.786 s | 5.688 s | 2.283 s | 73.445 s | 6.164 s |
| `fork_join` | 4,000,000 | 4.837 s | 5.023 s | 1.190 s | 15.826 s | 4.860 s |
| `wide_arith` | 1,500,000 | 4.944 s | 2.036 s | 0.086 s | 6.236 s | 0.380 s |
| `mem_rw` | 4,000,000 | 4.937 s | 3.645 s | 0.696 s | 23.079 s | 2.705 s |
| `func_call` | 1,500,000 | 4.917 s | 7.511 s | 0.005 s | 7.887 s | 0.395 s |
| `string_fmt` | 500,000 | 5.003 s | 8.080 s | 0.135 s | 18.770 s | 0.279 s |
| `dyn_queue` | 4,000,000 | 4.112 s | 6.686 s | 0.083 s | 16.275 s | 0.412 s |
| `gate_udp` | 400,000 | 5.378 s | 3.517 s | 0.046 s | 未対応 | 未対応 |
| `casez_struct` | 5,000,000 | 3.976 s | 4.117 s | 0.018 s | 22.159 s | 0.466 s |

- 本来の回数の 5 回の幅は中央値の 7.8% 以下でしたが、`comb_chain` の Verilator は 14.7% でした。
- kirimasim は Icarus Verilog より、`proc_wait`（1.19 倍）、`wide_arith`（2.4 倍）、`mem_rw`（1.35 倍）、
  `gate_udp`（1.53 倍）で遅く、`func_call`（1.53 倍）、`string_fmt`（1.61 倍）、`dyn_queue`（1.63 倍）で速く、
  残りの 4 つは差が 6% ほどの範囲でした。Verilator はすべての設計で kirimasim より速く、Lyra の既定の
  backend はビルドできたすべての設計で kirimasim より遅く、LLVM backend は `fork_join`（差は 1% 以内）を除いて
  速くなりました。
- kirimasim は `b66834254c65` で、同じ日の朝の `8a8607690aa6` より遅くなりました（`comb_chain` で 9.249 s と
  8.288 s）。Icarus Verilog と Verilator の時間は変わっていません。2 つの版を交互に 3 回ずつ実行すると、
  新しい版は `comb_chain` と `proc_wait` で 1.08 倍、`fork_join` で 1.14 倍の時間でした。`fork_join` での
  bisect では、変化は `b17234e420b6` にあります（親で 4.26 s、このコミットで 4.96 s）。
- `dyn_queue` では、kirimasim の最大メモリーが回数とともに増えます。GNU time で 100,000 回で 44 MiB、
  1,000,000 回で 164 MiB、4,000,000 回で 541 MiB と、1 回あたり約 130 バイトで、キューの要素は 33 個を
  超えません。理由は確かめていません。
- `+n=1` の実行は、どのツールも 1〜6 ms でした。別に計ったビルドの時間は、Icarus が 0.005〜0.024 s、
  Verilator が ccache ありで 0.34〜1.06 s、Lyra が空のキャッシュで 5.4〜6.3 s（既定）と 0.44〜0.50 s
  （LLVM backend）です。
- サンプリングした RSS の最大値（本来の回数の 5 回の中央値、MiB）は、kirimasim 21.6〜25.3 と `dyn_queue` の
  547.5、Icarus 7.7〜8.2、Verilator 1.4〜3.9、Lyra 4.3〜4.8（既定）と 5.2〜5.7（LLVM）です。Linux では、
  tools/bench.py は各プログラムの `VmHWM` を 10 ms ごとに読みます。終了の直前の最大は最後の読み取りより後に
  なることがあるので、これは観測した最大値で、最終的な最大 RSS を保証するものではありません。`func_call` の
  Verilator の 1.4 MiB は、数 ms で終わる実行のそうした値です。子の `ru_maxrss` には起動元の Python の分も
  入るので、サンプリングより前の夜間のジョブを含む Linux の値は、約 19 MiB を下回ることがありません。
  サンプリングの前後で、Linux の RSS の履歴は直接には比べられません。macOS は引き続き `ru_maxrss` を使います。
- 同じ日の朝に、最初の 4 つの設計を kirimasim `8a8607690aa6` で測った記録は、生データとして
  [measurements/2026-10-25-wsl2/bench.json](measurements/2026-10-25-wsl2/bench.json) に残しています。
- 生データ：[bench.json](measurements/2026-10-25-wsl2-designs/bench.json)。

### Linux の比較を再実行する方法

上記の版とビルド設定の実行ファイルを使います。`KIRIMASIM`、`IVERILOG`、`VERILATOR`、`LYRA` に
それぞれの実行ファイルのパスを設定してください。`vvp` は `iverilog` と同じディレクトリに必要です。
`LYRA_COMMIT` には Lyra をビルドしたコミットの全桁を指定します
（今回の記録は `63922502ff0a0cdc8d162009f73abc11b2401415`）。`VERILATOR_ROOT` が別の Verilator を指している
ときは外します。ほかの計測と重ねずに実行します。

```sh
env -u VERILATOR_ROOT python3 tools/bench.py --kirimasim "$KIRIMASIM" \
  --iverilog "$IVERILOG" --verilator "$VERILATOR" \
  --lyra "$LYRA" --lyra-llvm "$LYRA" --lyra-version "$LYRA_COMMIT" \
  --runs 5 --out build/bench-linux
```

`--lyra` は実行ファイルの既定の backend、`--lyra-llvm` は明示的に LLVM を選びます。
今回の Lyra には `--version` がないので、`--lyra-version` で指定したコミットを記録します。
Lyra 本体を `-c opt` でビルドしても、このコマンドで使う設計の既定値 `-O 0` は変わりません。

設計が失敗しても `results.json` と `summary.md` を出力し、1 件でも失敗すれば終了コードは 1 です。
失敗した設計には `stage` と `diagnostics` が残り、0 秒の実行としては扱いません。成功した記録には、
ウォームアップ、小さい回数と本来の回数の各 5 回、出力行、終了コード、タイムアウトの有無を残します。
Linux の `max_rss_mib` はサンプリングした `VmHWM` で、観測できなければ 0 ではなく `null` です。
`bench_report.py` は同じキーを読み、過去の RSS の取得方法を自動で区別しません。
測定方法の変更をまたぐ夜間の RSS 比較は、その違いを踏まえて解釈してください。

公開した JSON は元の試行を保持しています。再実行すれば新しい測定になります。
この記録の再現には記録したシミュレータの版を使い、新しい版で実行した場合はその版の測定として扱います。
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

- [Linux の bench.json](measurements/2026-10-25-wsl2-designs/bench.json)：2026-10-25 の 11 個の設計と 5 つの
  ツール・backend の列、全実行とビルドのコマンド。再実行方法は上の Linux の節にあります。最初の 4 つの設計で
  測った前の Linux の記録は、[measurements/2026-10-25-wsl2/bench.json](measurements/2026-10-25-wsl2/bench.json)
  （同じ日の朝、kirimasim `8a8607690aa6`）と
  [measurements/2026-10-24-wsl2/bench.json](measurements/2026-10-24-wsl2/bench.json)（古い版）に残しています。
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
