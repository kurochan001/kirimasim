# program実行の仕様修正案

[English](program-execution-proposal.md)。状態：**承認済み（2026-10-23）**。
本修正をSIR_SPEC・SCHEDULER_SPEC・SYSTEM_TASKSへ反映しました。実装は進行中です。

P1_SCOPEはprogramをP1へ前倒ししますが、不足・矛盾する詳細は実装前のレビューが
必要です。SCHEDULER_SPEC 16.1の `$exit` は全programの終了を待つ説明ですが、
IEEE 1800-2023 24.7では実行スレッドが起源を持つprogramを終了します。
この差とSIRの表現を具体化します。規格の節を手元で確認し、本文や例は転載していません。

## 承認済みの意味の修正

- 名前付きprogramのインスタンスをSIRの表で識別します。宣言・ポート・generate・
  対応済みの文は他のスコープと同じ変換と値の意味を使います。不正な項目と参照は
  slangが検査し、ソースの書き換えや診断の緩和は行いません（24.3・24.4・24.5）。
- programのinitialと連続代入はreactive側です。イベントと正の遅延からはReactive、
  #0からはRe-Inactiveへ再開し、NBAはRe-NBAへ入れます。呼び出し・再帰・forkの子も
  呼び出した側の領域を継承し、サブルーチンの宣言場所では変えません（24.3.1）。
- programのネットと接続先の駆動・解決はreactive更新に続いて行い、感度を持つ
  各プロセスは自分の側で起きます。moduleならActiveです。既存のポートと解決の
  規則を使います（24.3.2・SIR_SPEC 3.5）。
- 各program直下のinitialを数え、全initialが終わると、そのprogramのinitialから
  残るforkの子孫を既存のkill・disableの後始末で直ちに終了します。連続代入・finalは
  initialに数えません。initialを持たないprogramだけでは全体を終了しません
  （24.3・SCHEDULER_SPEC 10.7・10.8）。
- initialを持つprogramが1つでもあれば、それらのprogramが全て終了した直後に
  暗黙のfinishを行います。既存のfinishと同じくfinalを1回実行し、残るイベントは
  実行しません。別のprogramが残る間は、終了したスレッドの捕捉済みNBAなどは
  既存の後始末の規則に従います（24.3・SCHEDULER_SPEC 10.7・13・16）。
- `$exit` は実行スレッドの起源となるprogramの全initialと子孫を終了し、上の
  全体終了の規則を適用します。宣言の場所でなく、呼び出しとforkを通した起源を使います。
  programのinitialを起源に持たなければ警告せず無視します（24.7では警告は任意。
  警告しないことは本仕様の選択です）。
- anonymous programは新しいインスタンスを作りません。宣言は囲むpackage・
  コンパイル単位に属し、関数とタスクは呼び出すスレッドの側とprogramの起源を
  使います。別の未対応宣言の診断は保ちます（24.6）。
- static初期化はプロセスの開始前のままです。遅延アサーション・Postponed出力・
  final・並行アサーションは既存の領域規則を保ち、未対応のアサーションや
  クロッキング機能を暗黙に対応済みとしません。

## 表現と検証

SIRのインスタンス種別に `program` を追加します。プロセスの所属はインスタンスと
親から決め、動的なスレッドの側と起源のprogramは共通カーネルが保持して子へ継承します。
SIRのテキスト形式と検証器も新種別に対応します。VM・codegenはメタデータを渡して
操作を実行し、終了とスケジューリングは共通カーネルで決めます。公開ヘッダのC++20、
依存とグローバル状態の規則を保ち、新しい依存は追加しません。

自作の行列でmoduleとprogramの順序、#0と正の遅延、blocking・NBA、待機、宣言場所と
呼び出す側、forkと子孫、複数initial・program、明示・暗黙のfinish、別スコープと
program外の `$exit`、ポート、generate・インスタンス配列、トップ、空のprogram、
final、anonymous programのサブルーチンを確認します。既存スイートの本体と期待値は
保持します。既存の拒否期待値を変える必要がある場合は、実装より先に仕様上の根拠を
示す別コミットにします。

承認は実装を可能にするもので、実装済み・program全対応・Icarusとの全面的な同等性を
宣言しません。クラス・動的な格納・VPIなどの差はP1_SCOPEに従い別の仕組みとして進めます。
