# ソースから共有する字句的 automatic 記憶域へ変換する

[English](automatic-front-capture.md) が正。状態：ソース実装を `ebb0e14d` にコミット、2026-10-25。
その後、過去の期待値3件は別途承認後に `11096dc3` で移行しました。
現在の [main `304e1aec`](automatic-event-main-integration.ja.md) は automatic scalar/固定配列eventの
生成も含みます。以下の未対応・レビュー待ち・失敗数は `ebb0e14d` 時点の記録です。

## 契約と実装

[承認済み automatic 捕捉契約](automatic-capture-proposal.ja.md) の1〜4・6〜8節を
ソースから接続する。[承認記録](p1-execution-contracts-2026-10-25.ja.md) での対応先は
SIR_SPEC 7/8 と RUNTIME_ABI 4/6、言語上の根拠は IEEE 1800-2023 6.21、9.3.2、
13.5.2 である。実装とテストは独自作成で、他シミュレータの実装・テストコードは
読んでいない。依存、実行時スイッチ、別 IR、公開 ABI は追加しない。

Lowerer は字句スコープの出入りとソース変数の所属を記録する。実際に子を作る fork
に達すると promoteLexical が現在の automatic スコープの鎖全体を昇格し、同じ
スコープの後続宣言も ownFrame が所属させる。finishLexical は記録済みの位置に
出入り命令を挿入するため、fork 発見前に変換した初期化の前にも正しく配置する。
通常の本体と宣言だけの fork に昇格記憶域は不要である。コンパイラの一時値は通常の
フレームに残し、拡張するスコープのソース変数は全件、宣言の型と配置を保つ。
static 宣言は従来の slot を使う。

captureForkEnvironment は親の行順に全フィールドを取り込み、読まない型付き root
と空の字句スコープの寿命も保持する。孫は継承した寿命と不変の元の binding を転送する。
ref は子またはその子孫が使うときに取り込み、未使用の通常 ref 仮引数を不正な非同期
捕捉に変えない。const / ref static の修飾を保持する。通常 ref は join 内、ref static
は static 実引数に制限する厳格な slang 診断は残す。fork 宣言の値初期化で通常 ref
からコピーし、その値を子が捕捉する形は許される。

通常のブロック・fork の出口、break / continue / disable / return は同じ字句的な
所有の記録を使う。ループの制御変数は共有する一方、本体や fork のローカル宣言は
入るたびに別の記憶域を得る。戻り値の式はスコープを抜ける前に評価する。
[Interpreter の接続](automatic-vm-execution.ja.md) がコピーイン前の root、1度だけの
コピー戻し、子グループ全体の登録、callback の pin、解放、typed root を提供する。
意味を front の記録から渡し、VM に同じ判断を作り直さない。

## ソースと故障の確認

[新しい生成器](../../tools/gen_automatic_capture_matrix.py) は独自186設計を作る。
6記述場所 × 3 join × 2/4値 × 幅1/7/65/129 の144件、関数の戻り8件、型付き24件、
寿命・ref・制御の10件である。real、shortreal、string、null chandle、広幅固定配列、
構造体、queue、動的配列、親子・兄弟の共有、別呼び出し、ループ再入場、break / continue、
fork 初期化のスナップショット、孫への転送、output コピー戻し、通常 ref の join、
ref static、guard / コンテナ変数の置換、名前付き disable、入れ子からの return を含む。

[メタデータ確認器](../../tests/tools/check_automatic_capture_metadata.py) は読まれない
型付きフィールド全件と空のスコープを2世代にわたり検査し、元の所有者が戻った後に
孫を実行する。通常 ref の join_any / join_none、ref static への automatic 実引数、
const ref static への書き込み、読まない automatic event の5境界は引き続き拒否する。
本来の実装に戻した188確認はすべて通った。空のスコープ省略、未読フィールド省略、
継承寿命喪失、元の捕捉 binding 喪失、字句スコープへの入場省略、という5故障を実際に
コンパイルして検出した。コンパイル失敗は検出として数えない。

## 有限の比較

インストール済み Icarus 14.0 devel（s20260301-411-gca6b8d376）との独自87プローブを
再実行した。native の automatic_fork_capture だけが run_failed から passed に変わり、
ほかの状態と合格時の出力は変わらない。過去の clean 13.0 の記録に対する未成功差分は
22件から21件へ減った。新しい13.0の測定でも、全機能の同等性の証明でもない。

新規186設計を同じ Icarus でも実行し、16件は合格、132件はコンパイル非0終了、
38件は実行非0終了だった。正常終了して出力だけが違うケースはこの測定ではない。
失敗時の出力と診断は公開データに入れない。この参照側の失敗は、新規期待値全件の
独立した裏付けにはならない。根拠は規格の節と承認済み契約であり、参照の失敗を
言語の動作として採用しない。

正確な版を archive した P1 母集団1,466件は、合格1,451、失敗0、未対応15を維持した。
固定版 sv-tests 344件は合格244を維持した。外部 checkout の現在の版は使っていない。
Linux と標準の速度比較は、この段階では測り直していない。

## この区切りでのレビュー待ちと残作業

[別の期待値移行案](automatic-capture-expectation-migration.ja.md) は既存入力を残し、
旧未対応3テストを正常出力・終了の期待値に移す提案である。未承認で未適用。
最初の dev 全件ではこの3件と古い README 件数が失敗し、件数の再生成後はこの3件だけが
失敗している。[全 local CI の記録](../measurements/2026-10-25/front-capture-validation.json) は
17確認が通り、テストの4段階が失敗した。dev / ASan / UBSan は4,865/4,870、
release は4,866/4,871で、各構成で同じ3失敗・既知の2スキップである。
ビルド、静的確認、PerfCount、既存SIRダンプ29件は通った。コミット後の期待値保護確認も
通った。全 local CI が通ったとは記載しない。

機械可読の記録に [故障検査](../measurements/2026-10-25/front-capture-negative-controls.json)、
[87プローブ](../measurements/2026-10-25/icarus14-front-capture-probes.json)、
[新規ソースの参照比較](../measurements/2026-10-25/front-capture-reference-matrix.json)、
[母集団](../measurements/2026-10-25/front-capture-population.json)、
[sv-tests](../measurements/2026-10-25/front-capture-svtests.json) を残す。
実行はコミット前で、記録したソースハッシュは `ebb0e14d` と一致する。

automatic event の構築と所有 event / ref / object の提供は未対応。chandle は null
だけを確認しており、非 null のソースからの転送は未確認で、許される値の生成元を使う
独自確認が必要である。ソース実行全体への割り当て失敗注入、SSA 全体の ref 来歴、
記述子の全置換・世代経路も残る。固定プローブの過去 Icarus 13.0 合格・native 未成功は
21件残り、このプローブは言語全体を網羅しない。P1 は未完了である。
