# VPIのnet全体への書込み：P1接続契約案

[English](vpi-net-write-proposal.md) · 2026-11-03。

**状態：1〜8節は2026-11-03に承認済み、実装中です。**
9節の版の制限と、depositの公開強度Strong 6の選択を開示したうえで、保守者が明示的に承認しました。
最初のnet書込み拒否境界の有効な改定です。2023年版の直接照合や、既存期待値の移行まで
承認・完了したとは扱いません。
[最初のVPI契約4節](vpi-p1-proposal.ja.md#4-書込みとcallback)とVPI_SPECのnet書込み境界の改定案です。
最初の契約はnetへの書込みを明示的に拒否しています。承認済みのscheduler・物理ネット・値の規則を
保ち、接続に必要な選択を以下で具体化します。実装状態やP1の条件を変更する資料ではありません。

[独自61入力の比較](../measurements/2026-11-03/vpi-net-write-inventory.json)では、Icarus 13.0は
期待値一致54件、参照差4件、期待値を定めない観測3件でした。nativeは61件ともAPIを未対応として
拒否し、プロセスの終了0を成功には数えていません。[コンパイルした変異3件](../measurements/2026-11-03/vpi-net-write-checker-controls.json)は
外部の判定器の確認であり、未実装のnative機能の検証ではありません。

## 1. 最初に接続する範囲

検証済みの数値reflectionで表されるstaticな4値整数net宣言全体に、`vpiNoDelay`・
`vpiForceFlag`・`vpiReleaseFlag`を認めます。scalar/packed宣言、共有されたport alias、
対応するwire/tri/wired/pull/supply種別を含みます。通常の物理driverを使い、連続代入1本だけに
制限したり、変数aliasを捏造したりしません。名前handleと捕捉した実引数handleは同じ物理netを指します。

force/releaseは、既存の[MOS/switch契約5節](mos-switch-proposal.ja.md#5-forcereleaseと通過する寄与)に沿い、
登録済みの受動ネットワークも扱います。depositの最初の範囲は、物理componentに受動接続が無い
point netです。受動接続があるdepositは、状態を変える前に拒否します。観測した受動接続のdepositは
後続の棚卸しに残し、新しいP1例外にはしません。net全体へのforceからbit/part handleの書込み対応を推測しません。

netのbit/part handle、unpacked net配列のword、ユーザー定義・realのnettype、strength形式のput、
遅延付きnet put、返すevent handleと取消しは最初の接続の外に残します。既存の変数書込み・遅延は
その契約を保ちます。これらは実装境界であり、P1の必須範囲から取り除くものではありません。

## 2. 検証された物理的な書込み権限

任意のslotにnetフラグを付けても、書込みを許しません。front/SIRは、対象のreflectionを検証済みの
公開された物理記憶域と登録済みネットワークへ結び、無駆動netも含めます。VMはその数値の権限を
転送します。言語のnet参照は読み取り専用に保ち、既存のforeign-net参照の逸脱検査も保ちます。
VPIのbindingを言語の変数参照へ転用できません。

変更前にobject種別、完全な範囲、要素、layout、生存Context、物理登録、権限、操作を検証します。
論理aliasは同じdeposit/force状態を共有し、実行時に名前からdriver状態を選びません。
不正な自作IRは、実行時記憶域を割り当てる前に拒否します。

未登録のnet slotを作る既存単体テストは、引き続き拒否します。拒否したreleaseが呼出し元の値を
保つ検査も維持します。この案では既存期待値の移行を求めません。実際の接続で食い違いが分かった
場合は、具体的なソース・期待値・規格節を示し、別途承認を受けます。

## 3. 遅延なしのdeposit

depositはpoint netの公開された解決済み値を上書きし、物理driverの確定した寄与を変更したり、
永続driverを追加したりしません。そのnetの物理driverの確定寄与が実際に変化するまで上書きを
保ちます。その変化の前後で通常の解決結果が同じでも、上書きを解除します。同じ正規化済み寄与を
再実行しただけでは解除しません。無関係な信号変更や評価したことだけでも解除しません。
保留中の分布遅延・module pathは、最初の予約時ではなくdriverの寄与が実際に確定する時点で解除します。

解除時は現在の通常の解決結果を公開し、deposit前のキャッシュを戻しません。無駆動netでは、
対応する置換操作までdepositを保ちます。既存のforceを優先し、force中のdepositをrelease後に
持ち越しません。成功したdepositのためにprocess、RNGの所有者、timerを追加しません。

**レビューする表示上の選択：** 論理値だけの通常のdepositは、一時的な公開強度区間にStrong 6を
使い、Xは`[-6,6]`、Zは`[0,0]`とします。既存の値・強度変換を使い、物理的な駆動源にはしません。
`%v`、整数の読み取り、VPI、波形で同じ値を観測します。strength形式のVPI入力は拒否したままです。
この表示の選択について2023年版本文との直接照合は未確認であり、承認時にこの制限も確認します。

## 4. force/releaseと共通の所有権

VPIのforceは、HDL forceと同じ検証付きkernel機構へ値を設定します。既存のVPI所有者を使い、
HDL processを捏造しません。bitごとの所有は共通で、新しいHDL/VPI forceは対象範囲の所有者を
置き換え、releaseは設定元に関係なく現在のholdを解除します。置き換えられたHDL forceの評価器が
後のRHS変更で再び上書きすることはできません。部分的なHDL forceとVPIのnet全体の操作も、
既存の所有規則に従って相互作用します。

物理ネット契約5節を保ちます。forceしたnodeの局所源を抑え、Strong 6の仮想force寄与を既存の
辺で通過・減衰させ、対象の公開値を固定します。遠方の源はそのnodeを通過でき、Zへのforceも
接続を切る障壁にはしません。componentの全nodeを固定したり、ネットワークの解決を迂回したりしません。

force中も通常の物理driver、導通、保留中の分布遅延・module pathを背後で進めます。
releaseはholdを外し、呼出し元へ戻る前に現在の寄与で直ちに解決します。未来の到着を前倒ししません。
net全体のaliasは同じ確定更新を見ます。releaseのvalueポインタが非nullなら、要求した対応形式で
解除後の値を既存の所有bufferへ返します。nullも認めます。直前のforce値を返したり、releaseの
入力payloadを読み取ったりしません。

## 5. 公開・callback・領域

kernelの共通の確定・公開経路を使います。複数nodeに及ぶforce効果も含め、通知の前に完全な
複数bit更新を準備します。強度だけの内部通知と、通常の4値一致による値変更の判定を保ちます。
公開値が同じなら通常の値変更callbackは発火しません。入力時計の捕捉、値・辺のwaiter、VPI、
VCD/FST、monitor、postponed観測を共通化し、これらを迂回してnetの生の記憶域へ書きません。

承認済みの同期VPI callback・再入と、登録者を捕捉する規則を保ちます。有効な即時更新は、
`vpi_put_value`が戻る前にcallbackから見えます。再入した書込み・取消しで外側のbufferを壊したり、
同じ通知を重複させたり、vectorの途中を見せたりしません。起こされたソースprocessは通常の領域で
実行します。新しいscheduler領域や隠し利用者processは追加しません。

コンパイル・parse専用の段階では書込みを拒否します。実行時のソース・開始・時間・read-writeの
callbackには、既存の書込み可能な領域の規則を使います。Postponed/read-onlyの書込みは、既存の
実行時エラー方針で拒否し、netを変えません。3種類の即時モードでは、非nullでも`time_p`を
無視します。遅延として解釈したり、遅延を検証するために参照したりしません。

## 6. 失敗・終了・構造

操作前にflag、権限、範囲、入力・出力formatを検査します。不正・未対応の要求には安定した
`vpi_chk_error`診断を返し、driver、予約の世代、hold、deposit、呼出し元のrelease結果を保ちます。
割当て・overflow・割込みは既存の明示的な資源/終了経路を使い、最初のエラーを保ちます。
途中まで変更して成功と報告することはできません。

depositの情報、forceの所有、snapshot、予約はContextが所有します。構成・範囲公開では確定前に
割当てを準備し、呼出しを越えてCの値bufferを借用しません。kernelのエラー境界と同期的な
current-Contextの復元を再利用します。終了時はlibraryをunloadする前にbindingを切り、別の/終了した
Contextのhandleから新しい状態へ到達させません。

kernel公開ヘッダのC++20、本体のC++23、依存の固定版、公開C ABIを保ちます。library、
インストール、実行時の名前検索、可変global、allowlist項目は追加しません。静的な権限はfront、
物理的な公開・所有と値の規則はkernel、型付きoperandの転送はVMが担当します。
ソースの形だけの特例や、2つ目のnet解決器を作りません。

## 7. 承認後の受入れ条件

元の61入力・期待値・観測をすべて保ちます。nativeでは幅1/32/33/65/129/257、4状態、ソース文脈、
実引数/名前handle、alias・部分HDL force、net種別、複数/無駆動源、同じ寄与の再実行と解決結果が
同じdriver変更、force/depositの優先、分布遅延・module pathの予約、受動・抵抗ネットを検査します。
公開`%v`、VPI callbackのsnapshot・再入、VCD/FST、内外の読み取りも確認します。
force中の現在・将来の到着、何もしないrelease、同じ値での再forceも含めます。

Context/SIRの直接テストで、偽造した権限、不正な範囲・種別、読み取り専用段階、割当ての再試行と
原子性、終了、独立Contextを検査します。既存の変数書込み、release形式、scheduler、forceの検査を
保ちます。独立した期待値を使い、前記の外部判定器の変異3件をnativeのテスト・変異の代用にしません。

永続driver化するdeposit、解決済み出力だけによる解除、同じ寄与での解除、forceのdeposit化、
古いrelease値、force所有者・aliasの消失、予約到着の消失、重複・部分的な通知、偽造権限の受理を
nativeのコンパイルした変異で検査します。コンパイル成功後の意味のアサーションで検出し、ソースの
ハッシュを復元して基準テストを再実行します。影響する全体構成・静的検査を実施し、push前には
追跡ファイルを固定して全local CIを実行します。件数は全件成功したdev JUnitから別途更新します。
pushには、引き続き保守者の依頼が必要です。

## 8. 参照差と完了の扱い

Icarus 13.0と記述済み期待値には4件の差があります。解決結果がXのままのdriver変更でdepositを
解除しないこと、同じ寄与の再実行でdepositを解除すること、置き換えられたHDL forceが値を取り戻すこと、
同値のforce/releaseでcallbackが重複することです。差として保ちます。native案は、確認したdepositの
規則と承認済みの所有・通知の規則に従い、これらの出力を模倣しません。

受動接続forceの観測2件は、nativeのHDL forceによる対応実験と一致します。`tran`では固定しない側が
X、force対象が1、`rtran`では0と1になり、両方ともrelease後は0/0です。受動接続depositの観測は
移植可能な期待値にせず、最初のdeposit実装の外に残します。61件のnative API書込みは、実装するまでは
未対応で、プロセスの終了0を成功にはしません。

固定176プローブ、P1の分母、従来の母集団、任意の`wire real`例外は変えません。この有限の棚卸しで
P1の完了を示したり、cocotb全体を新しい完了条件にしたりしません。slangへの投稿保留も継続します。

## 9. 一次資料・版の制限・再現

実際に確認した一次本文は、[MIT公開のIEEE出版物](https://rfsoc.mit.edu/6S965/_static/F24/documentation/1800-2017.pdf)の
**IEEE 1800-2017 38.34・10.6.2**です。規格の本文・例や、他のシミュレータのソース・テストは
複写していません。既存のプロジェクト規則はSCHEDULER_SPEC 14、MOS/switch契約5節にもあります。

2026-11-03に[公式2023ページ](https://standards.ieee.org/ieee/1800/7743/)と
[Accelleraの配布案内](https://www.accellera.org/downloads/ieee)を再確認しました。IEEE GETリンクは
HTTP 418でした。**2023本文との条文ごとの直接照合は未確認**です。実装前に、この制限を含めて
保守者が1〜8節を明示的にレビューします。承認によって版の照合が済んだとは扱いません。
公式2023本文を取得できたら確認し、意味が異なる場合は、該当する実装の前に契約を改定します。

```sh
python3 tools/vpi_net_write_probe.py \
  --iverilog "$VPI_PROBE_IVERILOG" --vvp "$VPI_PROBE_VVP" \
  --iverilog-vpi "$VPI_PROBE_COMPILER" \
  --kirimasim build/dev/src/driver/kirimasim --output build/vpi-net-inventory.json
```

同じインストール済みIcarus 13.0の3実行ファイルを明示します。既定の終了0は、独立した失敗なしに
棚卸しを記録した意味です。`--require-reference-oracles`を付けると、保持した参照差4件で非0終了します。
`--case`・`--plugin-source`で、基準pluginを編集せず制限付きの判定器変異を再現できます。
nativeの実行ファイル・ソースのチェックポイント、実測した版、ハッシュ、出力、分類はリンク先のJSONにあります。

準備段階の確認：本体を変えていないチェックポイントで、既存VPI/CLIの33テストがすべて成功しました。
独自pluginは、プロジェクト独自のヘッダでもC11・`-Wall -Wextra -Werror`でコンパイルできます。
判定器の変異3件をコンパイルして検出した前後で、61件のソース・期待値・観測結果を保ちました。
`src/`、既存期待値、依存、件数ファイルは変更していません。これは提案・棚卸しの検証であり、
nativeのnet書込み実装の検証ではありません。
