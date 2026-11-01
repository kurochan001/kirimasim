# 部分inoutポートの期待値移行案

[English](partial-inout-expectation-migration.md)。状態：**承認済み・適用済み（2026-11-01）**。

IEEE 1800-2023 7.4・23.3.3.7と、2026-10-25承認のMOS/port契約4に従い、変換のない
静的なnetのinout接続を、既存の物理bitのidentityへ接続します。値のcarrierや新しい根は作りません。

承認された移行は、次の既存2件の受理分類だけです。

- CLI `tests/cli/unsupported_matrix/port_inout_part.sv`：SVソースを変えず、終了2・未対応診断1行から、
  終了0・stdoutなし・警告/エラーなしへ移します。実装で確認済みです。accepted一覧へ1名追加、
  stderr一覧から対応する1行だけを取り除きます。
- 単体 `ArrayElementPort.PartialInoutKeepsItsPositionedBoundary`：埋め込んだSVソースを変えず、
  コンパイル拒否から成功へ移し、名前を `PartialInoutMapsItsPhysicalElement` にします。
  成功だけに緩めず、identity 1本、物理要素の序数1・bit0、暗黙/明示の根なし、SIR検証と
  空stdoutでの実行も要求します。

具体的な差分は `build/partial-inout-expectations.patch` に用意しました。
幅変換のinout、force/uwire・動的/不正なidentity選択等の既存境界と、ほかの期待値・性能予算は保ちます。
期待値だけを実装と別コミットにします。pushやP1完了の承認ではなく、slang投稿も保留です。

確認用コピーのCLI全体・生成器と、提案した受理単体が成功しました。単体は実際の実装の
ライブラリへ別途コンパイルしています。同じ差分を、明示的な承認後にリポジトリの期待値へ適用しました。


## 承認と適用（2026-11-01）

保守者は提示した2項目・計3テストの移行を明示的に承認しました。この記録は上記の部分inoutの2件だけを扱います。
用意済みの差分を適用し、CLI・単体の元の設計ソースは変更しません。
CLIは未対応診断から受理へ移し、単体は物理identity・対象要素とbit・根なし・正常な空出力実行を要求します。
期待値は`src/`と別コミットにします。別途承認されたclass監視の変更と統合後、回帰検証と件数更新を行います。
main反映・push・slang投稿の承認とは扱いません。


統合後のdev全体11,299件が、除外なしですべて成功しました。[統合記録](p1-approved-class-inout-integration.ja.md)を参照してください。main反映・push・slang投稿は別の指示で行います。
