# 物理的な強さoriginとスカラ単位の慣性遅延の実行

[English](strength-driver-execution.md)が正。状態：カーネルの実装段階、2026-10-26。
ソースのMOS/スイッチ実行は残作業です。

**後の承認（2026-10-26）：** 人が別途、[管理eventのsafe point修正](managed-event-safepoints-proposal.ja.md)の
推奨案1を承認し、RUNTIME_ABI 6へ反映しました。以下の未承認という記述は以前の区切りの
状態を記録したものです。完全な公開手順と検証の実装は残作業です。

## 根拠と範囲

`f93da11b`で、承認済みの[MOS/スイッチ契約](mos-switch-proposal.ja.md)1/6/7と
[正式仕様への対応](p1-execution-contracts-2026-10-25.ja.md)のうち、独立したnodeの物理originと
時間処理を実装しました。VALUE_SEMANTICS 5、SCHEDULER_SPEC 9/12、IEEE 1800-2023の
4.9.5・10.3.3・28.7・28.16も根拠で、ここでは自分の言葉で要約しています。
実装とテストは独自で、他のシミュレータの実装やテストは読んでいません。
依存・C ABI・SIR formatの版・既存ソースの期待出力・性能予算・管理eventの回収時点は変えていません。

Contextが`StrengthDrivers`を所有します。静的なスカラnodeは、登録済みのslot/要素/bitと
同強度時のnet規則を指し、安定した物理origin IDはコピーした表の順番です。
各originは、確定した完全な区間、所有する保留区間1個、64bit世代を持ちます。
区間を射影してから作り直して保持することはせず、公開node値を物理originへ戻すこともありません。

今回解決するのは独立したnodeです。**受動edgeはなく**、nodeの読み出しをrootにすれば
接続されたtran網になる、という実装ではありません。呼び出し側が密なcomponent IDで一括公開の
単位を指定しますが、接続関係を発見するものではありません。今後のgraph構築/検証は、本当の
連結成分を求め、originと経路の相関を守る必要があります。承認済みの受動world solverの代用ではありません。

## 登録、初期化、解決

`configureStrengthDrivers`は表を一度コピーし、スカラ対象と登録metadataの対応、
slot/要素/bitの重複、区間、component IDの欠落、`uwire`の複数物理rootを検査します。
失敗した登録は再試行でき、表の一部を公開しません。component別のnode索引とnode別のorigin索引を
用意し、実行時は該当成分のnodeとoriginだけを読み、事前確保した結果/更新バッファを使います。

net種別はwire/tri・wand/triand・wor/trior・uwireです。pull/supplyのrootは呼び出し側が
明示的なoriginにして、node種別を正規化します。APIではtri0/tri1/supply0/supply1を拒否し、
解決中に暗黙rootを二重に足すことを防ぎます。通常のSIR driver slotやside maskの取り込み、
ソース宣言の検査、portのcollapseは、まだ行いません。

登録時はnetへの書き込み、event予約、callback、process生成をしません。公開の初期Xは最初の
スケジュールされた解決まで保ちます。通常originとして渡したXは残し、新規MOS originの`[0,0]`は
承認済みの初期化方針どおり未駆動で始めます。`startStrengthDrivers`は、素子評価を始める前に
初期Active処理を予約します。ソースloaderでこの順序を守る接続は必要です。

`driveStrength`はoriginを更新し、成分ごとに一度Active処理を予約します。発行processが
ReactiveでもActiveに送り、処理までの複数root変更は同じsnapshotにまとめられます。
既存の有限な点選択状態の計算とnode自身の同強度規則で解決し、全nodeの結果がそろってから
[`publishStrengths`](strength-store-publication.ja.md)へ渡します。完全な区間と射影値を全て更新してから
callbackを呼びます。callbackからのorigin変更は次のActive解決を予約し、進行中の解決へ再入したり、
確定済み通知を消したりしません。nodeとoriginの間で既存の割り込み/実時間上限のhookを確認します。

素子/タイマー処理は利用者のprocess・fork子・program起源・RNG・発行frameのポインタを持ちません。
保留eventはorigin IDと世代をコピーし、区間はContextに置きます。発行processが配信前に終了しても
更新は届きます。Contextの破棄で表を片付けます。今後ソースから作る購読は、callbackのframeを
破棄する前に明示的に解除する必要があります。

## スカラ単位の慣性遅延

`driveStrengthDelayed`は、一つの物理出力bitの捕捉済み完全区間と、正規化したrise/fall/off tickを受けます。

- 同じ保留区間は、別の遅延値で呼ばれても元の期限を保ちます。
- 異なる区間は旧世代を無効化します。確定区間への復帰は保留更新を残さず、
  本当に新しい結果は現在時刻から予約します。
- 行き先の1/0/Zでrise/fall/offを選び、Xへ射影する結果は片側のL/Hも含め最小値を使います。
  既知の値が同じで強さだけ変わる場合も、承認された同じ行き先分類です。
- 明示的なゼロ遅延はInactiveでoriginを確定し、Active解決を予約します。
  発行したActive評価の途中では公開しません。
- 配信時は保留の有無と世代を両方調べ、即時driveは既存タイマーを取り消します。
  originごとに状態を持ち、ある出力の期限変更は別の出力へ影響しません。
