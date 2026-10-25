# 宣言した強さの網をSIRとVMで実行する経路

[English](strength-network-execution.md)が正。状態：実行経路の実装段階、2026-10-26。
ソースのMOS/スイッチ対応は残作業です。

## 根拠と範囲

`cea83470`で選択したmetadataの読み出しとprocess待機の共通の寿命を実装し、`faadf4b7`で
正規化した網の宣言とSIRの追加3命令を実際のカーネルへ接続しました。
承認済みの[MOS/スイッチ契約](mos-switch-proposal.ja.md)1/6/7と
[正式仕様への対応](p1-execution-contracts-2026-10-25.ja.md)、SIR_SPEC 3/5/7/10、
VALUE_SEMANTICS 3.11/5、SCHEDULER_SPEC 9/12、IEEE 1800-2023の4.9.5・28.7・28.16が
根拠で、自分の言葉で要約しています。実装とテストは独自で、他のシミュレータの実装やテストは読んでいません。
依存・C ABI・性能予算・event回収時点は変えず、tier 1は引き続きkernelのC++ APIを直接呼びます。

既存の[物理origin表](strength-driver-execution.ja.md)に、C++の直接テストだけでなく、
検証済みのSIR宣言/命令から実行する経路ができました。SystemVerilogのfrontでMOSを有効にする変更ではありません。
表は引き続き独立nodeを解決し、受動edge・world相関・接続関係の発見は実装していません。

## 静的な宣言と所有

SIRには、kernelのC++型から独立した次のデータを追加しました。

- `StrengthNode`行は、正規化した物理slot/要素/bit、実効的な同強度時のnet規則、component ID、
  初期の公開完全区間、ソース位置を持ちます。スカラslotでも対象要素数/bit幅を明示的に1とします。
  通常のSlotオペランドは従来のスカラ構文を保ちます。
- `StrengthOrigin`行は、node、初期の完全寄与、種別、ソース位置を持ちます。
  process originは静的な連続process一つに属します。pull/supply originはprocessを持たず、
  絶対値5/7の符号付き固定点です。IDは名前ではなく、型付きの表の順番です。
- processは`strengthWrites`を通常のnet書き込みと分け、`strengthTriggers`を通常の値/辺の感度と分けて
  宣言します。既存の`kNeedsStrength`フラグが登録する物理netを示します。別名の読み出しでは、
  前にある物理slotと、その別名自身の配列宣言境界を使います。

検証器は、正の幅の4値net、全bit/要素の完全な被覆、正規化対象の重複、密なcomponent ID、
実効net種別、端点、ソースfile/行/列を検査します。origin所有者、暗黙点、uwireの物理root数、
宣言した書き込みの一意性、命令の所有/完全なオペランドも確認します。
nodeの実効種別からpull/supplyをもう一度作ることはありません。

登録netのpublisherは一つです。通常書き込み、変更可能なref、変更を行うsystem call、
変更可能なcall実引数から迂回できません。読み取り専用の値load、event制御、入力のcall引数、
明示的に対応する診断/波形callからは読み出せます。frontが使う全オペランド形式をつなぐソーステストは
なお必要で、登録netでSystemVerilogの全call形式が接続済みという意味ではありません。
登録netのhold接続までは、物理netと別名経由のforce指定を拒否します。既存ソースのforceは従来の非登録経路です。

手書きの網SIRも、VMがStore slotを作る前に全体を検証します。不正なmetadata/所有から、
網の一部を設置したり、書き換え可能なrefを公開したりしません。通常のdriverもSIRを検証しますが、
この追加確認はVMを直接呼ぶadapterのためです。網metadataのない通常設計は従来経路を使います。

## 命令と捕捉したselector

`strength.load`は、静的に特定した登録netの親bit範囲から、所有する`strength<w>`を作ります。
配列Slotの`numElems=1`は宣言順の固定ordinalを指し、`numElems=0`なら宣言次元ごとに捕捉済みの
整数添字を受けます。最後に捕捉した位置を付ければ親範囲内を選択できます。スカラには要素添字はありません。
位置を付けない結果幅は親範囲の幅で、位置を付ける場合は両側に範囲外bitがあっても構いません。

