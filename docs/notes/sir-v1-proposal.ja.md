# P2の提案：SIR v1――tier 2へ渡せる形の確定

[English](sir-v1-proposal.md)。Status: **direction approved (2026-10-24);
detailed contract pending review**。英語版が正本です。2026-10-24の提案後、下記の推奨方針を
人が承認しました。EBNF・record schema・API prototype・参照配置の詳細は引き続き案です。
[フェーズ番号の対応](phase-renumbering.ja.md)は別の承認済み事項です。
この承認で依存・道具を導入せず、既存の期待値も変更しません。

## 承認された方針（2026-10-24）

推奨時に説明した留保を含め、次の方針が承認されました。

1. P1を優先しながらP2を並行準備し、SIR v1の凍結はP1完了後に行います。
   旧仕様のP1中の最終文法確定をP2へ移し、他のP1範囲・合格条件はP1に保ちます。
   P2のgateが揃うまでformatVersionは0です。
2. dump表・定数identity・宣言した個数を保存する完全な正規record形式v1を使います。
   移行中は既存v0診断出力を保ちます。正確な文法・schema・公開CLI名は後続レビューです。
3. 共通schemaからOp enum・名前・基本検証metadata・仕様表を生成し、CIで照合します。
   複雑な型規則は名前付きvalidatorにします。棚卸しで見つかった意味の不足は別に解決します。
4. 16byte cell・8byte整列を保守的な評価基準にし、測定してから配置を凍結します。
   参照tokenの表現はP1のactivation・寿命規則が固まってから決めます。
5. C境界は待ち・process管理・container・fileなどの型付きサービス単位で整理します。
   付録Bの181個のA行は対応漏れを調べる候補であり、181公開関数や名前・prototypeの
   一括承認ではありません。tier 1のC++直接呼び出しを保ち、準備中はABI 0.x、
   全対応と境界テストが揃うP2完了時にABI 1.0へ安定させる方針です。
6. 解析別表の形式と失効規則をP2で決め、欠落・古い結果はunknownとします。
   アルゴリズムの選定と実装はP3です。

承認された準備順は、**Op schemaと照合 → v1パーサと往復検査 → 意味の共有とサービスAPI設計
→ frame・ABIの凍結**です。6節の詳細は後続レビューで決めます。この記録は文書の更新であり、
P2の開始・完了、本書の全詳細、承認待ちのP1意味修正案を承認した扱いにはしません。

**文書同期（2026-10-24）：** 承認済みの方針とP1後のP2での確定期限を、
SIR_SPEC 1・9・11、RUNTIME_ABI 4.2・8、P1_SCOPE、README、ARCHITECTURE、P1_SUPPORTへ
反映しました。6節の文法・サービスAPI・ref配置の詳細はレビュー待ちです。
2節の棚卸しは元の時点の記録を保ち、そこに挙げた旧P1文法期限は今回修正済みです。

## 1. 目的と範囲

**フェーズP3で作るtier 2（C++へのAOT生成）**と、P3後に採否を判断する将来の
tier 3（JIT）を、tier 1と同じSIRとカーネルから作れる状態にします。新しいIRは
作りません。P2は、完全な読み渡し・検証・実行境界の契約を確定する段階です。

テキスト文法とパーサ、全Opの定義、frame・値・参照の配置、C関数表、意味を共有する
ヘルパー、解析結果の別表の形式を対象にします。C++生成・JIT・X解析・static islandの
発見アルゴリズムは作りません。契約を完成させる名目で新しい言語機能をP2へ移しません。
Icarus対応に必要なクラス・動的データ・program・VPIなどは、引き続きP1の仕事です。
境界は**完了時のP1全体**を対象にし、現在の124命令だけで凍結しません。既存のP1条件と
期待値は緩めません。tier 1はカーネルのC++ APIを直接呼び続けます。公開ヘッダと
生成コードのC++20、生成コードの例外・RTTI不使用も保ちます。

根拠：[SIR_SPEC 1・5.1・7.2・7.4・9〜11](../spec/SIR_SPEC.ja.md)、
[RUNTIME_ABI 2〜4・6・8](../spec/RUNTIME_ABI.ja.md)、
[CLAUDEの「意味を1か所で決める」](../../CLAUDE.ja.md)、[P1_SCOPE](../spec/P1_SCOPE.ja.md)。
提案の考え方はこれらの既存契約と以下のリポジトリ調査に基づきます。
外部シミュレータや外部プロジェクトの実装コードは参照していません。

## 2. 棚卸し：事実・不足・確認方法

基準はフェーズ番号変更後の`d915150`で、実行コードは`c0388da`と同じです。
行番号はこの時点のものです。P1の変更後、凍結前には必ず棚卸しを更新します。
未確認事項には **not confirmed（未確認）** と、確認する方法を添えます。

### 2-a. Opの全件

付録Aは**enum全124件**の一覧です。各行にオペランド、型規則、終端か、待つか、
書き込み・副作用、仕様の節とverifierの入口行を記載します。根拠は
[design.h](../../src/sir/include/kirimasim/sir/design.h)、
[types.cpp](../../src/sir/types.cpp)のopName・isTerminator・isWait、
[verify.cpp](../../src/sir/verify.cpp)のcheckInstr（973〜2020行）です。
型欄は実装済みの検査の要約で、規格全体の正しさの証明ではありません。
共通条件はTemp/Constの値、範囲内のID・選択、明示的なドメイン変換、整数幅が正、
命令のソース位置です。配列・ref・出力の実引数にはslot/frameも使います。
「may」は停止の可能性で、必ず待つとは限りません。空のjoinや真のwait条件は
直ちに進めますが、isWaitはwait.cond・call.task・join_none以外のforkを待ち境界に
数えます。この境界をSSA一時値がまたぐことはできません。

確認できた仕様とコードの違いは次のとおりです。

- 5.2の命令表は「例」で、全件を定義する表ではありません。`inside`と`cond.x`が
  載っていますが、enum・opName・verifierにはありません。insideは
  front/lower_expr.cpp:2302〜2335で比較・論理演算・分岐へ展開します。
  cond.xにはこの時点で対応するOpがありません。承認済みの診断契約を黙って消さず、
  実装するか、代わりの表現を仕様修正として決める必要があります。
- `rnand`・`rnor`・`rxnor`は実在しますが、5.2は「否定も」とだけ書きます。
  分岐とreturnは5.1・7.2・9、コンテナとpattern.formatは5.8・5.9に分散しています。
  現状の5.2だけとenumの名前を照合しても一致しません。
- 5.2にはXを作りうる命令のフラグと、値とは別の比較マスクの説明がありますが、
  InstrにX生成フラグはなく、Operandにもマスク種別はありません。定数はa/b語、
  match.maskは値とモードを使います。X生成可能性がOp固有か条件付きかを定義します。
  解析で証明した事実をSIRへ混ぜることはしません。
- 4.1・4.3のクラス・連想配列・unpacked unionの構想には、まだTypeKindや
  TypeEntryKindに実装のないものがあります。11ではstreaming・代入パターン・
  動的データ命令が将来の項目ですが、一部は既存のlowering・5.8・5.9にあります。
  表示用pattern.formatだけでソース`%p`や全代入パターンを実装したことにはなりません。
- verifierの検査範囲は均一ではありません。Syscall/Builtinは主に名前を検査し、
  完全なシグネチャ表はありません（1877〜1909）。Loadはオペランド0がSlotのときに
  型を比べるだけの分岐です（1280〜1283）。非修飾refや添字付きrefの書き込みは
  通常のstoreと同じ対象型検査を持ちません（1400〜1448）。これはコード調査による
  指摘です。不正なDesignを実際に受理してどう動くかは **not confirmed** であり、
  独自のverifier陰性テストで確認します。既存の期待値は緩めません。
- SIR_SPEC 11とtext.hの文法確定・読み戻しの期限はP1です。本案のP2確定へ変更する
  ことには人の承認が必要です。これを理由にP1の他の義務が終わったとはしません。

### 2-b. 現在のテキスト形式（format 0）

[text.cpp](../../src/sir/text.cpp):26〜403と[text.h](../../src/sir/include/kirimasim/sir/text.h)は
出力だけを実装します。追跡対象のsrc/sirとdriverにはSIRの読み戻しパーサや入口が
見つかりませんでした。[version.cpp](../../src/sir/version.cpp)は0を返し、toTextは別に
`; sir format 0`を固定文字列で出します。「読み戻せる」は目的で、往復の実装は未完成です。

以下はwriterから抽出した現行文法の骨格で、承認済みの完全な文法ではありません。
NLは宣言・命令の区切り、IDは10進数、Qは引用符で囲むbyte列です。
エスケープは`\"`・`\\`・`\n`・`\t`、制御byteは`\xHH`、他はそのまま出ます。

```ebnf
text0       = "; sir format 0", NL, "precision", time, NL,
              { source }, { instance }, { type-row }, { pattern-row },
              { slot-row }, { alias }, { subroutine }, { process } ;
source      = "source", uint, Q, NL ;
instance    = "instance", uint, Q, "def=", Q, "parent=", (uint | "-"),
              [ "package" | "unit" | "program" ], NL ;
slot-row    = "slot", slotref, Q, ":", value-type, { dimension }, slot-kind,
              "inst=", uint, [ "sub=", subref ], [ "type=", typeref ],
              [ "driver-of=", slotref, "strength=(", uint, ",", uint, ")" ],
              [ "flags(", flag, { ",", flag }, ")" ], NL ;
alias       = "alias", Q, "->", slotref, NL ;
body        = "{", [ location ], NL, { frame-row }, block, { block }, "}", NL ;
frame-row   = "frame", frameref, Q, ":", value-type,
              [ "<", value-type, ">" ], { dimension }, [ "type=", typeref ], NL ;
block       = blockref, [ "(", tempref, ":", value-type,
              { ",", tempref, ":", value-type }, ")" ], ":", NL, instruction,
              { instruction } ;
instruction = [ tempref, "=" ], opname, [ value-type ],
              [ operand, { ",", operand } ], [ location ], NL ;
operand     = tempref | "const", value-type, literal | [ edge ], slotrange
            | frameref, [ bitrange ] | [ edge ], subref | processref
            | typeref | patternref | scoperef | Q
            | blockref, [ "(", operand, { ",", operand }, ")" ] ;
location    = ";", uint, ":", uint, ":", uint ;
```

省略した現行生成規則も有限のwriter分岐です。type行はenum値、packed/unpackedの
メンバー、配列の要素・次元、UDP行、dynarrayとqueueの容量を出します。
pattern行は種類・値・要素・個数・タグ、enum名とmember名のescape・bit位置を出します。
subroutineは名前・種類・automatic/static・may-wait・instance・戻り型・frame-of・param、
processは名前・種類・instance・parent・trigger/iff・writes/drivesを持ちます。
wait.eventのguardは**カンマなしの`iff @N`**、ブロック引数はcount分の後続operandです。
@Nは役割によってprocessかsubroutineかが決まります。カンマだけで分割しては
復元できません。text.cpp:103〜125・202〜239・287〜319・357〜400が根拠です。

現在の省略・情報損失は次のとおりです。SIR_SPEC 2・9とDesignを照合しました。

| 項目 | 現在の出力 | 読み戻しへの影響 |
| --- | --- | --- |
| dumpScopes・dumpVars | ない。2節が省略を明記 | $dumpvarsの行番号や波形選択を復元できず、verifierのdump表検査にも必要 |
| numScopes | ない | 使用済みIDから最小個数は分かるが、未使用の末尾を復元できない |
| process/subroutineのnumTemps | ない | 最大TempId+1は推定できるが、未使用分は失われる |
| slotのoffset/domain | offsetは省略、domainは型から決まる | 承認した対象layoutから再計算し、nativeアドレスは保存しない |
| 主たる名前 | slot名と同じaliasは省略 | 再構築が必要。元の名前表の順序・重複は表現されない |
| 定数表のID・未使用定数 | 使用箇所・enum・patternで値を直接表示 | 再internでIDや順序が変わり、未使用定数が消える |
| real/shortrealのbit | to_charsによる10進表示 | 全NaN payload/signを保持する保証がない。具体的な失敗入力は **not confirmed**。IEEEの分類とpayloadで検査する |
| operandのcount・edge・role | 括弧と文脈に展開。@Nは多義 | Opと表の種類に基づく解析が必要 |
| sources・instance種別・frame/target・pattern | 種類に応じて出力 | IDとbyte列を保つ。ソースパス正規化は別に決める |
| 非適用field・未知flag bit・位置の欠落 | writerは適用fieldと既知flagのみを出し、line=0の位置を省略 | 未検証structの全bitを保存するdumpではない。v1では非正規fieldを拒否するか明示保存する |
| contのdrives | writesと同じなら省略 | contのときだけdrives=writes、他は空と復元する |

現行dumpテストは**29設計**、固定P1母集団は**1,466項目**です。どちらも現状では
読み戻しのテストではありません。母集団の全設計がこの時点でloweringできるかは
本調査では **not confirmed** です。現時点の未対応を記録し、P1後のP2完了時には
全設計で往復できることを要求します。成功した部分だけを母集団と呼び直しません。

### 2-c. VMのカーネルC++ APIとC関数表

付録Bはvm.cppから参照するカーネル関数・メンバーの全件です。同じ関数の複数の呼び出しは
行番号を併記します。wide算術へ渡す関数ポインタ、kw別名、Rng別名、safe-point hookの
scheduler別名も含みます。データ型、enum値、集約初期化、暗黙の自明なコピー、std・SIR・
VMの関数は対象外です。HeapRoot生成とID/statusの小さなアクセサも明記します。
カーネル内部の推移的な全呼び出しグラフを主張する一覧ではありません。
既存コンパイラのASTでも呼び出しを照合しました。P1で別名が増えたら更新が必要です。

