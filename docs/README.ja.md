# kirimasim ドキュメント案内

[English](README.md) · 日英で食い違う場合は英語版を正本とします。

**現在の区切り（2026-11-04）：** main `91139559`に、単純な条件付きspecify、net全体へのVPI書込み、
明示クロック本体でのclass操作を取り込みました。全local CIと同じコミットのGitHub CIが成功しています。
[統合記録](notes/main-integration-2026-11-04.ja.md)を参照してください。P1は進行中です。

現行188プローブは、P1評価対象187件と、依存を確認した任意の`wire real`除外1件です。
必須範囲に残るdense UDPの差は、フロントエンドの時間切れという独立した失敗です。以前の母集団・測定は、
[状況の履歴](STATUS_HISTORY.ja.md)と[件数の記録](VALIDATION_COUNTS.ja.md)に測定日を保ちます。

目的に合わせて読む資料を選んでください。計画に載るインターフェースが実装済みとは限りません。
再現するときは実行ファイルの`--version`を保存します。[FAQ](FAQ.ja.md)・[用語集](GLOSSARY.ja.md)から調べ、
[機能例](P1_FEATURES.ja.md)で実行できます。

調べたい問いから、根拠となる資料を選べます。

| 調べたいこと | 最初に読む資料 | 確認する点 |
| --- | --- | --- |
| デザインをビルド・実行・調査するには？ | [ユーザーマニュアル](USER_MANUAL.ja.md)、[不具合調査](DEBUGGING.ja.md) | コマンド、入力、失敗する段階 |
| 現在実行できるソースの形は？ | [P1対応機能](P1_SUPPORT.ja.md) | 型、記憶域、選択方法、実行する文脈の制限 |
| 使えるコマンドラインオプションは？ | [リファレンスマニュアル](REFERENCE_MANUAL.ja.md) | 受理する書式、既定値、未実装の指定 |
| 合意済みの動作は？ | [詳細仕様](spec/) | 承認状態と有効な改定 |
| ある実行で確かめた内容は？ | [検証ガイド](VALIDATION.ja.md)、[件数の記録](VALIDATION_COUNTS.ja.md) | 版、測定日、母集団、参照版 |
| プロジェクトの規模は？ | [プロジェクトの規模](PROJECT_SIZE.ja.md) | 数えたコミットと日付 |
| 提案は実装済み？ | [ノート](notes/)と、そこからリンクする対応機能一覧 | 提案中・承認済み・実装済みの区別 |

## kirimasimを使い始める

- **よくある質問を読む：** [FAQ](FAQ.ja.md)。現在の制限、対応するインターフェース、
  貢献方法などをまとめています。
- **用語の意味を調べる：** [用語集](GLOSSARY.ja.md)。構成、値、スケジューリング、
  テストに関する用語を説明します。
- **ビルドして最初のデザインを動かす：** [ユーザーマニュアル](USER_MANUAL.ja.md)。
  必要な環境、カウンタの例、入力、VCD、上限、トラブルシューティングを説明します。
  実行例のソースは[examples/user_manual](examples/user_manual/)にあります。
- **既存シミュレータのコマンドを移す：** [移行ガイド](PORTING.ja.md)。
  Icarus Verilog・Verilator・Lyraから移る際の入力指定、コンパイルと実行の手順、
  出力の比較、制限を説明します。
- **オプションや終了コードを調べる：** [リファレンスマニュアル](REFERENCE_MANUAL.ja.md)。
  受理する書式、既定値、実行時の動作、システムタスクの分類、未実装の指定をまとめています。
- **言語機能の例を探す：** [P1機能例](P1_FEATURES.ja.md)。
  独自のソース、コマンド、デザインの期待出力、機能ごとの制限を示します。

最初はユーザーマニュアルを読み、必要なら移行ガイドを参照し、個別の疑問は
リファレンスマニュアルで調べる順序を勧めます。

## デザインを実行できるか確認する

- **検証結果を再現して読む：** [検証・再現ガイド](VALIDATION.ja.md)。
  ローカルCI、参照実装、外部母集団、機能プローブ、負の対照、性能の根拠を区別します。
