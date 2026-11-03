# Review: slang update and static function return expectations

[日本語](slang-d02b7cda-update-proposal.ja.md) · 2026-11-03.

## Requested dependency update

The upstream `master` head checked for this update is
`d02b7cda02d3183f35c6c4d4db2b2fc03f19f4ae`, 25 commits after the current
`e2d1021661f78b3f17d1edf7ea17f9d6f95fb168` pin. It includes the
[upstream #2072 recovery fix](https://github.com/MikePopoloski/slang/issues/2072).
The update was prepared in an isolated worktree. The expectation migration
was approved by the maintainer on 2026-11-03 and applied separately in `fd10bd23`.
No upstream issue or comment has been posted. The posting hold remains in force.

The unmodified new dependency builds. An original nested-block recovery matrix
passes all 324 parse/run checks; the frozen old binary aborts on four selected
invalid-input controls while accepting both valid controls. Existing compatibility
mode and executable-driver matrices pass 409 and 1,728 checks. The new
`AllowMultiDrivenDynamicallySizedArrays` compatibility relaxation is cleared
alongside the existing two relaxations during strict executable-driver analysis.
Ordinary frontend acceptance stays delegated to the selected slang profile.

The first complete dev run has **11,784 passes and three failures out of
11,787**, with no skips. These three failures are not recorded as passes.
At that initial checkpoint, all original source fixtures, SIR expectations and
initialization assertions were unchanged.

## Concrete expectation migration for human review

Upstream commit
[`3efe5877f286203f5dd69f1d383a9e177700a218`](https://github.com/MikePopoloski/slang/commit/3efe5877f286203f5dd69f1d383a9e177700a218)
gives an implicit function-return variable the same lifetime as the function,
instead of always making it automatic. IEEE 1800-2023 **6.21** gives variables
inside static functions a static lifetime by default; **13.4.1** defines the
implicit variable with the function's name. Both clauses were checked in the
local licensed reference. SIR_SPEC 3.3/8 already places static function
variables in slots. No new SIR instruction or runtime contract is proposed.

The proposed migration is limited to three existing files:

- `tests/dump_sir/subroutines.expected`: represent `top.addg.addg` as a static
  slot instead of frame variable `addg`, and adjust the subsequent slot IDs
  and writer ranges consistently. The source and returned expression stay the same.
- `tests/dump_sir/task_wait.expected`: represent `top.inc.inc` as a static
  slot instead of frame variable `inc`. The source and returned expression stay the same.
- `tests/cli/run_init_values_tests.py`, case `formals`: check the source-default
  value on the first call to `static_return`; after assigning `9'h152` on the
  second call, require that exact value inside the third call and in its result.
  Preserve all automatic-return, output-argument and initialization-policy checks.

The old initialization assertion requires the static return variable to reset
on the third call. This contradicts static lifetime. The replacement adds explicit
retention assertions; it does not remove the case or allow multiple outcomes.
The complete proposed diff is retained locally as
`build/slang-upgrade/return-lifetime-expectations.proposed.patch`.
The maintainer approved this exact migration, now applied in `fd10bd23`,
separately from implementation changes under AGENTS.md.

The independent runtime comparison also records limits rather than treating
another implementation as the standard: Icarus 13.0 fails the two scalar static
retention checks, passes their automatic controls, and rejects both array cases
at compile time. The new slang pin corrects kirimasim's scalar and wide packed
static returns. Unpacked array/structure return retention still fails with both
old and new pins because kirimasim explicitly puts that aggregate storage in
frames. That is a pre-existing lowering limitation requiring separate work;
this proposal does not claim to fix it or count it as supported.

## Acceptance after approval

The reviewed expectations have been applied separately. Follow-up build and CI
results are recorded in the [dependency update](slang-d02b7cda-update.md).
An upstream dependency update alone does not complete P1.