`strengthLoadShape`が検証器とVM loaderに共通の静的形式を判定します。VMは通常アクセスと同じ
全幅のindex/要素の変換経路で、捕捉したselectorをそれぞれ読みます。選択後の意味はkernelの
`readSelectedPacked`で定めます。不明・表現不能・範囲外の要素/位置はstrong Xとし、bitの一部だけが
外れる場合は内側の完全区間を保ち、足りないbitだけ`[-6,6]`にします。不正な選択をnode 0へ向けません。
成功時はpacked末尾をクリアし余分なワードを保ち、不正な静的形式では出力を触りません。

`strength.drive !dN, <strength<1>>`は、宣言した物理出力をkernelの即時drive APIへ渡します。
`strength.drive.delay`は捕捉した正規化済みrise/fall/off tickを追加し、C++から直接呼ぶ場合と同じ
スカラの慣性APIを使います。出力ごとに保留世代とコピーした区間を持ち、同じ保留期限の維持、
異なる結果の取消し、表現できない遅延の通常の位置付き実行時診断を共用します。
タイマーが発行元の利用者process/frameを保持することはありません。

strengthは言語型ではないSSA metadataのままです。slot、frame変数、定数、ref対象、引数、戻り値には置けません。
既存の支配関係、ブロック間の並行コピー、待ちをまたぐSSAの禁止を保ちます。
入れ子関数内でもloadから局所的にmetadataを消費し、通常の射影値を返せますが、
言語の関数戻り値がnetの強さを運ぶことにはなりません。

## ロード、起床、片付け

process実行前に、VMがnode宣言の初期状態から公開Store値と完全区間を構築します。
これは記憶域の初期構築で、観測可能な書き込みや早い解決ではありません。
node/origin表をコピーして登録し、素子processより先に初期Active解決を予約します。
static initializerは宣言した初期状態を、その後のActiveコードはスケジュールされた結果を見ます。
origin寄与を解決して初期公開値を作ることはしません。通常の初期X/side maskと新規素子Zを
承認済みの規則どおり宣言するfrontの接続は必要です。

`waitEventStrength`は通常値の購読と完全区間の内部購読を、kernel processの一つの待ちにまとめます。
Contextが数値process IDと待ち世代を持ち、Interpreterのframeポインタは持ちません。
どれかが一致すると一度だけ起床し、既存の待機の片付けで全ての代替項目を取り消します。
時間/名前付き待ちへの変更、取消し、kill、process終了/再利用、VM終了も同じ経路です。
準備中の確保失敗では前の待ちを保って資源上限停止を要求し、不正な強さ範囲でも前の待ちを保ちます。
既存の世代/通し番号付き購読表と、公開開始時の登録境界で行を再利用します。

VMは宣言とオペランドを運び、境界のエラーを確認します。区間の射影/選択、解決、保留状態の比較、
予約と取消しはkernelの処理です。タイマーやdirty component処理に利用者processや隠れたfork子を作りません。
POD値なので新しいGC root種別はなく、未承認のevent命令間safepoint修正も有効にしません。

## テキストと検証

formatは0のままです。writerは`strength-node !nN`・`strength-origin !dN`行、processの
感度/所有一覧、追加3命令を、存在する場合だけ決定的に出します。既存dump期待値は不変です。
テキストparserやprint/parse往復は導入せず、format 1の文法/往復は計画済みSIR v1フェーズの作業です。

独自19テストを追加しました。

- kernelの読み出し/待機7件は、親6幅、全120区間、配列3要素、親範囲2種、結果4幅、
  正常/不正な位置で、選択読み出し230,400回・区間11,750,400比較を確認します。
  余剰/末尾記憶域、通常値/強さの代替待ちの取消し、両process群、待ちの置換/kill/再利用、
  購読1行の512回再利用、前の待ちを保つ確保予算16段階も検査します。
