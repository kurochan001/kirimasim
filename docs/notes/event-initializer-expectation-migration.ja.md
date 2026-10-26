# event初期化：過去の拒否期待値3件の移行

[English](event-initializer-expectation-migration.md)が正です。状態：**人が承認し、2026-10-27に適用**。
求めた3件の分類だけを実装と別に変更しました。以下は2026-10-26の元提案を判断記録として保ち、
承認の依頼や未来形はその時点の状態を示します。実装の意味はautomatic-capture契約5ですでに
承認済みです。

[適用時の入力監査](../measurements/2026-10-27/event-initializer-approved-oracles.json)で、
変更前後の検査を実行し、元の24入力がbyte単位で一致して、期待が変わるのは承認済み3件だけと
確認しました。

## 承認を求める変更

`tests/tools/check_event_input_boundaries.py`の既存24プログラムをbyte単位で保ちます。
拒否22件のうち、static eventでautomatic eventを初期化する3件だけを成功側に移し、
「終了2・位置付きの`unsupported: event used as a value`」から、
「終了0・空のstdout・error診断なし・internal errorなし」へ期待を変えます。
残り19件の拒否と、既存の集約値の基底関数2件の成功検査はそのままです。
同じ入力24件に対し、拒否19件・成功5件を検査する形になります。

3件の1行目は`module top;event a,b;`、3行目は`endmodule`です。
2行目は元の次の各行です。

```systemverilog
initial begin automatic event shared=a; end
task automatic run();automatic event shared=a;endtask initial run();
function automatic void run();automatic event shared=a;endfunction initial run();
```

初期化文を削ったり別のプログラムへ置き換えたりしません。元のループでソース文字列を作る処理を
保ち、完成した3件を成功側へ分類します。変更はこの3件の分類/アサーションと末尾の情報表示だけに
限ります。その他の期待値・dump・suite本体・unit assertion・性能予算は変えません。
`src/`と別コミットにし、人のApproved-byトレーラーを付けます。
この提案だけでは期待値の移行やpushを許可しません。

## 根拠と観測

承認済みの[automatic-capture契約5/6](automatic-capture-proposal.ja.md)は、対応する初期値の
コピーが、新変数の待機より前に同じevent objectを共有すると定めています。
根拠はIEEE 1800-2023 6.8/6.21/15.5、SIR_SPEC 7.4/8で、規格本文や例は写していません。
RUNTIME_ABI 6の既存の型付き公開/回収を使い、通常の再関連付けと比較は未対応のままです。

[変更前後の記録](../measurements/2026-10-26/event-initializer-oracle-review.json)に、元入力全文と
digestを残しました。以前のreleaseバイナリ（実行時ソースは`63c64d83`）は3件とも終了2で拒否し、
作業中の実装は3件とも終了0・空stdout・errorなしです。新規208設計と、厳格な拒否14件・
constructor数4設計の新しい検査を加え、新規CTest 210件がすべて通りました。
過去の検査は期待値をまだ変えていないため、対応した最初の初期化で失敗します。

入力・実装・故障は独自です。他のシミュレーターのソース/テスト、非公開資料、新しい依存や
インストールは使いません。この移行や有限の行列ではP1全体の同等性は示せません。
