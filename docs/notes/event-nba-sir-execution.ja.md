# 型付きイベントNBAのSIRと一時的なVM callback

[English](event-nba-sir-execution.md)。状態：検証済みの独自SIR用の区切り`6f43e387`をmainへ取り込み、2026-10-27。
main `7cdd2a2e`が基です。ソースからのイベント制御付きNBAは引き続き未対応で、
P1の合格や、言語全体の同等性を示すものではありません。

## 根拠と境界

承認済みの[イベントNBAの実行契約](event-nba-proposal.ja.md)と
[仕様への対応](p1-execution-contracts-2026-10-25.ja.md)に従い、
[kernelの式monitor](event-nba-expression-monitors.ja.md)の後を実装します。
関係する節はSIR_SPEC 5.3/7.2/7.3/8/10、SCHEDULER_SPEC 8/9.5/10.8/13、
RUNTIME_ABI 5/6、VALUE_SEMANTICS 3.11/3.13です。背景となるIEEE 1800-2023の節は
9.4.2・9.4.5・10.4.2・13.4.4・15.5・18.14です。規格の本文・例、他のシミュレータの
ソース・テスト・再現コードは写していません。依存や詳細仕様の変更は追加しません。

frontは引き続きイベント制御付きNBAを拒否します。既存の未対応診断、ソースのテスト本体、
dumpの期待値、PerfCountの予算は維持します。新規の検証は独自SIRを直接組み立てるもので、
以下の転送境界を確かめます。ソースの評価順序や機能範囲の証拠には数えません。

## 1命令と不変の型付き配置表

`Op::NbaEvent`は終端でも値を返す命令でもありません。オペランドは`!nN`、捕捉した整数の
回数、捕捉した値です。Designの`NbaLayout`が値の型、順序付きのイベント項目と書き込み先を
持ちます。`NbaValueId`は捕捉値の順番で、SSA一時値の番号ではありません。
命令ごとのvectorや書き換え可能な実行状態は追加しません。

イベント項目は固定slot範囲、捕捉した名前付きevent値、式を返す関数を表し、1ビットのiff関数も
付けられます。書き込み先は固定要素・範囲、動的に選ぶ固定配列要素、親の範囲内のpacked選択、
static変数へのrefとそのbit選択です。複数の葉や有効な書き込み先が0件の場合も1つの制御を保ちます。
捕捉値は表の型と完全に一致するSSA値か定数です。

verifierは未使用の表も含め、ID、回数と捕捉値の形・値域、callbackの役割、書き込み先の適格性、
添字の数、refの葉の型、processのwrite集合を検査します。`sir::validSlotRange`を既存の検証と
共有し、`sir::nbaWrites`が保守的な静的write範囲を返します。refの実際の参照先は捕捉時に調べます。
不正な参照先は値を公開する前に設計エラーとします。範囲外の葉の診断が抜けて`internal`となる
初期不具合も検出し、現在の回帰検証は`error`とイベントNBAの診断を要求します。

テキストは`nba.layout !nN captures=(...)`と`nba.event !nN, ...`で、項目・葉の順番、
添字の捕捉番号、型を残します。表が空なら何も出さず、古いdumpの文字列を維持します。
`formatVersion`は**0**のままです。SIRパーサは追加せず、print/parse/printの保証は主張しません。
文法v1とその保証はP2で扱い、P1では今後も命令を追加できます。

## 共通kernelへの転送と値の所有

VMは既存の添字とpacked範囲の演算を使って、型付きの選択を解決します。不正な選択はその書き込み
だけを省き、制御の評価は省きません。範囲の警告処理も既存の読み書きと共有し、
`--bounds-check=warn`は範囲外・一部範囲外の書き込みを数え、同じソース位置の診断は1回にします。
警告によってsnapshotや範囲内の値を変えません。

登録前に参照先、値の語、書き込み先ごとに独立したコンテナのコピーを捕捉します。kernelが
生存期間、回数、通知のまとめ方、ready化、保留中の発行順を持ちます。frontは別途、回数やLHSの
副作用より前にRHSをsnapshotする必要があります。転送時のコピーで、そのソース評価順序を
代用することはできません。葉の全体を1つのpayloadにまとめます。

既存の全幅の回数の判定を、純粋関数`kernel::classifyNbaRepeat`へ取り出し、登録とVMで共有します。
0・負数・X/Zの回数はcallbackの準備や初期評価をせず、通常のNBAとしてenqueueします。
VMは回数を減らさず、子プロセスや再開PCも作りません。公開kernelヘッダはC++20、C ABIは0.3を維持します。

## 発行元の文脈で実行する新しいcallback領域

