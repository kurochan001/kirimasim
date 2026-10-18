# slang issue #2023 (constant evaluation of `$rtoi`) checked on x86_64

[日本語](slang-issue-2023-rtoi-x86_64.ja.md) · This English text is the main version; the Japanese version follows it, and where they differ, this text holds.

Date checked: 2026-10-17. Subject: slang v12.0 (`545f88b2e`, the version `cmake/Dependencies.cmake` pinned on that date; since 2026-10-18 it pins main `b7f11c1`, which has the fix: see the addendum at the end).

## Background

[slang issue #2023](https://github.com/MikePopoloski/slang/issues/2023) reports that slang returns 0 for a negative argument when it evaluates `$rtoi` as a constant. `$rtoi` truncates toward zero (IEEE 1800-2023 clause 20.5), so `$rtoi(-3.7)` should be -3. The cause is that `RtoIFunction::eval` in `source/ast/builtins/ConversionFuncs.cpp` writes `(uint64_t)val.real()`; converting a negative `double` directly to `uint64_t` is undefined behaviour in C++. The report was a result on macOS (arm64).

This document gathers the results of running the same version of slang on x86_64 Linux.

## Environment

- x86_64, Linux (Ubuntu on WSL2)
- Ubuntu clang 20.1.8
- The dev and ubsan configurations of kirimasim `f73226a`

## Inputs used

The example in the issue's text (constant evaluation only):

```systemverilog
module top;
  localparam int P = $rtoi(-3.7);
  localparam int Q = $rtoi(3.7);
  $info("P = %0d, Q = %0d", P, Q);
endmodule
```

The example in the issue's comment (a parameter, an assignment of a constant call, a call on a real variable):

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

The first was run with `kirimasim parse`, the second with `kirimasim`.

## Results

| Configuration | Example in the text | Example in the comment |
| --- | --- | --- |
| dev (`build/dev`) | `P = -3, Q = 3` | `P=-3 assign=-3 var=-3` |
| ubsan preset (`build/ubsan`, with `-fno-sanitize=float-cast-overflow` for slang only) | `P = -3, Q = 3`, nothing detected | `P=-3 assign=-3 var=-3`, nothing detected |
| ubsan with the workaround removed ("Steps to reproduce" below) | ubsan detects it and stops | ubsan detects it and stops |

The report ubsan gave with the workaround removed (paths made relative to the build directory):

```
_deps/slang-src/source/ast/builtins/ConversionFuncs.cpp:64:36: runtime error: -3.7 is outside the range of representable values of type 'unsigned long'
```

`-fno-sanitize-recover=all` is set, so processing stops here, and the `$info` and `$display` lines are not printed.

## What was found

- **On x86_64 the value happens to be right.** On macOS (arm64) it is `P = 0`, but on x86_64 it is -3.
- **The undefined behaviour happens on x86_64 too.** clang's `-fsanitize=undefined` includes the float-cast-overflow check, and with the workaround removed it is detected at the place named in the issue (`ConversionFuncs.cpp:64`). The issue's claim was confirmed on x86_64 too.
- **Why it is -3 on x86_64.** Disassembling `RtoIFunction::eval` in the dev configuration (`-O1`), the conversion from `double` to `uint64_t` had this branch-free form.

  ```
  cvttsd2si %xmm0,%rax        ; signed conversion: -3.7 → -3 (0xFFFFFFFFFFFFFFFD)
  mov       %rax,%rcx
  subsd     <2^63>,%xmm0      ; x - 2^63
  sar       $0x3f,%rcx        ; a mask of all ones or all zeros from the sign of the first result
  cvttsd2si %xmm0,%rdx        ; out of range, so 0x8000000000000000
  and       %rcx,%rdx
  or        %rax,%rdx         ; 0xFFFFFFFFFFFFFFFD | 0x8000000000000000 = 0xFFFFFFFFFFFFFFFD
  ```

  For negative values the bit pattern of the signed conversion's result is kept as it is, and truncated to 32 bits it is -3. arm64's unsigned conversion instruction makes negative values 0, so the results differ ("Results checked on arm64" below). The instruction sequence of the release configuration (`-O3`) was not looked at (not confirmed).

## Results checked on arm64 (added 2026-10-17)

### Environment

- arm64, macOS 27
- Apple clang 21.0.0
- The dev configuration of kirimasim `6cdbde8` (slang at `-O1`)

### Results

| Configuration | Example in the text | Example in the comment |
| --- | --- | --- |
| dev (`build/dev`) | `P = 0, Q = 3` | `P=0 assign=-3 var=-3` |
| A CLI with only slang built with `-fsanitize=float-cast-overflow` ("Steps to reproduce" below) | Detected (the values are `P = 0, Q = 3`) | — |

- In the comment's example, `assign` and `var` are -3 because, by the policy of section 6 of `docs/spec/SIR_SPEC.md`, expressions containing `$rtoi` are computed at run time. The parameter P is decided by slang, so it stays 0.
- The detection report (paths made relative to slang's source directory) is at the same place as on x86_64. The type name differs because on macOS `uint64_t` is `unsigned long long`.

  ```
  source/ast/builtins/ConversionFuncs.cpp:64:36: runtime error: -3.7 is outside the range of representable values of type 'unsigned long long'
  ```

- Disassembling `ConversionFuncs.cpp.o` of the dev configuration, the conversion from `double` to `uint64_t` within `RtoIFunction::eval` was this single instruction.

  ```
  fcvtzu    d0, d0           ; unsigned conversion; negative values become 0
  ```

  A small program that compiles to the same instruction (it only converts a negative `double` with `(uint64_t)`) was run on this Mac. -3.7 became 0. The x86_64 sequence keeps the result of the signed conversion and so gives -3, while this arm64 instruction gives 0. This is why the result differs by machine.

### Steps to reproduce on arm64

A CLI of slang alone with UBSan can be built without the network, using the dependency sources the dev configuration of kirimasim fetched. `<dir>` is any working directory.

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

Disassembly (`objdump` on macOS is LLVM's):

```sh
objdump -d --no-show-raw-insn -C \
  build/dev/_deps/slang-build/source/CMakeFiles/slang_slang.dir/ast/builtins/ConversionFuncs.cpp.o \
  | awk '/RtoIFunction::eval/{p=1} p{print} p&&/\tret/{exit}' | grep fcvtzu
```

## Effect on kirimasim (as of 2026-10-17)

This section describes kirimasim when the note was written. The workarounds it names were removed on 2026-10-18 (the addendum at the end).

- Section 6 of `docs/spec/SIR_SPEC.md` decides that expressions containing `$rtoi` are computed at run time, without relying on the constants slang folded. By this policy, expressions containing `$rtoi` get the right value on every machine.
- The same section says that parameter values are decided by slang, so this policy does not fix them. Parameter values differ by machine: -3 by chance on x86_64, 0 on arm64. Checking only on x86_64 does not reveal this error.
- The workaround in `cmake/Dependencies.cmake` (adding `-fno-sanitize=float-cast-overflow` only to slang in the ubsan configuration) suppresses this detection, as intended. When slang is raised to a fixed version, remove the workaround and check that ubsan detects nothing.

## Steps to reproduce

Build the dev and ubsan presets as usual.

```sh
cmake --preset dev   && cmake --build --preset dev   --target kirimasim
cmake --preset ubsan && cmake --build --preset ubsan --target kirimasim
```

The configuration with the workaround removed is made by putting the ubsan flags in `CMAKE_CXX_FLAGS` instead of `CMAKE_CXX_FLAGS_DEBUG`. The workaround in `cmake/Dependencies.cmake` is added by looking at `CMAKE_CXX_FLAGS_DEBUG`, so with this placement it is not added. No file of the repository is changed.

```sh
F="-O1 -g -fsanitize=undefined -fno-sanitize-recover=all"
cmake -S . -B build/ubsan-noworkaround -G Ninja -DCMAKE_BUILD_TYPE=Debug \
  -DCMAKE_CXX_FLAGS_DEBUG="" -DCMAKE_C_FLAGS_DEBUG="" \
  -DCMAKE_CXX_FLAGS="$F" -DCMAKE_C_FLAGS="$F" \
  -DCMAKE_EXE_LINKER_FLAGS=-fsanitize=undefined \
  -DCMAKE_SHARED_LINKER_FLAGS=-fsanitize=undefined
cmake --build build/ubsan-noworkaround --target kirimasim
grep -c float-cast-overflow build/ubsan-noworkaround/build.ninja   # 0 means the workaround is not added
```

Running:

```sh
build/<configuration>/src/driver/kirimasim parse rtoi.sv   # the example in the text
build/<configuration>/src/driver/kirimasim rtoi2.sv        # the example in the comment
```

Disassembly:

```sh
objdump -d --no-show-raw-insn -C \
  build/dev/_deps/slang-build/source/CMakeFiles/slang_slang.dir/ast/builtins/ConversionFuncs.cpp.o \
  | awk '/RtoIFunction::eval/{p=1} p{print} p&&/ret/{exit}'
```

## Addendum (2026-10-18)

Upstream, it was fixed on 2026-10-17 by commit 145b57c, which replaced `(uint64_t)val.real()` with slang's own conversion `SVInt::fromDouble(32, val.real(), true, false)` (truncating, not rounding). It was not in slang v12.0, which kirimasim pinned then, so at first the workaround of section 6 of SIR_SPEC.md stayed as it was.

On 2026-10-18, kirimasim raised slang to main commit b7f11c1 (which includes this fix), and removed the workaround of section 6 of SIR_SPEC.md and the exception for slang in the ubsan configuration.
