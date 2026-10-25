# 既存の回収境界で型付きSIR eventを実行する

[English](event-vm-boundaries.md)が正。状態：境界で動く部品を実装・検証済み、2026-10-26。ソースのautomatic eventはまだ有効にしていません。

## 根拠と実装

根拠は承認済みの[automatic捕捉契約](automatic-capture-proposal.ja.md)5/6/7、
SIR_SPEC 3.2.1/7/8、RUNTIME_ABI 4/6とIEEE 1800-2023 6.8・15.5です。
実装・テスト設計・追加したevent計時入力は独自です。外部の実装・テストは読まず、
依存・C関数表のABI・性能の基準・既存期待値も変えていません。

`7e188c56`で`event.new`、loadした値と旧slot対象、名前付き/混合待機、
即時/非ブロッキングtrigger、`.triggered`を既存の型付きカーネル操作へ接続しました。
生成pinはSSA値の公開まで保持します。同一性・null・保留操作の所有はカーネルが決め、
Interpreterは値を渡し、エラー種別を既存の終了理由へ対応させます。
nullのtriggerは何も予約せず、null待機は位置付き設計エラーです。
非nullの不正な同一性は内部エラーのままです。正規化した遅延をoptionalで受ける追加形式により、
カーネルが表現不能な遅延より先にnullを処理し、非nullの従来のoverflow動作を保ちます。

Interpreterは型付きslot・通常frame/配列行・保持されたscoped/captured領域・
対象の入力/遅延引数を公開します。既存のactivation参照グラフをたどり、
保留write snapshotは宛先slotの型で選別します。実行時の待機・保留trigger・monitorは
具体的対象を保持します。整数やrefの符号化をeventと推測しません。
列挙callbackは確保もソース実行もしません。

回収は**既存の**region/slot境界だけで、native process・同期呼出し・guard評価の外で行います。
SSAはwaitを越えられません。container heapや別のheap変更がなくてもeventを回収しますが、
heapの回収間隔と終了/final時の制限は仕様どおりです。終了時は元のhookを復元し、
残るVM callbackを解除します。event宛先情報は最初の`event.new`で準備します。

明示的な`.triggered`のmonitorでは、呼出した関数を含め、実際に読んだ対象をカーネルが捕捉します。
変数/indexへの依存は保ち、静的に推測した対象一覧は除きました。
再評価で対象依存を置き換え、配列内の全対象を永続的に追加しません。
frontが現在拒否している暗黙の名前付きevent感度を有効にする変更ではありません。

## 実行の証拠

独自の`VmEvent`11テストで手組みSIRの35設計を検査します。別々の新規SSA対象、
恒久配列からloadした選択値、null操作、slot/通常/scopedのスカラー/配列9組、
Active/Reactiveの遅延対象と名前付き/混合待機、対象依存の交換、入力/戻り値の共有、
未読配列の保持、Interpreter終了後のtrigger、resource/hookの後処理を含みます。
別のカーネル検査で表現不能な遅延を確認し、さらに別の検査で待機項目1/4/5件の
inline/overflow記憶域、置き換え失敗、不正対象を確かめます。
これはSIR/カーネルの実行検査で、ソース変換やevent言語契約全体の証明ではありません。

境界での再利用検査は128対象を作り、毎回1tick待って、container変更なしに物理行1個を再利用します。
未読配列の検査は、別の捕捉フィールドだけを読む間も全3対象が生存することを観測します。
program完了の終了動作も保ち、禁止されたVM回収点で最終回収を強制しません。

[最終の故障検出記録](../measurements/2026-10-26/event-vm-controls.json)は、移設3件を含む
13故障を検出し、全件ビルド成功後にテストが失敗したことを残します。復元後の対象76件も通りました。
[全local CI](../measurements/2026-10-26/event-vm-validation.json)の21確認はすべて成功し、
dev/ASan/UBSanは4,967/4,969、releaseは4,968/4,970、失敗0・各既知スキップ2件です。
既存dump全29件とPerfCountも通り、このVM段階では既存期待値を変更していません。
README件数は新しいdev JUnitから生成しました。当初の確認で足りなかった統合検査も見えるよう、
[当初の故障検出](../measurements/2026-10-26/event-vm-initial-controls.json)と
[中断したCI](../measurements/2026-10-26/event-vm-initial-ci.json)を保持しています。

新しい[87プローブ比較](../measurements/2026-10-26/icarus14-event-vm-probes.json)は、
native/Icarus 14の全判定と成功時の出力byteを維持しました。過去のIcarus 13合格との差分は
21件で、13の新規実行はありません。母集団・sv-tests・Linuxも再計測せず、全体の同等性は未確認です。

## 統合時の修正とコスト

`7e188c56`での最初の全CIは、既存の
`VmContainer.HeapMonitorFailuresHavePositionsAndWaitHooksNeverOutliveTheInterpreter`が
devとreleaseで失敗し、全合格とせず中断しました。従来の位置付きheap monitor診断より先に
型付き待機の一般エラーを報告していました。`39aa6aaa`でそのテストを変えずに報告順を戻し、
悪い順序を再現するコンパイル対象の故障も追加しました。

重点計測の交互5組で、当初の名前付きevent往復が約27%遅くなっていました。
カーネルは現在、解決した待機項目を4件までinlineで保持し、管理対象がある場合だけpin記憶域を
確保します。大きい一覧には失敗を返せるoverflow経路を保ちます。
検証と不可分な登録には同じカーネル操作を使います。予算0での検査は管理表の一時確保だけを
対象とし、スケジューラの他の全containerの確保まで覆ったとは扱いません。

全CI後の[最終コスト観測](../measurements/2026-10-26/event-vm-speed.json)は、cleanなrelease binaryを
20万回・交互5組で比べました。起動分を引いた通常frameアクセスは0.25967→0.26094秒（約+0.5%）、
名前付きevent往復は0.04154→0.04840秒（約**+16.5%**）です。全カウンタは同じで、
peak RSS中央値は両入力・両binaryとも9.203125 MiBでした。
[当初の計時](../measurements/2026-10-26/event-vm-speed-initial.json)にはinline化前の退行を保持しました。
残るeventコストは性能上の課題です。この小規模観測はPERFORMANCE 5.1の7夜baseline比較や
シミュレーション速度全体の再計測ではなく、通過条件やしきい値も変えていません。

## 残作業と人の判断

frontはautomaticの既定生成をまだ拒否します。[移行ノート](event-field-expectation-migration.ja.md)の
承認済み項目3に当たるソースメタデータ入力は変更・適用していません。
ソースの生成/コピー/index操作、入れ子callback中の未公開な受け渡し、SSA全体の公開、
独自ソース行列全体は残作業です。rootの受け渡し処理に対応caseがあるだけでは、
全経路を検証済みとは扱いません。
event固有の遅延report引数と一般の保留write snapshotは、個別設計による確認が**未確認**です。
ソースを有効にする前に、保持/破棄したholderと配送・キャンセル・失敗の組み合わせで確認します。

[管理eventのsafepoint修正案](managed-event-safepoints-proposal.ja.md)は**未承認**で、
命令間の回収は実装していません。境界での再利用検査は、待機なしループ・再帰・
長い関数実行の確保量が有界になる証明ではありません。完全な公開手順と人の判断が必要です。
native実行中のcontainer回収は別の課題で、この作業では有効にしていません。

Icarus全範囲の対応とP1は未完了です。有限のプローブで言語全体の完了条件は証明できません。
