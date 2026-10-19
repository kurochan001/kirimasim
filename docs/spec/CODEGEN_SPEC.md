# CODEGEN_SPEC: C++ generation

[日本語](CODEGEN_SPEC.ja.md) · This English text is the main version; the Japanese version follows it, and where they differ, this text holds.

Priority: P2 (moved from P1 on 2026-10-15, since C++ generation is P2 work). This document has headings only so far, except the section "Floating point", added on 2026-10-19 (proposed, awaiting approval).

## Rules of C++ generation

Not written yet (to be written in the phase the priority table gives).

## Static islands

Not written yet (to be written in the phase the priority table gives).

## Two-version switching

Not written yet (to be written in the phase the priority table gives).

## Floating point

Proposed 2026-10-19, awaiting approval.

- **Rule:** the execution kernel, the VM, the rest of kirimasim's own code (`src/`, `tests/`) and the C++ that tier 2 generates are built with `-ffp-contract=off`, and without `-ffast-math`, `-Ofast` or `-funsafe-math-optimizations` (CLAUDE.md). Dependencies are not held to this. Math functions (`$sin`, `$exp` and the like) are left to the system's libm, so their last bits may differ between systems; that is the one exception to NFR-2 for reals.
- **Why:** an expression on `real`, an IEEE 754 double (6.12 of the standard), must give the same bits on every tier and OS (NFR-2). A compiler that fuses a multiply and an add into one FMA instruction does not round the product before the addition, so the result can differ in its last bit from the two operations done apart. Whether it fuses depends on the compiler, the target and the flags.
- **Checked 2026-10-19:** `double f(double a, double b, double c) { return a * b + c; }` was compiled with `-std=c++23 -S`, at `-O2` and at `-O3` (the two gave the same), and the output searched for FMA instructions (`fmadd` on arm64, `vfmadd` on x86-64). The machine was an arm64 Mac; the other targets were compiled with `--target`, not run.

| Compiler | Target | Without `-ffp-contract=off` | With it |
| --- | --- | --- | --- |
| Apple clang 21.0.0 | arm64-apple-macos | FMA (`fmadd`) | No FMA (`fmul`, `fadd`) |
| clang 22.1.8 (Homebrew LLVM) | aarch64-linux-gnu | FMA | No FMA |
| clang 22.1.8 (Homebrew LLVM) | x86_64-linux-gnu | No FMA (`mulsd`, `addsd`) | No FMA |
| clang 22.1.8 (Homebrew LLVM) | x86_64-linux-gnu, `-march=haswell` | FMA (`vfmadd213sd`) | No FMA |
| GCC 16 (Homebrew) | arm64-apple-darwin | FMA | No FMA |

- So with the present presets, which give neither `-ffp-contract` nor `-march`, a build for arm64 (macOS, or Linux on arm64) fuses where a build for x86-64 does not, and a result on `real` can differ between the two. Apple clang fuses by default (it defines `__FP_FAST_FMA`).
- **Not confirmed:** GCC on x86-64 Linux, the compiler of the Linux CI jobs, with and without `-march` (to check: the same compile in a CI job); whether the macOS runner of CI (`macos-26`) is arm64 (to check: `uname -m` in a CI job); which tests of reals change once the flag is given (to check: run the suite before and after).
