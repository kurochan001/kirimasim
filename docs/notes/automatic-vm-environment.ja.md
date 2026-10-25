# VMの字句的activation環境

[English](automatic-vm-environment.md)。Status: 環境構築を実装・直接検証、2026-10-25。
Interpreterへの接続は残作業です。

## 契約と所有権

承認済み[automatic捕捉の契約](automatic-capture-proposal.ja.md)の1–4/6–8節から、
環境構築を実装します。[承認記録](p1-execution-contracts-2026-10-25.ja.md)の対応先は
SIR_SPEC 7/8とRUNTIME_ABI 4/6、言語上の参照はIEEE 1800-2023の6.21、9.3.2、13.5.2です。
外部実装・外部テストを使わず、依存・公開C ABI・形式の版・物理フレームABIも変更しません。

`ActivationPlan`は不変のSIR frame/scopeメタデータをkernelのlayout IDへ変換する実装用の表で、
別のIRではありません。layoutはContextが所有します。planは不変のSIR表を借り、環境より長く
生存する必要があります。move元は無効化します。ローカルのグラフ・所属・型・形状・権限を調べ、
process/subroutineの所有関係とCFGは既存のSIR検証器でも検査します。
所有event/refなど、実提供のないフィールドを追跡済みにはしません。

`ActivationEnvironment`は実行中の各ローカルscopeと取り込んだ各字句的extentに1つずつleaseを
持ちます。フレーム行は不変のkernel bindingへ対応させ、変更可能な値や複製は持ちません。
scopeの入口では新しい実体を作り、捕捉は権限を含むその実体へのbindingを渡します。
親が同じ字句ブロックへ再入しても、既存の子の参照先は変わりません。同じscopeの複数フィールドを
捕捉しても、物理記憶域やフィールドごとのleaseを増やしません。

子のグループのfactoryは全環境の解決・保持が済んでから返し、失敗時は途中のroot/importを
すべて解放して親を保ちます。呼び出し側は成功後に子を実行可能にし、RNGの進みを確定します。
スケジューラを含むfork登録全体の一括処理はまだ接続していません。環境自身はPC・制御スタック・
スレッド・RNG・process完了の所属を持ちません。

未読のフィールドと空scopeを含む実行中の字句的な全鎖を取り込み、親が取り込んだ鎖も明示的に
転送します。新しいローカルtokenとimportのtokenは構築上重ならず、二乗のtoken探索をせずに
一度ずつ保持します。callback用の一時pinも全体を保持します。巻き戻しではbindingを消してから
leaseを解放し、finishはimportも消して制御フレームの回収とは独立に解放します。
Interpreterはfinish前に待機/callbackを外す必要があります。確保失敗は結果で返し、解放は割り当てません。

refの捕捉は借用した参照bindingの固定で、参照先の値の複製や、仮引数の置き換え可能な表への
ポインタではありません。resolverがVM内部の参照表現からactualの由来・有効な権限・寿命を
検証します。環境はreadonlyを維持し、宣言した`ref static`にautomatic actualを渡すことを拒否します。
通常refはactualがstaticでも`join`だけです。空のextent鎖でも転送するrefには継承のメタデータが必要です。
借用refにはautomatic leaseを足しません。割り当てをしないvisitorが正当に保持したref bindingを
VMの型付きルート提供へ渡し、scopeの値のルートは共有kernel storeが提供します。

## 検証と接続の残作業

独自21テストで全joinの兄弟共有、呼び出しの独立性、ブロック再入、終了した所有者からの転送、
未読の型付きルートと空extent、readonly、ref修飾/joinの12組、仮引数再利用後のstatic ref転送、
不正/未提供layout、型と形状120通り、Context/planの同一性を調べます。3つの子の環境を作る
kernel側と環境側のPMRの確保失敗段階で、途中のleaseと値が解放され親が保たれることを確認します。
遅い段階での失敗を1,000回繰り返しても記憶域を再利用します。pin/入場の確保失敗も実行状態を
保ち、1万scopeの鎖を再帰せず構築・pin・解放します。初回のdev直接テストとclang-tidyは成功しました。

factoryは状態ベクトルに加えobject本体も指定したPMR resourceで確保します。構築に失敗したら
object用の領域を返し、構築済みなら対応するresourceのdeleterがfinish・破棄・解放を行います。
追加した検査では、返ったobjectが指定resourceの所有であること、予算0でResourceLimitを返すこと、
確保元と一致する解放を確認します。グループの確保失敗予算にobject本体も含め、大域hookは使いません。
指定resourceは返ったplan・環境・pinより長く生存する必要があります。ソースcallbackとスケジューラを
含むresource解放は、Interpreter接続時の残作業です。

objectのresourceの迂回、継承extentの欠落、誤ったbinding、外側scopeの欠落、実行所有権の解放遅延、途中グループの公開、
refの逃避、readonly喪失、`ref static`のautomatic actual、ref転送メタデータの欠落に対する
[コンパイルした故障10件](../measurements/2026-10-25/activation-environment-negative-controls.json)は
すべてビルド成功後に実際の部品テストで失敗し、元へ戻したビルドと21直接テストも成功しました。
`615afa0d`の[最終検証](../measurements/2026-10-25/activation-environment-final-validation.json)では
1回の全体実行でlocal CI全21項目が成功しました。dev/ASan/UBSanは4,650/4,652、
releaseは4,651/4,653成功で、失敗0・従来の2件スキップです。SIRダンプ29件と
PerfCount基準は変えず成功しました。Interpreterのソースは`65848df3`からバイト単位で
不変であり、前回の87プローブ・母集団・sv-testsは再実行や今回の部品の新しい証拠とは
扱いません。速度の新しい主張も加えません。

この部品は`vm`にリンクし、追加テストから直接使います。Interpreterへの環境の登録・接続は
まだ行わず、新しいSIR homeの拒否も維持します。frontも捕捉メタデータを生成しません。
ソースの捕捉・automatic eventの対応やIcarus差分の解消、P1全体の完成は主張しません。

次は論理`VarsId`の登録に環境を付け、[値アクセスの境界](automatic-vm-frame-access.ja.md)で
`FrameHome`を解決します。guard/watchは待機元の実際の環境を使います。
[scoped refの参照先解決の部品](automatic-vm-scoped-refs.ja.md)は不変の由来・選択範囲・世代・権限を
保つようになり、次にその表を接続して再入した親の現在行へ追従させないようにします。rootを
コピー入力前に構築し、outputの初期化と一度だけのコピー戻しを保ち、return/killで実行所有権を
解放して`tryRelease`まで保持しません。全fork環境の準備後に子とRNGを進めます。
callback/monitorの保持、型付きrefルート、named制御の巻き戻し、program終了、resource errorの
伝達を揃えてから実行拒否を外し、frontの捕捉を有効にします。
