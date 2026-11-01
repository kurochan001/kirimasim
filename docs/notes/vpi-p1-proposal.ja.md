# P1のVPI実行の提案

[English](vpi-p1-proposal.md)

状態：**提案、2026-11-01。詳細な実行契約は承認待ちです。**
保守者からVPIを次の優先対象に指定いただきました。その方針だけでは、VPI_SPECの
未記述の詳細は埋まりません。実行時のコードを作る前に判断できるよう、具体案を示します。
この案を置くことでVPIが実装済みになるわけではありません。

## 1. 範囲と既存の規則

P1_SCOPEはIcarus対応に必要なVPIをP1へ前倒しし、不足する詳細のレビューを求めます。
VPI_SPECは現在、見出しだけです。承認済みのSCHEDULER_SPEC 7/17のcallback領域と、
RUNTIME_ABI 5/9.6の規則を保ちます。Contextが状態を所有し、シミュレータの1スレッドで
同期的に呼び、startupはContextごとに一度、VPIを使うContextは順番に実行し、
外部コードの呼出しから戻るとcurrent-Contextのポインタを復元します。
kernelへslang・SIR・LLVMの依存は足しません。

最初の実装では、[独自の参照プローブ23件](../measurements/2026-11-01/vpi-reference-inventory.json)を対象にします。
登録、呼出し引数、整数・sized関数、静的オブジェクトの検索、幅広い4値、固定メモリの要素、
real・packed文字列、64ビット時刻、7種類のcallback、callbackの取り消し・再入、
変数への遅延書込み、変数のforce/release、反復、argv、finish、エラー照会です。
明示的に選んだIcarus 13.0/v13_0のコンパイラ・実行系で、23件全て成功しました。
標準のVPIを使うもので、新たなIcarus拡張や`wire real`の例外ではありません。
元の有限の176プローブと固定のP1母集団は変更しません。この小さな一覧だけでP1のVPI全範囲を
決めたり、同等性を証明したり、cocotb全体への対応を新たなP1完了条件に加えたりしません。
Icarusが対応する他の形も、対象外ではなく棚卸しの残作業です。

## 2. 読込みと登録の契約案

共有ライブラリのパスを明示する、繰返し可能なCLIオプション`--vpi=<file>`を加えます。
相対パスは起動時の作業ディレクトリが基準です。指定順にライブラリを読み、
`vlog_startup_routines`の表の順に呼び、callback・handle・外部呼出しが全て終わるまで保持します。
環境変数による検索、暗黙のpluginディレクトリ、`-M`・`-m`の別名、既定のpluginは増やしません。
ファイル不在・読込失敗・startupシンボル不在・空のパス・不正な登録・システム名の重複や
組込名との衝突は、入力エラーの終了2です。対応するLinux/macOSに既にあるOSのloaderを使い、
新しいパッケージはインストールしません。

対応する標準ABIの宣言を独自に書いた、C互換の`vpi_user.h`を提供します。
他シミュレータのヘッダや実装を取り込みません。別にコンパイルしたpluginで、サイズ・offset・
符号・公開C関数の型を確認します。VPIの32ビットaval/bvalをkernelの64ビットW4で
代用しません。この外部plugin ABIへの対応で、既存kirimasim C ABIの版を変えません。

`vpi_register_systf`はtaskと、整数・sizedの4値・realを返す関数に対応します。
slangを変更せず、既存の独自システムサブルーチンAPIを通し、ソース呼出しを束縛する前に
signatureを登録します。整数関数は符号付き32ビット、sized関数はエラボレーション時に
`sizetf`で正の幅を決定し、real関数はrealを返します。幅は登録ごとに保持し、
実行時の呼出しごとに`sizetf`を呼び直しません。
対応する静的オブジェクトと呼出し引数の情報を用意した後、実行の`calltf`やcallbackの前に、
エラボレーションされた呼出し箇所ごとに一度`compiletf`を呼びます。
slang内部の型検査が同じ式を訪れるたびには呼びません。

