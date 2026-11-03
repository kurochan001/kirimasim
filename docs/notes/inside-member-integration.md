# Integration of the existing inside-member repair

[日本語](inside-member-integration.ja.md)

Recorded on 2026-11-03 at local main `26a3e5a3` with uncommitted changes.
Upstream main remains `f3ea0379`. Two preceding local documentation commits
and unrelated handoff files are preserved. No commit or push is made here.

## Rule and implementation

The existing branch commit `b16163c7` contains the repair; it was not part of
current main. Its source change, generator, new matrix inputs and mutation
definitions have been integrated into the working tree. The tool-test
registration was adapted to the current CMake file. Existing expectations
and suite inputs are unchanged.

VALUE_SEMANTICS 2 and 3.10, with IEEE 1800-2023 §§11.4.13, 11.8.2 and 12.5.4,
requires each integral array member of an inside set to use the common
comparison type's extension rule. Extending by the member's own sign could
introduce wrong high bits or extra wildcard bits. `Lowerer::memberMatch`
centralizes scalar/listed and loaded-array member comparison;
`Lowerer::propagate` applies common-type extension. Ordinary inside, case
inside and their subroutine/continuous-assignment entries use those paths.
The kernel value semantics and reference implementation are unchanged.

## Verification

The reused `tools/gen_inside_member_matrix.py` emits four new suite inputs
for initial/task/always/fork. Each compares 120 expected output lines across
types, word boundaries, unknown bits, array dimensions, storage places and
array/listed/case-inside observations. The focused CTest selection passes all
12 entries. The [before/after harness record](../measurements/2026-11-03/inside-member-baseline.json) shows all four matrices fail on the original binary and pass on the repaired binary. The [full local-CI record](../measurements/2026-11-03/inside-member-validation.json) passes all 21 steps: dev, release, ASan, UBSan,
release flags, static policies, formatting, tidy, expectation guards and counts.
Dev/ASan/UBSan each pass 11,794 tests; release passes 11,795. No tests are excluded.

The unchanged external 2,785-case selection changes only the two inside
failures to PASS: 2,247 pass, 529 unsupported and nine remaining failures.
The original snapshot and complete-output replays remain in kirimasim-tests.
At that checkpoint, five label-cancellation failures awaited the
[semantic amendment](deferred-label-cancellation-proposal.md); the subsequently approved
[implementation and verification](deferred-label-cancellation.md) now repair them.
Four configuration failures are localized to the
[pinned elaborator](config-reset-dependency-gap.md); its isolated diagnostic
patch is not used in production or this full local CI.

Formal Nightly, seven consecutive Nightly fuzz runs, the frozen P1 population,
minimum-compiler CI and a new full external fixed-oracle suite run are not
claimed by these checks. Gate denominators and seed policies are unchanged.