- [P1対応機能](P1_SUPPORT.ja.md)は実装済み機能の一覧です。ソースの形、型、記憶域の寿命、
  実行する場所まで確認してください。フロントエンドで受理しても、実行には未対応の場合があります。
- [P1の範囲](spec/P1_SCOPE.ja.md)は必要な対応範囲を定めます。完了に必要な要件であり、
  既に達成したという宣言ではありません。
- [2026-10-28のP1完了条件の監査](history/P1_CRITERIA_GAPS_2026-10-28.ja.md)は、
  その時点の残作業の評価です。
- [2026-10-30の組合せ不足の具体例](history/P1_COMBINATION_GAPS_2026-10-30.ja.md)は、
  event・ref・複合型・予約代入・streaming・入出力を組み合わせた境界を、自作34入力で確認した日本語の記録です。
- [2026-10-31のP1完了に向けた優先順位](history/P1_PRIORITIES_2026-10-31.ja.md)は、残る1つの完了条件を基準に残作業を並べ、保守者の判断を記録したものです。
- [Icarusとの比較](notes/icarus-13-comparison.ja.md)と
  [P1の進捗](notes/p1-icarus-progress.ja.md)は、測定した版、有限のテスト集合、差分、
  過去の区切りを保ちます。結果は記録した入力と版の範囲に適用します。
- [検証件数の記録](VALIDATION_COUNTS.ja.md)は、リポジトリの件数と外部比較を分け、測定日・参照版を保ちます。
- [状況の履歴](STATUS_HISTORY.ja.md)は、READMEに以前載せていた日付付きの区切り、
  プローブの件数、検証の記録を保ちます。現在の状況はREADMEにあります。
- [性能測定](BENCHMARKS.ja.md)は、測定した設計、環境、日付、再現コマンドを示します。
  処理回数と経過時間は別の観点です。
- [プロジェクトの規模](PROJECT_SIZE.ja.md)は、ライブラリごとのソース、テスト・道具・文書の行数と履歴を、
  日付つきのコミットについて、数えるコマンドとともに記録します。