- SIR/VMの12件は、独立した`valsem`に対する区間/素子/制御5,760設計、最大129幅の
  配列/別名/selector 7,680設計、入れ子関数8設計、通常遅延12設計と想定する広い遅延エラー1設計、
  即時/遅延での強さだけの再評価、初期Activeより前の初期状態、processなしの暗黙rootを確認します。
  妥当な手書きSIRの実行は13,465件で、想定する実行時エラー1件を含みます。
- 不正な静的表27種類、所有/命令16種類、load/待ち10種類、独立した被覆/一意性/所有4種類を確認します。
  別名forceを含む不正なVMロード32件は、Store構築前に拒否します。ソースの機能数ではありません。

既存のunit診断アサーション1個では、producer一覧に`strength.load`を加え、同じ不正producerの拒否を保ちました。
通常consumerの既存故障も、新しい判定文に合わせてコンパイルし直します。
suiteソース、dumpの期待出力、性能の期待値は変えていません。

[コンパイルした16故障](../measurements/2026-10-26/strength-network-controls.json)は、
公開C++20ヘッダーtargetを含む全体ビルド成功後に全て検出しました。kernelの選択/待ち7件、
metadata/所有4件、VM接続4件、更新した既存consumer故障1件です。コンパイル失敗は検出に数えません。
元と復元後は既存dump全29件とPerfCountを含む125件が通り、登録済み719故障すべてが適用できます。
別の公開ヘッダー構文検査も、例外/RTTI無効のC++20で通りました。

[全local CI](../measurements/2026-10-26/strength-network-validation.json)の21確認が成功し、
dev/ASan/UBSanは5,206/5,208、releaseは5,207/5,209、各失敗0・既知スキップ2件です。
静的検査、生成件数、保護する既存の期待値も通り、実装2コミットの手動期待値変更検査も成功しました。
README件数は新しい全dev JUnitから生成し、unit 591件、suiteソース4,407件です。
最終検証後にソースは変えていません。macOS arm64のローカル結果で、新しいGitHub Actions/Linux実行ではありません。

[通常経路の時間測定](../measurements/2026-10-26/strength-network-speed.json)は、全ビルド/CI終了後に
releaseの`f93da11b`と`faadf4b7`を20万反復・交互5組で比較しました。起動時間を差し引く推定は、
frame accessが0.255550791→0.256257333秒（+0.3%）、従来eventが
0.047531292→0.047827875秒（+0.6%）です。全カウンタは一致し、peak RSS中央値は
両バイナリ・両負荷で9.203125 MiBでした。少数回の比率で安定した悪化は断定しません。
どちらも網metadataを使わず、新しい実行経路とソースMOSの負荷は未計測です。
PERFORMANCE 4/5に従いPerfCountは合格し、実時間は合否ではなく報告の対象とし、予算/閾値は変えていません。
nightly 7回の基準や、外部シミュレータの比較表の再計測ではありません。

## 残る接続

ソースMOSには、front変換、通常driver/sideの取り込み、構造的データの出所、
強さだけのデータ通知で無関係な関数/RNGを再実行しないための制御式の捕捉/伝播が必要です。
今回の静的表はnode/originで、承認済みのidentity/受動edge、制御binding、graph接続の表はありません。
worldを保つ受動solver、条件付きedgeの時間処理、登録netのforce/release、portのcollapse/同一性、
`%v`、プリミティブ配列の全ソース行列、ソース期待値の移行は残作業です。
今回の接続だけでは、手書きの網IRでも`%v`へ完全区間を渡せません。
公開nodeの読み出しを物理rootへ戻して、この不足を埋めてはいけません。

大域allocator全体の故障注入、64bit世代上限の到達は**未確認**で、専用allocator/上限直前の世代を
設定できるテスト機構で調べられます。ソースの参照比較、Icarus 13との差分、P1母集団、sv-tests、
Linux、最小コンパイラは再計測していません。過去のIcarus 13合格プローブの差分21件は、
欠けている全機能の一覧ではありません。ソース差分縮小やP1/Icarus全体の完了は主張しません。
