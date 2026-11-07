# 検証済みの Design への 1 つの関門

[English](verified-design.md) · 英語版が正本です。

状態：2026-11-07 に設計を承認（P2 レビューの M4。方針は [sir-v1-proposal](sir-v1-proposal.ja.md) の「P2 レビュー後の修正」に記録）。未実装。

## なぜ

format 1 で kirimasim の外で書かれた SIR を読むようになると、そのテキストと実行のあいだにあるのは検証器だけになります。今は、front が作る Design をどのビルドでもすべて検証し、VM が読み込みのときに一部をもう一度確かめます。確かめるのは最大 6 回で、それぞれ Design が特定の機能を持つときだけです。手で SIR を組む単体テストのように別の道で VM に届いた Design は、この部分的な検査でしか止まりません。

## 型

`sir::VerifiedDesign` は検査を通った Design を持ち、`sir::verifyDesign` だけがそれを作れます。

```cpp
namespace kirimasim::sir {
class VerifiedDesign {
public:
    const Design& design() const noexcept;
    Design release() && noexcept;           // Design を返す。戻すにはもう一度検証する
private:
    explicit VerifiedDesign(Design d);
    friend VerifyOutcome verifyDesign(Design design);
    Design design_;
};

struct VerifyOutcome {
    std::optional<VerifiedDesign> verified; // errors が空のときにだけ値がある
    std::vector<VerifyError> errors;
};

VerifyOutcome verifyDesign(Design design);
}
```

- エラーは例外ではなく値で返します。Design は移動で受け渡し、コピーしません。
- VerifiedDesign から届くのは `const Design&` だけなので、検査の後で変わりません。変えるときは取り出して、もう一度検証します。
- `verifyDesign` は、今 VM が読み込みで行う検査（`verify`、`verifyActivationScopes`、`validatePatterns` のエラーの部分）を 1 か所で行います。新しい検査は足しません。

## 作る側と受け取る側

- `front::compile` は、それを `CompileResult` に入れて返します。`success` の意味は変えず、検査の失敗は今と同じ文言の内部エラーのままです。
- parse1（P2 の後の段階で作る format 1）は VerifyOutcome を返します。
- `vm::run` は `const sir::VerifiedDesign&` を受け取り、第 2 層も同じにします。`--dump-sir` はその Design を出力し、テキストは変わりません。
- VM の重複した検査はなくなります。pattern の形は今までどおり `validatePatterns` で求めますが、関門の後ではエラーは起こりえないので、起きれば内部エラーにします。この実行で何ができるかについての VM の読み込みエラー（VPI のホストがない、クラスの初期値の上書き・観測・記憶域が未接続）は残します。SIR の正しさの話ではないからです。

## テスト

- 手で SIR を組む単体テストは、`vm::run` の前に `verifyDesign` を通します。検証エラーならテストを失敗させる共通の関数を使います。`EXPECT_*` と `ASSERT_*` は弱めません。
- 6 つの単体テストのファイル（`vpi_front_test`、`activation_sir_test`、`implicit_container_test`、`vm_nba_event_test`、`ff_expression_test`、`class_vm_guard_test`）は、わざと不正な Design を `vm::run` に渡し、VM の読み込みエラーを期待しています。各ケースを「`verifyDesign` が同じ Design を拒む」テストに移します。既存の期待値の変更なので独立したコミットにし、作る前に、元の確認と新しい確認を並べてお見せします。
- 新しいテスト：違反の種類ごとに `verifyDesign` が拒むこと、`vm::run` が素の Design を受け取らないこと（コンパイル時に確かめる）、取り出した Design をもう一度検証すること。
- `tests/dump_sir` の SIR のテキストも、ほかの期待値も変わりません。

## 検証器の規則

関門の後、次のものを検証器が受け付けないようにします。1 規則 1 コミットで、陰性テストを付けます。chandle の大小比較（6.14 が chandle に許すのは、等価、ケース等価、真偽の判定、代入だけ）、real の `Ceq`/`Cne`（表 11-1）、sir-v1-proposal 2-a の緩い点（Syscall と Builtin、Load、参照を通した書き込み）です。最初の 2 つは、今の front は作りません。slang と `compareValues` が拒否します。

## コミットの順

1. `VerifiedDesign`、`verifyDesign` とそのテスト（`sir` だけ）。
2. front がそれを返し、driver と、設計をコンパイルするテストがそれを使う。
3. 6 つのファイルの期待値を `verifyDesign` に移す（先にお見せする）。
4. `vm::run` がそれを受け取り、重複した検査を除き、単体テストの手組みの SIR が関門を通る。
5. 検証器の規則を 1 つずつ。

測っていないこと：今、読み込みで重複した検査にかかっている時間。
