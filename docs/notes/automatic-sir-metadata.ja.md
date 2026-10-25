# SIRのautomaticスコープと捕捉メタデータ

[English](automatic-sir-metadata.md)。Status: 表現と検証器の実装段階、2026-10-25。
VM・ソースへの接続は残作業です。

後続の[VM値アクセスの移行](automatic-vm-frame-access.ja.md)は実装・検証しました。
新しいhomeの解決と、所有する環境・不変のrefの構築は、実行を有効にする前の残作業です。


現在の接続状況：[Interpreterでの実行](automatic-vm-execution.ja.md)を追加しました。
以下は各部品の段階の記録であり、その時点のVM拒否・残作業は当時の状態です。
現在は妥当な表現済みSIR homeを実行できますが、frontによるソースの捕捉はまだ有効にしていません。

## 承認済み契約と境界

承認済み[automatic捕捉の契約](automatic-capture-proposal.ja.md)の1–3/7/8節のうち
表現と検証を実装します。[承認記録](p1-execution-contracts-2026-10-25.ja.md)で
SIR_SPEC 3/7/8に対応付けています。言語上の根拠はIEEE 1800-2023の6.21、9.3.2、
13.5.2で、表現はこの契約に従う実装上の選択です。他のシミュレータの実装や外部テストを
読んだり移植したりせず、依存・外部ABIも追加しません。P2/SIR v1の確定ではなく、
formatVersionは0、パーサは未導入、物理フレームのサイズ・整列・位置は未決定です。

[記憶域](automatic-storage-foundation.ja.md)と[待機通知](automatic-storage-observation.ja.md)の
カーネル基盤はあります。新しいhomeを通常フレームの独立した語として誤実行しないよう、
型付きアクセス・ref・初期化/コピー戻し・巻き戻しをすべて接続するまでは、VMが実行前に
明示的に拒否します。frontもまだ生成しません。この段階でソースのfork捕捉・automatic event・
新しいソース型は有効にせず、Icarusが通る差分の解消とも数えません。

## 表現

`FrameHome`は通常行・スコープのフィールド・捕捉ビューを区別します。
フィールドは本体内のActivationScopeIdを持ち、スコープ表はフィールドの配置順と字句的な親を
記録します。`ActivationInfo::root`は引数のコピー入力前に作る記憶域です。ブロックには
`activation.enter #N`と`activation.leave #N`を使い、所有する環境は待ちをまたぐ一時値ではなく
フレーム状態で持ちます。設計全体のnamed disableのIDとは別で、物理位置は指定しません。

捕捉行は`Process::parent`が指す親process/taskのフレーム行を指定します。値の複製でも
親の現在の表要素への別名でもなく、bindingを表します。`ActivationInfo::extents`には
未読・空のスコープを含む親の字句的な全鎖を記録し、`inheritCaptures`で親が継承した
extentを孫へ渡します。動的な呼び出し/processの祖先とは分けます。実行時の取得と、
全取得が済んでから子を開始する処理は残作業です。

フレーム/引数はreadonlyと`ref static`を保持します。捕捉は型・宣言型ID・次元・refの対象・
static修飾を一致させ、権限を狭めることはできても書き込み権限は取り戻せません。
通常refの捕捉は`join`だけ、static修飾のbindingは各joinを許します。ここでは宣言した
メタデータを検査し、呼び出し時のstatic actualの由来と、参照の全実行/ルート経路は
残作業です。借用refを無期限の所有権には変えません。

所有フィールドは実際のActivationStore提供がある二値/四値整数、real/shortreal、
string/chandle、型付きコンテナhandleと検査済み固定次元を扱います。所有event/refの
フィールドは実提供ができるまで拒否し、未追跡の整数handleにはしません。
通常のフレームrefや捕捉したref bindingは、所有フィールドとは別です。

## 検証とテキスト

共通の`verifyActivationScopes`はフィールド所属、root/親グラフ、次元の桁あふれ、
捕捉の形と権限、字句的extent全体、fork親の循環、仮引数のroot配置を検査します。
CFGの辺が合流するときは実行中の所有権スタックが一致し、waitは保持、returnは実行環境を
解放し、named制御の出口は記録したautomatic深さへ戻します。入退場は最内の字句スコープです。
guard/watchのフレーム読み取りには待機/triggerの位置でスコープが必要で、helperが自分の
スコープを所有したりreadonly捕捉へ書いたりすることは許しません。主検証器の既存の型・
SSA・到達可能性・スケジューリング検査も維持します。

テキストはスコープのフィールド/親/root、捕捉extent、home、readonly、static-ref修飾、
2命令を決定的に出します。通常設計に新しい表や属性は出しません。出力だけの実装で、
print/parse/printや文法の安定を確認したとは扱いません。

## 検証結果と次の作業

独自のActivationSir 16テストで66種類の型/配列形状、不正な表25種、所有権の失敗8種、
ref修飾とjoinの12組、待機、再入、named disable、readonly helper、引数のroot配置、
1万スコープの非循環/循環グラフを調べます。不正な実行入力はslot/記憶域の確保前に拒否します。
`56a10e05`の[コンパイルした9故障](../measurements/2026-10-25/activation-sir-negative-controls.json)は
すべて追加した実際の検証器テストで失敗し、元に戻したビルドも成功しました。判定ツール自身の失敗は数えません。
[最終検証](../measurements/2026-10-25/activation-sir-final-validation.json)はlocal CI全21項目成功で、
dev/ASan/UBSanが4,585/4,587、releaseが4,586/4,588成功、失敗0・既知2スキップです。
既存のスイート期待値・SIRダンプ29件・PerfCount基準は維持しています。

別の[独自の初期化3プローブ](../measurements/2026-10-25/automatic-entry-initialization-observations.json)は
既存の通常フレーム/static slotを調べます。nativeはautomaticなスカラー/配列をブロックの3回の
入場で初期化し、static配列は前回の書き込みを保持します。両参照ではautomaticも保持されました。
既存の承認済み再入契約に対する差であり、参照出力を期待値に採用したり、新しい捕捉を実行した
証拠にはしません。表現だけの今回の段階で固定87項目・母集団・sv-testsは再計測していません。

次はVMの論理フレーム/refアクセスを型付きkernel bindingへ接続し、call/block所有権と
fork環境を付けます。幅広/packed/配列、ファイル/走査、コンテナ、乱数seed、コピー戻しを
すべて同じ経路へ通し、leaseを落とす前に待機を外します。戻ったcallの制御と保持した記憶域を
分け、コピー戻しは一度だけ行います。その後frontが字句スコープ全体を昇格し、ソース行列を
有効にします。実行の根拠が揃ってからVMの拒否を外します。P1全体は未完了です。

forkの元はprocess rootのフレームではなく、同期関数や待つtaskを含む現在のSIR本体の
`act_.varsId`から解決します。子やreportが残る制御フレームを`tryRelease`で回収するまで
記憶域の実行所有権を保持してはいけません。scoped refは親の置き換え可能なbinding表ではなく、
権限を含む実際のスコープtokenへの不変bindingを持ちます。fork環境全体の取得後に子を
実行可能にし、乱数seedの進みもそれより前には変えません。
