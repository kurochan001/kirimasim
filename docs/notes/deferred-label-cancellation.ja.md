# 遅延アサーションの報告の選択的取消し

[English](deferred-label-cancellation.md)

2026-11-03に保守者が[仕様修正案](deferred-label-cancellation-proposal.ja.md)を承認しました。
upstream main `7654825d`を取り込んだ作業ツリーに実装し、先のinside修正も保持しています。
既存suiteの入力と期待値は変更していません。

## 規則と実装の境界

IEEE 1800-2023 §16.4.4と承認済みSCHEDULER_SPEC 15.2–15.4は、ラベル指定の取消しと
プロセス全体のflush点を区別します。frontはソースの綴りではなく、slangの意味モデルから
遅延即時アサーションへ直接付けたラベルを識別します。`Lowerer::scanDisables`が文IDを記録し、
`deferAction`がpass/fail両wrapperへ同じIDを付け、`lowerDisable`が通常のscope終了に代えて
`report.cancel`を生成します。`report.call.named`がIDを渡します。
SIR_SPEC 5.7に両命令を定義し、検証器は被演算子の種類・範囲、領域、wrapper、引数を確認します。
無名の既存`report.call`の表現は保持します。

`Scheduler::discardDeferredAssertions`が、従来の所有者全体のflushと新しいID指定の
選別を一か所で扱います。IDはContext内で静的かつ実体ごとに一意です。未成熟の呼出しだけを選び、
unique/priority報告、別ID、確定済みのReactive callback、過去の出力を保持します。
引数評価と副作用は巻き戻しません。キュー変更前に破棄一覧を確保し、保持した順序と件数を
更新してから、既存の破棄callbackで引数rootと所有者の保持を一度だけ解放します。
VMは検証済みIDを渡し、引数取得と呼出しの既存動作を維持します。

initial・task・forkから同じ規則へ到達します。initialの最上位の文へ直接付けたラベルは
最外blockのactivationではなくアサーションIDです。完了した所有者の報告も、blockを
巻き戻さずに取り消せます。実行中アクションの追加制御、アサーション制御システムタスク、
未対応のラベル名前解決形式は追加していません。

## 検証と保持した証拠

`tools/gen_deferred_label_matrix.py`が、assert/assume/cover、Observed/final、
initial/task/forkを交差した18入力を生成します。5周期で、登録前、2件登録後、登録間、
取消しなし、成熟後を確認します。両wrapper、入力取得の副作用、報告時の生値、兄弟の順序、
同じラベル名を持つ別プロセスを観測します。Postponedを含め、アクションは表示だけを行います。
最上位の文の対照2件では、完了済み所有者も確認します。

[実装前18件の記録](../measurements/2026-11-03/deferred-label-before.json)は元の不一致を保持します。
[実装後の新規20入力](../measurements/2026-11-03/deferred-label-after.json)はすべて一致しました。絞った37検査では、不正SIR、成熟済み報告、unique/priority報告、
繰返し取消し、所有者完了後のコンテナsnapshotのrootも確認しています。
外部の既存deferred-disable 31件も、元の期待値ですべて一致しました。

外部の全2,785件は2,252 pass・529 unsupported・configの4 failです。
inside修正時点からの変化はラベル取消し5件のfail→passだけで、入力fingerprintは不変です。
全選択ケースPASSの有限モデルが一つ増え、75モデル中48モデルとなりました。
外部の記録はkirimasim-testsに保存し、ビルド記録でupstreamの版、未コミットのソース、
実際のバイナリを区別しています。

取消し欠落と兄弟報告の巻込みの2故障を`tools/mutants.py`へ登録しました。
[実際にコンパイルした2故障](../measurements/2026-11-03/deferred-label-mutations.json)は、各20入力すべてで終了コード0の出力不一致として検出し、文字列の適用可否検査と分けています。
[全local CIの21項目](../measurements/2026-11-03/deferred-label-validation.json)はすべてPASSです。dev・ASan・UBSanは各11,827件、releaseは11,828件で、除外はありません。正式Nightlyや7連続Nightlyの
条件を、この検査で置き換えることはありません。

## コマンド

```sh
python3 tools/gen_deferred_label_matrix.py --check
ctest --preset dev --output-on-failure -R 'deferred_label|AssertionIdentity|NamedCancellation'
python3 tools/mutants.py --check
CTEST_PARALLEL_LEVEL=8 CMAKE_BUILD_PARALLEL_LEVEL=6 python3 tools/local_ci.py
```

実装をコミットした後は、標準mutation実行器で
`python3 tools/mutants.py --only 'assertion identity'`を使えます。
実行器は未コミットのソースでは実行しない設計です。今回の分離実験はkernel archiveの複製と
同じsuite harnessを使い、本番のソース・バイナリを変更しません。同じ目的の別実行基盤は追加していません。