[abi.h](../../src/kernel/include/kirimasim/kernel/abi.h)は**ABI 0.3、呼べるfieldが18個**です。
add_process・start・wait_delay・wait_event・wait_monitor・wait_named・write・schedule_nba・
trigger・trigger_nb・triggered・time・request_end・ending・fail・slot_words・wait_any・wait_mixedです。
[abi.cpp](../../src/kernel/abi.cpp)がC++の同じ処理へ適合します。
ただしwaitNamedのflushPoint=false（join用、1383行）はCのwait_namedにはありません。
heapを含むmonitorの情報も現在のC構造体にはありません。名前付きの混合waitAnyには
wait_anyだけでなく**wait_mixed**が対応します。

不足の群はRUNTIME_ABI 4.2の「待ち・スケジューリング」「値の書き込み」「時刻・終了」
「システムタスク」「オブジェクト」「報告」で分類します。Context・counter・stack・
host初期化は4.2にはない基盤なので、3・5・9節に基づく例外として区別します。
純粋な公開inline値演算は3節の経路で、C関数表にあるとは書きません。

### 2-d. vm.cpp内にある意味の判断

命令と型の選択を超える条件を、行番号・重複の理由・移動先で整理します。
すべてを不具合と断定する一覧ではありません。機械的なdispatch、frame再利用、
ブロック引数の同時コピーで使う小bufferの選択はVMに残せます。

| vm.cpp行・仕組み | 命令と型以外の条件／重複する判断 | 移す先／影響する仕様 |
| --- | --- | --- |
| 189–206, 391–421, 492–505 | 数字文字列の受理、符号付き整数/実数変換、実数truth、X/Z分岐、時刻の綴りが値に依存する | 純粋値はvalsem、診断・時刻表示はkernel。VALUE_SEMANTICS 3/7、SYSTEM_TASKS 10 |
| 660–680, 735–785 | frame ref/配列、既定slot bit、driverの初期X、要素別event、force復元が記憶域と由来で変わる | front/SIRの共通layout descriptor、kernelの初期化/hold。SIR 3/7.4、ABI 4.3 |
| 793–876, 1737–1769, 1791–1841 | RNGの生成順、program origin、static-init順、時刻0起動、感度の登録時点がprocess種別/状態で変わる | frontが明示descriptor、kernelが起動・感度・program方針。SIR 7.1/7.6、SCHEDULER 6/12 |
| 1265–1425 | 子のbirth/group、空join、終了後の子孫保持、報告をflushしないjoin待ち | kernelのprocess/activationサービス。SCHEDULER 10、ABI 4.2/4.3/6 |
| 1463–1505, 2559–2563 | Postponedの書き込み禁止がlocal/static subroutine記憶域と外部/heap refを区別 | kernelのeffect検査、frontの対象scope/provenance。SCHEDULER 2.2、SIR 5/10 |
| 1531–1544, 2630–2641, 4098–4105 | 未知delayは0、上位語や時刻overflowは実行エラーになる | valsemのdelay変換、kernelの時刻上限。SIR 1/7.2、SCHEDULER 8 |
| 1693–1731, 3485–3489, 4025–4072 | 宣言方向、未知/範囲外添字、切り詰め、位置別警告回数 | frontが次元/windowを保持、valsemが添字/clip、kernelが診断。VALUE_SEMANTICS 3.11、CLI 2.2 |
| 1903–2083, 3258–3324 | 初期化、全引数の捕捉、ref束縛、static/automatic copy-out、disable時の出力抑止 | frontの明示call binding、kernelのactivation/ref寿命と共通copy。SIR 7.4/8、ABI 4.3 |
| 2085–2240, 2309–2359 | slot/frame/heap tag、古いidentity、call/report/NBAのroot、入れ子receiver | kernelの不透明ref/root visitor、frontのdescriptor。SIR 5.8/8、ABI 6 |
| 2243–2306, 2362–2526 | heap statusの言語診断化、確保、通知、実receiverによるqueue操作 | kernelのcontainer facadeと共通診断、valsemのcell規則。SIR 4.2.1/5.8、ABI 6 |
| 2537–2578, 2598–2600, 2760–2838 | step上限、分岐checkpoint、callback後cancel、時刻overflowとend note | kernelのcheckpoint/status/error。SCHEDULER 16、ABI 4.4 |
| 2644–2709, 2886–3117 | watch/guard/named event、null、read依存の発見、呼出先local除外、heap依存 | front/SIRで意味上のread/effect、kernelでref束縛と待ち管理。SIR 7.2/8、SCHEDULER 9.5 |
| 2747–2827, 2850–2884, 3235–3244 | descriptorとhold状態に基づくfile/global monitor、保持bitの復元 | kernelのmonitor/override。SIR 5.6/7.1、SYSTEM_TASKS 4/6.2 |
| 3120–3233 | scope/task disableがcall深さとbirthでactivation/子孫をunwindしcopy-outを抑える | kernelのactivation/unwind、frontのscope ID/exit。SCHEDULER 10.4/10.7 |
| 3246–3257, 3371–3439 | 無効状態の既定値、driver maskと幅に基づくstrength解決 | valsemの参照意味、kernel高速版。SIR 3.5、VALUE_SEMANTICS 5 |
| 3492–3856, 3952–4023 | wideのunknown/select/reduction/power/shift/論理含意にローカルな値判断がある | valsemとkernelの幅汎用helper、VMは型/幅dispatchだけ。VALUE_SEMANTICS 3/7 |
| 4106–4259 | 文字列繰り返し警告、未知条件の合流、文字列添字、shortreal丸め、実数truth | valsemの規則、kernel文字列/実数演算と診断状態。VALUE_SEMANTICS 3.9/7.3–7.6 |
| 4270–4559 | refの種類と無効要素、NBA捕捉、遅延assertionの引数、null event | kernelのref/NBA/report、frontの明示capture。SIR 5.3/5.7/8 |
| 4655–4740, 5293–5383 | 未知scan形式、readmem方向/既定範囲、scan書戻し、部分fread、memory出力の直接stdio | kernel scan/memory-file facade、valsem変換。SYSTEM_TASKS 6/7 |
| 4748–4861 | null triggered、文字列method境界、case folding、数字規則、substring範囲 | valsemの参照意味とkernel string-method facade。VALUE_SEMANTICS 7.6、SYSTEM_TASKS |
| 4864–5040, 5062–5133 | patternの走査/union view、実行時container長、format引数消費 | frontが表示layout、kernelが捕捉/走査/表示。SIR 5.9、SYSTEM_TASKS 3.9/3.13 |
| 5135–5181 | 警告の重複抑止、procedure/initializer RNGの選択、seed再設定 | kernel診断/RNG Contextサービス。SYSTEM_TASKS 12/13、UNSPECIFIED K1–K3 |
| 5188–5290, 5386–5946 | stack診断scope、dump種別、descriptor検査、timeformat、fatal/exit、分布乱数、plusargs、syscall変換 | frontがtask/method IDとmetadata、kernelがtask facade、valsemが変換。SYSTEM_TASKS、WAVEFORM、ABI 4.2/4.4 |

実行時の状態を調べる処理をfrontへ移しません。スケジューラの判断はvalsemへ置きません。
両tierは同じkernelの操作へ入り、高速な値演算をvalsemと突き合わせます。
型・幅による実装の選択は機械的なtierの処理です。参照・root・cancelは承認済みの
動的コンテナ契約を守って共有します。

### 2-e. 未確定事項をいつ決めるか

SIR_SPEC 11とRUNTIME_ABI 8の各箇条書きをすべて分類しました。
P4以降にできるのはP1で必要な範囲の外だけです。

| 出典・未確定事項 | P3（C++生成）前に必要 | P4以降でよい部分 |
| --- | --- | --- |
| SIR 11 streaming/代入パターン | P1完了時の全encodingを明記し、表示と構築を区別、P1の追加Opもschemaへ入れる | P1外の追加機構のみ |
| SIR 11 unpacked union | 完了P1が受けるなら配置を確定、他は明示拒否と範囲を記録 | 必要なP1外の広いunionのみ |
| SIR 11 frameサイズ/整列 | 共通descriptorとlayoutが必須 | 現存型は延期しない |
| SIR 11 class/dynamic/random/assertion/coverage命令 | 完了P1のclass/dynamic/VPIに必要な表現、古い一律後回し記述を解消 | P1外のconstraint/assertion/coverageはP4 |
| SIR 11 階層解決 | frontの規則とエラーを確定、実行時はIDのみ | P1外の追加解決機構 |
| SIR 11 テキスト文法 | P2文法/パーサ/往復。「P1末」の修正を明示承認 | SIR v1に必要な部分は延期しない |
| SIR 11 slang対応表 | P1受理構文からSIRへの全対応と明示拒否 | 後の言語追加 |
| ABI 8 関数表/C構造体 | P1の全実行効果とmodel loadに必須 | 本当に後の機能だけのentry |
| ABI 8 status/error ID | 意味・構造化エラー・ソース対応を確定 | 意味を変えない診断追加 |
| ABI 8 一時値/共有frame | SSAは待ちをまたがず、捕捉FrameVarとscratchを分離 | tier切替の実行採否はP3 |
| ABI 8 Windows | C幅・calling convention・loader所有を決め、Windows AOT有効化前にMSVC C++20 fixture | 完全なWindows AOT toolchain/性能検査はP3。load実行は検査まで **not confirmed** |
| ABI 8 class/mailbox/weak GC/fiber/DPI task | P1に必要なclass root/寿命・親frame保持、拡張可能なhook | P1外のweak/mailbox/fiber/DPI taskはP4 |
| ABI 8 子が使う親frame保持 | capture案の承認待ちでも、activation所有/layout/rootの口を決める | P1に必要なら延期しない |
| ABI 8 DPI exportのfinish/引数寿命 | 一般の終了/status/借用buffer寿命は今決める | DPI特有のexport/svLogicVecVal/open arrayは、P1に不要な部分はP4 |
| ABI 8 並列static island | 副作用要約と逐次参照意味を記録 | thread分割/並列実行はP5 |

## 3. 設計詳細（方針は承認済み、詳細の留保は上記参照）

### D1. テキスト文法・パーサ・版

**format 1は完全な正規record形式とする案を推奨**します。現行v0 dumpは移行中の
別の診断表示として残します。Designの意味に関わる全field、dump表、宣言した個数を
保存します。波形情報を捨てたり、IDを振り直したり、元ソースから別Designを再生成して
往復できたことにはしません。既存の道具で書く手製のbyte lexerとschema依存parserを使い、
パーサ生成器の依存は足しません。公開前に既存verifierを通し、構文・検証エラーでは実行可能な
Designを返しません。確保前に個数・サイズ上限とoverflowを検査します。

外側・字句文法は次のEBNFです。合法なrecord・field・型・operandの役割は下表とD2で
決め、未知の属性を汎用的に受け入れません。WSはASCII空白・tab・CR・LFでtoken間に
許します。数や識別子の中には入れません。`//`コメントはLFまでで意味上の位置ではなく、
ソース位置は明示fieldです。designの後はEOFでなければなりません。
EBNFの引用terminal内のbackslashはC記法で、文字範囲はASCII順です。enum atomには
StaticInitなどschemaの名前を使い、旧表示のハイフン入り綴りを暗黙に受け入れません。

```ebnf
file       = "sir", "1", record, EOF ;
record     = ident, "{", { field }, "}" ;
field      = ident, "=", value, ";" ;
value      = sint | bytes | ident | list | record ;
list       = "[", [ value, { ",", value } ], "]" ;
sint       = [ "-" ], digit, { digit } ;
ident      = letter, { letter | digit | "_" | "." } ;
bytes      = '"', { printable | escape }, '"' ;
escape     = '\\', ( '"' | '\\' | "n" | "t" | "x", hex, hex ) ;
digit      = "0" | "1" | "2" | "3" | "4" | "5" | "6" | "7" | "8" | "9" ;
letter     = "A" … "Z" | "a" … "z" ;
hex        = digit | "a" … "f" | "A" … "F" ;
```

printableは0x20〜0x7eから引用符とbackslashを除いたbyteです。他は小文字のhex2桁による
`\xHH`とし、後続がhexでも必ず2桁だけ消費します。空文字・NULも表現します。
locale依存の数・指数表記・UTF-8検証は使いません。正規整数は0か先頭非0の数で、
plus・負の0は出しません。入力の余分な空白・コメントを受けても出力では正規化します。
boolはtrue/false、IDなしはnone、uintには負数や範囲外を認めません。wordはuint64全域です。
realは10進数ではなく**IEEEの生bit**をuint語で保存します。語は下位から並べ、余剰bitは0です。

必須recordのfieldと出力順を以下で定義します。既定値・空listもすべて出します。
重複・未知のkeyは拒否し、vectorの位置がIDです。typeはkind,width,detail、locは
file,line,column、dimensionはleft,right、ownerはkind,id、rangeはslot,firstElem,
numElems,lsb,widthです。該当しないIDはnoneにします。

