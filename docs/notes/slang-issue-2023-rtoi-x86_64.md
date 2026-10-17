# slang の Issue #2023（`$rtoi` の定数評価）を x86_64 で確かめた結果

調べた日：2026-10-17。対象：slang v12.0（`545f88b2e`、`cmake/Dependencies.cmake` で固定している版）。

## 背景

[slang の Issue #2023](https://github.com/MikePopoloski/slang/issues/2023) は、slang が `$rtoi` を定数評価するとき、負の引数で 0 を返すという報告である。`$rtoi` は 0 の方向に切り捨てる（IEEE 1800-2023 の 20.5 節）ので、`$rtoi(-3.7)` は -3 になるべきである。原因は `source/ast/builtins/ConversionFuncs.cpp` の `RtoIFunction::eval` が `(uint64_t)val.real()` と書いていることで、負の `double` を直接 `uint64_t` に変換するのは C++ の未定義動作である。報告は macOS（arm64）での結果だった。

この文書は、同じ版の slang を x86_64 の Linux で動かした結果をまとめる。

## 環境

- x86_64、Linux（WSL2 の上の Ubuntu）
- Ubuntu clang 20.1.8
- kirimasim `f73226a` の dev と ubsan の構成

## 使った入力

Issue の本文の例（定数評価だけ）：

```systemverilog
module top;
  localparam int P = $rtoi(-3.7);
  localparam int Q = $rtoi(3.7);
  $info("P = %0d, Q = %0d", P, Q);
endmodule
```

Issue のコメントの例（パラメータ、定数の呼び出しの代入、real 変数での呼び出し）：

```systemverilog
module top;
  localparam int P = $rtoi(-3.7);
  int i;
  real r;
  initial begin
    i = $rtoi(-3.7);
    r = -3.7;
    $display("P=%0d assign=%0d var=%0d", P, i, $rtoi(r));
  end
endmodule
```

前者は `kirimasim parse`、後者は `kirimasim` で実行した。

## 結果

| 構成 | 本文の例 | コメントの例 |
| --- | --- | --- |
| dev（`build/dev`） | `P = -3, Q = 3` | `P=-3 assign=-3 var=-3` |
| ubsan プリセット（`build/ubsan`、slang だけ `-fno-sanitize=float-cast-overflow`） | `P = -3, Q = 3`、検出なし | `P=-3 assign=-3 var=-3`、検出なし |
| ubsan の回避策を外した構成（下の「再現の手順」） | ubsan が検出して止まる | ubsan が検出して止まる |

回避策を外した構成で ubsan が出した報告（パスはビルドの場所からの相対に直した）：

```
_deps/slang-src/source/ast/builtins/ConversionFuncs.cpp:64:36: runtime error: -3.7 is outside the range of representable values of type 'unsigned long'
```

`-fno-sanitize-recover=all` を付けているので、ここで処理が止まり、`$info` と `$display` の行は出ない。

## 分かったこと

- **x86_64 では値が偶然正しい。** macOS（arm64）では `P = 0` になるが、x86_64 では -3 になる。
- **未定義動作は x86_64 でも起きている。** clang の `-fsanitize=undefined` には float-cast-overflow の検査が含まれており、回避策を外すと Issue の箇所（`ConversionFuncs.cpp:64`）で検出される。Issue の主張は、x86_64 でも裏付けられた。
- **x86_64 で -3 になる理由。** dev の構成（`-O1`）で `RtoIFunction::eval` を逆アセンブルすると、`double` から `uint64_t` への変換は分岐の無い次の形になっていた。

  ```
  cvttsd2si %xmm0,%rax        ; 符号付きの変換：-3.7 → -3（0xFFFFFFFFFFFFFFFD）
  mov       %rax,%rcx
  subsd     <2^63>,%xmm0      ; x - 2^63
  sar       $0x3f,%rcx        ; 最初の結果の符号で、全ビット 1 か 0 の覆いを作る
  cvttsd2si %xmm0,%rdx        ; 範囲外なので 0x8000000000000000
  and       %rcx,%rdx
  or        %rax,%rdx         ; 0xFFFFFFFFFFFFFFFD | 0x8000000000000000 = 0xFFFFFFFFFFFFFFFD
  ```

  負の値では、符号付きの変換の結果のビットの並びがそのまま残り、32 ビットに切り詰めると -3 になる。arm64 の符号なしの変換の命令は負の値を 0 にするので、結果が分かれる（下の「arm64 で確かめた結果」）。release の構成（`-O3`）の命令列は見ていない（未確認）。

## arm64 で確かめた結果（2026-10-17 に追記）

### 環境

- arm64、macOS 27
- Apple clang 21.0.0
- kirimasim `6cdbde8` の dev の構成（slang は `-O1`）

### 結果

| 構成 | 本文の例 | コメントの例 |
| --- | --- | --- |
| dev（`build/dev`） | `P = 0, Q = 3` | `P=0 assign=-3 var=-3` |
| slang だけを `-fsanitize=float-cast-overflow` で作った CLI（下の「再現の手順」） | 検出される（値は `P = 0, Q = 3`） | — |

- コメントの例の `assign` と `var` が -3 になるのは、`docs/spec/SIR_SPEC.md` の 6 節の方針で、`$rtoi` を含む式を実行時に計算するため。パラメータ P は slang が決めるので 0 のまま。
- 検出の報告（パスは slang のソースの場所からの相対に直した）は、x86_64 と同じ箇所である。型の名前が違うのは、macOS では `uint64_t` が `unsigned long long` だから。

  ```
  source/ast/builtins/ConversionFuncs.cpp:64:36: runtime error: -3.7 is outside the range of representable values of type 'unsigned long long'
  ```

- dev の構成の `ConversionFuncs.cpp.o` を逆アセンブルすると、`RtoIFunction::eval` の中の `double` から `uint64_t` への変換は、次の 1 命令だった。

  ```
  fcvtzu    d0, d0           ; 符号なしの変換。負の値は 0 になる
  ```

  同じ命令になる小さなプログラム（負の `double` を `(uint64_t)` で変換するだけ）を、この Mac で動かした。-3.7 は 0 になった。x86_64 の命令列は符号付きの変換の結果を残すので -3 になり、arm64 のこの命令は 0 になる。これが、マシンによって結果が分かれる理由である。

### arm64 での再現の手順

slang だけの UBSan 付きの CLI は、kirimasim の dev の構成が取ってきた依存のソースを使って、ネットワークを使わずに作れる。`<dir>` は好きな作業用のディレクトリ。

```sh
D=build/dev/_deps
cmake -S $D/slang-src -B <dir> -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo \
  -DSLANG_INCLUDE_TOOLS=ON -DSLANG_INCLUDE_TESTS=OFF -DSLANG_USE_MIMALLOC=OFF \
  -DCMAKE_CXX_FLAGS=-fsanitize=float-cast-overflow \
  -DCMAKE_EXE_LINKER_FLAGS=-fsanitize=float-cast-overflow \
  -DFETCHCONTENT_FULLY_DISCONNECTED=ON \
  -DFETCHCONTENT_SOURCE_DIR_FMT=$PWD/$D/fmt-src \
  -DFETCHCONTENT_SOURCE_DIR_BOOST_REGEX=$PWD/$D/boost_regex-src \
  -DFETCHCONTENT_SOURCE_DIR_TOMLPLUSPLUS=$PWD/$D/tomlplusplus-src
cmake --build <dir> --target slang_driver
<dir>/bin/slang rtoi.sv
```

逆アセンブル（macOS の `objdump` は LLVM のもの）：

```sh
objdump -d --no-show-raw-insn -C \
  build/dev/_deps/slang-build/source/CMakeFiles/slang_slang.dir/ast/builtins/ConversionFuncs.cpp.o \
  | awk '/RtoIFunction::eval/{p=1} p{print} p&&/\tret/{exit}' | grep fcvtzu
```

## kirimasim への影響

- `docs/spec/SIR_SPEC.md` の 6 節は、`$rtoi` を含む式を slang の畳み込んだ定数に頼らず、実行時に計算すると決めている。`$rtoi` を含む式はこの方針で、どのマシンでも正しい値になる。
- 同じ節は、パラメータの値は slang が決めるのでこの方針では直らない、と書いている。パラメータの値はマシンによって違う。x86_64 では偶然 -3 になり、arm64 では 0 になる。x86_64 だけで確かめると、この誤りは見えない。
- `cmake/Dependencies.cmake` の回避策（ubsan の構成で slang にだけ `-fno-sanitize=float-cast-overflow` を付ける）は、意図どおり、この検出を抑えている。slang を直った版に上げたら、回避策を外して、ubsan で検出が出ないことを確かめる。

## 再現の手順

dev と ubsan のプリセットは、ふつうに作る。

```sh
cmake --preset dev   && cmake --build --preset dev   --target kirimasim
cmake --preset ubsan && cmake --build --preset ubsan --target kirimasim
```

回避策を外した構成は、ubsan のフラグを `CMAKE_CXX_FLAGS_DEBUG` ではなく `CMAKE_CXX_FLAGS` に置いて作る。`cmake/Dependencies.cmake` の回避策は `CMAKE_CXX_FLAGS_DEBUG` を見て付くので、この置き方では付かない。リポジトリのファイルは変えない。

```sh
F="-O1 -g -fsanitize=undefined -fno-sanitize-recover=all"
cmake -S . -B build/ubsan-noworkaround -G Ninja -DCMAKE_BUILD_TYPE=Debug \
  -DCMAKE_CXX_FLAGS_DEBUG="" -DCMAKE_C_FLAGS_DEBUG="" \
  -DCMAKE_CXX_FLAGS="$F" -DCMAKE_C_FLAGS="$F" \
  -DCMAKE_EXE_LINKER_FLAGS=-fsanitize=undefined \
  -DCMAKE_SHARED_LINKER_FLAGS=-fsanitize=undefined
cmake --build build/ubsan-noworkaround --target kirimasim
grep -c float-cast-overflow build/ubsan-noworkaround/build.ninja   # 0 なら回避策は付いていない
```

実行：

```sh
build/<構成>/src/driver/kirimasim parse rtoi.sv   # 本文の例
build/<構成>/src/driver/kirimasim rtoi2.sv        # コメントの例
```

逆アセンブル：

```sh
objdump -d --no-show-raw-insn -C \
  build/dev/_deps/slang-build/source/CMakeFiles/slang_slang.dir/ast/builtins/ConversionFuncs.cpp.o \
  | awk '/RtoIFunction::eval/{p=1} p{print} p&&/ret/{exit}'
```
