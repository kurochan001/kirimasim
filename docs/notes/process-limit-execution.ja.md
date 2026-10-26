# 未終了のruntime processの上限

[English](process-limit-execution.md)。状態：2026-10-27、ローカルtopicで実装。
未commit・未merge・未pushで、P1は未完です。

## 承認済みの規則と実装

根拠はCLI 2.7とSCHEDULER_SPEC 10.8/16.2です。数値の上限と停止時の扱いは承認済みの
プロジェクトの方針であり、IEEEが要求する上限とはしません。IEEE 1800-2023の
9.3/9.5/9.6/24.3はprocess・fork・終了の文脈です。他シミュレータのソース・再現コード、
規格の本文・例、新しい依存・インストールは使いません。テストと故障注入は独自に作りました。

`--max-procs=<n>`は、実行時に作った未終了のprocessの上限です。既定は1000000、0で
無効にします。エラボレーションで作る静的process、programのinitial、内部callbackは
言語上のchildの枠を消費しません。子は登録された時点から、未開始・待機中でも数えます。
終了した子は、子孫やレポートのために記録を保持していてもすぐに枠を返します。
保持記録のメモリ量、静的process数、関数/taskの呼び出し深さの上限ではありません。

`Scheduler::addChildProcess`をすべての子の登録の共通入口にします。登録前にContextごとの
未終了数を比較し、超える場合は`kNone`を返し、`ResourceLimit`とCLI 2.7の診断を記録します。
拒否した登録は`proc.created`を増やしません。groupとprogramの起点の継承は変えません。
既定値は`kernel::Limits`の1か所に置き、driverはそこから既定値を取り、選んだ値を
既存のscheduler設定へ渡します。

`Scheduler::markProcessDone`が枠を一度だけ返します。通常終了、kill/disable、program終了、
final callbackの終了をこの入口に通します。記録を実際に解放する時点では未終了数を
変えません。再利用する記録はruntime childの印をリセットし、後に静的/内部processとして
使った記録が別の子の枠を返すことを防ぎます。可変の大域状態は追加しません。

VMは登録結果だけを確認します。`kNone`なら既存のfork準備の後始末を使い、まだ開始して
いない登録済みの子をkillし、準備した型付きframeの値/environmentを既存の失敗経路で後始末します。
拒否した子をschedulerへ登録しません。fork内宣言として親がすでに実行した副作用は残し、
利用者の文まで巻き戻しません。driverは時刻が進まない場合の接頭辞を付けずに上限の診断を
出し、finalを実行して終了コード6で止まります。`--max-time-ok`でも成功にはしません。

SIR命令/版、C関数表の版、依存、値の意味、性能予算は変えません。kernel公開ヘッダは
C++20のままです。将来の実行層も同じ登録・終了操作を使い、VMで別に枠を計算したり、
ソースの形で意味を変えたりしません。

## 検証

[生成器](../../tools/gen_process_limit_matrix.py)は、AIが期待値を起草した**独自90設計**を
追加します。module・static/automatic task・function・interface・program・generate、
各join、上限0/1/2と大きな値/既定値、繰り返し生成、終了した祖先の子孫、disable、
時間/named event/guard待ち、保留レポート、整数/string/queue/eventの捕捉、親の初期化の
副作用、静的processの除外を含みます。programのレポート用設計は、Postponedが処理される
前にprogramが終了しないよう、時刻1までinitialを残します。

[CLI runner](../../tests/cli/run_process_limit_tests.py)は90設計に加え、13条件を確認します。
不正な数値7条件、別の制限optionとの組み合わせ2条件、parse/SIRのみの2条件、同じoptionの
後勝ち2条件です。標準出力全体、終了コード、理由、失敗時刻、上限の診断、内部エラーがない
こととhelpへの掲載を確認します。CTestはCLI 1件と生成器1件であり、90件とは数えません。

独立した[kernel単体テスト10件](../../tests/unit/process_limit_test.cpp)は、既定値/無制限、
静的/programの除外、登録拒否、解放前の終了、繰り返したkill/end、記録再利用、保持された
祖先、待つ子、programの終了、Context分離を確認します。既存Contextのsequential・parallel・
churnにも、上限0/1/2で実行する設計を追加します。既存CLI optionテストを含む
**関連16件**が通り、追加するCTestは**12件**です。

[コンパイルした8故障](../measurements/2026-10-27/process-limit-controls.json)を検出します。
登録判定の省略、境界が1つずれること、0を無制限としないこと、静的登録の計上、終了後も
枠を返さないこと、二重終了、driverでの設定省略、VMが登録失敗を使うことです。
全故障がビルドでき、実際のunit/CLI/Contextの結果が検出します。置換箇所の照合だけの
検査は検出根拠から除外します。元のソースをbyte単位で戻し、関連16件が再度通ります。

[修正前後の5観測](../measurements/2026-10-27/process-limit-before-after.json)に、バイナリの
実際の版とSHA-256を記録します。以前のバイナリは新しいoptionを拒否し、新しいものは
指定どおりの上限で動きます。optionなしと新しい無制限指定では、5設計とも標準出力全体と
全カウンタが修正前と一致します。動作/回数の証拠であり、実時間の比較ではありません。

[最終検証記録](../measurements/2026-10-27/process-limit-validation.json)に全local CIと
更新した母集団・sv-tests・独自プローブの結果を記録します。既存の期待値・suite本体・仕様・
PerfCount予算は変えません。全local CIは**21/21項目成功**で、dev/ASan/UBSanは各
6,637/6,639、releaseは6,638/6,640、失敗0・既知skip各2件です。
PerfCount・812件の故障置換箇所・format・公開ヘッダのC++20・clang-tidyの116翻訳単位も通りました。
固定母集団は1,456/1,466（失敗0・未対応10）、native sv-testsは244/344で以前の成功を維持します。
独自87プローブのIcarus 13/14比較も差7件・参照不一致4/7件とnativeの合否/出力を維持します。
process上限は資源の方針であり、これらの言語機能の差を解消するものではありません。
公開済みの過去の測定は、元の日付とソース版の記録として残します。

## P1の残作業

仕様の未終了runtime process上限に対応した変更です。クラスオブジェクト、パラメータ付き
クラスのstatic記憶域、event制御NBA、残るcontainer/net/timing、VPI、FST、ほかのCLIの
未実装項目は残っています。速度向上、Linux実行、ホスト側CI、Icarusとの全面的な同等性は
主張しません。残作業は[README](../../README.ja.md)を参照してください。