| record | field（順序も固定） |
| --- | --- |
| design | precision, sources, instances, types, patterns, slots, names, constants, subroutines, processes, numScopes, dumpScopes, dumpVars |
| instance | scopeName, definition, parent, kind |
| typeEntry | kind, name, value, items, fields, element, dims, inputs, sequential, rows, initial, maxElements |
| enumItem / field / udpRow | name,value / name,lsb,width,type / inputs,state,output |
| patternLayout | kind,value,enumerators,members,element,count,tag,tagEscaped |
| patternEnumerator / patternMember | name,value,escaped / name,layout,escaped,lsb |
| slot | name,type,kind,dims,flags,instance,subroutine,declType,driverOf,strength1,strength0 |
| nameEntry / constant | path,slot / type,aWords,bWords,bytes |
| subroutine | name,kind,automatic,mayWait,instance,params,returnType,frame,blocks,numTemps,frameOf,loc |
| param / frameVar | name,dir,type,slot,frameVar / name,type,declType,dims,target |
| process | name,kind,instance,triggers,writes,drives,frame,blocks,numTemps,parent,loc |
| trigger / block / blockParam | range,edge,guard / params,instrs / temp,type |
| instr | op,type,result,operands,loc |
| operand | kind,id,lsb,width,firstElem,numElems,count,edge,text |
| dumpScope / dumpVar | name,path,kind,parent / scope,name,range,type,width,slot |


各listの要素はschemaで固定します。sourcesはbyte列、wordはuint64、IDはuint32かnone、
enumは正規の名前です。先頭recordはdesign、入れ子のtagは表中のものとtype・loc・dimension・
owner・rangeに限ります。任意の拡張recordは実行しません。slot offsetは対象別の導出値で、
保存しません。未使用を含めIDはvector indexを保ち、operand kindも明記します。
drivesとdump情報は省略しません。凍結までに増えたP1 fieldもschemaへ加えます。
これは既存Designの交換表現で、別のIRや新しい命令集合ではありません。

保証は`print1(parse1(print1(D))) == print1(D)`のbyte単位の一致と、offset/cacheを除く
意味上のDesign一致です。parse直後にverifierが通り、実行とVCDの観測を保ちます。
非正規入力Tはprint1(parse1(T))で正規化し、次から固定点になります。一般のparse0は
約束しません。既存goldenはv0を保ち、移行するなら期待値だけの別の承認済み変更にします。
新しい往復jobは各設計をfrontでSIRにしてformat 1で往復し、既存.expectedを変更しません。
公開するv1 dump/loadのCLI名はCLI.mdの修正として承認が必要で、隠れた設定は作りません。

文法・schemaの承認、reader/writer、全件の往復、エラーとOp照合のgateが揃って初めて
formatVersionを1にします。writer headerとloaderも同じ版の定義を使います。
1以降は解釈を変えうるOp・field・型・役割・意味の変更ごとに整数版を上げ、未知の版を
拒否します。正規byte列が変わる表示変更も版を上げます。enumの順序値をwire tagにせず、
必要な版移行は別の明示的な道具・決定にします。製品版・ABI major/minor・解析表schema版は別です。

**代案：** 現在の行単位文法を完全に拡張します。見た目の変化は減りますが、定数・dump・
個数の節とroleの区別が結局必要です。JSONと人向けdumpを分ける案もありますが、依存・
byte列・uint64の方針は別途承認が必要です。JSON/MLIR/FileCheckなどは導入しません。
**影響：** SIR 1・2・4・5・9・10・11、CLI 2、TEST_SUITEと期待値保護。

### D2. Opの定義を1か所に置く

sirに宣言的schemaを置き、名前・オペランド役割と個数・型関係のkey・結果・終端・
join種類を含む待ち・副作用・X生成可能性・仕様の節を記載する案を推奨します。
既存Pythonでenum・名前表・parserの構造検査・共通verifier・仕様の全命令表を生成します。
不規則な型規則は名前付きC++ validatorに残し、schemaがその関数名を列挙しCIが存在を検査します。
Syscall/Builtinにも意味のIDとシグネチャ表を置き、frontか共通load準備で名前を解決します。
tierごとに名前の意味を再解釈しません。

CIは一時ディレクトリで再生成し、保存済みの生成物との差とschema・enum・仕様表の
**集合の完全一致**を検査します。未知・欠落・重複・未処理・古い名前・型規則keyの不足は失敗です。
生成は意味の正しさの証明ではないため、行列・verifier陰性テスト・valsemとの比較も残します。
inside/cond.xは人が決めてから規範的な表にします。外部生成器は要りません。

**代案：** 手書きswitchを保って抽出・照合します。初期実装は小さくなりますが、名前だけでは
役割・待ち・副作用が守れず、同じmetadataの検査が必要です。
**影響：** SIR 5・7・8・10、CLAUDEの意味を1か所で決める規則、TEST_SUITE。

### D3. 共通frameと参照の配置

SIRから一度計算する**対象別layout descriptor**を両tierで使う案を推奨します。
C++20のstatic_assertで生成側の配置も検査します。std::vectorのsizeofやnative classの
配置は境界に出しません。制御recordはkernel所有で不透明にし、payloadはoverflowを
検査するbyte offset・サイズ・整列・activation/root mapで記述します。

最初の保守的な案は、整数はceil(width/64)個（最低1個）、real・shortreal・string・event・
chandle・container tokenは1個のcellです。cellはuint64語2本の16byte、整列は8byteです。
型ごとの未使用b語・上位bitは0とし、4値は既存a/b符号を保ちます。固定配列は宣言順で
右端次元が最も速く進みます。FrameVarId順にoffset=alignUp(previousEnd,8)、
全体size=alignUp(end,8)とします。unpacked集約のleafは確定したSIR descriptorに従います。

refはVMの現行bit tagを公開せずkernelの不透明tokenにします。未承認の詳細候補はuint64×2の16byteで、
Contextの表にkind・activation/cell identity・generation・要素/leaf/window・権限を持たせます。
世代の切り捨ては認めません。完了したP1を安全に表現できなければ、v1凍結前に別サイズを
承認します。無効refとGC rootはkernelが検査し、実行時pointerは保存形式に出しません。
root mapにはref・container・classの位置と要素descriptorを記載します。

SSA temp・ブロック引数は待ちをまたがないscratchです（SIR 5.1・10）。待ち後に必要な値は
frontが明示FrameVarにします。NBAや遅延呼び出しは自分の捕捉値を保ちます。resume位置・
呼び出し鎖・名前付きscope・保持する親はscratchとは別にkernelの寿命を持ちます。
将来のtier切り替えはdescriptor/版の一致が必要ですが、P2で切り替え実行は要求しません。

**代案：** ドメイン別に詰める、または現VMの「scalarを先、配列を後」のW4 vector配置
（660〜680行）を採用します。前者は対象別packingの検証が増え、後者はVMの都合が永続的な
境界になります。PERFORMANCEに沿って保守案を測ってから凍結します。
**影響：** SIR 3・4・5.1・7.4・8・11、ABI 3・4.3・6・8、未承認のautomatic-capture/class-runtime案。

### D4. C境界を揃え、tier 1のC++直接呼び出しは保つ

付録Bの**A行は必要な操作に対応するC entryの候補**です。承認された方針に従い、
型付きサービスへ整理し、各行がどのサービスで満たされるか対応表を保ちます。
181候補の関数数・名前・prototypeは個別には未承認です。確定したC/C++の両経路は
同じkernel実装へ入ります。std型・例外・RTTI・VM Frame・SIR DesignはC境界に出さず、固定幅整数、
pointer/countのbyte・word span、Context所有の不透明ID、plain C構造体を使います。
結果bufferは呼び出し側所有です。借用viewの有効期限は呼び出し/指定checkpointまでと明記し、
後の確保の都合で暗黙に無効化しません。サービス境界・entry名・prototype・H/I例外は実装前に別途レビューします。

- プロセス・activation：子/program登録、起動・final・終了報告、遅延driver、cancel・kill・解放、
  fork/join・scope/task unwind、flushしないjoin待ち、heap付きmonitor、override、file/global monitor。
- 時刻・終了・診断：checkpoint、overflow・resource/runtime error、error ID・位置・scope、
  終了理由/note、stack trace。
- 報告：deferred report/callのenqueue、捕捉引数の保持、実行/破棄/cancelの寿命。
  C callback typedefだけでは登録関数の不足を解決しません。
- システムタスク：Files・Output・Dump・Strings、format・scan・memory file・乱数、
  共通シグネチャ表で解決したtask/methodの引数束縛と書き戻し。
- オブジェクト：型layout、確保、安定cell、queue変更・copy/snapshot、root visitor/safepoint、
  monitor依存、借用read。
- out-of-lineの値ヘルパー：wide算術、実数/文字列変換、strength/UDP、表示などのC span適合関数。

2-dの判断を移すため、まだC++ APIにまとまっていない共有サービスとして、さらに
`activation_enter`・`activation_leave`・`activation_disable`・`activation_retain`・
`activation_release`・`ref_make`・`ref_read`・`ref_write`・`ref_schedule_nba`・`ref_validate`・
`fork_begin`・`join_wait`・`process_checkpoint`・`diagnostic_emit`・`task_invoke`・
`method_invoke`・`frame_layout_get`・`model_initialize`・`memory_file_read`・`memory_file_write`を提案します。typed ID・権限・捕捉・
effectを明示した要求/結果でkernelサービスを呼ぶのであり、C関数表の裏に別のbytecode
interpreterを置く案ではありません。frontが意味のdescriptorを渡し、tierは判断を再発明しません。

既存0.3のfield offsetと構造体配置を保ちます。互換な関数は末尾へ加えてminorを上げます。
monitor拡張も新構造体・新entryを使い、既存の値渡し構造体のfieldを同じABIで増やしません。
末尾にcapability/size queryを追加し、major一致・必要minor以上・必要機能の存在を確かめて
新fieldへ触れます。既存prototype・値/ref配置・calling convention・意味を変えるならmajor変更です。
P2完了時に**ABI 1.0**へ安定させる案を推奨します。そこまでの0.x minorは途中のP1変更に
依存するので **not confirmed** で、今「0.4」と決めつけません。SIR v1とABI 1.0は独立した判断です。

H行はP3でもC++ loader/driverやtier内に残し、生成resumeから呼びません。I行はABI 3節の
公開C++20 inline値演算を同じ実装のまま使います。表に理由とP3での再確認を記載し、
JIT採否前にも見直します。P1の実行操作をP4という理由だけで外しません。

**代案：** P3までABI 0.xを保つと変更しやすい代わり、model再生成が増えます。
完全な対応と互換性検査はその案でも必要です。汎用request dispatcher一本の案はfieldを
減らせますが、別interpreter/命令契約を隠しやすいため、型付きのサービスentryを推奨します。
**影響：** ABI 2〜6・8・9、SIR 5・7・8、SYSTEM_TASKS・WAVEFORM・CODEGEN_SPEC。

### D5. switchだけでなく判断の責任を移す

2-dの責任分担を仕組みごとに承認します。frontは意味ID・捕捉・origin・位置/scope・
範囲・read/writeの由来を明示します。kernelは実行中のactivation・effect・待ち・状態と
エラーを持ち、valsemは値・添字・変換・文字列の参照意味を定めます。高速kernelをそれと比較します。
SIRの純粋なlayout/descriptor計算は共有できますが、kernel→sir依存は作りません。
front/loaderがplain kernel dataへ変換します。

**代案：** 現VMの判断をそのまま共通helper libraryにします。重複は減りますが、判断の
置き場所を正さず依存規則も壊しかねません。codegen→VM、kernel→slang/SIRにはしません。
**影響：** CLAUDEと2-dで列挙した各仕様の節。

### D6. 解析結果の別表

SIRの内容hash・formatVersion・解析schema版・対象/layout ID・pass/options hashに紐づく
不変の別表を推奨します。slotはSlotId、値や命令はbody種別/body ID/BlockId/TempId/
operand indexなど、scope付きのIDで引きます。文字列名で引きません。
proven-no-X/unknown、island所属、level順、可観測/force-preserve理由を別々に持ち、
根拠種別と生成側の版も記録します。欠落・古い表は**unknown**であり、最適化許可ではありません。
使用前にhash・ID範囲・型幅・依存順序を検査します。解析の事実をSIRには書きません。

P2はschema・失効規則・検証口とunknown/空fixtureを決め、アルゴリズムと計算はP3です。
**代案：** 巨大設計では疎なmapを使い、保存時はkey順に並べます。identity/unknown規則は同じです。
SIRを書き換えて解析の事実を埋める案は1節に反します。
**影響：** SIR 1・5.2、CODEGEN_SPEC static islands、ARCHITECTUREのanalysis。

### D7. P1との順序

A案はP1完了後にP2をすべて始めます。機能一覧が一度固まり変更の往復は減りますが、
parser・境界の不足が遅れて見つかり、P3着手が遅れる可能性があります。

**推奨のB案**は、棚卸し・schema検査・parser試作・共有サービスの計画をP1と並行します。
P1を優先しformatVersionは0を保ちます。P1でOp・型・kernel操作が増えるたびに更新し、
P1完了・残る意味修正の人の承認・全gate達成後にだけ凍結します。不足を早く見つける利点と、
繰り返しのschema変更・調整コストがあります。毎回の基準commit/hashを記録し、
現在の部分的な一覧でP2完了とはしません。

2026-10-24に人がB案を選びました。進め方とP2での凍結期限は承認済みです。
未確定と記した詳細契約は、実装前に後続レビューを行います。
**影響：** SIR 1・9・11のP1中の文法期限、ABI 8、P1_SCOPE、README。

## 4. マイルストーンと完了条件

1. **M0：詳細契約のレビュー。** 方針と並行準備・P1後凍結の順序は2026-10-24に承認済みです。
   6節に残る文法/schema・API・参照配置・意味の判断をレビューし、仕様だけのOpとP1の
   未承認案を整合します。実装前に承認された方針を対象仕様へ反映し、詳細の未確定と区別します。
   方針の承認だけでM0完了とはしません。