`parse --vpi=...`と`--dump-sir --vpi=...`も登録・signature・コンパイル時の検査を行いますが、
`calltf`やシミュレーションのcallbackは呼びません。最初の実装では、登録関数を定数式の
文脈に使う形は位置付きの未対応境界に残します。定数の畳込みへ任意のC実行を加えません。
未知のtask・関数の既存診断は保ちます。

## 3. 関数・オブジェクト・値

最初のhostが提供する標準の入口です。

- 登録・呼出し：`vpi_register_systf`、`vpi_handle(vpiSysTfCall, ...)`、
  `vpi_iterate(vpiArgument, ...)`、`vpi_scan`。
- 静的な検索：`vpi_handle_by_name`、`vpi_handle_by_index`、`vpi_handle`、
  `vpi_iterate`、`vpi_scan`、`vpi_get`、`vpi_get_str`、`vpi_free_object`。
- 値・時刻：`vpi_get_value`、`vpi_put_value`、`vpi_get_time`。
- callback：`vpi_register_cb`、`vpi_remove_cb`。
- 診断・制御：`vpi_printf`、`vpi_get_vlog_info`、`vpi_chk_error`、
  `vpi_control(vpiFinish, ...)`、`vpi_control(vpiStop, ...)`。

対応するmodule scope、静的な整数net・変数、real変数、定数、固定unpackedメモリ・要素、
システム呼出し・引数を記述します。宣言名、階層・generate・escaped名、幅・符号・宣言範囲、
ファイル・行、親scopeを保ちます。実行時の格納は数値のSIR bindingで選びます。
名前検索はVPIのreflectionサービスに限り、通常の信号実行へ増やしません。
整数netは、任意の物理driverではなく解決した値を観測します。

対応するオブジェクト型と、適用できる箇所の`vpiType`、`vpiName`、`vpiFullName`、
`vpiSize`、`vpiSigned`、`vpiFile`、`vpiLineNo`、`vpiTimeUnit`、`vpiTimePrecision`を提供します。
メモリ・packedの添字は、降順・負の境界を含め、宣言した添字を使います。
root module・子module・net・変数・メモリ・引数のiteratorは安定したソース順です。
末尾に達するとnullを返して反復の状態を解放し、その前の明示的な解放にも対応します。
外部に残るhandleが識別し得る間、解放したiteratorを再利用しません。
名前検索の不成立や通常の反復の末尾はnullで、架空のオブジェクトは返しません。

対応する値の形式は、scalar、32ビット整数、vector、2/8/10/16進の文字列、string、real、
値の抑止です。X/Zの全ビットと、既存の値の規則による符号・幅の変換を保ちます。
vectorは下位から32ビットaval/bvalの組を並べ、未使用の上位ビットをマスクします。
packedの文字列変換は既存のSVのbyte/NUL規則に従います。動的なSV stringオブジェクトや、
あらゆる集約オブジェクトへの対応を宣言するものではありません。

`vpi_get_time`は整数のシミュレーションtickと、scaled-realの時刻に対応します。
scaled時刻には囲むscopeの単位、globalの整数tickには設計の精度を使います。
32ビットを超えるhigh/lowの両方を保ち、scaled遅延の丸め・overflowは既存のSCHEDULER_SPEC 1に従います。
`vpi_get_vlog_info`は実行時plusargを含む元の順序のargvの所有コピーを公開し、
version/productの文字列もContextが所有します。

`calltf`はソースの呼出しが実行される位置で同期的に呼びます。引数の記録は評価時の値と、
代入可能な実体のbindingの両方を保ちます。数値を持つだけの読み取り専用式を、
書込み可能にはしません。関数は自身の呼出しhandleへの`vpi_put_value`で値を返します。
reflectionの情報を得るためにソース式を二度評価しません。

## 4. 書込みとcallback

