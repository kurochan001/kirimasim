# 性能の測定記録

[English](BENCHMARKS.md) · 英語版が主で、日本語版はそれに従います。

2026-10-23 に、第 1 層の `dec94ff21979` と比較用シミュレータで測り直した記録です。
[README の要約](../README.ja.md)に対応します。性能の合否基準を変更する文書ではありません。

[P1 の使用例](P1_FEATURES.ja.md)にある実行制御オプション、コンテナの統合、
繰り返しイベント制御、集約値の保持・時間制御の変更は、この測定の後の追加です。
これらを含む版の性能は、この記録ではまだ測り直していません。
最新の[機能検証](notes/p1-icarus-progress.ja.md)と
[Icarus 13.0との機能比較](notes/icarus-13-comparison.ja.md)は新しい実装を使いますが、
以下の時間・メモリ測定値を更新するものではありません。

## 測定条件

- kirimasim 0.0.0、コミット `dec94ff21979`。release ビルド、`-O3 -DNDEBUG`、
  `-ffp-contract=off`、AppleClang 21.0.0.21000334。実行オプションは既定値で、
  `--bounds-check` は `off`。未コミットの文書・測定ツールの変更は実行バイナリに影響しません。
- Icarus Verilog 14.0 devel、`s20260301-541-gb25510638`。
- Verilator 5.053 devel、`v5.052-349-g2cb54975a`。
- Apple M5 Max、18 コア、128 GiB、macOS 27.0.1 arm64。ベンチマーク前の
  1・5・15 分の負荷平均は 2.37・2.54・3.26。この測定ではベンチマーク、sv-tests、
  自前スイートを順番に実行し、互いを重ねていません。OS の背景処理は止めていません。
- 使用した比較用ツールはローカルにある上記の版です。各プロジェクトの最新版かは未確認です。
  Linux や別の CPU での結果はこの測定には含みません。

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

- [bench.json](measurements/2026-10-23/bench.json)：各 5 回の経過時間、中央値、RSS、
  版、負荷、実行条件、設計・スクリプト・kirimasim バイナリの SHA-256。
- [svtests.json](measurements/2026-10-23/svtests.json)：各テストの判定と時間。
- [population.json](measurements/2026-10-23/population.json)：各テストの判定と時間、
  使用したランナー 4 ファイルの SHA-256。

以下の変数には、上記の版の実行ファイル・チェックアウトのパスを設定します。
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
