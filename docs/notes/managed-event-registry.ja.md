# 管理 event の同一性、型付き root とタイムスロットの状態

[English](managed-event-registry.md) が正。状態：カーネル段階を `13006ba7` にコミット、2026-10-25。
automatic event のソース実行は未有効。

**後続の区切り：** [実行時の操作所有](managed-event-runtime.ja.md)で型付きのカーネル待機・
trigger・monitorのpinを実装しました（23テスト、コンパイルした16故障を検出）。
SIR/VM/frontへの接続は残作業です。旧3期待値の移行は別途承認され`11096dc3`にコミット済みです。
以下は以前の`13006ba7`実行の記録として保持します。

## 根拠と実装境界

根拠は [承認済み automatic 捕捉契約](automatic-capture-proposal.ja.md) の5・6・8節、
[仕様との対応](p1-execution-contracts-2026-10-25.ja.md)、RUNTIME_ABI 4/6、
SIR_SPEC 3.2.1/7/8、SCHEDULER_SPEC 9.4/9.5、IEEE 1800-2023 6.8・6.21・15.5.3。
実装、テスト、世代検査は独自作成で、他シミュレータの実装やテストは読んでいない。
依存、ソース用オプション、公開 C ABI の版、SIR 形式の版は変更しない。

Context::managedEvents は世代を検査する不透明な event 値を管理する。0 は null で、
不正・古い非0トークンは妥当性の失敗とする。move 専用の実行時 pin は、ソース変数や
プロセスの祖先・PC とは独立して具体的なオブジェクトを保持する。C++ pin 操作は
管理表の同一性を確認する。数値トークンは Context 内の値であり、別 Context へ渡せない。
管理用の割り当て後に backend を確保し、作成失敗ではトークンを公開しない。
backend は割り当て失敗を返り値にし、既存スケジューラの名前付きオブジェクトを使う。
再利用時には待機・監視の参照と発火状態を消す。

ソースの型付き保持値は安全な収集点で列挙し、未公開の一時値と実行中の利用には pin が
必要である。信頼する列挙器は割り当てもソース実行もせず、同じ root が複数あっても
同じ1オブジェクトを指す。列挙失敗なら全解放を取りやめ、再入する変更・収集を拒否する。
root だけの変更でも収集可能にする。解放時に割り当てない free list で実体を再利用し、
最大世代では巻き戻さず退役させる。ソースのプロセスは増やさない。

activation store に Event フィールドの root 提供を追加した。読まれない配列要素や、
子孫が保持するスコープの値も、存続する物理フィールドから全件列挙する。捕捉ビューは
保持値の複製ではない。同じ数値を持つ通常の整数は root にせず、null と永久的な従来の
static event は管理 root を必要としない。event の root 変更フラグは heap のフラグと
分け、heap 側の確認完了で event の置換や最後のスコープ解放を見失わない。

この段階では、event の生成や管理値オペランドを SIR / VM / front へ接続していない。
通常フレーム・slot・SSA の event root と、保留トリガー・待機・callback による
pin の自動所有は残作業である。テストは既存スケジューラ API の利用時に pin を明示的に
保持しており、その API 自体が管理 pin を取ると証明したものではない。
automatic event 宣言は、読まない場合も引き続き未対応として拒否する。

## 既存 event 状態の修正

端の検査で、未発火の番兵時刻が有効な最大時刻 UINT64_MAX と重なり、一度も発火して
いない event が発火済みになる不具合を確認した。Scheduler::triggered は、明示的な
発火済みビットと現在スロットの時刻を合わせて判定し、trigger と管理実体の再利用が
そのビットを更新する。従来・管理 event の両方で、表せる全時刻に同じカーネル規則を
使う。時刻が進んで triggered が偽になることだけでは callback を呼ばない。
独自ソースの before は修正前1、修正後0となり、明示的に発火した after はどちらも1。

また、通常の関数による .triggered の読み出しまで一律に拒否していた。front は関数の
オブジェクト読みを記録し、入れ子・再帰を含む実際の暗黙の感度の呼び出し元で拒否する。
通常の呼び出しと明示的な guard は許す。明示的な guard は、関数内 static event の
物理的な依存も保持する。呼び出した関数の通常フレーム slot の感度を除外しても、
その永久的な event オブジェクトまで除外してはいけない。独自の入れ子関数による待機は
修正前には時刻7で起きずに終わり、修正後は woke 7 と total 1 を出力する。
static な暗黙 event 感度は未対応のままである。

