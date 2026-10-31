# 名前付き fork の disable の実行

[English](named-fork-disable-execution.md)

IEEE 1800-2023 9.3.2/9.6.2、SCHEDULER_SPEC 10.4/10.7/10.8、SIR_SPEC 7.2/7.5 に従い、
disable されたブロックが始めた活動をすべて終了します。join_any/join_none の後も子が残り、
終了済みの子の先で孫が動くため、親の現在の実行位置だけでは識別できません。

`Lowerer::lowerFork` は対象の名前付き fork を通常の数値 scope.enter/scope.leave と継続先へ変換します。
宣言は子の開始前に親で初期化し、空の fork も同じ出口を通します。`lowerDisable` は名前付きの
statement block に既存のスコープ番号を使い、sequential と parallel を同じ命令へ接続します。

SIR の fork ごとに、子へ生成時の親の数値スコープ活動を記録します。10.8 の保持する親子リンクで
その先の子孫にも届き、親の scope.leave や呼び出しからの return の後も記録を保ちます。
プロセス記録の解放時に消します。disable は既存のスケジューラを通じて待機と子孫を終了し、
スコープの前や後で始めた無関係な子には触れません。活動中の親は既存の継続先へ巻き戻し、
中断した呼び出しの出力をコピーしません。別の宣言は別番号、同じ宣言の複数の活動は一緒に終了します。

function 内で宣言したブロックの活動は、fork から戻った後もその function から終了できます。
所有関係は SIR 本体の数値 scope.enter で確認し、呼び出し元など外のブロックを function から
終了する形は実行時エラーを保ちます。終了対象の未確定 deferred assertion は既存のカーネル API で
破棄し、確定済みの報告と予約済み NBA は既存の意味を保ちます。class のメソッドや reactive の
program も同じ仕組みを通します。

生成器は独自297ソースです。9配置、3つのjoin、自分/外部/終了済み親からの終了、時刻/event/task待ち、
開始前の終了、automatic な fork 内宣言の捕捉、再入と繰り返し、別スコープ、function の所有、
報告・NBA・記録の再利用を調べます。object と event の組み合わせは既存の未接続の境界を保ちます。
別の検査は4回と80回で生成プロセス数と保持数のピークを比べます。新規変異9個を用意し、
既存の空 fork の変異も同じ欠落を起こすまま共通の終了処理へアンカーを移しました。

保守者は元の disable_fork_block の入力を終了0・stdoutなし・エラー診断なしへ分類する変更を承認しました。
未対応診断6行を移行した期待値だけの `e543394e` で、元ソースと他の拒否入力は保っています。
実装・測定・件数は目的ごとに分けます。P1 の完了、always_ff の式の辺、specify、VPI の対応は
この段階で示しません。[property target の棚卸し](class-property-target-inventory.ja.md)は別に記録しました。
