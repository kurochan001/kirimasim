# kirimasim ドキュメント案内

[English](README.md) · 日英で食い違う場合は英語版を正本とします。

目的に合わせて読む資料を選んでください。kirimasimは開発初期で、P1は進行中です。
計画に記載したインターフェースが、実装済みとは限りません。結果を再現するときは、
実行ファイルの`--version`出力を保存してください。

## kirimasimを使い始める

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
  手元のCI、参照実装、外部母集団、機能プローブ、負の対照、性能の根拠を区別します。
- [P1対応機能](P1_SUPPORT.ja.md)は実装済み機能の一覧です。ソースの形、型、記憶域の寿命、
  実行する場所まで確認してください。フロントエンドで受理しても、実行には未対応の場合があります。
- [P1の範囲](spec/P1_SCOPE.ja.md)は必要な対応範囲を定めます。完了に必要な要件であり、
  既に達成したという宣言ではありません。
- [2026-10-28のP1完了条件の監査](P1_CRITERIA_GAPS_2026-10-28.md)は、
  その時点の残作業の評価です。この監査は現在、日本語で記載しています。
- [Icarusとの比較](notes/icarus-13-comparison.ja.md)と
  [P1の進捗](notes/p1-icarus-progress.ja.md)は、測定した版、有限のテスト集合、差分、
  過去の区切りを保ちます。結果は記録した入力と版の範囲に適用します。
- [性能測定](BENCHMARKS.ja.md)は、測定した設計、環境、日付、再現コマンドを示します。
  処理回数と経過時間は別の観点です。

失敗した場合は、ユーザーマニュアルの
[トラブルシューティング](USER_MANUAL.ja.md#トラブルシューティング)から始め、
[不具合調査ガイド](DEBUGGING.ja.md)でソースを最小化し、対応機能一覧を確認します。
調査ガイドにはSIRの確認方法と、意図的に失敗する実行例もあります。
[Issueフォーム](../.github/ISSUE_TEMPLATE/)に報告に必要な情報を示しています。

## kirimasimを開発・保守する

[CONTRIBUTING](../CONTRIBUTING.ja.md)で参加とレビューの規則を確認し、
[開発者ガイド](DEVELOPMENT.ja.md)でソースからカーネルまでの実例と機能追加の手順を辿ってください。
[アーキテクチャ](ARCHITECTURE.ja.md)はライブラリの境界、依存の向き、共通の実行モデルを
説明します。エージェント向けのリポジトリ固有の規則は[AGENTS.md](../AGENTS.md)にあります。

詳細な契約は、変更する境界ごとに分かれています。

- **値と言語の実行順序：** [値の意味](spec/VALUE_SEMANTICS.ja.md)、
  [スケジューラ](spec/SCHEDULER_SPEC.ja.md)、
  [未規定の動作](spec/UNSPECIFIED_BEHAVIOR.ja.md)。
- **中間表現とランタイム：** [SIR](spec/SIR_SPEC.ja.md)、[Runtime ABI](spec/RUNTIME_ABI.ja.md)。
- **利用者向けの機能：** [CLI](spec/CLI.ja.md)、
  [システムタスク](spec/SYSTEM_TASKS.ja.md)、[波形](spec/WAVEFORM.ja.md)。
- **検証：** [テストスイート](spec/TEST_SUITE.ja.md)、[性能](spec/PERFORMANCE.ja.md)、
  [PerfCountの保守手順](../tests/perf_counts/README.ja.md)、
  [期待値の監査](review/oracle_audit.ja.md)。
- **後続または未完のインターフェース：** [コード生成](spec/CODEGEN_SPEC.ja.md)、
  [乱数化](spec/RANDOMIZE_SPEC.ja.md)、
  [アサーションとカバレッジ](spec/ASSERTION_COVERAGE_SPEC.ja.md)、
  [ライブラリ出力](spec/LIB_EXPORT_SPEC.ja.md)、[VPI](spec/VPI_SPEC.ja.md)。
  これらの計画から指定を使う前に、実装状況を確認してください。

## 根拠と提案を読む

[notes/](notes/)には提案、承認の記録、実装の説明、調査報告があります。
提案の状態と、採用した仕様へのリンクを確認してください。ファイル名だけでは、
承認や実装済みとは判断できません。[measurements/](measurements/)は日付付きの生データです。
ソースの版、オプション、比較相手の版、結果の解釈は、対応する説明文書を参照してください。

現在使える動作は対応機能一覧、合意した契約は詳細仕様書、ある実行で確かめた内容は
測定記録で確認します。食い違いがあれば報告し、過去の観測をそのまま現在の契約にはしません。
古い段階名は[段階の対応表](notes/phase-renumbering.ja.md)で確認できます。

## 方針と文書の保守

[CHANGELOG](../CHANGELOG.md)は利用者から見た動作の変更を記録します。
[セキュリティ](../SECURITY.ja.md)、[行動規範](../CODE_OF_CONDUCT.ja.md)、
[AI利用方針](spec/AI_POLICY.ja.md)、[ライセンス](../LICENSE)はそれぞれの方針を定めます。

英語版と日本語版を合わせて保守してください。機能一覧や契約の表全体を各ガイドへ複製せず、
正本へリンクします。根拠には測定日と版を付け、未確認の内容はその旨と確かめる方法を示します。
利用者に見えるソースやオプションの変更は関係するマニュアルにも反映し、
古い測定記録は当時の範囲を保ちます。