遅延なしの変数へのdepositは既存の型付きstoreと変更通知を使います。
inertial/transportの遅延書込みは、値・時刻・対象・取り消し世代を所有し、
API呼出し後まで借用したCバッファを保持しません。`vpiInertialDelay`は同じ対象の
置き換わった書込みを取り消し、transportは予定された更新を保ちます。
時刻のoverflow、不正なflag・対象型は予定を登録する前に検出します。
netへのdepositは持続するdriverを作らず、解決netの公開を迂回しません。
次のdriverの解決で値が置き換わることがあります。netへのdeposit、解決net全体のforce・
強度の観測は後続の統合作業で、最初の実装ではこれらの書込みモードを明示的に拒否します。
将来の実装は承認済みの物理networkを使い、架空の変数aliasで代用しません。
変数のforceプローブだけでnet書込みへの対応を示すものではありません。

変数のforce/releaseは既存のforceの意味を保ちます。通常の代入はforceを置き換えず、
release後の変数は次の通常代入までforce中の最後の値を保ち、netのreleaseではnetworkを再解決します。
forceを一度だけのdepositで代用しません。`vpiStop`は既存の`--stop`方針、`vpiFinish`は
finish・final・終了callbackの規則を使います。kernel/VPI hostから直接processをexitしません。

承認済みの時刻callbackの配置を保ちます。

- `cbAfterDelay`・`cbNextSimTime`：Pre-Active。
- `cbReadWriteSynch`：Post-NBA。書込みはactive側の再反復を起こします。
- `cbReadOnlySynch`：Postponed。書込みと、同じslotへの新たなイベントは不可です。
- `cbStartOfSimulation`：時刻0の実行cycleの前。
- `cbEndOfSimulation`：finish・quiescenceを含め、final手続きの後に一度。

同じ領域では登録順です。時刻callbackは既存仕様どおり`vpiSimTime`・`vpiScaledRealTime`を
受け、nullやsuppressed timeを拒否します。`cbNextSimTime`は渡された時刻の値を無視し、
実際に進む次の時刻を観測します。それだけで静止したシミュレーションの時刻を進め続けません。
`cbValueChange`は時刻・値の抑止形式を受け付けます。

値変更の選択案：実効値の各更新を不可分に公開した後、書いた側が再開する前に、
シミュレーションの同じスレッドでsubscriberを同期的に呼びます。そのため、独自の参照プローブで
観測したように、遅延なしのVPI書込みは`vpi_put_value`から戻る前に値変更callback経由でCへ再入し得ます。
logical equalityや表示結果ではなく4値の一致で変更を調べます。subscriberの表を辿る間の書込みは
既存の変更を保留するwalkの仕組みを使い、再入で外側のiteratorや借用値を無効にしません。
更新で起こされるソースのprocessは既存のスケジューラの領域で実行し、C callbackにはなりません。

時刻・同期のcallbackは一度だけ、値変更callbackは解除するまで有効です。
登録時の構造体・時刻・値形式の記述・scalarの項目はコピーし、`user_data`はpluginのポインタのままです。
実行開始前の取り消しは有効で、別callbackからの取り消しと自己解除も含めます。
新しい登録を、既に保持した通知batchへ混ぜません。実行中のcallbackの記録は戻るまで保ち、その後に退役させます。
隠れたユーザーprocess・forkの子・RNGの所有者は作りません。callbackの処理も既存の
スケジューラのcounter・停止検査・resource制限を使います。

他のcallback理由・handle型・APIは、棚卸しと実装が済むまで明示的に未対応です。
既存のcallbackの意味を緩めたり、必要なP1作業を減らしたりするものではありません。

## 5. 所有・エラー・ライブラリ境界

Contextが、登録表、静的reflection、反復状態、書込みpayloadのコピー、callback、
エラー情報、戻り値バッファを所有します。opaque handleは所有Contextの生存中だけ有効です。
有効なtokenを使う前に型・世代・現在の表への所属を確認し、pluginから渡されたポインタを
内部slotのアドレスとして参照しません。handleの解放はHDLオブジェクトの削除ではありません。
戻す文字列・値のバッファは、次の関連するVPI呼出し・再入までの借用です。
その後も保持するpluginはコピーが必要で、Context間のhandleの自動移行には対応しません。

