# CODEGEN_SPEC：C++ 生成の仕様

[English](CODEGEN_SPEC.md) · 英語版が正本で、これはその日本語版です。食い違ったときは英語版に従います。

**改訂（2026-10-24）：** 旧P2→P3・旧P3→P4・旧P4→P5と付け替え、新しいP2（SIR v1）を設けました（tier番号は不変）。[対応表](../notes/phase-renumbering.ja.md)。

優先度：P3（2026-10-15 に P1 から移した。C++ の生成は P2 の作業のため）。この文書はまだ見出しだけです。ただし「浮動小数点」の節と、「C++ 生成の規則」と「静的な島」の最初の規則は 2026-10-19 に足しました（承認済み）。

## C++ 生成の規則

残りは表の優先度のフェーズで書く。次の規則は 2026-10-19 に承認した。

- 同じ SIR からは、1 バイトも違わない同じ C++ を作る（NFR-6）。CI で 2 回作って比べる。
- `&&`、`||`、`->`、`?:` の短絡評価を保つ。SIR はそれを分岐で表し（SIR_SPEC.md の 5.2 節）、生成コードが論理演算をビット演算に変えるのは、右辺に副作用も null の参照も無いと証明できたときだけ。
- 副作用のある式は、SIR が示す回数だけ評価する。コードを複製するのは、副作用が無いと証明できたところだけ。
- VALUE_SEMANTICS.md の 3.1 節の左から右の順が、呼び出しの引数のように評価の順が決まっていない C++ の構文に出会うところでは、値をその順に一時変数に通す。

## 静的な島

残りは表の優先度のフェーズで書く。次の規則は 2026-10-19 に承認した。

- 静的な島の中では、`analysis` が決めるレベル順に、各プロセスを 1 回ずつ評価する。第 1 層も島のプロセスに同じ順を使うので、時刻スロットの中で通る値（グリッチを含む）が、両方の層で同じになる。

## 2 版切り替え

未記入（表の優先度のフェーズで書く）

## 浮動小数点

2026-10-19 に承認。

- **決まり：** 実行カーネル、VM、そのほかの kirimasim 自身のコード（`src/`、`tests/`）と、第 2 層が生成する C++ は、`-ffp-contract=off` を付け、`-ffast-math`、`-Ofast`、`-funsafe-math-optimizations` を付けずにビルドする（CLAUDE.md）。依存には課さない。数学の関数（`$sin`、`$exp` など）はシステムの libm に任せるので、最後のビットがシステムによって違いうる。これが実数についての NFR-2 の唯一の例外である。
- **理由：** IEEE 754 の倍精度である `real`（規格の 6.12）の式は、どの層と OS でも同じビットにならなければならない（NFR-2）。乗算と加算を 1 つの FMA の命令にまとめるコンパイラは、積を丸めずに足すので、2 つの演算を別々に行ったときと最後のビットが違いうる。まとめるかどうかは、コンパイラ、対象、フラグで変わる。
- **2026-10-19 に確かめたこと：** `double f(double a, double b, double c) { return a * b + c; }` を `-std=c++23 -S` で、`-O2` と `-O3` で翻訳し（2 つは同じ結果だった）、出力に FMA の命令（arm64 は `fmadd`、x86-64 は `vfmadd`）があるかを調べた。使った機械は arm64 の Mac で、ほかの対象は `--target` で翻訳しただけで、実行はしていない。

| コンパイラ | 対象 | `-ffp-contract=off` なし | あり |
| --- | --- | --- | --- |
| Apple clang 21.0.0 | arm64-apple-macos | FMA（`fmadd`） | FMA なし（`fmul`、`fadd`） |
| clang 22.1.8（Homebrew の LLVM） | aarch64-linux-gnu | FMA | FMA なし |
| clang 22.1.8（Homebrew の LLVM） | x86_64-linux-gnu | FMA なし（`mulsd`、`addsd`） | FMA なし |
| clang 22.1.8（Homebrew の LLVM） | x86_64-linux-gnu、`-march=haswell` | FMA（`vfmadd213sd`） | FMA なし |
| GCC 16（Homebrew） | arm64-apple-darwin | FMA | FMA なし |

- したがって、2026-10-28 まで `-ffp-contract` も `-march` も付けなかったプリセットでは、arm64（macOS と arm64 の Linux）向けのビルドはまとめ、x86-64 向けのビルドはまとめないので、`real` の結果が両者で違いえた。Apple clang は既定でまとめる（`__FP_FAST_FMA` を定義する）。2026-10-28 からは `CMakeLists.txt` が kirimasim 自身のコードに `-ffp-contract=off` を付け、`tools/check_release_flags.py` が、リリースビルドの `src/` と `tests/` のすべての翻訳単位にそれがあり、`-Ofast`、`-ffast-math`、`-funsafe-math-optimizations` のどれも無いことを確かめる。
- フラグを付けても、macOS arm64 の Apple clang 21.0.0 で、dev ビルドの 8,170 個のテストのどれも結果が変わらなかった（2026-10-28 に確認）。
- **未確認：** Linux の CI のジョブのコンパイラである x86-64 Linux の GCC と clang で、`-march` のあるときと無いとき（確かめ方：CI のジョブで同じ翻訳をする）。CI の macOS のランナー（`macos-26`）が arm64 か（確かめ方：CI のジョブで `uname -m`）。
