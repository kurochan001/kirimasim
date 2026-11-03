# レビュー案：slang更新とstatic関数の戻り値の期待値

[English](slang-d02b7cda-update-proposal.md) · 2026-11-03。

## 依存更新

今回確認した上流`master`の先頭は
`d02b7cda02d3183f35c6c4d4db2b2fc03f19f4ae`で、現行pin
`e2d1021661f78b3f17d1edf7ea17f9d6f95fb168`から25コミット先です。
[上流#2072の回復処理の修正](https://github.com/MikePopoloski/slang/issues/2072)
を含みます。専用worktreeで準備し、2026-11-03に保守者が期待値移行を承認しました。
移行は別コミット`fd10bd23`で適用しました。
上流へのissue投稿・コメントは行わず、投稿保留を継続しています。

新版は未改変でビルドできました。独自の入れ子ブロック回復行列は
parse/runの324検査がすべて成功しました。保存した旧バイナリでは、選んだ
不正入力4検査でabortし、正常入力の対照2検査は成功しました。
既存の互換モード409検査と実行driverの1,728検査も成功しています。
新版の互換緩和`AllowMultiDrivenDynamicallySizedArrays`は、実行用driverの
厳密な再解析時には従来の2フラグとともに解除します。通常の構文受理は
選択したslangの互換profileに委ねる方針を維持します。

最初のdev全体検査は**11,787件中11,784件成功・3件失敗**、スキップ0件です。
この3件を成功として扱っていません。この最初の測定時点では既存の入力、
SIR期待値、初期値のアサーションは変更していませんでした。

## 人に確認していただく具体的な期待値移行

上流コミット
[`3efe5877f286203f5dd69f1d383a9e177700a218`](https://github.com/MikePopoloski/slang/commit/3efe5877f286203f5dd69f1d383a9e177700a218)
は、関数名を持つ暗黙の戻り値変数を、常にautomaticとする扱いから、
関数と同じ寿命に修正しています。IEEE 1800-2023 **6.21**はstatic関数内の
変数の既定の寿命をstaticとし、**13.4.1**は関数名を持つ暗黙の変数を
定義しています。両節を手元の正規の参照資料で確認しました。
SIR_SPEC 3.3・8節もstatic関数の変数をslotに置く方針です。
新しいSIR命令や実行時契約の追加は提案していません。

移行案は既存の次の3ファイルに限定します。

- `tests/dump_sir/subroutines.expected`：フレーム変数`addg`を
  static slot `top.addg.addg`に置き換え、後続のslot番号とwriter範囲を
  一貫して変更します。入力と戻り値の式は同じです。
- `tests/dump_sir/task_wait.expected`：フレーム変数`inc`を
  static slot `top.inc.inc`に置き換えます。入力と戻り値の式は同じです。
- `tests/cli/run_init_values_tests.py`の`formals`：`static_return`の初回だけ
  ソース既定値を検査します。2回目で`9'h152`を代入した後は、3回目の
  関数内と戻り値の両方でその値の保持を要求します。automaticの戻り値、
  output引数、各初期値方針の検査は維持します。

旧アサーションはstatic戻り値変数が3回目で初期化されることを要求しており、
static寿命と矛盾します。置き換えでは値の保持を明示的に検査し、ケース削除や
複数結果の許容はしません。具体的な差分は手元の
`build/slang-upgrade/return-lifetime-expectations.proposed.patch`に保存しています。
保守者はこの具体的な移行を承認し、`fd10bd23`で適用しました。
AGENTS.mdに従い実装変更と分離しています。

独自の実行比較では、他の処理系の結果を規格と見なさず限界も記録しました。
Icarus 13.0はscalarのstatic保持2件が失敗し、そのautomatic対照は成功しました。
配列2件はコンパイル時に拒否されました。新版pinではkirimasimのscalarと
幅の広いpacked戻り値のstatic寿命が改善します。一方、unpacked配列・構造体の
戻り値の保持は旧pin・新版pinの両方で失敗しました。kirimasimがその格納先を
明示的にフレームへ置く既存の変換制限で、別途対応が必要です。本案で修正済み・
対応済みとは扱いません。

## 承認後の受け入れ

承認された期待値移行を別の変更として適用しました。その後のビルドとCIの結果は
[依存更新](slang-d02b7cda-update.ja.md)に記録します。依存更新だけでP1完了とはしません。
