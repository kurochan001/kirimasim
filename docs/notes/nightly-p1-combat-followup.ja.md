# Nightly の確認記録（2026-11-28）

[English](nightly-p1-combat-followup.md) · [Nightly 45924320526](https://github.com/kurochan001/kirimasim/actions/runs/45924320526) の対象は b1ad0cb9。再現には a07f2d66 を使用し、その後の main の 2 コミットは仕様・計測文書を更新した。

- **p1-gates。** Ubuntu 24.04 arm64、GCC 13.3、固定 slang e81bcd25、sv-tests 15490e95fd97、固定リスト 41085fe3432b。Linux の sv-tests は記録値 253 件に対し実測 256/344 件。P1 固定リストは 1,466/1,466 件。Linux の Icarus 211 件は以前の記録。測定日は日本時間 11 月 28 日、UTC 11 月 27 日。所定の更新ツールで Linux の kirimasim 件数・日付と、そこから生成する日英の記録を同期した。[再照合用の全件データ](../measurements/2026-11-28/linux-svtests-count-replay.json)
- **mutants 19/32。** 純粋なシステム関数の引数内の副作用を探索しなくなる改変が CTest 14,038 件を生き残った。追加した回帰テストは `$clog2(n++)` を `&&` と `||` の右辺に置き、初期値 4 で論理結果と n の両方を確かめる。AND 短絡・評価、OR 短絡・評価の順に通常版は 4、5、4、5。改変版は 5、5、5、5。専用の生成テストと期待値ファイルでこの差を確かめる。[回帰テスト](../../tests/suite/values/short_circuit_pure_operand.sv)・[期待出力](../../tests/suite/values/short_circuit_pure_operand.expected)
- **slang-main は次回の pin 更新時に確認。** 固定版 e81bcd25 では cli/relaxed_warnings が合格。Nightly 時点の c441ab91 は 1800-2023 の扱いの変更で乱数変数を含まない文字列部分式の診断が出ず、string-in-constraint の 2 検査が失敗。c445fa43 では randsequence API が変わり、RandSeqProductionSymbol::Rule と getRules を使うフロントエンドのビルドで停止した。pin 更新時に API 対応と診断期待値を照合する。[上流の制約変更](https://github.com/MikePopoloski/slang/commit/7bef384e75e8b3aa9d61399948ff3e4dd7ce41af)・[上流の randsequence 変更](https://github.com/MikePopoloski/slang/commit/592a0c724397032929e400b9081e5f35c97f9fea)

```sh
python3 tools/readme_counts.py update --os linux --date 2026-11-27 \
  --svtests docs/measurements/2026-11-28/linux-svtests-count-replay.json
python3 tools/readme_counts.py check --os linux \
  --svtests docs/measurements/2026-11-28/linux-svtests-count-replay.json
python3 tools/gen_short_circuit_matrix.py --check
ctest --preset dev -R short_circuit_pure_operand --output-on-failure
```
