# kirimasim への貢献

[English](CONTRIBUTING.md) · 英語版が正本で、これはその日本語版です。食い違ったときは英語版に従います。

関心を持っていただき、ありがとうございます。kirimasim は開発の初期にあり、最初の実行層を作る段階 P1 を進めています。状況は README に書いています。

## 今受け付けているもの

今のところ、kirimasim が受け付けるのは**仕様の提案と Issue だけ**です。プルリクエストを開けるのは共同作業者だけです。受け付ける範囲を広げるときは、このファイルと README でお知らせします。

受け付けるもの：

- **仕様の提案**：`docs/spec/` の下の文書の変更や追加。Issue のフォーム「Specification proposal」から。
- **不具合の報告**：Issue のフォーム「Bug report」から。
- **テストの期待値の誤りの報告**：Issue のフォーム「Expected value change」から。
- **議論**：GitHub Discussions で。質問は Q&A のカテゴリに、まだ具体的な提案になっていない考えは Ideas のカテゴリに。

受け付けないもの：

- プロジェクトの外からの**コードのプルリクエスト**。

Issue と議論には、IEEE 1800 の規格の本文、図、例のコードや、他の人が書いたコードを貼らないでください。勤務先の秘密の情報も含めないでください。節番号を示し、自分の言葉で説明し、最小の例は自分で書いてください。

### 仕様の提案が受け入れられたとき

kirimasim は仕様から実装します。提案が受け入れられたら、まずメンテナが `docs/spec/` の下の仕様を更新し、実装はその更新した仕様に従います。それらの変更から、提案の Issue にリンクします。

このファイルの残りは、共同作業者がリポジトリを変えるときに従う決まりです。

## Sign-off

今のところ、コミットに `Signed-off-by` の行は要りません。外からのコードのプルリクエストを受け付け始めるときに、Developer Certificate of Origin（DCO）か、コントリビューターのライセンス契約（CLA）を導入します。

## AI ツールの利用

AI ツールは歓迎します。kirimasim のコードの大半は AI が生成しています。AI ツールがコミットを手伝ったら、`Assisted-by` のトレーラーでそう書いてください。

```text
Assisted-by: <ツール>:<モデルと版>
```

AI ツールに `Co-authored-by` は使わないでください。出した行のすべてに責任を持つのはあなたです。レビューし、テストし、説明できるようにしておいてください。[docs/spec/AI_POLICY.ja.md](docs/spec/AI_POLICY.ja.md) を見てください。

## 期待される結果を変える

テストの期待値は、このプロジェクトにとっての正しい動作の定義なので、守られています。

- **既存の**期待値（`tests/perf_counts/expected.json` の項目、`tests/**/expected*` の行、`:expected_stdout:` の行、`tests/suite/` の下のテストスイートの既存のファイルの本体）を変える・消す・緩める変更は、`src/` の下の実装の変更とは別の、**それだけのプルリクエスト**にしてください。既存のテストファイルの削除と名前の変更も同じです。プルリクエストには理由を書き、IEEE 1800 の節番号を示してください。
- ユニットテスト（`tests/unit/`）は、それが呼ぶコードと一緒に直してかまいません（関数の名前を変えたときなど）。CI は、変えたか消した `EXPECT_*`・`ASSERT_*` の行をすべて警告として並べ、レビューする人がアサーションを弱めていないかを確かめます。
- 新しいテストとその期待値を、実装と同じプルリクエストで足すのはかまいません。レビューする人が、期待値の出どころを確かめます。
- CI は `tools/check_expected_change.py` でこれを検査します。

## コードの決まり

- kirimasim 本体は C++23。カーネルの公開ヘッダ（`src/kernel/include/`）は C++20 でコンパイルできること。
- `src/kernel/` と `src/vm/` では、`std::shared_ptr`、`dynamic_cast`、`std::function`、`getenv`、`typeid` を使わないこと。`std::unordered_map` と `std::unordered_set` には、同じ行か直前の行に `// kirimasim: unordered-ok <理由>` のコメントが要り、その走査の順が結果に影響してはいけません。`tools/check_banned.py` がこれを検査します。
- 書き換え可能な大域や static の状態とシングルトンを持たないこと。状態はコンテキストのオブジェクトが持ちます。
- ライブラリどうしの依存は、[docs/ARCHITECTURE.ja.md](docs/ARCHITECTURE.ja.md) に挙げた向きだけにすること。
- IEEE 1800 の規格の本文、図、例のコードを写さないこと。節番号を示し、動作は自分の言葉で説明してください。
- 他のシミュレータ（Icarus Verilog や Verilator など）のソースコードを読んだり写したりしないこと。動作は、その出力だけから知ってください。
- `clang-format`（リポジトリの `.clang-format`）で整形すること。

## ビルドとテスト

```sh
cmake --preset dev
cmake --build --preset dev
ctest --preset dev                 # ユニットテスト、テストスイート、PerfCount
ctest --preset dev -L PerfCount    # 事象の回数の回帰の検査だけ
python3 tools/check_banned.py
```

release のビルドでは、`cmake --preset release` の後に `python3 tools/check_release_flags.py` も走らせてください。`asan` と `ubsan` のプリセットは、同じテストをサニタイザ付きで走らせます。`asan` は slang を mimalloc 無しでビルドします。mimalloc の確保した領域の中は、アドレスサニタイザから見えないためです。

CI が走らせる検査のすべて（4 つのプリセット、release のフラグの検査、禁止の機能の検査、clang-format と clang-tidy、まだ push していない各コミットが既存の期待値を守っているかの検査）を一度に走らせるには：

```sh
python3 tools/local_ci.py
```

CI は push の後に Linux と macOS で走ります。push の前に `tools/local_ci.py` を走らせて先に失敗を見つけ、その要約をプルリクエストに貼ってください。

## 変更の記録

[CHANGELOG.md](CHANGELOG.md) は、kirimasim を使う人のためのものです。使う側から見て何が変わったかを書きます。細部はコミットの履歴にあります。

- Keep a Changelog の区分で書くもの：**Added**（新しく動くようになった構文、システムタスク、オプション）、**Changed**（同じ入力で出力、終了コード、メッセージが変わるもの。仕様書の決まりの変更、ABI の版の変更、目に見える速さやメモリの変化を含む）、**Fixed**（誤っていた結果を直したもの）、**Removed** と **Deprecated**、**Security**（SECURITY.md から届いた報告で直したもの）。
- 書かないもの：テストの追加、CI、内部の作り替え、文書だけの変更、PerfCount の回数の変化。
- 1 項目 1 行で、使う人の言葉で書き、根拠になる仕様書の節を添えます（例：`(VALUE_SEMANTICS.md 3.2)`）。このファイルは英語だけです。
- 変更と同じコミットで、`[Unreleased]` の下に項目を足します。レビューする人が確かめます。`src/` の変更の多くは内部の変更なので、道具では強制しません。
- 最初のリリースは、段階 P1 の終わりに 0.1.0 とする予定です。そのとき `[Unreleased]` を `[0.1.0] - <日付>` にし、ファイルの末尾に比較のリンクを置きます。版が 0.x のあいだは、Semantic Versioning が認めるとおり、minor の版が上がるときに互換が崩れることがあります。

## プルリクエスト

今のところ、プルリクエストは共同作業者からだけ受け付けます。プルリクエストのテンプレートを埋めてください。1 つのコミットには 1 つの目的だけを入れ、コミットのメッセージは英語で、要約の行を 72 文字以内で書いてください。

参加することで、[行動規範](CODE_OF_CONDUCT.ja.md)に従うことに同意したものとします。