## 検証

独自カーネル20テストは、妥当性、pin、型付き root、Context、再利用、列挙失敗、再入、
待機・監視の解放、従来との共存、Active/Reactive の遅延対象、最大時刻、未読の保持配列、
root だけの置換、管理領域の8割り当て予算と backend の失敗を確認する。
スケジューラ全体の割り当て失敗の網羅は未確認で、通常コンテナへの別の失敗注入が必要。

[ジェネレータ](../../tools/gen_event_triggered_epoch_matrix.py) は、7記述場所 × 時刻0/7/最大の
21件と、global / 関数内 static event の入れ子呼び出し経由の待機6件の独自27設計を作る。
[境界確認器](../../tests/tools/check_event_triggered_function_boundary.py) は、4種類の
暗黙の感度で直接・入れ子・再帰の12入力を位置付きで拒否する。対象49 CTest は全件合格。
実際にコンパイルした16故障を検出し、コンパイル失敗や判定器の自己検査は数えない。

[単独検査](../../tools/managed_event_generation.cpp) は、本番32ビットの全
4,294,967,295世代を1行で実行し、退役、別行・別トークンへの移行、最後の古いトークンの
拒否を確認して正常終了した。退役を無効にした別のコンパイル済み故障も全範囲を実行し、
退役数の不変条件で終了4になった。この故障が次のトークンの確認まで進んだとは主張しない。
世代の幅を縮める設定は使っていない。

固定87プローブの native / インストール済み Icarus 14 比較は、前のソース捕捉段階から
全状態と合格時の出力を維持し、過去の Icarus 13 合格・native 未成功は21件のままである。
新規27ソースは native で通るが、同じ Icarus 14.0 devel は全27件をコンパイル時に拒否し、
新規期待値の合格出力を独立に比較できたとは扱わない。根拠は規格の節と承認済み契約で、
参照の失敗を言語動作として採用しない。正確な母集団は1,451/1,466、失敗0、未対応15、
固定 sv-tests は244/344を維持した。clean 13.0、Linux、標準速度比較は新しく測っていない。

[全 local CI の記録](../measurements/2026-10-25/managed-event-validation.json) は
17確認が通り、4構成のテストは同じ旧期待値3件だけで失敗した。dev/ASan/UBSan は
4,914/4,919、release は4,915/4,920で、各2件は既知のスキップである。
ビルド、公開ヘッダの C++20、静的確認、PerfCount、既存 SIR ダンプ29件は通った。
実装コミットの期待値保護確認も通った。
[旧期待値の移行](automatic-capture-expectation-migration.ja.md) は未承認で変えておらず、
全 CI 合格とは扱わない。

[コンパイル済み故障](../measurements/2026-10-25/managed-event-negative-controls.json)、
[全世代](../measurements/2026-10-25/managed-event-generation.json)、
[87プローブ](../measurements/2026-10-25/icarus14-managed-event-probes.json)、
[参照27ソース](../measurements/2026-10-25/managed-event-reference-matrix.json)、
[母集団](../measurements/2026-10-25/managed-event-population.json)、
[sv-tests](../measurements/2026-10-25/managed-event-svtests.json) に記録する。
実行はコミット前で、記録したソースハッシュは `13006ba7` と一致する。
ここを、ユーザーが依頼した機能追加を一旦止める前の保存地点とする。
P1 と Icarus 全範囲の対応は未完了である。

## 再開するとき

1. 具体的な旧期待値移行をレビューし、承認されたら独立コミットで適用してから全CI合格を報告する。
2. event.new、読み出した event 値による wait / trigger / triggered を SIR の表示・検証と
   tier 1 へ接続する。従来 slot 形式、厳格な診断、承認済み null の挙動、対象と遅延の1度の
   評価を保持する。
3. 通常・捕捉・SSA・input の event root を公開し、安全な収集点にカーネルスコープ全件を
   登録する。未公開の値は公開まで pin する。
4. 保留トリガーと待機・guard・callback が具体的な event pin を所有するようにし、
   巻き戻し、実行、取り消し、program 終了も扱う。NBA/Re-NBA を保ち、発行者の実行フレームを
   余分に保持しない。
5. automatic event の全ソース行列と故障検査を追加する。再束縛・比較、event を含む動的
   コンテナ、公開 VPI event 値はこの修正契約の外であり、別の契約が必要。