既存のRUNTIME_ABI 9.4/9.6の例外として、guardを通すcurrent-Contextの
`thread_local`ポインタを1つだけ使います。戻りと入れ子の入口で前の値を必ず復元し、
globalの既定Contextは持ちません。実装時に加えるallowlistの具体案は
`src/kernel/vpi.cpp | currentVpiContext | 9.4 item 4 | synchronous VPI entry and restoration`です。
それ以外の変更可能な状態は全てContextが持ちます。別にコンパイルするユーザーfixtureの
startup表は公開C ABIに従うもので、hostのsingletonではありません。

不正な型・形式・flag・段階・未対応操作は、APIの失敗・抑止の戻り値を返し、
Context所有の`vpi_chk_error`へ安定した診断codeを残します。未対応を成功のゼロで隠しません。
既存のスケジューラの禁止書込み、時刻overflow、resourceエラーは現在の終了規則を保ち、
内部不変条件の破れも内部エラーのままです。C ABIを越えて例外を投げません。
pluginのC callbackは、シミュレータをblockしたりnative stackを中断して時刻を進めたりしません。

frontにはkernelのヘッダ・リンクではなく、狭いadapterで単純な登録・型・呼出し箇所の情報を渡し、
driverがContextへ接続します。kernelにslang・SIRの依存を入れません。
SIRは型付きの数値external-call ID、引数の値・binding・位置を持ち、検証器が結果の幅と
不正な書込みbindingをVM実行前に拒否します。読み取り専用の引数オブジェクトへの書込みは、
hostがAPI呼出し時に拒否します。
VMは共有kernelサービスへ外部呼出しを委ねます。SIRのtextへ読込パスやnativeアドレスを
実行可能ポインタとして埋めません。将来の第2層も同じサービスを使います。

## 6. 受入れと残る確認

最初の範囲を実装済みとする前に、以下を要求します。

- 元の参照入力を独立した意味の検査でnative実行する。loaderエラーや未対応をPASSにしない。
- module/interface/program、名前付き・generate scope、automaticな関数・task、入れ子呼出しの
  文脈行列。幅広い符号付き4値、宣言したメモリ境界、時刻単位とhigh/lowの繰上がり。
- NBA/read-write/read-onlyの配置、実効値の変更とX/Zの無変化、再入、dispatch中の登録・解除、
  遅延の取り消し、final/終了の順、quiescence/next-timeの挙動。
- Context A/Bの再利用、loaderの解放、callback/handleの寿命、Cバッファのコピー、
  readonly・不正な要求、TLSの復元、C++20ヘッダ。
- これらの仕組みのコンパイル変異と、dev・関連するrelease/ASan/UBSan検査。
  既存のsuite・処理回数の期待値は保ち、古い拒否期待値の移行は別のレビュー・変更にする。

この案はIEEE 1800-2023の第38章、38.36、4.10/Table 4-1と、リポジトリで確認済みの
スケジューラ契約を参照します。callback以外の個別APIの正確な節番号と、参照側が最適化で
除いた未参照オブジェクトの挙動は**未確認**です。レビュー時に規格で確認し、プローブでは
明示的に参照して保持した対象を使います。ソースは独自に作り、他シミュレータの実装と
生成されたシミュレーションのbytecodeは読んでいません。インストール済みの公開VPIヘッダは
外部プローブのコンパイラだけが使い、ここへコピーしていません。cocotbの現行統合は未検査です。

Icarus公式の[VPIの使用方法](https://steveicarus.github.io/iverilog/usage/vpi.html)と
[loaderのflag](https://steveicarus.github.io/iverilog/usage/vvp_flags.html)は起動方法だけに使い、
実装の期待値の根拠にはしていません。
