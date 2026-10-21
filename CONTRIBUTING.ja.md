# kirimasim への貢献

[English](CONTRIBUTING.md) · 英語版が正本で、これはその日本語版です。食い違ったときは英語版に従います。

関心を持っていただき、ありがとうございます。kirimasim は開発の初期にあり、最初の実行層を作る段階 P1 を進めています。状況は README に書いています。

## 今受け付けているもの

今のところ、kirimasim が受け付けるのは**仕様の提案と Issue だけ**です。このリポジトリではプルリクエストの機能を止めているので、プルリクエストは作れません。共同作業者は `main` に push します。受け付ける範囲を広げるときは、このファイルと README でお知らせします。

受け付けるもの：

- **仕様の提案**：`docs/spec/` の下の文書の変更や追加。Issue のフォーム「Specification proposal」から。
- **不具合の報告**：Issue のフォーム「Bug report」から。
- **テストの期待値の誤りの報告**：Issue のフォーム「Expected value change」から。
- **議論**：GitHub Discussions で。質問は Q&A のカテゴリに、まだ具体的な提案になっていない考えは Ideas のカテゴリに。

受け付けないもの：

- **プルリクエスト**：このリポジトリでは機能を止めています。

Issue と議論には、IEEE 1800 の規格の本文、図、例のコードや、他の人が書いたコードを貼らないでください。勤務先の秘密の情報も含めないでください。節番号を示し、自分の言葉で説明し、最小の例は自分で書いてください。

### 仕様の提案が受け入れられたとき

kirimasim は仕様から実装します。提案が受け入れられたら、まずメンテナが `docs/spec/` の下の仕様を更新し、実装はその更新した仕様に従います。それらの変更から、提案の Issue にリンクします。

このファイルの残りは、共同作業者がリポジトリを変えるときに従う決まりです。

## AI ツールの利用

AI ツールは歓迎します。kirimasim のコードの大半は AI が生成しています。AI ツールがコミットを手伝ったら、`Assisted-by` のトレーラーでそう書いてください。

```text
Assisted-by: <ツール>:<モデルと版>
```

AI ツールに `Co-authored-by` は使わないでください。出した行のすべてに責任を持つのはあなたです。レビューし、テストし、説明できるようにしておいてください。[docs/spec/AI_POLICY.ja.md](docs/spec/AI_POLICY.ja.md) を見てください。

## 期待される結果を変える

テストの期待値は、このプロジェクトにとっての正しい動作の定義なので、守られています。

- **既存の**期待値（`tests/perf_counts/expected.json` の項目、`tests/**/expected*` の行、`:expected_stdout:` の行、`tests/suite/` の下のテストスイートの既存のファイルの本体）を変える・消す・緩める変更は、`src/` の下の実装の変更とは別の、**それだけのコミット**にしてください。既存のテストファイルの削除と名前の変更も同じです。コミットのメッセージに理由を書き、IEEE 1800 の節番号を示してください。CI は `main` に push された各コミットを 1 つずつ確かめます。
- ユニットテスト（`tests/unit/`）は、それが呼ぶコードと一緒に直してかまいません（関数の名前を変えたときなど）。CI は、変えたか消した `EXPECT_*`・`ASSERT_*` の行をすべて警告として並べ、レビューする人がアサーションを弱めていないかを確かめます。
- 新しいテストとその期待値を、実装と同じコミットで足すのはかまいません。レビューする人が、期待値の出どころを確かめます。
- CI は `tools/check_expected_change.py` でこれを検査します。

## コードの決まり

- kirimasim 本体は C++23 で、C++26 の機能は使わないこと。カーネルの公開ヘッダ（`src/kernel/include/`）は C++20 でコンパイルできること。
- `src/kernel/` と `src/vm/` では、`std::shared_ptr`、`dynamic_cast`、`std::function`、`getenv`、`typeid` を使わないこと。`std::unordered_map` と `std::unordered_set` には、同じ行か直前の行に `// kirimasim: unordered-ok <理由>` のコメントが要り、その走査の順が結果に影響してはいけません。`tools/check_banned.py` がこれを検査します。
- 大域状態を持たないこと。状態は Context（`kernel::Context`）が持ちます。`src/` では、静的またはスレッドの記憶域期間を持つ変数は `constexpr` か `constinit const` に限り、シングルトンも持ちません。決まりと少しの例外は [RUNTIME_ABI.md の 9 節](docs/spec/RUNTIME_ABI.ja.md)にあります。clang-tidy（`--experimental-custom-checks` 付き）と `tools/check_global_state.py` がこれを検査します。例外を足すには、人の承認と `tools/global_state_allowlist.txt` への 1 行が要ります。
- `src/driver/` の外では、環境変数、ロケール、時刻、外からの乱数の種を読まず、プロセスを終わらせず、プロセスの標準出力と標準エラーに書かないこと。`abort`、`std::terminate`、`assert` はどこでも呼びません。`tools/check_banned.py` がこれを検査します。
- ライブラリどうしの依存は、[docs/ARCHITECTURE.ja.md](docs/ARCHITECTURE.ja.md) に挙げた向きだけにすること。
- IEEE 1800 の規格の本文、図、例のコードを写さないこと。節番号を示し、動作は自分の言葉で説明してください。
- 他のシミュレータ（Icarus Verilog や Verilator など）のソースコードも、そのほかの GPL や LGPL のソースコードも、読んだり、写したり、移植したりしないこと。AI ツールにも読ませないでください。ツールの動作は、規格と、その出力だけから知ってください。
- よそから取り入れてよいのは考え方だけで、出典は、論文、公式ドキュメント、Issue の議論、人が書いた設計メモ（コードを含まないもの）に限ります。それらに引かれたコードは取り入れません。考え方を使ったときは、出所を書いてください。使う `docs/spec/` の節が出典を挙げ、長くなるメモは `docs/notes/` に（英語と日本語で）置きます。
- テストも同じです。他のプロジェクトの Issue の再現コードやテストスイートを写さず、バグの内容から書き直してください。
- `clang-format`（リポジトリの `.clang-format`）で整形すること。

