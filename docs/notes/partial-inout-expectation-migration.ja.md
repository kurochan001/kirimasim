# 部分inoutポートの期待値移行案

[English](partial-inout-expectation-migration.md)。状態：**承認待ち（2026-11-01）**。

IEEE 1800-2023 7.4・23.3.3.7と、2026-10-25承認のMOS/port契約4に従い、変換のない
静的なnetのinout接続を、既存の物理bitのidentityへ接続します。値のcarrierや新しい根は作りません。

承認をお願いするのは、次の既存2件の受理分類だけです。

- CLI `tests/cli/unsupported_matrix/port_inout_part.sv`：SVソースを変えず、終了2・未対応診断1行から、
  終了0・stdoutなし・警告/エラーなしへ移します。実装で確認済みです。accepted一覧へ1名追加、
  stderr一覧から対応する1行だけを取り除きます。
- 単体 `ArrayElementPort.PartialInoutKeepsItsPositionedBoundary`：埋め込んだSVソースを変えず、
  コンパイル拒否から成功へ移し、名前を `PartialInoutMapsItsPhysicalElement` にします。
  成功だけに緩めず、identity 1本、物理要素の序数1・bit0、暗黙/明示の根なし、SIR検証と
  空stdoutでの実行も要求します。

具体的な差分は `build/partial-inout-expectations.patch` に用意しました。
幅変換のinout、force/uwire・動的/不正なidentity選択等の既存境界と、ほかの期待値・性能予算は保ちます。
承認後、期待値だけを実装と別コミットにします。pushやP1完了の承認ではなく、slang投稿も保留です。

確認用コピーのCLI全体・生成器と、提案した受理単体が成功しました。単体は実際の実装の
ライブラリへ別途コンパイルしています。リポジトリの期待値ファイルは承認待ちのまま保持しています。
