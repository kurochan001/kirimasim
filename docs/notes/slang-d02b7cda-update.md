# slang d02b7cda dependency update

[日本語](slang-d02b7cda-update.ja.md) · 2026-11-03.

The unmodified frontend is pinned to
`d02b7cda02d3183f35c6c4d4db2b2fc03f19f4ae`, the upstream `master` head
checked for this update, 25 commits after `e2d1021`. This development revision
after 12.1 includes [#2072](https://github.com/MikePopoloski/slang/issues/2072),
subroutine error recovery, class/type lookup, function lifetime, random seed
binding and non-ANSI interface-port fixes. Its existing mimalloc dependency
advances to v3.6.1 (`56dc5eb80145f9a3ba7ca23d47c6a248b9d8586c`). Both distributed
MIT texts are unchanged ([license verification](../measurements/2026-11-03/slang-d02b7cda-licenses.json)).
No new top-level dependency or installed tool is introduced. Posting remains
on hold; no upstream issue or comment was posted.

## Integration and independent controls

The [current profile inventory](../measurements/2026-11-03/compatibility-d02b7cda-profiles.json)
adds `AllowMultiDrivenDynamicallySizedArrays` to `vcs` and `all`. Strict executable
driver reanalysis clears it alongside the existing two relaxations. A new API
test checks dynamic arrays, queues and associative arrays, three conflicting
writer families and both modes, preserving single-writer controls and the
existing associative-array type limitation. Parse acceptance does not imply
executable support. A compiled omission control builds but fails this test;
restoration preserves the source hash and passes the focused tests
([recovery and control record](../measurements/2026-11-03/slang-d02b7cda-recovery.json)).

The original recovery matrix has 162 inputs: six scopes, three join forms,
three positions of neighboring statements and three nested-body forms.
All **324 parse/run checks** pass: disallowed function forks return a positioned
input error (2), while valid `join_none` controls succeed. The frozen old binary
aborts in four selected invalid checks and passes both valid controls. Existing
compatibility and driver CLI matrices pass **409** and **1,728** checks.

`$random(5)` is now rejected at 18:23 by slang's assignability check, before the
former seed check at 18:15. The current pin uses the existing strict `slang-main`
diagnostic profile. The source fixture, header and both profile oracles are
unchanged; the exact new position, diagnostic and exit 2 are required.

## Approved expectation migration

The first full dev run had 11,784 passes and three failures out of 11,787.
Slang now gives the implicit return variable the function's lifetime (IEEE
1800-2023 6.21 and 13.4.1). The maintainer approved the
[three-file migration](slang-d02b7cda-update-proposal.md) on 2026-11-03.
Commit `fd10bd23` changes only those expectations, separately from implementation:
two SIR dumps use static slots, and the initialization matrix requires retention
of the assigned static return value on the third call. Its **122 checks pass**.
Existing source fixtures and automatic-lifetime checks remain in place.

The [independent lifetime observations](../measurements/2026-11-03/slang-d02b7cda-return-lifetimes.json)
retain unsuccessful comparisons too. Scalar and wide packed static returns
improve with the new pin. Unpacked aggregate return retention fails with both
pins because kirimasim explicitly uses frame storage there. Icarus 13.0 also
fails the scalar static controls and rejects the array controls. These results
are not relabeled as passes or used to redefine standard lifetime. Aggregate
retention remains separate P1 work; this update does not implement it.

## Before/after comparisons

The [frozen population comparison](../measurements/2026-11-03/slang-d02b7cda-comparison.json)
has **1,456 passes, zero failures and ten unsupported cases out of 1,466** with
both binaries, with identical per-case statuses. It uses an immutable archive
of kirimasim-tests `1fd46b2adb860a8587988a509cf9e01bbe29d8f1`, without using or
changing its local edits. sv-tests at `15490e95fd978ada1c1dc59b7c4b53a7edb23b78`
retains **252/344** with no changed pass/fail outcomes.

All **176 original finite probes** retain their native status and stdout
([comparison](../measurements/2026-11-03/slang-d02b7cda-probes.json)). There are
175 P1-evaluated probes and one confirmed optional Icarus `wire real` extension
case. `dense_udp_coverage` still times out and remains an independent failure
and P1 gap. Updating slang does not establish complete Icarus parity. Earlier
dated measurements retain their original pins. Linux validation remains
unconfirmed; run the same gates there.

## Final local validation

[All 21 local CI checks](../measurements/2026-11-03/slang-d02b7cda-validation.json)
pass at `3859f210`: dev, ASan and UBSan each pass **11,787/11,787**,
release **11,788/11,788**, with zero failures or skips. clang-format,
clang-tidy (191 translation units), release flags, source policies, expectation
separation and generated counts all pass. All four presets use the exact
selected, unmodified dependency sources. All 1,157 mutation definitions apply;
the new compiled omission control was executed separately. This is validation
of the isolated branch, excluding concurrent edits in the main working tree.
