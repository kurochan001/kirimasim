# Nightly の件数と変異テストの修正（2026-11-02）

[English](nightly-repair-2026-11-02.md)

## 確認した失敗

[Nightly run 41355215502](https://github.com/kurochan001/kirimasim/actions/runs/41355215502)の
対象は `2c81a591105b5b364ac9c0f106b99082987b3c51` です。Linux の `p1-gates` と
macOS の `svtests-macos` は `README counts` で失敗しました。両方とも 344 件中
252 件の kirimasim 成功を実測しましたが、`docs/counts.json` は 244 件のままでした。
Icarus は Linux 211 件、macOS 210 件のままです。公開ログの測定日は 2026-11-01 UTC で、
今回のローカル再測定ではありません。固定した独自母集団も 1,456/1,466 件（99.3%）の成功を維持しています。

変異テストの全 8 ジョブは、ホストの 6 時間の実行上限を超えました。
1,126 変異を 1 ジョブあたり 140〜141 件に分けており、ログにある検出は途中までの結果で、
一覧全体の成功ではありません。さらに分割 4 は `container VM: uses the formal receiver bound` で
`DOES NOT BUILD` を記録しています。置換後のコードが削除済み VM メンバー `heapTypes_` を参照していました。
sanitizer、単体のシャッフル、ThreadSanitizer、compiler、arm64 のジョブは成功しました。
[ログから抽出した監査記録](../measurements/2026-11-02/nightly-failure-audit.json)を参照してください。
この workflow は個別 sv-tests の `results.json` を保存していないため、代用品を捏造していません。

## 修正

- 両 OS の sv-tests 記録を実測の 252 件・実際の UTC 測定日へ同期し、README の進捗表と
  生成した検証記録を日英で更新します。固定母集団・参照コミット・厳密な件数照合を維持します。
  その他の過去の測定と P1 完了条件は変更しません。
- 変異一覧全体を空でない 32 ジョブへ分け、その時点で各 35〜36 件にします。
  変異に欠落・重複はなく、ビルド失敗や生き残りは引き続き失敗です。
  変更後のホスト上の所要時間は、新しい workflow の完走まで未確認です。
- formal receiver bound の対照は残し、古い記憶域参照を `heapPlan().containers()` へ直します。
  receiver の実際の型の代わりに formal の型を使う誤実装を引き続き注入します。
  simulator 本体・既存テスト入力・期待値・分類は変更しません。

## 確認

既存の Ruby YAML library で workflow の構文を確認しました。行列の検査では 1,126 変異すべてを
32 分割へちょうど 1 回ずつ割り当て、最大 36 件でした。一覧の名前・順序・総数は不変です。
置換の適用検査は 1,126 件を確認し、失敗 0 件でした。

実際の変異 runner は、未変更時の CTest 全件成功、修正した対照のコンパイル、
`suite/containers/container_sequence_program_actual_bounds` による検出を確認しました。
変異実行では一覧自身の検査 2 件を除外しています。runner の問題数は 0 で、ソースの元の SHA への復元・
未変更ソースの再ビルドに成功し、その同じテストも再び成功しました。ローカルログは専用検証 worktree の
ignored `build/nightly-formal-bound-*` にあります。

```sh
python3 tools/readme_counts.py check --build build/dev
python3 tools/mutants.py --check
python3 tools/mutants.py --preset dev \
  --only 'container VM: uses the formal receiver bound'
ctest --preset dev -R '^suite/containers/container_sequence_program_actual_bounds$' \
  --output-on-failure
```

変異は、ソースが clean な専用 worktree でのみ実行します。修正後の Nightly 全体で、
両 OS の件数ジョブと全 32 分割の完走を引き続き確認する必要があります。
古い版を再実行するだけでは、この修正の検証にはなりません。ホスト上の全変異成功、
7 晩連続の fuzz 成功、P1 完了をここで主張するものではありません。
