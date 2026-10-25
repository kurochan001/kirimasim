# 共有automatic記憶域に向けたVMの値アクセス

[English](automatic-vm-frame-access.md)。Status: アクセス経路の移行、2026-10-25。
automatic環境の構築は実装途中です。

## 契約と変更

承認済み[automatic捕捉の契約](automatic-capture-proposal.ja.md)の2–4/6–8節に
必要な値の受け渡しを実装します。[承認記録](p1-execution-contracts-2026-10-25.ja.md)の
対応先はSIR_SPEC 7/8とRUNTIME_ABI 4/6、言語上の参照はIEEE 1800-2023の
6.21、9.3.2、13.5.2です。外部実装・外部テスト・新しい依存は使いません。
物理フレーム配置、formatVersion 1、新しいC ABIも決定しません。

以前の`frameVar`・`frameElem`・`refWord`は変更可能な語への参照を返し、
スカラー、幅広の値、メモリファイル、ファイル入力、走査、seed更新、引数の
受け渡しが直接書き込んでいました。スカラー代入だけ共有bindingへ変えても、
他の経路が共有実体や通知を通らず書き込むことになります。

`detail::FrameValue`は1つの値/要素を受け渡す短命なビューで、通常の語を借りるか
不変のkernel activation bindingを使います。記憶域・leaseを所有せず、変更可能な
値のキャッシュも持ちません。bindingの読み取りは`ActivationStore::read`、書き込みは
`Scheduler::writeActivation`を通し、幅広い値全体または選択したビット範囲を渡します。
正規化・同一性・権限・観測の意味はkernelで決めます。通常の経路は語のコピーと
既存のkernelの選択演算を使い、ビュー自身は割り当てをしません。refのW4の両半分も保ちます。

命令・ファイル・走査・引数の既存経路を共通のアクセスへ移しました。
packedの部分書き込みは実際に覆うビットだけ、`$fread`は読み取ったバイトの範囲だけを
一度で書き込みます。残りの値を読み直して書き込みを作ることはしません。
配列全体は宣言順の要素ごとに受け渡します。`bindArgs`・`readOuts`・`writeOuts`は
登録した論理activationを使います。待つcallと同期callの両方で、コピー戻しを一度行ってから
calleeを解放します。taskと遅延callの入口もコピー入力前に登録しますが、登録だけでは
まだautomatic rootを構築しません。

## 検証と制限

独自7テストは通常/共有の選択270通りを独立したビット単位モデルと比較し、refの全payload、
不正buffer/範囲、readonly、選択した読み書きの依存範囲、Active/Reactiveで幅広い値が
揃ってからの通知、二値の正規化、スコープ再利用後の古い世代の拒否を調べます。
手組みSIRの24通りで通常/refの配列アクセス・範囲外へはみ出す部分書き込み・スカラーの
コピーを実行し、別の幅広いフレーム行を置いて論理行からの位置の変換も検証します。
通知・上位chunk・要素の同一性・readonly・選択読み取りの故障定義を5件追加しました。
既存のwait-ref位置と`$fread`バイト順の2故障は、同じ誤りを入れる位置だけ移しました。
既存期待値とスイート本体は変更しません。全体検証とコンパイルした故障の結果は実行後に記録します。

現在VMのSIRからの位置解決は通常の語のビューだけを選びます。
[新しいSIR home](automatic-sir-metadata.ja.md)は明示的に拒否したままで、frontも
生成しません。binding版は追加テストで直接検証しており、新しく受け入れたソースの実行では
ありません。この段階でIcarus差分の解消やP1完了とは扱いません。

次はscoped/captured homeの不変bindingへの解決と、root/block/子の環境全体の
一括構築を行い、字句的extent全体を保持し、制御フレームとは独立に実行所有権を解放します。
refには親の現在のbinding表へのポインタではなく、不変のscopedな参照先と権限が必要です。
guard/watchは待機元の実際の論理activationで依存を解決します。宣言の初期化、引数bindingの
初期化、コピー戻し、return/disable/programの解放、型付きルートを揃えてから実行拒否を外し、
frontの捕捉を有効にします。管理するeventなどのフィールドは実際の提供元も必要です。
