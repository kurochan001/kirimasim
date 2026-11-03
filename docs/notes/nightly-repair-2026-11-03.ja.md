# CI・Nightly修正：空のFSTとSIGINT変異の検査

[English](nightly-repair-2026-11-03.md) · 2026-11-03。

[`7554bb47`のCI](https://github.com/kurochan001/kirimasim/actions/runs/41587222529)は、
空のFSTからlibcへNULLのバッファが渡り、Linux/GCCのUBSanで3テストが失敗しました。
[FSTのアダプタと回帰記録](fst-execution.ja.md#空のfstとubsan2026-11-03)に修正を記録しています。
依存ソース・既存期待値・sanitizerの検査は保持します。修正後のLinux実行は、次の公開後CIで確認します。

[定期Nightly](https://github.com/kurochan001/kirimasim/actions/runs/41556717656)の対象は、
古い`786c94bb`です。`slang-main`の3件は、すでに承認済みの
[依存更新と期待値移行](slang-d02b7cda-update.ja.md)が扱ったstatic戻り値の差です。
以前の[run 41468624550](https://github.com/kurochan001/kirimasim/actions/runs/41468624550)の
macOS失敗は、ホストされたランナーの確保失敗でした。今回の確認時点で、定期実行のmacOSの
ASan・UBSan・TSan・sv-testsジョブはすべて成功しています。

## 未検出だったSIGINT変異

[変異shard 16](https://github.com/kurochan001/kirimasim/actions/runs/41556717656/job/125657296788)で、
`kernel: SIGINT is taken again in final procedures`が未検出と報告されました。
`Scheduler::takeInterrupt`の要求消費済みの代入を外す変異です。既存CLIのfinal本体は
分岐の安全点を通らず、wall-clockのテストは同じフラグへの別の`takeWallTimeLimit`の代入を通ります。

SCHEDULER_SPEC 16.2は、最初の要求を一度だけ受け付け、すべてのfinal手続きを完了する規則です。
追加したkernelテストは、Active/Reactiveの入口、final実行中に要求を解除・保持する形、
それぞれ3安全点を通るfinal手続き2本、外部フラグを共有する独立Contextを組み合わせます。
すでに正しかった本体を検査するもので、実行コードは変更していません。

[コンパイルした対照の記録](../measurements/2026-11-03/nightly-sigint-control.json)にソースhashを
保存しました。未検出だった変異を実際にコンパイルし、追加した意味のアサーションで検出した後、
本体ヘッダを復元すると再び成功します。この検出にカタログ自身の検査は含めません。
その後のdev全件は**11,828/11,828成功**、失敗・skipなしでした。その成功JUnitから件数を更新し、
固定RTL母集団と既存期待値は保持しています。このローカル結果は次のホスト上のNightlyの代わりではありません。