2. **M1：実行できるOp/schema契約。** P1の全Op・型・task/methodのシグネチャとeffectを
   揃え、CIでenum・生成metadata・仕様表の集合一致と生成物の古さを検査します。
   Op不足/追加・待ち/型/シグネチャの誤り・古い別表を陰性対照で拒否できることを示します。
3. **M2：パーサと正規テキスト。** SIR_SPEC 9の文法を確定し、parse直後にverifierを通します。
   **tests/dump_sirとtests/gates/p1_population.txtの全設計**でprint1→parse1→print1が
   byte単位で一致し、復元Designが検証を通ることを要求します。試行/成功/未対応/失敗を
   分け、未対応・失敗0件を完了条件にします。凍結前のP1追加も含めます。
   byte/ID/定数/NaN bit/符号付きzero/wide XZ・ブロック引数・scope・pattern・container・
   dumpvars表・不正/過大入力も検査します。すべて揃ってformatVersionを1にし、既存v0期待値を保ちます。
4. **M3：配置とABI。** frame/refのサイズ・整列・root/寿命・エラーを確定し、C++20 fixtureで
   offset・C callback・版不一致拒否・本物のkernelで小modelの停止/再開を検査します。
   これは境界fixtureで、P3の生成器ではありません。VMの実行操作はすべてC表に対応するか、
   理由と次の確認時期のあるH/I例外をレビューします。P1完了時に付録Bを更新しC/C++経路を比較します。
5. **M4：意味の共有と回帰なし。** 2-dの独立判断をVMに残さず、承認された責任者と入口、
   行列とvalsem比較を持ちます。P1母集団・sv-tests・CTest・PerfCountの既存結果を保ちます。
   新テストは増やせますが、実装と同じコミットで既存期待値を直しません。全local_ciと外部gateを
   固定入力・オプション・ツール版で実行し、未対応を成功と数えません。**本ドキュメント作業では
   外部gateを再計測していません。**
6. **M5：P3へ渡す。** P1が完了し、証拠をレビューしP2阻害事項0、版/capability検査が有効で、
   英日仕様が一致していること。Design・layout descriptor・任意の解析別表・kernel APIを渡し、
   第2のIRは渡しません。

速度は[PERFORMANCE 4・5](../spec/PERFORMANCE.ja.md)の現行基準で評価します。
PerfCount期待値と10%の回帰検査を保ち、期待値変更は別の承認済み変更にします。
実時間/RSSは5回の中央値、過去7夜の中央値をbaselineとし、15%の実時間/RSS・25%のbuild警告を
新しい合否条件に変えません。build/run/parseを分け、対象・コンパイラ・オプションを記録し、
機械の識別情報は記録しません。parserの起動コストを隠さず、compact配置は測定して比較します。

## 5. リスクと未確認事項

- P1未完了のためOp・object/ABI・layoutが増えます。P1完了時に更新し、最終gateまで0を保ちます。
- dump表省略・定数inlineのため、今のdumpだけを読む実装では不十分です。文字列の固定点だけで
  意味の一致を証明せず、metadata・実行・VCDも検査します。
- VMのref tagを偶然公開しません。automatic captureとclass runtimeの案は承認待ちで、
  最終の意味・layoutは **not confirmed** です。承認とP1実装を確認してからtokenを凍結します。
- 診断・GC・cancelの移動で順序、報告の寿命、seed、out/ref、PerfCountが変わりえます。
  既存の行列を保ち、食い違いは期待値変更より先に調べます。
- 全real bitとWindows ABIの実行は **not confirmed** で、bit-pattern/MSVC fixtureで確認します。
  今回その検査を実行したとは主張しません。
- 呼び出し一覧は字句抽出と調査を基に既存コンパイラASTでも照合しましたが、将来の別名・macroを
  自動的に検出し続けるCI gateは **not confirmed** です。既存toolchainのASTかリポジトリ内検査器で
  凍結前に検査し、新しいanalyzerは承認なしに導入しません。
- 新しいベンチマーク、sv-tests、外部母集団の合格率は今回再計測していません。同等性の証拠ではありません。
- MLIR・LLVM FileCheck・parser framework・serialization libraryを含む依存や新しい道具の導入は
  別途人が決めます。推奨案はそれらを必要とせず、今回も導入していません。

## 6. Points to decide（後続レビューで決める詳細）

1. format 1の正確なEBNF・field schema・byte/数の規則・版管理・v0/v1 dump/loadの公開CLI名。
   完全なrecord形式とP1後の凍結は選択済みです。
2. Op/task/method schemaと生成物の境界。inside/cond.xと不足するverifier契約を、
   承認済みの意味や既存期待値を黙って緩めずに整合する方法。
3. frame配置の測定結果、ref tokenのサイズ・表現、activation/rootの寿命、対象別の検査。
   P1の関連契約が固まってからレビューします。16byte cell・8byte整列は評価基準で、ABI凍結ではありません。
4. サービス境界・API名/prototype/関数数・エラー/所有権、各host/inline例外と全参照の対応。
   181個のA行と追加サービス候補名は関数一覧として未承認です。Windows AOT有効化前の確認条件も決めます。
5. 解析別表の正確なschema・hash/ID・失効検査。別表化と欠落/古い結果をunknownとする方針は
   選択済みで、アルゴリズムはP3です。
6. assertion/randomize/library-exportのPriority P3とP4/P5の実装計画の関係、
   承認待ちのP1意味修正案の個別承認。この方針承認では解決していません。

## 当初の提案文書の検査記録

フェーズ変更の全local CIと明示的な期待値保護検査は[対応ノート](phase-renumbering.ja.md)に
記載しました。本提案はenumの124件と全Op名、英日274 API行・行番号・181追加entry名、
両EBNF、localリンク、スキップ対象不変、非公開pathの不在を検査しました。
既存コンパイラをbuild/devのcompile commandに従って
`-fsyntax-only -Xclang -ast-dump=json`で動かし、member/free-functionの参照も照合しました。
ID/statusの小さなアクセサ6行もその結果です。新しいcompilerは導入していません。
これは棚卸しを作る手順の確認で、提案中のparserやABIを実装・検証した記録ではありません。
提案コミットの変更は本ノートの英日2ファイルだけです。

## 付録A. Op全件

英語版と同じ124行を記載します。識別子と型の表記は正本と同じです。条件の意味を変えずに日本語で記載します。
result onlyは純粋な結果生成で、必要な文字列internなどを含むことがあります。
read/create/writeなど状態のある操作は個別に記載します。no=いいえ、yes=はい、may=停止しうる、
result=結果、same=同じ型、fits=代入可能、integral=整数、equivalent=型の等価性です。
verifier行番号は検査の入口で、外部の出典ではありません。

