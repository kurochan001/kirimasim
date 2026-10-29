# Proposal: common simulator input options

[日本語](cli-common-input-options-proposal.ja.md). Status: **approved by the user
(2026-10-29)**. Source baseline: `7eb9606e`. The approved amendment is recorded
in [CLI 2.1](../spec/CLI.md#21-input-passed-to-slang-as-it-is).

## Policy and initial scope

Accept established spellings where simulator input options share a meaning.
Document differences in argument forms, defaults and search behavior instead
of promising complete command-line compatibility.

The approved change adds `-I` and `-D`, and exposes `-y`, which CLI 2.1 already
specified but the driver's direct-argument filter rejected. Retain
the existing `+incdir+`, `+define+`, `+libext+`, `-f` and `--top` behavior.

Approved additions to CLI 2.1:

- `-I<dir>` and `-I <dir>` add an include search directory. Repeatable;
  preserve user-directory order, including interleaving with `+incdir+`.
  Keep the existing local-include lookup policy. Do not make `-I` implicitly
  add a module-library directory.
- `-D<name>[=<value>]` and `-D <name>[=<value>]` define a macro. Omitted values
  expand to `1`, following the pinned slang frontend and the existing
  `+define+` path. An explicit empty value stays empty. Preserve the complete
  value, including `+` and additional `=` characters; do not translate the
  option through the differently delimited `+define+` syntax.
- Accept the already specified `-y <dir>` and `--libdir=<dir>` for library
  search; also accept attached `-y<dir>`. Use the existing slang library
  resolver and `+libext+` policy. Do not adopt additional include-search
  effects from another tool's `-y`.
- Apply these forms to both compile-and-run and `parse`. Missing or empty
  directory operands and empty macro names are input errors (exit 2).
  Preserve valid paths containing spaces and the distinction between
  compile-time macros and runtime plusargs.

The portable example uses explicit macro values:

```sh
kirimasim -Iinclude -DWIDTH=8 -DENABLE=1 -y rtl/lib -f sources.f
```

`-f` keeps its current working-directory-relative path policy. Other input
options such as preprocessing-only mode need their own behavior review.
Option names alone do not establish compatibility: Icarus `-v` means verbose
output, whereas Verilator and slang use it for source-library files.

## Evidence and limits

Official references checked on 2026-10-29:

- [Icarus command-line flags](https://steveicarus.github.io/iverilog/usage/command_line_flags.html)
  describe `-I`, `-D`, `-y` and their roles.
- [Verilator arguments](https://verilator.org/guide/latest/exe_verilator.html)
  document the attached `-I`/`-D` forms; `-y` also affects include lookup.
- [Lyra README](https://github.com/hankhsu1996/lyra/blob/main/README.md)
  says frontend options are shared with slang.
- [slang command-line reference](https://sv-lang.com/command-line-ref.html)
  documents include paths, macros, library search and command-file paths.

Original temporary preprocessing probes, without reading simulator source:

- Installed Icarus reports `14.0 (devel) (s20260301-411-gca6b8d376)`;
  installed Verilator reports `5.052`, build revision `vUNKNOWN-built20260905`.
- Both accept attached `-Iinc`, `-DVALUE=2+3`, explicit `-DFLAG=1` and
  `-DFLAG=`; the plus sign is preserved. Bare `-DFLAG` expands to `1` in
  this Icarus build and to an empty replacement in this Verilator build.
- Icarus accepts separated `-I inc` and `-D FLAG`; this Verilator build
  rejects both. The proposed separated forms are additional accepted syntax,
  not a claim that every reference tool accepts them.
- Before this amendment, the kirimasim dev executable reporting `938d60c833a6`
  rejected direct `-Iinc`. Its `parse -f` path accepted the same attached/separated
  frontend options and an explicit macro value. The source at the baseline
  retains this direct-argument restriction.
- Lyra execution and Icarus 13.0 behavior for these cases are **unconfirmed**.
  Confirm them with version-recorded original probes before claiming exact
  three-tool parity. Lyra is not installed locally; no tool was installed.

## Implementation and verification

The driver now validates and forwards the approved forms without rewriting
macro values. Help, both user manuals and both feature inventories are updated.
The new `cli/common_input` runner performs 93 independent checks across both
execution modes: attached/separate forms, local/user include order, spaces,
explicit/omitted/empty macro values, punctuation, existing-form mixing,
library order/extensions/top selection, distinct library/include paths,
runtime plusargs, command-file paths and invalid operands.

The new redefinition tests initially assumed the last predefinition wins.
Original probes through the existing `-f` path and the pinned frontend instead
keep the first predefinition. Those new oracles were corrected to preserve
the existing frontend behavior; the implementation was not changed to reorder
or replace definitions. No existing test body or expectation was changed.

Development CTest passes all 8,242 tests, with no failures or skips; the new
test count is regenerated from the full JUnit result. The new CLI runner also
passes in release, ASan and UBSan. Formatting, clang-tidy for the changed
translation unit, release flags, banned APIs, SPDX, suite-file consistency,
global-state checks and README counts pass. The manual's explicit-value
counter example prints `width=8 cycles=6` and ends at 70 ns as expected.
No dependency or language-semantics change is added. Full three-tool CLI
parity remains unconfirmed, as described above.
