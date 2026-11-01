# 捕捉した強度の表示と登録netのforce/release

[English](strength-format-execution.md)。2026-11-01。

2026-10-25承認の[MOS/switch契約](mos-switch-proposal.ja.md)5・7・8節を接続します。
P1全体の同等性を示すものではありません。
[既存期待値の移行](strength-format-expectations.ja.md)は別レビューです。

## 対応する動作

`%v`は、対応する4値の整数net参照について、捕捉した符号付き区間をそのまま使います。
packed選択、固定配列の要素、物理aliasも対象です。net以外の整数式はStrongの表示です。
[正式な表示規則](../spec/SYSTEM_TASKS.ja.md#310-v)に従い、点、複数強度の既知値、
L/H、X、HiZを表示します。ベクトルは上位bitから`_`で結び、幅と左右寄せは全体に適用します。

リテラル、実行時string/packed書式、display/writeの各形、文字列整形、ファイル出力、
strobe、monitorが同じ表示処理を使います。実行時書式は、現在`%b`や`%p`を選んでいても、
net参照の各実引数の由来を保持します。元の値・符号・patternの型は別に保持し、
`%b`・`%p`の値を強度情報で置き換えません。選択添字は一度だけ評価し、次の引数へ進む前に
値と区間を同時点で捕捉します。明示castや計算式は通常の値の経路を使います。

固定版slangはリテラルの`%0v`や他の`%v`幅指定を変換前に拒否します。
実行時書式では承認済みの全体への幅指定に対応します。このフロントエンドの制限は維持し、
成功やskipにしません。入力scanの`%v`、未対応net領域、他の書式の制限は変わりません。

登録した強度netのforce/releaseは、既存のbitごとの手続き所有者からネイティブの
node単位APIへ接続します。forceはStrongのrank 6で、局所rootを抑制し、遠隔rootの通過を
保ちます。Zのforceはそのnode自身の読み値だけを固定します。releaseは現在の下層rootから
同期的に解決します。所有者の重なり、alias、部分対象は従来の選択規則に従います。
既存のポート同一性/collapseの受理範囲や、VPIのインターフェースは広げません。

## 表現と寿命

`StrengthLoad`は通常loadと同じ型付き選択先から区間を捕捉し、不正な選択にはStrong Xを
補います。strength SSA値はframeや通常slotに保存できません。内部の型付き呼出し
`$strength.format`は所有するStringを返します。net由来のある実行時引数は
`$sformatf.runtime.strength`で、patternの型、元値/空マーカー、名前付き/位置順の文字列、
強度/空マーカーの5フィールドを使います。従来の4フィールドの内部呼出しも維持し、net由来が
ない引数に使います。VM構築前に検証器が形と幅を検査します。SIR opcodeや公開C ABIは変えません。

Postponedのmonitor評価では、選択して読んだ強度範囲を記録し、捕捉後に購読を置き換えます。
区間だけの変化は該当monitorの更新を予約し、通常の信号イベントは起こしません。
置換、detach、ファイルcloseでは、コールバックframeが消える前に購読を解放します。
強度情報はContextが所有し、名前による検索やGC rootを追加しません。

## 再実行する確認

- `tools/gen_strength_format_matrix.py --output build/strength-format-inputs`で、
  独自のソースプログラムと正確な文字列の期待値をレビュー用に出力できます。
- `ctest --preset dev -R '^(StrengthFormat|StrengthDisplay|StrengthNetworkVm.InactiveForce|cli/strength_format)' --output-on-failure`で、
  表示、ソースの文脈/API、型付き情報、寿命を検査します。
- `tests/unit/strength_format_test.cpp`は、120区間のリテラル表、全体への幅指定、
  1/8/9/65/129 bitの格納境界を検査します。
- `tests/unit/strength_display_test.cpp`は、選択購読、Postponedの実行、置換/detach、
  bitごとの所有者と同期releaseを検査します。既存の物理forceの失敗・rollback検査も適用します。

[最終検証](strength-format-validation.ja.md)に、dev全11,364件の成功、他の各構成の関連63件、
コンパイルした8故障の検出を記録しました。今回の行列全体について外部参照との同等性は未確認です。ネイティブの期待値は承認済み契約に従い、シミュレータの実装ソースから
意味を推測したものではありません。