失敗した場合は、ユーザーマニュアルの
[トラブルシューティング](USER_MANUAL.ja.md#トラブルシューティング)から始め、
[不具合調査ガイド](DEBUGGING.ja.md)でソースを最小化し、対応機能一覧を確認します。
調査ガイドにはSIRの確認方法と、意図的に失敗する実行例もあります。
[Issueフォーム](../.github/ISSUE_TEMPLATE/)に報告に必要な情報を示しています。

## kirimasimを開発・保守する

[CONTRIBUTING](../CONTRIBUTING.ja.md)で参加とレビューの規則を確認し、
[開発者ガイド](DEVELOPMENT.ja.md)でソースからカーネルまでの実例と機能追加の手順を辿ってください。
[アーキテクチャ](ARCHITECTURE.ja.md)はライブラリの境界、依存の向き、共通の実行モデルを
説明します。エージェント向けのリポジトリ固有の規則は[CLAUDE.ja.md](../CLAUDE.ja.md)にあります。

詳細な契約は、変更する境界ごとに分かれています。

- **値と言語の実行順序：** [値の意味論](spec/VALUE_SEMANTICS.ja.md)、
  [スケジューラ](spec/SCHEDULER_SPEC.ja.md)、
  [未規定の動作](spec/UNSPECIFIED_BEHAVIOR.ja.md)。
- **中間表現とランタイム：** [SIR](spec/SIR_SPEC.ja.md)、[Runtime ABI](spec/RUNTIME_ABI.ja.md)。
- **利用者向けの機能：** [CLI](spec/CLI.ja.md)、
  [システムタスク](spec/SYSTEM_TASKS.ja.md)、[波形](spec/WAVEFORM.ja.md)。
- **検証：** [テストスイート](spec/TEST_SUITE.ja.md)、[性能](spec/PERFORMANCE.ja.md)、
  [PerfCountの保守手順](../tests/perf_counts/README.ja.md)、
  [期待値の監査](review/oracle_audit.ja.md)。
- **部分対応・予定のインターフェース：** [コード生成](spec/CODEGEN_SPEC.ja.md)、
  [乱数化](spec/RANDOMIZE_SPEC.ja.md)、
  [アサーションとカバレッジ](spec/ASSERTION_COVERAGE_SPEC.ja.md)、
  [ライブラリ出力](spec/LIB_EXPORT_SPEC.ja.md)、[VPI](spec/VPI_SPEC.ja.md)。
  これらの計画から指定を使う前に、実装状況を確認してください。

## 根拠と提案を読む

承認は意図する契約を決めるもので、実行対応の証明とは別であり、提案・承認・実装・検証の読み分けは[文書の状態のFAQ](FAQ.ja.md#document-states)で説明しています。

[notes/](notes/)には提案、承認の記録、実装の説明、調査報告があります。
提案の状態と、採用した仕様へのリンクを確認してください。ファイル名だけでは、
承認や実装済みとは判断できません。[measurements/](measurements/)は日付付きの生データです。
ソースの版、オプション、比較相手の版、結果の解釈は、対応する説明文書を参照してください。

現在使える動作は対応機能一覧、合意した契約は詳細仕様書、ある実行で確かめた内容は
測定記録で確認します。食い違いがあれば報告し、過去の観測をそのまま現在の契約にはしません。
古い段階名は[段階の対応表](notes/phase-renumbering.ja.md)で確認できます。

## 方針と文書の保守

[CHANGELOG](../CHANGELOG.ja.md)は利用者から見た動作の変更を記録します。
[セキュリティ](../SECURITY.ja.md)、[行動規範](../CODE_OF_CONDUCT.ja.md)、
[AI利用方針](spec/AI_POLICY.ja.md)、[ライセンス](../LICENSE)はそれぞれの方針を定めます。

英語版と日本語版を合わせて保守してください。機能一覧や契約の表全体を各ガイドへ複製せず、
正本へリンクします。根拠には測定日と版を付け、未確認の内容はその旨と確かめる方法を示します。
利用者に見えるソースやオプションの変更は関係するマニュアルにも反映し、
古い測定記録は当時の範囲を保ちます。


公開する説明文書は、CHANGELOG、日付付き監査、公開済みの投稿草稿も含め、英語・日本語の全文を維持します。
`X.md` と `X.ja.md` を対にし、相互の言語リンクを付け、本文だけでなく表・例・脚注・再現手順も翻訳します。
公開 Issue フォームは同じフォーム内に両言語を用意します。未追跡の非公開の引き継ぎ・草稿は対象外です。
コードの識別子、生の診断・出力、データの schema はそのまま保ち、両言語で説明します。
正式な法的文面は変えず、参照用の翻訳を明示して添えます。

README は現在の実装・制限・ビルドと実行の入口を中心にします。経緯や古い測定は、元の版・日付とともに
日英の履歴へ移します。`tools/readme_counts.py` で、README の現在の要約と
[日時付きの記録](VALIDATION_COUNTS.ja.md)を同期し、古い測定を現在のものと表示しないでください。

## 最近の実行記録

- [単純な状態依存specify](notes/specify-state-execution.ja.md)：スカラーの`if`・`ifnone`。独自322入力を検査し、参照差は別に保ちます。
- [net全体へのVPI書込み](notes/vpi-net-write-execution.ja.md)：point netの即時depositと登録netのforce/release。部分・遅延書込みと受動depositは残作業です。
- [明示クロック本体のclass操作](notes/class-clocked-execution.ja.md)：通常の信号トリガーでプロパティやメソッドを実行。object依存のトリガー評価は未対応です。
- [native FST](notes/fst-execution.ja.md)：`--wave=<file>.fst`のバイナリ出力を復号して検査。viewer互換性と性能は未確認です。

- [ワイルドカード関数の依存](notes/wildcard-function-dependencies.ja.md)：実引数だけのトリガー、関数内のcontainer読み取り、保持するevent・object監視の境界。

各ノートでは、承認・実装・検証の時点を区別します。[公開mainの記録](notes/main-integration-2026-11-04.ja.md)で
統合状況を確認してください。有限の入力だけでP1全体の完了とは判定しません。
