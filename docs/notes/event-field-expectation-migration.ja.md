# automatic eventの旧拒否検査3件の移行

[English](event-field-expectation-migration.md)が正。Status: **approved (2026-10-25)**。
利用者が以下の移行3件を承認しました。正規providerの実装`1dc06096`の後、項目1/2を
`ecbe4abb`で適用しました。項目3はソース実行全体の接続待ちです。
以下の提案本文では、期待値変更だけを実装と別に承認依頼しています。
[automatic捕捉契約](automatic-capture-proposal.ja.md)5/6/7は承認済みですが、
その承認には既存の期待値変更を含まないと明記されています。

## 理由と根拠

IEEE 1800-2023 6.8・6.21・15.5、承認済み契約とSIR_SPEC 3.2.1/7/8・
RUNTIME_ABI 4/6への規範的対応により、型付きautomatic eventフィールド、新規生成、
字句スコープの保持とevent同一性の共有が必要です。管理表・root visitor・保留操作の所有は
実装済みで、次のSIR/VM/front接続を旧未対応テストに合わせて拒否し続けることはできません。

AI_POLICYの「期待値を守る決まり」は、期待値の変更に理由・規格の節と人の承認が
必要と定めています。移行は`src/`変更のない別コミットにします。
依存・速度の基準・既存ソース入力・無関係のアサーションは変えません。

## 変更案の全件

1. `tests/unit/activation_sir_test.cpp`の
   `MalformedScopeAndCaptureTablesAreRejectedByTheSharedVerifier`のcase 11で、
   不正入力だけを`Type{TypeKind::Event, 0}`から`Type{TypeKind::Event, 1}`へ変えます。
   正規のevent型の幅は0です。`typed storage`の拒否検査と他の全caseは保ち、
   正規のスカラー/固定配列eventの肯定検査を実装時に追加します。
2. `tests/unit/activation_environment_test.cpp`の
   `UnsupportedOwnedProvidersAndMalformedTablesAreRejected`の最初の入力でも、
   同じ幅0→1の変更だけをします。`InvalidLayout`、生存割り当て0、全11caseの検査と
   他の入力は保ちます。実装では非正規のevent型をカーネルlayout公開前に拒否します。
   正規eventのproviderには別途肯定検査を追加します。
3. `tests/tools/check_automatic_capture_metadata.py`の拒否ループの5番目、
   `event unread; int value=7;`を持つtaskとforkの独自ソースだけを受理検査へ移します。
   ソース本文は1バイトも変えません。終了0、標準出力ちょうど`7\n`、標準エラー空、
   dump中の`event.new`が1個、親のscoped記憶域と子のcapture viewにeventフィールドが
   あることを要求します。他の不正なref入力4件と位置付きエラーの検査は変更せず、
   道具の要約だけを実態に合わせます。

## 適用時点と確認

1/2は正規のeventフィールドと実際の型付きproviderが接続された時点で、3はソース生成・
スコープ全体のroot・一時値の所有・型付き実行操作が接続され検証された時点で適用します。
それまでは旧検査を保ちます。機能を有効にした後、承認済み移行をまだ適用していないなら
全CI成功とは扱いません。

移行コミットだけを`check_expected_change.py`で検査します。不正型の検査を保ち、独立した
スカラー/配列/捕捉/rootの肯定検査を追加します。旧ソースの置換・失敗のスキップ・
他の検査や性能基準の緩和をせず、全local CIとソース行列を実行します。
