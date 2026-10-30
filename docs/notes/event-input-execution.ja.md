# ソースのevent入力引数と実行時選択による入力部分配列

[English](event-input-execution.md)。状態：2026-10-26、`63c64d83`をmainに取り込み、ローカル検証済みです。
P1全体は未完了です。[コミット後の検証](../measurements/2026-10-26/event-input-committed-validation.json)で
コミットした全テストのlocal CI 21項目が通りました。下の作業ツリーの記録は取り込み前の
日付付き証拠として残し、現在のブランチ/公開状態を示すものではありません。

## 契約と実装

根拠は承認済みの[automatic capture契約3/5/6](automatic-capture-proposal.ja.md)、
[event公開時点の改訂](managed-event-safepoints-proposal.ja.md)、SIR_SPEC 7.4/8、
RUNTIME_ABI 6、IEEE 1800-2023 6.21/13.5/15.5です。実装・ソース検査・故障は独自に
作成し、他のシミュレーターのソースやテストは読んでいません。依存・インストール・公開API・
SIR命令/版・ABI版・オプション・既存期待値の移行は追加していません。

`Lowerer::lowerInput`をevent入力の専用入口としました。名前付きeventと固定配列の要素は
`eventOperand`と既存の型付きplace/index規則で読みます。既定の入力引数を含む`null`は
event型のゼロになり、対応する型変換でも同じevent値を保ちます。仮引数は実引数のobject・
待ち行列・triggered状態を共有し、コピーのための`event.new`は出しません。
`frameVarOf`はautomatic eventの**入力**仮引数を許可し、output/inoutの境界を保ちます。
通常のscalar入力の変換経路と診断位置は従来どおりです。

`lowerCallTo`はscalar event入力をこの入口へ渡します。固定配列は型付きの要素ごとの
転送を使います。遅延アサーションのアクションは評価時点でevent入力をsnapshotし、
生成したアクションは既存の置換表から読みます。呼び出し元のスコープ終了後に元の式を
再評価しません。event以外の遅延snapshotは変更していません。

VMとkernelは変更していません。既存の型付きfield、入力転送pin、保持したlexical extent、
保留操作、event SSA rootにより、同期呼び出し・待機task・再帰・fork子孫・遅延callbackの
所有権を保ちます。既存の転送をソースから使えるようにした変更であり、VMにソースの
実行場所を判定する分岐は追加していません。

隣接する配列入力の検査で、実行時prefixの既存の未対応を見つけました。
`arrayArgument`は、定数添字の簡潔な経路で表せない入力部分配列に`storageOf`と
`copyArrayStorage`を使います。添字を一度だけ評価し、残る次元は両側それぞれの左端から
コピーします。`arrayTarget`は基底を変換する**前に**添字が定数かを調べ、実行時prefixを
見つけた際に副作用を持つ基底を二度評価しないようにしました。event・整数・real・stringに
共通するfrontの処理です。

## 検証

[ジェネレータ](../../tools/gen_event_input_matrix.py)は新規104設計を作ります。

- scalar 44件：module/program、automatic/staticのfunction/task、slot/frame実引数、
  階層名、定数/実行時の添字。
- 配列全体36件：automatic/static仮引数、slot/frame実引数、昇順/降順/多次元、
  同期/待機呼び出し。
- 保持した子孫12件：module/program、名前付き/混合/triggered待機、
  blocking/遅延nonblocking trigger。
- null/既定null、回収を伴う再帰、遅延アクション2件、実行時選択の部分配列4件、
  整数・4値・real・stringの隣接要素型4件。

[境界検査](../../tests/tools/check_event_input_boundaries.py)は22件で、位置付きの終了2・
空のstdout・internal errorがないことを要求します。3か所での通常の代入/比較/初期化別名、
automatic scalar/配列のoutput/inout、null待機、直接/入れ子の入力呼び出しを通るtriggeredの
暗黙の感度を含みます。

コンパイルした4故障は、値をnullへ変更・実引数の二重評価・nullでobjectを生成・
遅延snapshotの無視です。すべて実行検査で検出し、anchor照合だけで合格扱いにしません。
ソースをbyte単位で復元した後、関連CTest 106件がすべて通ります。
[故障の記録](../measurements/2026-10-26/event-input-controls.json)にソースのdigestと
検出テストを残します。追加の2件では、部分配列の基底を返す関数を、定数/実行時prefixの
どちらでも一度だけ呼ぶことを検査します。

[全local CI](../measurements/2026-10-26/event-input-validation.json)は21/21項目が通りました。
dev/ASan/UBSanは5,849本中5,847成功、releaseは5,850本中5,848成功で、
失敗0・同じ既知skip 2件です。PerfCount、mutant anchor 795件、整形、clang-tidy 115翻訳単位、
方針検査が通りました。最後にテストだけを強化した基底関数2件を追加し、全4presetの関連CTest
106件が改めて通りました。実行時ソースは変更していません。

## 制限と別の観測

入力の共有を追加した変更です。通常のevent再関連付け・merge・比較・初期化子の別名・
eventを返す式・event値の条件式・event pattern・automatic structのevent field・
eventを含む動的コンテナは追加していません。output/inoutによる再関連付けの新しい意味も
決めていません。null triggerはno-op、null待機は承認済みの実行時エラーを維持します。
保守的なevent SSAの寿命と、final/heapの回収境界も保ちます。

real/stringの隣接検査では通常の等値比較を使います。別の独自probeでは、Icarus 13は
real/stringの`===`/`!==`を拒否し、固定版slangは受理する一方、native変換が出す
`ceq`/`cne`はSIR verifierに拒否されました。ソース言語上の扱いは**未確認**です。
IEEE 1800-2023 11.4.5とslangの型規則を確認してから、診断か変換の修正を選びます。
今回は両演算子と期待値を変更せず、推測の意味を有効にしません。

固定P1母集団は再実行して1,466本中1,456成功・失敗0・未対応10を維持しました。
nativeのsv-testsは344本中244成功・未対応62で、以前の成功の後退はありません。
Icarus 13/14との比較は独自87 probeを全件再実行し、差分7件・参照不一致4/7件を維持し、
nativeのstdout変化と以前の成功の後退もありません。
[母集団](../measurements/2026-10-26/event-input-population.txt)、
[sv-tests](../measurements/2026-10-26/event-input-svtests.json)、
[Icarus 13](../measurements/2026-10-26/icarus13-event-input-probes.json)、
[Icarus 14](../measurements/2026-10-26/icarus14-event-input-probes.json)の記録を残します。
有限の母集団では完全な同等性を示せません。hosted CIやLinux比較の再実行は主張しません。
新たなwall-clock計測や速度基準の変更も主張しません。PerfCountの予算は変更していません。
