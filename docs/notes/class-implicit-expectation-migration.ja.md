# classのcomb/latch：旧拒否期待値2箇所のレビュー

[English](class-implicit-expectation-migration.md) · 2026-11-05。

状態：**2026-11-05承認済み・別コミット`4e177301`で適用済み**。実装は承認済みの
[class契約7節](class-runtime-proposal.ja.md#7-event依存と観測)と
[SCHEDULER_SPEC 6.3](../spec/SCHEDULER_SPEC.ja.md#63-always_comb-の暗黙の感度)に従います。
この確認は下記の既存単体テスト2箇所だけが対象で、新しい意味の契約、mainへの反映、push、
slangへの投稿の承認ではありません。

class参照がcomb/latchの感度へ加えるのは、メソッドの実引数の式だけです。引数のない直接の
プロパティ読取りでは、暗黙のプロパティトリガーを追加しません。本体は時刻0に実行します。
通常関数とstaticメソッドの読取りは既存の感度を保ち、instanceメソッドとconstructorの本体の読取りは
感度へ加えません。実際にobject依存となる明示的な監視と`@*`は、現在の未対応の境界を保ちます。

以下の提案とレビュー前の観測は、当時の文面を保ちます。保守者が2箇所を明示的に承認しました。
未来形と22件中20件成功という記録は、承認前の時点を示します。

## 具体的な移行案

対象は、どちらも`tests/unit/class_front_test.cpp`です。

1. `ClassFront.UnsupportedClassBoundariesAreSourceDiagnostics`に含まれる
   `class Box;int data;endclass module top;Box a;int v;initial a=new;always_comb v=a.data;endmodule`を、
   **ソースを変えずに**新しい成功テスト`PlainClassPropertyReadHasNoImplicitPropertySensitivity`へ移します。
   コンパイル成功、combの暗黙トリガーが空、quiescentでの正常終了、`v == 0`、生存objectが1個であることを
   要求します。他の拒否ケースとアサーションはすべて保ちます。
2. `ClassFront.VMStillRejectsImplicitSensitivityBeforeRuntimeStorage`は、変換済みのinitial本体を
   `ProcessKind::Comb`へ変え、loaderの拒否を期待しています。この本体は今回から実行できます。
   元のソースと**全アサーションを保ち**、合成する種別だけを`ProcessKind::CombAt`へ変え、
   名前を`VMStillRejectsAtStarObjectObservationBeforeRuntimeStorage`へ移します。
   残る未対応の`@*`について、監視エラーと実行時slot・object entryが0であることを引き続き要求します。

既存suiteのソース・期待stdout・fixture・性能予算は変更しません。元の2箇所は承認まで維持します。
承認後、この移行だけを`src/`の変更と別コミットへ適用します。新しい成功テストでCTestが1件増え、
全devが成功した結果を得てから件数を公開します。

## レビュー前の確認

提案したコピーを別にコンパイル・リンクし、新規の仕組みの6テストと移した成功ケースを含む、
**ClassFront全23件が成功**しました。元のテストバイナリは22件中20件成功で、上記の2箇所だけが失敗します。
これを全件成功とは報告しません。独立した新規行列の**120ソースと生成器の検査**も成功しました。
独自の小さな4プローブは、記述した期待値でIcarus 13.0と今回の実装が成功しています。

具体的な手元の差分は`build/p1-class-implicit/expectation-migration.patch`、提案ソース・コンパイルと
リンクのコマンド・実行ログは同じ場所にあります。mainの作業ツリー、既存期待値の行、非公開の未追跡草稿は
変更していません。全体の回帰検査と件数の公開は、承認後に行います。

## 承認どおりの適用と除外なしの検証

レビューした差分を、`src/`を含まない`4e177301`へ適用しました。追跡対象のClassFrontは
**全23件成功**です。件数検査を含む除外なしのdevも、失敗・skipなしの**12,140/12,140件**です。
件数だけの`be1b7e87`は、この成功したJUnitを使い、外部測定の日付と値を保っています。
[承認後の検証記録](../measurements/2026-11-05/class-implicit-approved-validation.json)を参照してください。
class期待値の移行に承認待ちは残っていません。main反映・push・slang投稿は、それぞれの承認規則を保ちます。