| Op / テキスト名 | オペランド | 現行の型規則 | 終端 | 待つ条件 | 書くもの・副作用 | SIR_SPEC節・verifier行 |
| --- | --- | --- | --- | --- | --- | --- |
| `Load` / `load` | slot range | 結果＝slot範囲の値型 | いいえ | no | slotのread | 5.2; 1280 |
| `Store` / `store` | slot range, value | 値が対象へ代入可能 | いいえ | no | slot now / queued NBA | 5.2; 1284 |
| `Nba` / `nba` | slot range, value | 値が対象へ代入可能 | いいえ | no | slot now / queued NBA | 5.2; 1285 |
| `NbaDelay` / `nba.delay` | slot range, value, ticks | 値が対象へ代入可能、ticksは整数 | いいえ | no | queued slot NBA | 5.2; 1293 |
| `DriveDelay` / `drive.delay` | slot range, value, rise, fall, off | contのみ、対象と値とticksは整数 | いいえ | no | delayed inertial driver update | 5.4; 1303 |
| `LoadElem` / `load.elem` | slot/frame array, index... | 次元ごとに整数添字1個、結果＝要素型 | いいえ | no | 要素のread／既定値 | 5.3; 1314 |
| `StoreElem` / `store.elem` | slot/frame array, index..., value | 要素へ代入可能、整数添字、NBAはframe不可 | いいえ | no | element now / queued NBA | 5.3; 1315 |
| `NbaElem` / `nba.elem` | slot/frame array, index..., value | 要素へ代入可能、整数添字、NBAはframe不可 | いいえ | no | element now / queued NBA | 5.3; 1316 |
| `NbaElemDelay` / `nba.elem.delay` | slot array, index..., value, ticks | 整数添字/ticks、値は要素へ代入可能 | いいえ | no | captured element NBA | 5.3; 7.3; 1317 |
| `StoreBits` / `store.bits` | slot/frame, index..., position, value | 整数添字/位置、同じdomain、幅≦親、NBAはframe不可 | いいえ | no | clipped bits now / NBA | 5.3; 1342 |
| `NbaBits` / `nba.bits` | slot/frame, index..., position, value | 整数添字/位置、同じdomain、幅≦親、NBAはframe不可 | いいえ | no | clipped bits now / NBA | 5.3; 1343 |
| `NbaBitsDelay` / `nba.bits.delay` | slot, index..., position, value, ticks | nba.bitsと同じ、ticksは整数 | いいえ | no | captured clipped bits NBA | 5.3; 7.3; 1344 |
| `Fload` / `fload` | frame | 結果と同じ型、scalarまたはref | いいえ | no | frameのread | 7.4; 1364 |
| `Fstore` / `fstore` | frame, value | scalar frame/refへ代入可能 | いいえ | no | frameの値 | 7.4; 1370 |
| `RefMake` / `ref.make` | slot/frame, [index...] | 結果はref、整数添字は0個か次元数、container対象は修飾 | いいえ | no | reference value | 8; 1376 |
| `LoadRef` / `load.ref` | ref, [constant leaf] | 結果は非void/非ref、修飾refは対象と同型、修飾containerにleaf不可 | いいえ | no | read referent | 8; 1400 |
| `StoreRef` / `store.ref` | ref, [constant leaf], value | 値operand、修飾refは対象と同型、非修飾の対象は実行時に表現 | いいえ | no | referent now / queued NBA | 8; 1401 |
| `NbaRef` / `nba.ref` | ref, [constant leaf], value | 値operand、修飾refは対象と同型、非修飾の対象は実行時に表現 | いいえ | no | referent now / queued NBA | 8; 1402 |
| `LoadRefElem` / `load.ref.elem` | ref frame argument, index... | 次元数の整数添字、結果＝refの対象型 | いいえ | no | read referent element | 5.3; 8; 1418 |
| `StoreRefElem` / `store.ref.elem` | ref frame argument, index..., value | 次元数の整数添字、値operand（対象型への適合検査は不完全） | いいえ | no | referent element | 5.3; 8; 1419 |
| `StoreRefBits` / `store.ref.bits` | ref frame argument, index..., position, value | 添字/位置/対象は整数、有効な親window、値operand（適合検査は不完全） | いいえ | no | clipped referent bits | 5.3; 8; 1420 |
| `ContainerNew` / `container.new` | size, initializer | 結果dynarray<!T>、sizeはs4<64>、初期値の要素は等価型 | いいえ | no | new independent heap cells | 4.2.1; 5.8; 1188 |
| `ContainerCopy` / `container.copy` | container | 結果container、元と結果の要素が等価型 | いいえ | no | new independent heap cells; destination bounds | 5.8; 1189 |
| `ContainerSnapshot` / `container.snapshot` | container | 元と結果のcontainer型が一致 | いいえ | no | new independent cells; actual source bounds | 5.8; 1190 |
| `ContainerAssign` / `container.assign` | !T, receiver ref, source | ref<!T>、元の要素が等価型 | いいえ | no | receiver handle/cells after evaluating source | 5.8; 1230 |
| `ContainerPack` / `container.pack` | element value... (zero allowed) | 各値が結果containerの要素と等価型 | いいえ | no | new heap cells | 5.8; 1191 |
| `ContainerSize` / `container.size` | container | 結果i2<64> | いいえ | no | read logical size | 5.8; 1215 |
| `ContainerRead` / `container.read` | container, index | 整数添字、結果＝要素型 | いいえ | no | read cell/default | 5.8; 1216 |
| `ContainerCell` / `container.cell` | !T, receiver ref, index | ref<!T>と整数添字、結果は要素ref、入れ子なら修飾 | いいえ | no | read cell identity | 5.8; 1229 |
| `ContainerWrite` / `container.write` | !T, receiver ref, index, value | ref<!T>と整数添字、要素が等価型 | いいえ | no | receiver cell; change notifications | 5.8; 1231 |
| `ContainerClear` / `container.clear` | !T, receiver ref | ref<!T> | いいえ | no | receiver handle/cells | 5.8; 1232 |
| `QueueInsert` / `queue.insert` | queue !T, receiver ref, index, value | ref<!T>と整数添字、要素が等価型 | いいえ | no | queue structure/cells | 5.8; 1233 |
| `QueuePushFront` / `queue.push.front` | queue !T, receiver ref, value | ref<!T>、要素が等価型 | いいえ | no | queue structure/cells | 5.8; 1234 |
| `QueuePushBack` / `queue.push.back` | queue !T, receiver ref, value | ref<!T>、要素が等価型 | いいえ | no | queue structure/cells | 5.8; 1235 |
| `QueuePopFront` / `queue.pop.front` | queue !T, receiver ref | ref<!T>、結果＝要素型 | いいえ | no | queue structure/cells; result/default | 5.8; 1236 |
| `QueuePopBack` / `queue.pop.back` | queue !T, receiver ref | ref<!T>、結果＝要素型 | いいえ | no | queue structure/cells; result/default | 5.8; 1237 |
| `QueueErase` / `queue.erase` | queue !T, receiver ref, index | ref<!T>と整数添字 | いいえ | no | queue structure/cells | 5.8; 1238 |
| `Add` / `add` | x, y | operandと結果は同じ整数型または実数型 | いいえ | no | 結果のみ | 5.2; 1450 |
| `Sub` / `sub` | x, y | operandと結果は同じ整数型または実数型 | いいえ | no | 結果のみ | 5.2; 1451 |
| `Mul` / `mul` | x, y | operandと結果は同じ整数型または実数型 | いいえ | no | 結果のみ | 5.2; 1452 |
| `Div` / `div` | x, y | operandと結果は同じ整数型または実数型 | いいえ | no | 結果のみ | 5.2; 1453 |
| `Mod` / `mod` | x, y | operandと結果は同じ整数型 | いいえ | no | 結果のみ | 5.2; 1466 |
| `Pow` / `pow` | base, exponent | 底と結果は同じ整数型、指数は任意の整数型、または全て同じ実数型 | いいえ | no | 結果のみ | 5.2; 1458 |
| `Neg` / `neg` | x | operandと結果は同じ整数型または実数型 | いいえ | no | 結果のみ | 5.2; 1474 |
| `Lt` / `lt` | x, y | operandは同じ整数/実数/string/chandle型、結果は同domainの1bit | いいえ | no | 結果のみ | 5.2; 1483 |
| `Le` / `le` | x, y | operandは同じ整数/実数/string/chandle型、結果は同domainの1bit | いいえ | no | 結果のみ | 5.2; 1484 |
| `Gt` / `gt` | x, y | operandは同じ整数/実数/string/chandle型、結果は同domainの1bit | いいえ | no | 結果のみ | 5.2; 1485 |
| `Ge` / `ge` | x, y | operandは同じ整数/実数/string/chandle型、結果は同domainの1bit | いいえ | no | 結果のみ | 5.2; 1486 |
| `Eq` / `eq` | x, y | operandは同じ整数/実数/string/chandle型、結果は同domainの1bit | いいえ | no | 結果のみ | 5.2; 1487 |
| `Ne` / `ne` | x, y | operandは同じ整数/実数/string/chandle型、結果は同domainの1bit | いいえ | no | 結果のみ | 5.2; 1488 |
| `Ceq` / `ceq` | x, y | operandは同じ整数型、結果は2値1bit | いいえ | no | 結果のみ | 5.2; 1519 |
| `Cne` / `cne` | x, y | operandは同じ整数型、結果は2値1bit | いいえ | no | 結果のみ | 5.2; 1520 |
| `Weq` / `weq` | x, y | operandは同じ整数型、結果は同domainの1bit | いいえ | no | 結果のみ | 5.2; 1499 |
| `Wne` / `wne` | x, y | operandは同じ整数型、結果は同domainの1bit | いいえ | no | 結果のみ | 5.2; 1500 |
| `MatchMask` / `match.mask` | x, y, casez/casex | operandは同じ整数型、結果は2値1bit | いいえ | no | 結果のみ | 5.2; 1508 |
| `Lnot` / `lnot` | x | 入力は整数/実数/chandle、結果は入力と同domainの1bit | いいえ | no | 結果のみ | 5.2; 1528 |
| `Land` / `land` | x, y | operandは整数/実数、結果1bit、入力のどちらかが4値なら4値 | いいえ | no | 結果のみ | 5.2; 1546 |
| `Lor` / `lor` | x, y | operandは整数/実数、結果1bit、入力のどちらかが4値なら4値 | いいえ | no | 結果のみ | 5.2; 1547 |
| `Limp` / `limp` | x, y | operandは整数/実数、結果1bit、入力のどちらかが4値なら4値 | いいえ | no | 結果のみ | 5.2; 1548 |
| `Leqv` / `leqv` | x, y | operandは整数/実数、結果1bit、入力のどちらかが4値なら4値 | いいえ | no | 結果のみ | 5.2; 1549 |
| `Truth` / `truth` | x | 入力は整数/実数/chandle、結果は入力と同domainの1bit | いいえ | no | 結果のみ | 5.2; 1529 |
| `And` / `and` | x, y | operandと結果は同じ整数型 | いいえ | no | 結果のみ | 5.2; 1467 |
| `Or` / `or` | x, y | operandと結果は同じ整数型 | いいえ | no | 結果のみ | 5.2; 1468 |
| `Xor` / `xor` | x, y | operandと結果は同じ整数型 | いいえ | no | 結果のみ | 5.2; 1469 |
| `Xnor` / `xnor` | x, y | operandと結果は同じ整数型 | いいえ | no | 結果のみ | 5.2; 1470 |
| `Not` / `not` | x | operandと結果は同じ整数型 | いいえ | no | 結果のみ | 5.2; 1479 |
| `Rand` / `rand` | x | 入力は整数、結果は同domainの1bit | いいえ | no | 結果のみ | 5.2; 1536 |
| `Rnand` / `rnand` | x | 入力は整数、結果は同domainの1bit | いいえ | no | 結果のみ | 5.2; 1537 |
| `Ror` / `ror` | x | 入力は整数、結果は同domainの1bit | いいえ | no | 結果のみ | 5.2; 1538 |
| `Rnor` / `rnor` | x | 入力は整数、結果は同domainの1bit | いいえ | no | 結果のみ | 5.2; 1539 |
| `Rxor` / `rxor` | x | 入力は整数、結果は同domainの1bit | いいえ | no | 結果のみ | 5.2; 1540 |
| `Rxnor` / `rxnor` | x | 入力は整数、結果は同domainの1bit | いいえ | no | 結果のみ | 5.2; 1541 |
| `Shl` / `shl` | x, amount | xと結果は同じ整数型、移動量は整数 | いいえ | no | 結果のみ | 5.2; 1558 |
| `Shr` / `shr` | x, amount | xと結果は同じ整数型、移動量は整数 | いいえ | no | 結果のみ | 5.2; 1559 |
| `Ashr` / `ashr` | x, amount | xと結果は同じ整数型、移動量は整数 | いいえ | no | 結果のみ | 5.2; 1560 |
| `Select` / `select` | condition, x, y | 条件は1bit、両選択肢と結果は同じ非void型 | いいえ | no | 結果のみ | 5.2; 1598 |
| `Bitsel` / `bitsel` | x, index | operandは整数、結果はxと同domainの符号なし1bit | いいえ | no | 結果のみ | 5.2; 1603 |
| `Partsel` / `partsel` | x, constant position | 位置は既知の64bit以下の整数、範囲はx内、結果はxと同domainで符号なし | いいえ | no | 結果のみ | 5.2; 1608 |
| `IdxPartsel` / `idxpartsel` | x, position | operandは整数、結果はxと同domainの符号なし整数 | いいえ | no | 結果のみ | 5.2; 1625 |
| `Concat` / `concat` | x... (nonempty) | 全てstringならstring、整数なら結果は幅の和で符号なし、domainは入力の和集合 | いいえ | no | 結果のみ | 5.2; 1630 |
| `Repl` / `repl` | count, x | 整数定数count>0、符号なし結果の幅=count*width(x)、または整数countとstringからstring | いいえ | no | 結果のみ | 5.2; 1654 |
| `Zext` / `zext` | x | 同domainの整数、結果幅≧入力幅 | いいえ | no | 結果のみ | 5.2; 1671 |
| `Sext` / `sext` | x | 同domainの整数、結果幅≧入力幅 | いいえ | no | 結果のみ | 5.2; 1672 |
| `Trunc` / `trunc` | x | 同domainの整数、結果幅≦入力幅 | いいえ | no | 結果のみ | 5.2; 1676 |
| `To2` / `to2` | x | 4値から同じ幅/符号の2値へ | いいえ | no | 結果のみ | 5.2; 1680 |
| `To4` / `to4` | x | 2値整数から同じ幅/符号の4値へ | いいえ | no | 結果のみ | 5.2; 1684 |
| `Itor` / `itor` | x | 整数からreal/shortrealへ | いいえ | no | 結果のみ | 5.2; 1689 |
| `Rtoi` / `rtoi` | x | real/shortrealから整数へ | いいえ | no | 結果のみ | 5.2; 1693 |
| `Rconv` / `rconv` | x | realとshortrealの間、同じ型への変換は不可 | いいえ | no | 結果のみ | 5.2; 1697 |
| `Itos` / `itos` | x | 整数からstringへ | いいえ | no | 結果のみ | 5.2; 1701 |
| `Stoi` / `stoi` | x | stringから整数へ | いいえ | no | 結果のみ | 5.2; 1705 |
| `StrGet` / `str.get` | string, index | 整数添字、結果は2値8bit | いいえ | no | 結果のみ | 5.2; 1709 |
| `Trigger` / `trigger` | event slot | slotにある1個のevent値 | いいえ | no | named event generation | 5.2; 7.2; 1714 |
| `TriggerNb` / `trigger.nb` | event slot, [ticks] | event slot、任意の整数ticks | いいえ | no | queued named event generation | 5.2; 7.3; 1719 |
| `Disable` / `disable` | process/subroutine/text | 識別子種別を検査、verifierは旧text指定も許す | いいえ | no | process/call cancellation | 5.2; 7.2; 1725 |
| `DisableFork` / `disable.fork` | none | void | いいえ | no | descendant cancellation | 7.2; 7.5; 1730 |
| `ReportDefer` / `report.defer` | construct, content, scope | 制約されたtext operand3個 | いいえ | no | pending report queue | 5.2; 1844 |
| `ReportCall` / `report.call` | #0/final, function, values... | function、input引数だけ、各値が代入可能 | いいえ | no | captured deferred-call queue | 5.7; 1823 |
| `Resolve` / `resolve` | net kind, (value, strength1, strength0, [side])... | 結果4値、driverは同型、強さは定数0..7、任意のsideは2値で幅2*w | いいえ | no | 結果のみ | 3.5; 5.2; 1564 |
| `Udp` / `udp` | UDP !T, inputs... | 組み合わせUDP表、各入力1bit、結果i4<1> | いいえ | no | 結果のみ | 5.5; 1760 |
| `UdpSeq` / `udp.seq` | UDP !T, state, previous..., current... | 順序UDP表、stateと2*n個の1bit値、結果i4<1> | いいえ | no | result only (state stored explicitly) | 5.5; 1761 |
| `OverrideStart` / `override.start` | assign/force, override process, targets... | 保持対象に印のある整数範囲、processはoverride | いいえ | no | hold registration and process scheduling | 5.6; 1786 |
| `OverrideStop` / `override.stop` | assign/force, targets... | 保持対象に印のある整数範囲 | いいえ | no | hold removal and restoration | 5.6; 1787 |
| `StoreOverride` / `store.override` | assign/force, slot range, value | override process、整数値の幅＝範囲、writes内 | いいえ | no | held slot bits | 5.6; 1803 |
| `Strobe` / `strobe` | postponed process, [descriptor] | postponed種別、任意の整数descriptor | いいえ | no | postponed run / monitor registration | 7.1; 1747 |
| `Monitor` / `monitor` | postponed process, [descriptor] | postponed種別、任意の整数descriptor | いいえ | no | postponed run / monitor registration | 7.1; 1748 |
| `ScopeEnter` / `scope.enter` | scope #N, exit block | scope<numScopes、引数なしblock | いいえ | no | activation record | 7.2; 1731 |
| `ScopeLeave` / `scope.leave` | scope #N | scope<numScopes | いいえ | no | activation record | 7.2; 1736 |
| `DisableScope` / `disable.scope` | scope #N | scope<numScopes | いいえ | no | activation unwind and descendant cancellation | 7.2; 1737 |
| `DisableTask` / `disable.task` | task @N | task種別 | いいえ | no | task activations and descendants | 7.2; 8; 1741 |
| `Call` / `call` | subroutine, args... | guard/watch以外、mayWait=false、引数/方向/戻り型が一致 | いいえ | no | callee effects + copy-out + optional result | 8; 1863 |
| `Syscall` / `syscall` | $name, operands... | 名前の先頭は$、結果は任意、$monitor/$dumpvarsに追加制約、全taskの型検査はない | いいえ | no | task-specific I/O, state, scheduler, heap or result | 5.2; SYSTEM_TASKS; 1877 |
| `FormatPattern` / `pattern.format` | !pN, named/positional, values... | 有効なpattern木、payload形状が一致、結果string | いいえ | no | result/string allocation; heap reads | 5.9; 1170 |
| `Builtin` / `builtin` | method name, operands... | 空でないtext、結果は任意、methodごとの型検査はない | いいえ | no | method-specific result/string/event access | 5.2; 8; 1906 |
| `Br` / `br` | block(args...) | 引数個数/型がblockと一致 | はい | いいえ | parallel block-argument binding | 5.1; 7.2; 1910 |
| `BrIf` / `br.if` | condition, true block(args...), false block(args...) | 条件1bit、block引数が一致 | はい | いいえ | selected parallel block binding | 5.1; 7.2; 1913 |
| `Ret` / `ret` | [value] | voidのprocess/taskは値なし、それ以外は戻り型に代入可能 | はい | いいえ | return/copy-out | 7.2; 8; 1920 |
| `WaitDelay` / `wait.delay` | ticks, continuation | ticksは整数、継続blockは引数なし | はい | 停止しうる | wait registration | 7.2; 1929 |
| `WaitEvent` / `wait.event` | slot/watch [iff guard]..., continuation | slot範囲または待ち元frameのwatch/guard、継続blockは引数なし | はい | 停止しうる | mixed wait registration | 7.2; 1933 |
| `WaitCond` / `wait.cond` | guard, continuation | guardは待ち元frameを読み1bitを返す、継続blockは引数なし | はい | 偽なら | monitor registration if false | 7.2; 1964 |
| `WaitNamed` / `wait.named` | event slot, continuation | event slot、継続blockは引数なし | はい | 停止しうる | named wait registration | 7.2; 1973 |
| `Fork` / `fork` | child process..., join kind, continuation | 子は呼出元が所有、join/join_any/join_none、継続blockは引数なし | はい | join_none以外。直ちに完了する場合もある | children + join state | 7.2; 7.5; 1979 |
| `WaitFork` / `wait.fork` | continuation | 継続blockは引数なし | はい | 生きた直接の子があれば | join state | 7.2; 7.5; 2001 |
| `CallTask` / `call.task` | task, args..., continuation | taskはmayWait=true、引数一致、継続blockは引数なし | はい | 停止しうる | call frame + callee effects | 7.2; 8; 2005 |