callbackは引数0個・待機なしのautomatic関数です。guardは整数1ビット、watchは元の幅と値域を
保った整数を返します。自分のframeや字句スコープを持てますが、借用するframe owner、
外側のcapture行、継承する字句領域は持てません。待機元frameを使うGuard/Watchとは別に、
通常のSIR関数として表します。

通常待機のcallback vectorとは別の所有物に、安定した呼び出し情報を置きます。各呼び出しで
`callWith`が新しい作業変数を登録し、正常時も失敗時も通常の後片付けを行います。
保持した発行元frameのRNG、ソースと報告の識別情報、領域を使います。RNGをコピー・初期化したり、
発火したprocessの呼び出しとして扱ったりしません。初期callbackの前に完全なkernel登録へ
後片付けを移し、登録中に同期的に完了しても二重に解放しません。

内部のwrapperはソースの呼び出し深さやstacktraceのframeに数えません。別processのtask内から
発火する検証で、VMの現在の呼び出し深さが発火側のものになる不具合を見つけました。
callbackでは発行元の実際の呼び出し記録から論理的な深さを求め、後片付けで外側のVM実行の深さへ
戻します。物理的なstack検査は実行中のthreadを調べます。直接の発火もtask経由の発火も、
深さ上限1で発行元と発火側のRNGの続きを維持します。

`staticReadsOf`はslot・heap・event・呼び出し関数のprivate objectの情報を、通常待機とイベントNBAで
共通に設定します。通常待機はさらに借用するactivation/ref依存を解決します。新しいcallbackの
localは、発行元が待機中・復帰済みのtaskから借りません。

## 検証と残作業

新規はSIRのverifier/text検証13件とVM検証16件です。module/program、終了済み・継続中の発行元、
通常待機、即時登録、連続通知、新しいprivate作業領域、任意幅の回数、上位の値・不定bit、
符号付き・X/Zの待機省略、実際のref、範囲内への切り詰めと警告、独立したheapコピー、RNG、
automatic taskの復帰後、重複項目の順番、空のpayload、heap依存の捕捉を検査します。
Contextのメモリを使う検証は、queue/watchが成功する実行の各確保位置で1回ずつ失敗させ、
観測した58箇所のresourceの結果と全メモリ解放を調べます。C++の既定allocatorの失敗を注入するものではありません。
配置表の構築に失敗した場合も、空になったvariantをverifierから例外として漏らさないことを検査します。
ライブラリが元の有効な選択肢を保つ場合も許します。不正・欠落した配置表は、捕捉値の記憶を読む前に
内部SIRエラーとして停止します。

[最終の故障検出](../measurements/2026-10-27/nba-sir-controls.json)は20件すべてを検出し、復元した
source/test/toolのhashを記録します。[初回の16件](../measurements/2026-10-27/nba-sir-initial-controls.json)も
コンパイルして検出しました。[20件へ広げた初回](../measurements/2026-10-27/nba-sir-preliminary-controls.json)は
19件を検出しました。VM側だけのエラー分類変更では、kernelが確定済みの終了理由は変わりませんでした。
終了理由を決めるkernelの処理へ注入するよう修正し、確保失敗の検証を変えずに検出しました。
修正後は20件をすべて再ビルドして検出しました。[最終検証](../measurements/2026-10-27/nba-sir-validation.json)は
local CI全21項目成功、dev/ASan/UBSanは各7,109/7,111、releaseは7,110/7,112で、
失敗0・同じ既知skip各2件です。単体773件もすべて通りました。
[実行・参照比較](../measurements/2026-10-27/nba-sir-comparison.json)は母集団1,456/1,466、
sv-tests 244/344を維持します。両版の固定95プローブでもnativeの状態・stdoutと既知差分7件を
維持しました。この有限の集合で言語全体の対応を示すものではありません。
[性能記録](../measurements/2026-10-27/nba-sir-benchmark.json)は、新しい制御を使わない3設計をmainと
5組のAB/BA交互測定で比べます。ソースevent-NBAの速度を測るものではありません。

frontの捕捉順序とaggregate変換、実ソースのcallback/RNG/scope/生存期間の行列、通常NBAとの
queueの相互作用、program終了、入れ子のcallbackなどが残ります。既存の拒否期待値を変える前には
別のレビューが必要です。前のmonitorノートにある、呼び出し関数のprivate状態への直接の階層参照と
ソースの純粋性の差分は未解決です。managed eventのpayloadの差し替え、字句activationと
実ソースのscopeの全行列も未確認で、ソース経路を有効にする前に独自の行列で確かめる必要があります。純粋性、slangの厳格な診断、P1全体の到達点は変えません。
