# 強度表示：既存期待値の移行案

[English](strength-format-expectations.md)。2026-11-01。**保守者が2026-11-01に承認済みです。**

実装は2026-10-25承認のMOS/switch契約5節（登録netのforce/release）と
8節（完全な強度表示）に従います。IEEE 1800-2023の参照は10.6.2、21.2.1.4です。
新しい意味の承認ではなく、AGENTS.mdが定める既存期待値の別レビューです。
承認済み契約でも、期待値の移行は別途レビューすると明記されています。

[2ファイルの具体的なパッチ](strength-format-expectation-migration.patch)は、
すべてのソース入力を保ち、次の5つの期待値を変更します。

- `tests/tools/check_source_mos_boundaries.py`：既存のMOS netへの直接forceと
  alias経由のforceは、正常終了と空の標準出力を要求します。既存の未駆動nmosを
  `%v`で表示する入力は、正確に`HiZ\n`を要求します。tranの受理と、残る2件の
  フロントエンド拒否については位置付き診断の検査を保ちます。
  実行せずに未対応の結果を成功へ分類し直すものではありません。
- `StrengthNetworkSir.MalformedStaticRowsAndForeignOwnersAreRejectedBeforeVmConstruction`：
  mode 14（非稼働のforce-targetフラグ）に、検証の成功、quiescentでの終了、
  物理slotが1つ、読み値`[6,6]`、稼働forceなしを要求します。他の26の不正入力は
  従来どおり拒否し、storeが空でbankが未構成であることを検査します。
- `StrengthNetworkSir.ImplicitOriginsStayFixedAndForceThroughAliasesIsRejected`：
  同じalias入力に非稼働forceフラグを付けた後も、27個すべてのpull/supply区間、
  全nodeにforceなし、物理slotが1つ、生成processが0、エラーのないquiescentでの
  終了を要求します。

検査する本体を減らさず、ファイルの削除・改名もしません。新規テストでは実際の
force、重なり、release、強度表示も検査します。承認どおり実装とは別コミットで適用し、除外なしの回帰結果から件数を更新します。