- 不正入力では状態を変えません。時刻の加算超過は既存schedulerのoverflowフラグを立て、
  `InvalidValue`を返し、以前の保留区間/世代を保ちます。今後のSIR呼び出し側は、通常の位置付き診断経路で
  フラグを処理する必要があります。世代の上限では巡回せず資源上限で停止します。

登録と公開での確保失敗はカーネルエラーで返します。予約での確保失敗は新origin状態を設定する前に
資源上限の停止を要求します。公開失敗時はorigin入力が新しく公開出力が旧状態の場合がありますが、
成分の一部を見せず実行を停止します。公開済みの失敗を再試行することはありません。

## 検証

独自16テストで次を確認します。

- 同強度時の規則6種で全120区間と全15強さ点を組み合わせ、uwireは単一originで全区間を調べます。
  node結果12,600件を独立した`valsem`列挙と比較します。129bit・3要素の記憶域で、
  位置が変わる区間のpacked読み出しと末尾も検査します。
- 通常X/新規素子Zの初期化、明示pull/supply、不正な静的表14種類、表のコピー、更新の集約、
  Contextの独立性。確保予算24段階で失敗/成功する登録を検査し、各Contextで公開の最初の確保を失敗させ、netの部分更新を防ぎます。
- 2nodeを全て公開してからのcallback、次の別snapshotを予約するcallback、
  完全な入力区間3種類を内部購読からMOS全6種類へ伝えるカーネル回路。
  隠れた利用者processは作りません。これはソースの変換ではありません。
- 行き先30遷移、同じ保留期限の維持、同じ値の強さ変更、確定値への復帰、即時取消し、
  独立したスカラのタイマー、明示ゼロ/Reactiveの領域カウンタ、最大時刻、公開前の割り込み。

[コンパイルした13故障](../measurements/2026-10-26/strength-driver-controls.json)は、
公開C++20 probeを含む全体ビルドの成功後に全て検出しました。コンパイル失敗は検出に数えません。
元と復元後は既存SIR dump全29件とPerfCountを含む91件が通り、登録済み704故障すべてが適用できます。
別の公開ヘッダ構文検査も、例外/RTTI無効のC++20で成功しました。

[全local CI](../measurements/2026-10-26/strength-driver-validation.json)の21確認が通りました。
dev/ASan/UBSanは5,187/5,189、releaseは5,188/5,190、各失敗0・既知スキップ2件です。
静的検査、生成件数、既存の期待値も通り、実装コミットの手動の期待値変更検査も成功しました。
macOS arm64のローカル結果で、検証後にソースは変えていません。README件数は新しい全dev JUnitから生成しています。

[通常経路の小規模測定](../measurements/2026-10-26/strength-driver-speed.json)では、releaseの
`99dafab4`と`f93da11b`を、ビルド/CIとの競合が終わってから20万反復・交互5組で比較しました。
起動時間を差し引く推定は、frame accessが0.252392458→0.257728333秒（+2.1%）、
従来eventが0.047633000→0.047149750秒（−1.0%）です。全カウンタは一致し、
peak RSS中央値は両バイナリ・両負荷で9.203125 MiBでした。
少数回の比率で、高速化や安定した悪化は断定しません。どちらも新しい表を登録しないため、
物理originの通信やソースMOSの負荷は未計測です。PERFORMANCE 4/5に従い、PerfCountは合格し、
実時間は合否ではなく報告の対象とし、閾値や予算は変えていません。nightly 7回の基準や、
外部シミュレータの比較表の再計測ではありません。

## 残作業と限界

SIRの静的な網の表、`strength.load`と`strength.drive`/delay、捕捉した実行時selector、
宣言した書き込み所有、front接続、ソースからの素子感度は残作業です。承認済みの受動solver、
条件付きedgeの時間制御、portの同一性/collapse、登録netのforce/release、`%v`、
プリミティブ配列/ソースの全行列も未実装です。通常driverのside初期化は、従来の未使用maskを変えずに
取り込む必要があります。登録metadata slotのforceは以前の公開APIで引き続き拒否し、今回も有効にはしません。

ソースでのIcarus差分縮小、新しいIcarus 13比較、P1母集団、sv-tests、Linux、最小コンパイラの結果は
主張しません。全ての大域allocator確保失敗の注入と64bit世代の到達は**未確認**で、
schedulerが使う既定allocatorへの故障注入と、上限近くの世代を設定できる専用テスト機構で確認できます。
取消し後の古いID/世代eventは、訪問されるまで通常のeventキューに残ります。
保留区間がoriginにつき1個であることは、任意に高速な差し替えで確保済みキュー項目も1個になる保証ではありません。

P1とIcarus全範囲対応は未完了です。管理eventの命令間safepoint修正案は独立した未承認事項です。

## 後続のSIR網への接続

[宣言した網の実行](strength-network-execution.ja.md)、`cea83470`と`faadf4b7`で、選択したmetadata load、
スカラの即時/遅延origin drive、正規化した静的node/origin宣言、分離した強さ感度をVMへ接続しました。
共通kernelの待機の片付けで、frame破棄前に購読を解除します。新規19テストとコンパイルした16故障で
この接続を検査しました。上の該当する残作業はこの進展で置き換わり、以前の測定はその段階の記録です。
ソース変換/通常driverの取り込み/制御の捕捉、受動graph/solver、登録netのforce/release、port、書式、
ソースの全実行は残作業です。
