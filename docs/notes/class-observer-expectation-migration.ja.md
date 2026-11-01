# classと通常監視の期待値移行案

[English](class-observer-expectation-migration.md) · 英語版が正本です。

状態：**承認済み・適用済み（2026-11-01）**。
保守者は提示した2項目・計3テストの移行を明示的に承認しました。この記録はclass監視の1件だけを扱います。

[今回の実装](class-independent-observers.ja.md)はIEEE 1800-2023 8.4・9.4.3と承認済みclass契約7に
従います。常に真の条件の`wait`はオブジェクトに依存せず、無関係なclassの記述子があることを
理由に未対応にはなりません。

既存の自作SIRの単体テスト
`ClassVmExecution.UnsupportedObservationCombinationsStillFailBeforeAnyRuntimeValueIsCreated`
1件は、この入力で事前検査の内部エラーとスロット未確保を要求していました。
今回の実装では、スロットを初期化して正常にquiescentで終了します。
承認前のdev全体では不一致はこの1件だけで、11,069件中11,068件が成功し、skipはありませんでした。

作るSIRのバイト列を変えず、承認された次の移行を適用します。

- テスト名を`OrdinaryConstantWaitCoexistsWithClassMetadata`に変更します。
- 内部の監視エラーの代わりに、`EndReason::Quiescent`と空のエラーを要求します。
- 設計のスロット数と同じ数の実行時スロットの初期化、および既存のobject・flagスロットの
  null/zero初期値を要求します。
- オブジェクトのentryが0という検査を保ち、残るactivationが0という検査も加えます。

具体的な差分は`build/class-observer-expectation.patch`に用意しました。
提案したテストのコピーを、実際の実装のライブラリへコンパイル・リンクし、ClassVmExecutionの22件全ての成功を確認しました。
同じ差分をリポジトリのテストへ適用し、この移行だけを`src/`とは別コミットにします。
オブジェクトに依存する拒否テストのアサーションは保ちます。別途承認された部分inoutの移行と
統合した後、全体検証を行います。main反映・push・slang投稿の承認とは扱いません。


統合後のdev全体11,299件が、除外なしですべて成功しました。[統合記録](p1-approved-class-inout-integration.ja.md)を参照してください。main反映・push・slang投稿は別の指示で行います。
