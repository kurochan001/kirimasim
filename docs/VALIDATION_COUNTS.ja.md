# 検証件数の記録

[English](VALIDATION_COUNTS.md)

以下の結果にはそれぞれ測定日と母集団があり、合計を1つの対応率にまとめられない理由は[件数のFAQ](FAQ.ja.md#プローブctest固定母集団の件数が違うのはなぜですか)で説明しています。

[counts.json](counts.json) に保存した最新の記録を、測定日・母集団ごとに示します。
以下の元の 95 プローブは 2026-10-27 の結果です。後の機能範囲は[現行の対応表](P1_SUPPORT.ja.md)と
[188プローブの範囲記録](measurements/2026-11-05/icarus13-star-functions-after.json)を参照してください。
これらのデータは言語全体の完成率を表しません。リポジトリの CTest 件数は、外部比較と独立して変わり得ます。
ゲートの規則・版・再現方法は[検証ガイド](VALIDATION.ja.md)にあります。
2026-11-01 UTC の Linux/macOS の sv-tests 測定は
[Nightly run 41355215502](notes/nightly-repair-2026-11-02.ja.md)によるものです。
両 OS の実際の成功数が 244 から 252 に増えたため、同 run の件数照合が失敗していました。

<!-- counts:begin -->
<!-- tools/readme_counts.py が docs/counts.json から書く。手で直さない。 -->

| 合格条件 | 記録した結果 | 測定時の達成状況 |
| --- | --- | --- |
| [sv-tests](https://github.com/chipsalliance/sv-tests) のシミュレーションテスト（固定コミットの手書きテスト344件）のパス数が、Linux・macOSの両方でIcarus Verilog以上 | Linux：kirimasim 252件、Icarus Verilog 211件。macOS：kirimasim 252件、Icarus Verilog 210件（14.0 devel、04cc493。合格条件で指定されたバージョンを各OSでビルド） | 達成 |
| 独自RTLテスト群（1,466件、対象リスト固定）のパス率90%以上 | 1,456件パス（99.3%）、0件失敗、10件は未対応構文の診断で終了 | 達成 |
| 必須テスト（P4が必要な1件を除く）の全件パス | 全件パス | 達成 |
| 高速な値演算と参照実装（`valsem`）の比較で不一致がないこと：試行回数100倍のナイトリーファジングを7晩連続でパス | 7晩連続でパス（2026-10-27まで） | 達成 |
| Icarus Verilog 13.0 と比べる固定したプローブの一覧（[P1_SCOPE](spec/P1_SCOPE.ja.md)）。承認済みの除外はソースのハッシュで指定 | 過去の95プローブ：P1評価対象94、対象外1件（`real_net`）。参照が通る生の差は6/6件、必須範囲の差は5/5件、参照不一致は4/7件（13.0 stable/14.0 devel）。観測結果は不変で、有限プローブだけでは全機能の網羅を示さない | 未達 |

kirimasim の外部 sv-tests は Linux が 2026-11-01、macOS が 2026-11-01、母集団は 2026-10-27、Icarus のプローブは 2026-10-27 に測ったもので、右の 2 列の Icarus Verilog と Verilator の数は 2026-10-17 に、その日の main からビルドして測ったものです。nightly の実行が sv-tests と母集団を測り直し、これと違えば失敗します。

| テスト群 | テスト数 | kirimasim | Icarus Verilog | Verilator |
| --- | --- | --- | --- | --- |
| このリポジトリのテスト（CTest、dev の構成） | 13,610 | 13,610件パス、0件スキップ | — | — |
| [sv-tests](https://github.com/chipsalliance/sv-tests) のシミュレーションのテスト | 344 | 252 | 213 | 321 |
| 独自テストスイートのP1評価対象テスト群 | 1,466 | 1,456 | 1,274 | 1,329 |

テスト種別ごとの件数：ユニットテスト（GoogleTest）1,454、`tests/suite/` 11,837（`tools/` の201個のテストジェネレータで生成）、ツール 217、`tests/dump_sir/` 29、状態とContext 21、コマンドラインとイベントカウント 52。

P1対象外：`real_net` — Icarusのwire real拡張への依存を確認。P1では任意。方針確認日2026-11-01。過去の観測の範囲分類だけを見直し、再測定やPASSへの変更はしていません。
<!-- counts:end -->

`python3 tools/readme_counts.py update` で、両 README と本書の両言語版を同期します。
実際に再測定した入力だけを渡し、外部の過去の件数を現在のビルド件数で置き換えないでください。
`check` は 4 文書すべてを検証します。[旧 README](history/README_2026-11-01.ja.md)には、
文書整理前の表をそのまま保存しています。

## WSL2 上の Linux（2026-11-07）

WSL2 上の Linux で、`60c26d7c` の dev 構成をビルドして実行しました。
**CTest 12,581 件中 12,581 件が成功**し、スキップ 0 件、失敗 0 件、所要時間は 895 秒です。
ビルドの警告は 0 件で、警告はエラーとして扱っています。このコミットの時点では、総数と種類ごとの件数が記録済みの dev の件数と同じで、
`tools/readme_counts.py check` もこの実行の JUnit ファイルを受理しました。上の件数は後のコミットに合わせて更新されるため、
これより多いことがあります。`60c26d7c` より後に追加されたテストは、この実行に含まれません。
コマンドと結果は[機械可読の記録](measurements/2026-11-07-wsl2/dev-ctest.json)にあります。

- **環境：** AMD Ryzen 9 7950X（WSL2 上で論理 CPU 32 個）、110 GiB、WSL2 上の Ubuntu 24.04.4 LTS、
  カーネル 6.18.54.1、glibc 2.39、x86-64。このマシンの `c++` と `cc` のとおり、C++ は Ubuntu clang 20.1.8、
  C は GCC 15.2.0 でビルドします。CMake は 4.2.1 です。ビルドは新しいツリーではなく、既存の `build/dev` を使いました。
- **実行していないもの：** release・asan・ubsan の各プリセット、`tools/local_ci.py` の静的検査・clang-format・
  clang-tidy、sv-tests、P1 の母集団、Icarus のプローブ、ファジング、速度の測定。この実行はこれらについて何も示しません。
- **このマシンの asan プリセット：** 2026-10-30 に、`f303beca` にドキュメントのコミットを 1 つ載せた状態で、
  `cc` と `c++` が GCC と Clang のまま、asan のビルドが「linked against incompatible ASan runtimes」で止まりました。
  `build/asan` を `CC=clang CXX=clang++` で作り直すと、ビルドとテストが通りました。`60c26d7c` では確かめ直していません。