## 付録B. カーネルAPI全件と追加案

274種類のAPI参照です。A= P3前にレビュー済みサービスで満たす操作の候補（公開関数一覧の承認ではない）、H= host/tier内部に残しP3で再確認する例外、I= 公開inline実装を使いP3で再確認する例外、existing= 現存C entryです。Aは181行（現存サービスの部分的な不足3件を含む）、Hは19行、Iは60行、完全対応は14行です。Hのstack/counterは生成コードの停止・stack検査を省く許可ではなく、D4のprocess_checkpointで実施します。行番号はすべてsrc/vm/vm.cppです。追加関数は元のC++ signatureをそのまま公開せず、C互換viewへ適合します。関数名と行番号は英語正本と同一です。

| C++ API参照 | vm.cpp行 | ABI 0.3の対応／不足する群 | 追加entry案または例外の理由 |
| --- | --- | --- | --- |
| `counters_.add` | 887 | なし。基盤（ABI 3/5/9、4.2の群ではない） | H: P3でもhost loader/driverかtier内計数。生成resumeからは呼ばない |
| `counters_.increment` | 888, 913, 922, 939, 952, 965 | なし。基盤（ABI 3/5/9、4.2の群ではない） | H: P3でもhost loader/driverかtier内計数。生成resumeからは呼ばない |
| `ctx.counters` | 623 | なし。基盤（ABI 3/5/9、4.2の群ではない） | H: P3でもhost loader/driverかtier内計数。生成resumeからは呼ばない |
| `ctx.dump` | 622 | なし。基盤（ABI 3/5/9、4.2の群ではない） | H: P3でもhost loader/driverかtier内計数。生成resumeからは呼ばない |
| `ctx.files` | 622 | なし。基盤（ABI 3/5/9、4.2の群ではない） | H: P3でもhost loader/driverかtier内計数。生成resumeからは呼ばない |
| `ctx.heap` | 623 | なし。基盤（ABI 3/5/9、4.2の群ではない） | H: P3でもhost loader/driverかtier内計数。生成resumeからは呼ばない |
| `ctx.output` | 621 | なし。基盤（ABI 3/5/9、4.2の群ではない） | H: P3でもhost loader/driverかtier内計数。生成resumeからは呼ばない |
| `ctx.randomSeed` | 622 | なし。基盤（ABI 3/5/9、4.2の群ではない） | H: P3でもhost loader/driverかtier内計数。生成resumeからは呼ばない |
| `ctx.scheduler` | 621 | なし。基盤（ABI 3/5/9、4.2の群ではない） | H: P3でもhost loader/driverかtier内計数。生成resumeからは呼ばない |
| `ctx.stack` | 624 | なし。基盤（ABI 3/5/9、4.2の群ではない） | H: P3でもhost loader/driverかtier内計数。生成resumeからは呼ばない |
| `ctx.store` | 621 | なし。基盤（ABI 3/5/9、4.2の群ではない） | H: P3でもhost loader/driverかtier内計数。生成resumeからは呼ばない |
| `ctx.strings` | 623 | なし。基盤（ABI 3/5/9、4.2の群ではない） | H: P3でもhost loader/driverかtier内計数。生成resumeからは呼ばない |
| `dump_.all` | 5493 | なし。システムタスク | A: `dump_all` |
| `dump_.file` | 5472 | なし。システムタスク | A: `dump_file` |
| `dump_.finish` | 901 | なし。システムタスク | A: `dump_finish` |
| `dump_.flush` | 5495 | なし。システムタスク | A: `dump_flush` |
| `dump_.limit` | 5494 | なし。システムタスク | A: `dump_limit` |
| `dump_.off` | 5491 | なし。システムタスク | A: `dump_off` |
| `dump_.on` | 5492 | なし。システムタスク | A: `dump_on` |
| `dump_.setup` | 885 | なし。システムタスク | A: `dump_setup` |
| `dump_.vars` | 5488 | なし。システムタスク | A: `dump_vars` |
| `files_.atEnd` | 5701 | なし。システムタスク | A: `files_at_end` |
| `files_.close` | 5766 | なし。システムタスク | A: `files_close` |
| `files_.error` | 5711 | なし。システムタスク | A: `files_error` |
| `files_.flush` | 5772 | なし。システムタスク | A: `files_flush` |
| `files_.flushAll` | 5771 | なし。システムタスク | A: `files_flush_all` |
| `files_.getChar` | 5612 | なし。システムタスク | A: `files_get_char` |
| `files_.openFd` | 5579 | なし。システムタスク | A: `files_open_fd` |
| `files_.openMcd` | 5584 | なし。システムタスク | A: `files_open_mcd` |
| `files_.readBytes` | 5660 | なし。システムタスク | A: `files_read_bytes` |
| `files_.readLine` | 5599 | なし。システムタスク | A: `files_read_line` |
| `files_.scan` | 5644 | なし。システムタスク | A: `files_scan` |
| `files_.seek` | 5694 | なし。システムタスク | A: `files_seek` |
| `files_.targets` | 5279 | なし。システムタスク | A: `files_targets` |
| `files_.tell` | 5674 | なし。システムタスク | A: `files_tell` |
| `files_.ungetChar` | 5619 | なし。システムタスク | A: `files_unget_char` |
| `files_.write` | 5557, 5569 | なし。システムタスク | A: `files_write` |
| `heap_.actualType` | 2335 | なし。オブジェクト | A: `heap_actual_type` |
| `heap_.addLayout` | 2290 | なし。オブジェクト | A: `heap_add_layout` |
| `heap_.addType` | 2292 | なし。オブジェクト | A: `heap_add_type` |
| `heap_.cell` | 2418 | なし。オブジェクト | A: `heap_cell` |
| `heap_.clear` | 2437 | なし。オブジェクト | A: `heap_clear` |
| `heap_.collect` | 2231 | なし。オブジェクト | A: `heap_collect` |
| `heap_.construct` | 2376 | なし。オブジェクト | A: `heap_construct` |
| `heap_.copy` | 2377, 2431 | なし。オブジェクト | A: `heap_copy` |
| `heap_.describeType` | 2413 | なし。オブジェクト | A: `heap_describe_type` |
| `heap_.describeWord` | 2327 | なし。オブジェクト | A: `heap_describe_word` |
| `heap_.erase` | 2439 | なし。オブジェクト | A: `heap_erase` |
| `heap_.fromValues` | 2392 | なし。オブジェクト | A: `heap_from_values` |
| `heap_.insert` | 2448 | なし。オブジェクト | A: `heap_insert` |
| `heap_.memoryResource` | 4888 | なし。オブジェクト | A: `heap_memory_resource` |
| `heap_.needsCollection` | 2230 | なし。オブジェクト | A: `heap_needs_collection` |
| `heap_.pop` | 2441 | なし。オブジェクト | A: `heap_pop` |
| `heap_.push` | 2450 | なし。オブジェクト | A: `heap_push` |
| `heap_.reaches` | 3034 | なし。オブジェクト | A: `heap_reaches` |
| `heap_.read` | 2407, 5018 | なし。オブジェクト | A: `heap_read` |
| `heap_.readCell` | 2343, 2480 | なし。オブジェクト | A: `heap_read_cell` |
| `heap_.size` | 2401, 4948 | なし。オブジェクト | A: `heap_size` |
| `heap_.snapshot` | 2378 | なし。オブジェクト | A: `heap_snapshot` |
| `heap_.storeNested` | 2354, 2520 | なし。オブジェクト | A: `heap_store_nested` |
| `heap_.words` | 2479 | なし。オブジェクト | A: `heap_words` |
| `heap_.write` | 2446 | なし。オブジェクト | A: `heap_write` |
| `heap_.writeCell` | 2524 | なし。オブジェクト | A: `heap_write_cell` |
| `kernel::HeapLayoutId::index` | 2422 | なし。純粋値（ABI 3、4.2の関数表の操作ではない） | I: P3で共通の公開C++20 inlineを使う。動的kernel symbol不要 |
| `kernel::HeapResult<kernel::HeapTypeId>::operator bool` | 2336 | なし。純粋値（ABI 3、4.2の関数表の操作ではない） | I: P3で共通の公開C++20 inlineを使う。動的kernel symbol不要 |
| `kernel::HeapRoot::cell` | 2120 | なし。オブジェクト | A: `heap_root_cell` |
| `kernel::HeapRoot::container` | 2108, 3032 | なし。オブジェクト | A: `heap_root_container` |
| `kernel::HeapStatus::operator bool` | 2236, 2253 | なし。純粋値（ABI 3、4.2の関数表の操作ではない） | I: P3で共通の公開C++20 inlineを使う。動的kernel symbol不要 |
| `kernel::HeapTypeId::index` | 2318 | なし。純粋値（ABI 3、4.2の関数表の操作ではない） | I: P3で共通の公開C++20 inlineを使う。動的kernel symbol不要 |
| `kernel::PatternFormatResult::operator bool` | 5027 | なし。純粋値（ABI 3、4.2の関数表の操作ではない） | I: P3で共通の公開C++20 inlineを使う。動的kernel symbol不要 |
| `kernel::Rng::child` | 803, 1402 | なし。システムタスク | A: `rng_child` |
| `kernel::Rng::draw` | 5180 | なし。システムタスク | A: `rng_draw` |
| `kernel::SlotId::index` | 741, 742, 743, 2134 | なし。純粋値（ABI 3、4.2の関数表の操作ではない） | I: P3で共通の公開C++20 inlineを使う。動的kernel symbol不要 |
| `kernel::StackRange::ofThisThread` | 723 | なし。基盤（ABI 3/5/9、4.2の群ではない） | H: P3でもhost loader/driverかtier内計数。生成resumeからは呼ばない |
| `kernel::add4` | 4565 | なし。純粋値（ABI 3、4.2の関数表の操作ではない） | I: P3で共通の公開C++20 inlineを使う。動的kernel symbol不要 |
| `kernel::allX` | 3247, 4630, 5918 | なし。純粋値（ABI 3、4.2の関数表の操作ではない） | I: P3で共通の公開C++20 inlineを使う。動的kernel symbol不要 |
| `kernel::and4` | 4599 | なし。純粋値（ABI 3、4.2の関数表の操作ではない） | I: P3で共通の公開C++20 inlineを使う。動的kernel symbol不要 |
| `kernel::asSigned` | 413 | なし。純粋値（ABI 3、4.2の関数表の操作ではない） | I: P3で共通の公開C++20 inlineを使う。動的kernel symbol不要 |
| `kernel::ashr4` | 4612 | なし。純粋値（ABI 3、4.2の関数表の操作ではない） | I: P3で共通の公開C++20 inlineを使う。動的kernel symbol不要 |
| `kernel::bits4` | 4620, 4625, 4630 | なし。純粋値（ABI 3、4.2の関数表の操作ではない） | I: P3で共通の公開C++20 inlineを使う。動的kernel symbol不要 |
| `kernel::caseMatch4` | 4586 | なし。純粋値（ABI 3、4.2の関数表の操作ではない） | I: P3で共通の公開C++20 inlineを使う。動的kernel symbol不要 |
| `kernel::ceq4` | 4581 | なし。純粋値（ABI 3、4.2の関数表の操作ではない） | I: P3で共通の公開C++20 inlineを使う。動的kernel symbol不要 |
| `kernel::cne4` | 4582 | なし。純粋値（ABI 3、4.2の関数表の操作ではない） | I: P3で共通の公開C++20 inlineを使う。動的kernel symbol不要 |
| `kernel::compileUdpRow` | 652 | なし。値の書き込み／純粋helper適合口（ABI 3） | A: `value_compile_udp_row` |
| `kernel::concat4` | 4636 | なし。純粋値（ABI 3、4.2の関数表の操作ではない） | I: P3で共通の公開C++20 inlineを使う。動的kernel symbol不要 |
| `kernel::countBits4` | 5430 | なし。純粋値（ABI 3、4.2の関数表の操作ではない） | I: P3で共通の公開C++20 inlineを使う。動的kernel symbol不要 |
| `kernel::distChiSquare` | 5746 | なし。システムタスク | A: `system_dist_chi_square` |
| `kernel::distErlang` | 5750 | なし。システムタスク | A: `system_dist_erlang` |
| `kernel::distExponential` | 5742 | なし。システムタスク | A: `system_dist_exponential` |
| `kernel::distNormal` | 5740 | なし。システムタスク | A: `system_dist_normal` |
| `kernel::distPoisson` | 5744 | なし。システムタスク | A: `system_dist_poisson` |
| `kernel::distT` | 5748 | なし。システムタスク | A: `system_dist_t` |
| `kernel::distUniform` | 5738 | なし。システムタスク | A: `system_dist_uniform` |
| `kernel::div4` | 4568 | なし。純粋値（ABI 3、4.2の関数表の操作ではない） | I: P3で共通の公開C++20 inlineを使う。動的kernel symbol不要 |
| `kernel::dumpOrder` | 5331 | なし。システムタスク | A: `system_dump_order` |
| `kernel::eq4` | 4579 | なし。純粋値（ABI 3、4.2の関数表の操作ではない） | I: P3で共通の公開C++20 inlineを使う。動的kernel symbol不要 |
| `kernel::eventHandle` | 773 | なし。値の書き込み／純粋helper適合口（ABI 3） | A: `value_event_handle` |
| `kernel::eventOfHandle` | 3091, 4745, 4755 | なし。値の書き込み／純粋helper適合口（ABI 3） | A: `value_event_of_handle` |
| `kernel::formatInt` | 4851, 5086, 5342 | なし。システムタスク | A: `system_format_int` |
| `kernel::formatPattern` | 5024 | なし。システムタスク | A: `system_format_pattern` |
| `kernel::formatReal` | 4855, 5080 | なし。システムタスク | A: `system_format_real` |
| `kernel::formatString` | 5072 | なし。システムタスク | A: `system_format_string` |
| `kernel::formatStringCall` | 5529 | なし。システムタスク | A: `system_format_string_call` |
| `kernel::formatTime` | 5085 | なし。システムタスク | A: `system_format_time` |
| `kernel::formatTimeReal` | 5074 | なし。システムタスク | A: `system_format_time_real` |
| `kernel::formattedString` | 5092 | なし。システムタスク | A: `system_formatted_string` |
| `kernel::fullyInside` | 4033, 4059 | なし。純粋値（ABI 3、4.2の関数表の操作ではない） | I: P3で共通の公開C++20 inlineを使う。動的kernel symbol不要 |
| `kernel::ge4` | 4578 | なし。純粋値（ABI 3、4.2の関数表の操作ではない） | I: P3で共通の公開C++20 inlineを使う。動的kernel symbol不要 |
| `kernel::gt4` | 4577 | なし。純粋値（ABI 3、4.2の関数表の操作ではない） | I: P3で共通の公開C++20 inlineを使う。動的kernel symbol不要 |
| `kernel::initialState` | 799 | なし。システムタスク | A: `system_initial_state` |
| `kernel::inside` | 1719, 2511, 3616, 3939 | なし。純粋値（ABI 3、4.2の関数表の操作ではない） | I: P3で共通の公開C++20 inlineを使う。動的kernel symbol不要 |
| `kernel::land4` | 3721, 3725, 3726, 4254, 4590 | なし。純粋値（ABI 3、4.2の関数表の操作ではない） | I: P3で共通の公開C++20 inlineを使う。動的kernel symbol不要 |
| `kernel::le4` | 4576 | なし。純粋値（ABI 3、4.2の関数表の操作ではない） | I: P3で共通の公開C++20 inlineを使う。動的kernel symbol不要 |
| `kernel::lnot4` | 3709, 3723, 3726, 4222, 4257, 4588, 4592 | なし。純粋値（ABI 3、4.2の関数表の操作ではない） | I: P3で共通の公開C++20 inlineを使う。動的kernel symbol不要 |
| `kernel::loadMemory` | 5366 | なし。システムタスク | A: `system_load_memory` |
| `kernel::lor4` | 3722, 3723, 3727, 4255, 4257, 4591, 4592 | なし。純粋値（ABI 3、4.2の関数表の操作ではない） | I: P3で共通の公開C++20 inlineを使う。動的kernel symbol不要 |
| `kernel::lt4` | 4575 | なし。純粋値（ABI 3、4.2の関数表の操作ではない） | I: P3で共通の公開C++20 inlineを使う。動的kernel symbol不要 |
| `kernel::mask` | 759, 760, 1722, 1723, 2283, 3428, 4722, 4723, 4737, 4765 | なし。純粋値（ABI 3、4.2の関数表の操作ではない） | I: P3で共通の公開C++20 inlineを使う。動的kernel symbol不要 |
| `kernel::mod4` | 4569 | なし。純粋値（ABI 3、4.2の関数表の操作ではない） | I: P3で共通の公開C++20 inlineを使う。動的kernel symbol不要 |
| `kernel::mul4` | 4567 | なし。純粋値（ABI 3、4.2の関数表の操作ではない） | I: P3で共通の公開C++20 inlineを使う。動的kernel symbol不要 |
| `kernel::ne4` | 4580 | なし。純粋値（ABI 3、4.2の関数表の操作ではない） | I: P3で共通の公開C++20 inlineを使う。動的kernel symbol不要 |
| `kernel::neg4` | 4574 | なし。純粋値（ABI 3、4.2の関数表の操作ではない） | I: P3で共通の公開C++20 inlineを使う。動的kernel symbol不要 |
| `kernel::negate1` | 3744, 3746, 3748 | なし。純粋値（ABI 3、4.2の関数表の操作ではない） | I: P3で共通の公開C++20 inlineを使う。動的kernel symbol不要 |
| `kernel::not4` | 4603 | なし。純粋値（ABI 3、4.2の関数表の操作ではない） | I: P3で共通の公開C++20 inlineを使う。動的kernel symbol不要 |
| `kernel::or4` | 4600 | なし。純粋値（ABI 3、4.2の関数表の操作ではない） | I: P3で共通の公開C++20 inlineを使う。動的kernel symbol不要 |
| `kernel::parseFormat` | 1236 | なし。システムタスク | A: `system_parse_format` |
| `kernel::plusargValue` | 5866 | なし。システムタスク | A: `system_plusarg_value` |
| `kernel::position4` | 3487 | なし。純粋値（ABI 3、4.2の関数表の操作ではない） | I: P3で共通の公開C++20 inlineを使う。動的kernel symbol不要 |
| `kernel::pow4` | 4571 | なし。純粋値（ABI 3、4.2の関数表の操作ではない） | I: P3で共通の公開C++20 inlineを使う。動的kernel symbol不要 |
| `kernel::preparePlusargFormat` | 5855 | なし。システムタスク | A: `system_prepare_plusarg_format` |
| `kernel::prepareScanFormat` | 4679 | なし。システムタスク | A: `system_prepare_scan_format` |
| `kernel::randomInt` | 5736 | なし。システムタスク | A: `system_random_int` |
| `kernel::readField` | 1558, 1559, 1592, 1593 | なし。純粋値（ABI 3、4.2の関数表の操作ではない） | I: P3で共通の公開C++20 inlineを使う。動的kernel symbol不要 |
| `kernel::readFile` | 5355 | なし。システムタスク | A: `system_read_file` |
| `kernel::redAnd4` | 4604 | なし。純粋値（ABI 3、4.2の関数表の操作ではない） | I: P3で共通の公開C++20 inlineを使う。動的kernel symbol不要 |
| `kernel::redNand4` | 4605 | なし。純粋値（ABI 3、4.2の関数表の操作ではない） | I: P3で共通の公開C++20 inlineを使う。動的kernel symbol不要 |
| `kernel::redNor4` | 4607 | なし。純粋値（ABI 3、4.2の関数表の操作ではない） | I: P3で共通の公開C++20 inlineを使う。動的kernel symbol不要 |
| `kernel::redOr4` | 4606 | なし。純粋値（ABI 3、4.2の関数表の操作ではない） | I: P3で共通の公開C++20 inlineを使う。動的kernel symbol不要 |
| `kernel::redXnor4` | 4609 | なし。純粋値（ABI 3、4.2の関数表の操作ではない） | I: P3で共通の公開C++20 inlineを使う。動的kernel symbol不要 |
| `kernel::redXor4` | 4608 | なし。純粋値（ABI 3、4.2の関数表の操作ではない） | I: P3で共通の公開C++20 inlineを使う。動的kernel symbol不要 |
| `kernel::repl4` | 4640 | なし。純粋値（ABI 3、4.2の関数表の操作ではない） | I: P3で共通の公開C++20 inlineを使う。動的kernel symbol不要 |
| `kernel::resolve4` | 3412, 3433 | なし。値の書き込み／純粋helper適合口（ABI 3） | A: `value_resolve4` |
| `kernel::scanString` | 5838 | なし。システムタスク | A: `system_scan_string` |
| `kernel::select4` | 4613 | なし。純粋値（ABI 3、4.2の関数表の操作ではない） | I: P3で共通の公開C++20 inlineを使う。動的kernel symbol不要 |
| `kernel::sext4` | 4644 | なし。純粋値（ABI 3、4.2の関数表の操作ではない） | I: P3で共通の公開C++20 inlineを使う。動的kernel symbol不要 |
| `kernel::shl4` | 4610 | なし。純粋値（ABI 3、4.2の関数表の操作ではない） | I: P3で共通の公開C++20 inlineを使う。動的kernel symbol不要 |
| `kernel::shr4` | 4611 | なし。純粋値（ABI 3、4.2の関数表の操作ではない） | I: P3で共通の公開C++20 inlineを使う。動的kernel symbol不要 |
| `kernel::sub4` | 4566 | なし。純粋値（ABI 3、4.2の関数表の操作ではない） | I: P3で共通の公開C++20 inlineを使う。動的kernel symbol不要 |
| `kernel::to2` | 3784, 4083, 4738, 5941 | なし。純粋値（ABI 3、4.2の関数表の操作ではない） | I: P3で共通の公開C++20 inlineを使う。動的kernel symbol不要 |
| `kernel::trunc4` | 4645 | なし。純粋値（ABI 3、4.2の関数表の操作ではない） | I: P3で共通の公開C++20 inlineを使う。動的kernel symbol不要 |
| `kernel::truth4` | 420, 4160, 4235, 4589, 4594, 4595 | なし。純粋値（ABI 3、4.2の関数表の操作ではない） | I: P3で共通の公開C++20 inlineを使う。動的kernel symbol不要 |
| `kernel::udpOutput` | 4376 | なし。値の書き込み／純粋helper適合口（ABI 3） | A: `value_udp_output` |
| `kernel::udpOutputRead` | 4368 | なし。値の書き込み／純粋helper適合口（ABI 3） | A: `value_udp_output_read` |
| `kernel::udpStep` | 4382 | なし。値の書き込み／純粋helper適合口（ABI 3） | A: `value_udp_step` |
| `kernel::udpStepRead` | 4370 | なし。値の書き込み／純粋helper適合口（ABI 3） | A: `value_udp_step_read` |
| `kernel::urandomRange` | 5176 | なし。システムタスク | A: `system_urandom_range` |
| `kernel::weq4` | 4583 | なし。純粋値（ABI 3、4.2の関数表の操作ではない） | I: P3で共通の公開C++20 inlineを使う。動的kernel symbol不要 |
| `kernel::wide::add` | 3632 | なし。値の書き込み／純粋helper適合口（ABI 3） | A: `value_wide_add` |
| `kernel::wide::allX` | 3256, 3795 | なし。値の書き込み／純粋helper適合口（ABI 3） | A: `value_wide_all_x` |
| `kernel::wide::ashr` | 3761 | なし。値の書き込み／純粋helper適合口（ABI 3） | A: `value_wide_ashr` |
| `kernel::wide::bitAnd` | 3659 | なし。値の書き込み／純粋helper適合口（ABI 3） | A: `value_wide_bit_and` |
| `kernel::wide::bitNot` | 3665 | なし。値の書き込み／純粋helper適合口（ABI 3） | A: `value_wide_bit_not` |
| `kernel::wide::bitOr` | 3660 | なし。値の書き込み／純粋helper適合口（ABI 3） | A: `value_wide_bit_or` |
| `kernel::wide::bitXnor` | 3662 | なし。値の書き込み／純粋helper適合口（ABI 3） | A: `value_wide_bit_xnor` |
| `kernel::wide::bitXor` | 3661 | なし。値の書き込み／純粋helper適合口（ABI 3） | A: `value_wide_bit_xor` |
| `kernel::wide::bits` | 2515, 3620, 3783, 3798, 3943, 5010 | なし。値の書き込み／純粋helper適合口（ABI 3） | A: `value_wide_bits` |
| `kernel::wide::caseMatch` | 3698 | なし。値の書き込み／純粋helper適合口（ABI 3） | A: `value_wide_case_match` |
| `kernel::wide::ceq` | 3691 | なし。値の書き込み／純粋helper適合口（ABI 3） | A: `value_wide_ceq` |
| `kernel::wide::chunks` | 2514, 3464, 3619, 3942 | なし。値の書き込み／純粋helper適合口（ABI 3） | A: `value_wide_chunks` |
| `kernel::wide::cne` | 3692 | なし。値の書き込み／純粋helper適合口（ABI 3） | A: `value_wide_cne` |
| `kernel::wide::countBits` | 5428 | なし。値の書き込み／純粋helper適合口（ABI 3） | A: `value_wide_count_bits` |
| `kernel::wide::div` | 3637 | なし。値の書き込み／純粋helper適合口（ABI 3） | A: `value_wide_div` |
| `kernel::wide::eq` | 3689 | なし。値の書き込み／純粋helper適合口（ABI 3） | A: `value_wide_eq` |
| `kernel::wide::fromReal` | 3845, 4228, 5902 | なし。値の書き込み／純粋helper適合口（ABI 3） | A: `value_wide_from_real` |
| `kernel::wide::fromText` | 4121, 4125, 4709, 4735, 5096, 5605, 5716 | なし。値の書き込み／純粋helper適合口（ABI 3） | A: `value_wide_from_text` |
| `kernel::wide::ge` | 3686 | なし。値の書き込み／純粋helper適合口（ABI 3） | A: `value_wide_ge` |
| `kernel::wide::gt` | 3681 | なし。値の書き込み／純粋helper適合口（ABI 3） | A: `value_wide_gt` |
| `kernel::wide::hasUnknown` | 1538, 4660, 5829, 5917 | なし。値の書き込み／純粋helper適合口（ABI 3） | A: `value_wide_has_unknown` |
| `kernel::wide::le` | 3676 | なし。値の書き込み／純粋helper適合口（ABI 3） | A: `value_wide_le` |
| `kernel::wide::lt` | 3671 | なし。値の書き込み／純粋helper適合口（ABI 3） | A: `value_wide_lt` |
| `kernel::wide::mod` | 3642 | なし。値の書き込み／純粋helper適合口（ABI 3） | A: `value_wide_mod` |
| `kernel::wide::mul` | 3634 | なし。値の書き込み／純粋helper適合口（ABI 3） | A: `value_wide_mul` |
| `kernel::wide::ne` | 3690 | なし。値の書き込み／純粋helper適合口（ABI 3） | A: `value_wide_ne` |
| `kernel::wide::neg` | 3655 | なし。値の書き込み／純粋helper適合口（ABI 3） | A: `value_wide_neg` |
| `kernel::wide::position` | 3489 | なし。値の書き込み／純粋helper適合口（ABI 3） | A: `value_wide_position` |
| `kernel::wide::pow` | 3648 | なし。値の書き込み／純粋helper適合口（ABI 3） | A: `value_wide_pow` |
| `kernel::wide::put` | 2516, 3624, 3810, 3820, 3945, 5667 | なし。値の書き込み／純粋helper適合口（ABI 3） | A: `value_wide_put` |
| `kernel::wide::redAnd` | 3743, 3744 | なし。値の書き込み／純粋helper適合口（ABI 3） | A: `value_wide_red_and` |
| `kernel::wide::redOr` | 3745, 3746 | なし。値の書き込み／純粋helper適合口（ABI 3） | A: `value_wide_red_or` |
| `kernel::wide::redXor` | 3747, 3748 | なし。値の書き込み／純粋helper適合口（ABI 3） | A: `value_wide_red_xor` |
| `kernel::wide::resize` | 3465, 3830 | なし。値の書き込み／純粋helper適合口（ABI 3） | A: `value_wide_resize` |
| `kernel::wide::select` | 3769 | なし。値の書き込み／純粋helper適合口（ABI 3） | A: `value_wide_select` |
| `kernel::wide::shl` | 3759 | なし。値の書き込み／純粋helper適合口（ABI 3） | A: `value_wide_shl` |
| `kernel::wide::sub` | 3633, 4718 | なし。値の書き込み／純粋helper適合口（ABI 3） | A: `value_wide_sub` |
| `kernel::wide::to2` | 3476, 3831, 4726, 5013 | なし。値の書き込み／純粋helper適合口（ABI 3） | A: `value_wide_to2` |
| `kernel::wide::toReal` | 3841, 5065 | なし。値の書き込み／純粋helper適合口（ABI 3） | A: `value_wide_to_real` |
| `kernel::wide::toText` | 4116, 4661 | なし。値の書き込み／純粋helper適合口（ABI 3） | A: `value_wide_to_text` |
| `kernel::wide::truth` | 3704, 3709, 3718, 3719 | なし。値の書き込み／純粋helper適合口（ABI 3） | A: `value_wide_truth` |
| `kernel::wide::weq` | 3693 | なし。値の書き込み／純粋helper適合口（ABI 3） | A: `value_wide_weq` |
| `kernel::wide::wne` | 3694 | なし。値の書き込み／純粋helper適合口（ABI 3） | A: `value_wide_wne` |
| `kernel::wne4` | 4584 | なし。純粋値（ABI 3、4.2の関数表の操作ではない） | I: P3で共通の公開C++20 inlineを使う。動的kernel symbol不要 |
| `kernel::writeBits4` | 4346, 4515 | なし。純粋値（ABI 3、4.2の関数表の操作ではない） | I: P3で共通の公開C++20 inlineを使う。動的kernel symbol不要 |
| `kernel::writeField` | 759, 760 | なし。純粋値（ABI 3、4.2の関数表の操作ではない） | I: P3で共通の公開C++20 inlineを使う。動的kernel symbol不要 |
| `kernel::xnor4` | 4602 | なし。純粋値（ABI 3、4.2の関数表の操作ではない） | I: P3で共通の公開C++20 inlineを使う。動的kernel symbol不要 |
| `kernel::xor4` | 4601 | なし。純粋値（ABI 3、4.2の関数表の操作ではない） | I: P3で共通の公開C++20 inlineを使う。動的kernel symbol不要 |
| `output_.message` | 897, 1087, 1441, 4149, 5141, 5407, 5788 | なし。システムタスク | A: `output_message` |
| `output_.write` | 5214, 5559 | なし。システムタスク | A: `output_write` |
| `sched_.addChildProcess` | 1408 | なし。待ち・スケジューリング | A: `scheduler_add_child_process` |
| `sched_.addDelayedDriver` | 693 | なし。待ち・スケジューリング | A: `scheduler_add_delayed_driver` |
| `sched_.addEvent` | 773, 1376 | なし。待ち・スケジューリング | A: `scheduler_add_event` |
| `sched_.addFileMonitor` | 2789 | なし。待ち・スケジューリング | A: `scheduler_add_file_monitor` |
| `sched_.addProcess` | 810, 816, 852, 859, 866, 871, 876, 2787 | あり: `add_process` | 現存 |
| `sched_.addProgram` | 828 | なし。待ち・スケジューリング | A: `scheduler_add_program` |
| `sched_.addProgramInitial` | 850 | なし。待ち・スケジューリング | A: `scheduler_add_program_initial` |
| `sched_.cancelWait` | 3047, 3240 | なし。待ち・スケジューリング | A: `scheduler_cancel_wait` |
| `sched_.deferCall` | 4542 | なし。報告 | A: `scheduler_defer_call` |
| `sched_.deferReport` | 4560 | なし。報告 | A: `scheduler_defer_report` |
| `sched_.detachDisplayMonitor` | 3046 | なし。待ち・スケジューリング | A: `scheduler_detach_display_monitor` |
| `sched_.driveDelayed` | 2742 | なし。値の書き込み | A: `scheduler_drive_delayed` |
| `sched_.ended` | 823 | なし。待ち・スケジューリング | A: `scheduler_ended` |
| `sched_.ending` | 813, 2598, 2658, 2692, 2762, 2769, 2830, 4089 | あり: `ending` | 現存 |
| `sched_.error` | 821, 903 | なし。時刻・終了 | A: `scheduler_error` |
| `sched_.exitProgram` | 5775 | なし。待ち・スケジューリング | A: `scheduler_exit_program` |
| `sched_.fail` | 1429, 2177, 2182, 2192, 2211, 2237 | あり: `fail` | 現存 |
| `sched_.failedHeapMonitorProcess` | 3054 | なし。待ち・スケジューリング | A: `scheduler_failed_heap_monitor_process` |
| `sched_.finished` | 2175 | なし。時刻・終了 | A: `scheduler_finished` |
| `sched_.flushHeapChanges` | 4087 | なし。待ち・スケジューリング | A: `scheduler_flush_heap_changes` |
| `sched_.hasPending` | 1285 | なし。待ち・スケジューリング | A: `scheduler_has_pending` |
| `sched_.heapMonitorLimited` | 3052 | なし。待ち・スケジューリング | A: `scheduler_heap_monitor_limited` |
| `sched_.hold` | 2866 | なし。値の書き込み | A: `scheduler_hold` |
| `sched_.holdsAny` | 3238 | なし。値の書き込み | A: `scheduler_holds_any` |
| `sched_.inPostponed` | 2560 | なし。待ち・スケジューリング | A: `scheduler_in_postponed` |
| `sched_.kill` | 1421, 3153 | なし。待ち・スケジューリング | A: `scheduler_kill` |
| `sched_.markHeld` | 785 | なし。値の書き込み | A: `scheduler_mark_held` |
| `sched_.printingTo` | 5556 | なし。待ち・スケジューリング | A: `scheduler_printing_to` |
| `sched_.programStopped` | 2537, 2599, 2763, 2770, 2831 | なし。時刻・終了 | A: `scheduler_program_stopped` |
| `sched_.reason` | 820, 902 | なし。時刻・終了 | A: `scheduler_reason` |
| `sched_.release` | 2813 | なし。値の書き込み | A: `scheduler_release` |
| `sched_.releaseProcess` | 1294 | なし。待ち・スケジューリング | A: `scheduler_release_process` |
| `sched_.requestEnd` | 2555, 5409, 5790 | あり: `request_end` | 現存 |
| `sched_.resourceLimit` | 1088, 2151 | なし。時刻・終了 | A: `scheduler_resource_limit` |
| `sched_.run` | 891 | なし。基盤（ABI 3/5/9、4.2の群ではない） | H: P3でもhost loader/driverかtier内計数。生成resumeからは呼ばない |
| `sched_.runFinal` | 892 | なし。基盤（ABI 3/5/9、4.2の群ではない） | H: P3でもhost loader/driverかtier内計数。生成resumeからは呼ばない |
| `sched_.runtimeError` | 1442 | なし。時刻・終了 | A: `scheduler_runtime_error` |
| `sched_.scheduleNba` | 1571, 1646 | あり: `schedule_nba` | 現存 |
| `sched_.setEndNote` | 2554 | なし。時刻・終了 | A: `scheduler_set_end_note` |
| `sched_.setMonitor` | 2804 | なし。待ち・スケジューリング | A: `scheduler_set_monitor` |
| `sched_.setMonitoring` | 5465, 5467 | なし。待ち・スケジューリング | A: `scheduler_set_monitoring` |
| `sched_.setSafePointHook` | 731, 733, 2300 | なし。待ち・スケジューリング | A: `scheduler_set_safe_point_hook` |
| `sched_.start` | 853, 890, 1411, 3228 | あり: `start` | 現存 |
| `sched_.strobe` | 2775, 2777 | なし。待ち・スケジューリング | A: `scheduler_strobe` |
| `sched_.takeEndedFileMonitor` | 1322 | なし。待ち・スケジューリング | A: `scheduler_take_ended_file_monitor` |
| `sched_.takeEndedProgramProcess` | 1314 | なし。待ち・スケジューリング | A: `scheduler_take_ended_program_process` |
| `sched_.takeInterrupt` | 2571, 2577 | なし。時刻・終了 | A: `scheduler_take_interrupt` |
| `sched_.takeOverflow` | 2635, 2836 | なし。時刻・終了 | A: `scheduler_take_overflow` |
| `sched_.time` | 1453, 4558, 5406, 5488, 5787, 5799, 5884 | あり: `time` | 現存 |
| `sched_.trigger` | 1273, 2719 | あり: `trigger` | 現存 |
| `sched_.triggerNb` | 2728 | あり: `trigger_nb` | 現存 |
| `sched_.triggered` | 4755 | あり: `triggered` | 現存 |
| `sched_.visitPendingValues` | 2221 | なし。待ち・スケジューリング | A: `scheduler_visit_pending_values` |
| `sched_.waitAny` | 2684 | あり: `wait_mixed（namedなしならwait_any）` | A: `scheduler_wait_mixed_ex` heap付きdescriptorのため |
| `sched_.waitDelay` | 2634 | あり: `wait_delay` | 現存 |
| `sched_.waitEvent` | 1754 | あり: `wait_event` | 現存 |
| `sched_.waitMonitor` | 3112 | あり: `wait_monitor（slotのみのdescriptor、部分対応の注記参照）` | A: `scheduler_wait_monitor_ex` heap依存のため |
| `sched_.waitNamed` | 1383, 2707 | あり: `wait_named（flushPoint=trueのみ）` | A: `scheduler_wait_named_ex` 明示flush方針のため |
| `sched_.write` | 1565, 1618 | あり: `write` | 現存 |
| `sched_.writeHeld` | 2819 | なし。値の書き込み | A: `scheduler_write_held` |
| `stack_.nearlyUsedUp` | 1074, 2897, 3315 | なし。基盤（ABI 3/5/9、4.2の群ではない） | H: P3でもhost loader/driverかtier内計数。生成resumeからは呼ばない |
| `store_.add` | 737 | なし。基盤（ABI 3/5/9、4.2の群ではない） | H: P3でもhost loader/driverかtier内計数。生成resumeからは呼ばない |
| `store_.element` | 756, 773, 1556, 1584, 2172, 3033, 3090 | あり: `slot_words` | 現存（検証済みread view、書き込みはwrite/NBA） |
| `store_.layout` | 751, 1554, 1582, 1610, 1624, 1638 | なし。基盤（ABI 3/5/9、4.2の群ではない） | H: P3でもhost loader/driverかtier内計数。生成resumeからは呼ばない |
| `store_.size` | 724 | なし。基盤（ABI 3/5/9、4.2の群ではない） | H: P3でもhost loader/driverかtier内計数。生成resumeからは呼ばない |
| `strings_.get` | 4109, 4658, 4759, 4801, 4918, 5072, 5297, 5451, 5472, 5516, 5574, 5577, 5828, 5875 | なし。システムタスク | A: `strings_get` |
| `strings_.intern` | 634, 4111, 4732, 4762, 5092, 5211, 5603, 5714 | なし。システムタスク | A: `strings_intern` |
