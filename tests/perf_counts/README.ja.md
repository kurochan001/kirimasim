# PerfCount：事象の回数の回帰テスト

[English](README.md) · 英語版が正本で、これはその日本語版です。食い違ったときは英語版に従います。

`expected.json` は、挙げたテストのファイルごとに、kirimasim が `--stats=json` で出すカウンタを記録しています。`:type:` に `simulation` を含むテストは終わりまで実行し（`kirimasim --stats=json <ファイル>`）、ほかのテストは構文解析だけをします（`kirimasim --stats=json parse <ファイル>`）。`sim.end_reason` のような文字列のカウンタは、完全に一致しなければなりません（PERFORMANCE.md の 4 節）。CTest のラベル `PerfCount`（`ctest --preset dev -L PerfCount`）は `tools/perfcount_check.py` を走らせます。これは、カウンタが期待値より **10% 以上多い**とき、期待値にあるカウンタが無いとき、`expected.json` に無いカウンタを実行が出したときに失敗します。

実時間の代わりに回数を使うのは、回数が機械の負荷や最適化の度合いに左右されず、忙しいノート PC でも CI でも安定しているためです。

## 期待値の更新

1. ビルドしてテストを走らせ、新しい回数を見ます。

   ```sh
   cmake --build --preset dev
   build/dev/src/driver/kirimasim --stats=json tests/suite/<パス>.sv        # シミュレーション
   build/dev/src/driver/kirimasim --stats=json parse tests/suite/<パス>.sv  # 構文解析だけ
   ```

2. `expected.json` を手で直します。鍵はリポジトリの根からの相対のパスで、値はカウンタの名前から回数への対応です。
3. 回数が**なぜ**変わったかを書いたメッセージでコミットします（たとえば、どの変更がカウンタを上げ、それをなぜ受け入れられるか。あるいは、新しいテストかカウンタを登録すること）。

決まり（CONTRIBUTING.md を見てください）：

- `expected.json` に新しいテストか新しいカウンタを足すのは、実装の変更と同じプルリクエストでかまいません。
- **既存の**項目を変える・消す変更（予算を上げることを含む）は、別のプルリクエストにしなければなりません。CI で `tools/check_expected_change.py` がこれを検査します。
- 回数が下がったら、別のプルリクエストで期待値を下げ、改善を固定します。