## ビルドとテスト

```sh
cmake --preset dev
cmake --build --preset dev
ctest --preset dev                 # ユニットテスト、テストスイート、PerfCount
ctest --preset dev -L PerfCount    # 事象の回数の回帰の検査だけ
python3 tools/check_banned.py
python3 tools/check_global_state.py
```

release のビルドでは、`cmake --preset release` の後に `python3 tools/check_release_flags.py` も走らせてください。`asan` と `ubsan` のプリセットは、同じテストをサニタイザ付きで走らせます。`asan` は slang を mimalloc 無しでビルドします。mimalloc の確保した領域の中は、アドレスサニタイザから見えないためです。`tsan` のプリセットは、状態と Context のテスト（`tests/context/`、RUNTIME_ABI.md の 9.8 節）だけを、スレッドサニタイザ付きで、やはり mimalloc 無しでビルドして走らせます。`dev`、`asan`、`ubsan`、`tsan` のプリセットは、C++ の標準ライブラリの範囲の検査（libstdc++ は `_GLIBCXX_ASSERTIONS`、libc++ は extensive の hardening モード）も、slang の分も含めて有効にします。範囲外の添字は、その場で実行を止めます。`release` のプリセットは有効にしません。

kirimasim 自身のコードは `-Wall -Wextra -Wpedantic` でビルドします。`tools/local_ci.py` と CI の clang のビルドと同じく警告を誤りにするには、configure に `-DKIRIMASIM_WERROR=ON` を付けてください。

CI が走らせる検査（4 つのプリセット、release のフラグの検査、禁止の機能の検査、大域状態の検査、clang-format と clang-tidy、まだ push していない各コミットが既存の期待値を守っているかの検査）を一度に走らせるには：

```sh
python3 tools/local_ci.py
```

CI は push の後に Linux と macOS で走ります。push の前に `tools/local_ci.py` を走らせて先に失敗を見つけてください。CI は、kirimasim が対応する最も古いコンパイラでもビルドとテストをします（README の「ビルド」に挙げています）。開発の機械にはそうしたコンパイラがまず無いので、`tools/local_ci.py` はこれをしません。夜間の実行は、kirimasim を C++26 としてもビルドしてテストします。

README の件数（このリポジトリのテスト、sv-tests、自前のテストスイートの P1 の母集団）は `docs/counts.json` から来ていて、`tools/readme_counts.py` が各 README の範囲に書きます。テストを足したり消したりしたら、dev のテストを `--output-junit` を付けて走らせ、`python3 tools/readme_counts.py update --build build/dev --junit <ファイル>` を走らせてください。sv-tests や母集団を測ったときは、`--svtests` や `--population` も付けます。件数が古いと CI が失敗し、nightly の実行も自分の測った値と違えば失敗します。

## 変更の記録

[CHANGELOG.md](CHANGELOG.md) は、kirimasim を使う人のためのものです。使う側から見て何が変わったかを書きます。細部はコミットの履歴にあります。

- Keep a Changelog の区分で書くもの：**Added**（新しく動くようになった構文、システムタスク、オプション）、**Changed**（同じ入力で出力、終了コード、メッセージが変わるもの。仕様書の決まりの変更、ABI の版の変更、目に見える速さやメモリの変化を含む）、**Fixed**（誤っていた結果を直したもの）、**Removed** と **Deprecated**、**Security**（SECURITY.md から届いた報告で直したもの）。
- 書かないもの：テストの追加、CI、内部の作り替え、文書だけの変更、PerfCount の回数の変化。
- 1 項目 1 行で、使う人の言葉で書き、根拠になる仕様書の節を添えます（例：`(VALUE_SEMANTICS.md 3.2)`）。このファイルは英語だけです。
- 変更と同じコミットで、`[Unreleased]` の下に項目を足します。レビューする人が確かめます。`src/` の変更の多くは内部の変更なので、道具では強制しません。
- 最初のリリースは、段階 P1 の終わりに 0.1.0 とする予定です。そのとき `[Unreleased]` を `[0.1.0] - <日付>` にし、ファイルの末尾に比較のリンクを置きます。版が 0.x のあいだは、Semantic Versioning が認めるとおり、minor の版が上がるときに互換が崩れることがあります。

## コミット

プルリクエストの機能は止めていて、共同作業者は `main` に push し、CI が push された各コミットを確かめます。1 つのコミットには 1 つの目的だけを入れ、コミットのメッセージは英語で、要約の行を 72 文字以内で書いてください。

参加することで、[行動規範](CODE_OF_CONDUCT.ja.md)に従うことに同意したものとします。
