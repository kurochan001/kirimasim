# Update the slang pin to e2d1021

[日本語](slang-e2d1021-update.ja.md). Status: dependency update requested by the
user; validated locally, 2026-10-27. No existing oracle is changed.

## Selected revision and sources

The selected unmodified dependency is
[`e2d1021661f78b3f17d1edf7ea17f9d6f95fb168`](https://github.com/MikePopoloski/slang/commit/e2d1021661f78b3f17d1edf7ea17f9d6f95fb168),
the upstream `master` head confirmed when the user requested this update.
It is a development commit after release 12.1, 24 commits after the previous
pin `16bd3b920dccfbcde000195f9d38d9b00432c9c8`.
The selection is a full SHA, rather than a moving branch reference.

Primary sources are the [upstream comparison](https://github.com/MikePopoloski/slang/compare/16bd3b920dccfbcde000195f9d38d9b00432c9c8...e2d1021661f78b3f17d1edf7ea17f9d6f95fb168),
the [selected revision's changelog](https://github.com/MikePopoloski/slang/blob/e2d1021661f78b3f17d1edf7ea17f9d6f95fb168/CHANGELOG.md),
and the maintainer's [resolution of issue #2064](https://github.com/MikePopoloski/slang/issues/2064).
The latter identifies fix `a9f9110c276564c13204ded0e760a292e700e600` for an
internal failure after recovery from an invalid class member specifier.
That fix is an ancestor of the selected revision. Other changes affect constant
literals/selections, replicated struct patterns, type/name resolution and ports.
The changelog also reports stricter `$bits` checks for non-bitstream types;
SYSTEM_TASKS 8 already requires rejecting those types.

`cmake/Dependencies.cmake` selects this SHA without a slang patch. Existing
slang build options still disable its own tests, tools and installation. Its
existing mimalloc dependency resolves to **v3.6.0**, commit
`b18ab4eef383f655e760881b02eff8af02800879`, in the measured dev build.
No new top-level library or installed tool is introduced. The nightly job
identifier `slang-main` is retained; its clone follows the upstream default
branch, whose actual name is `master`.

## Independent verification

The baseline is kirimasim `13fe2b65` with the previous pin. A fresh dev build
and all **8,138** existing tests complete with zero failures and the two known
skips ([baseline record](../measurements/2026-10-27/slang-e2d1021-baseline.json)).
After changing only the dependency pin, the same 8,138 tests also pass, keeping
every existing expected value, source fixture and dump unchanged.

The new original [class-specifier recovery tool](../../tests/tools/check_class_specifier_recovery.py)
checks compilation-unit/module/package declarations × function/task/constraint
members × one valid and three invalid markers × parse/run: **72 checks**.
The old binary accepts the 18 valid controls but returns Internal (exit 3) for
all 54 invalid checks. The new binary accepts the valid controls and returns
InputError (exit 2) with positioned class-specifier diagnostics for all invalid
checks. This exercises CLI 5's input-error boundary, without changing language
semantics or weakening diagnostics. The [differential record](../measurements/2026-10-27/slang-e2d1021-recovery.json)
contains the original inputs, results and both frozen binary hashes.
No upstream test or issue reproducer was copied. No other simulator's source
was read. The permanent regression also fails when explicitly run against the
old binary and passes all 72 checks with the selected pin ([control](../measurements/2026-10-27/slang-e2d1021-control.json)).

[Final validation](../measurements/2026-10-27/slang-e2d1021-validation.json) passes
all 21 local CI checks: dev/ASan/UBSan pass 8,137/8,139, release passes
8,138/8,140, with zero failures and the same two known skips. All four build
presets use the exact selected SHA with clean, unmodified slang source trees.
The existing 29 dump expectations and PerfCount budgets pass unchanged.

[Old/new comparisons](../measurements/2026-10-27/slang-e2d1021-comparison.json)
retain sv-tests 244/344 and both 95-probe native status/stdout inventories.
The frozen P1 population is **1,452/1,466**, with **four failures** and ten
unsupported cases, for both dependency versions. A [per-case differential](../measurements/2026-10-27/slang-e2d1021-population-differential.json)
confirms identical outcomes and failing output for all 1,466 cases. The failures
are `ch6/test_string_atoreal`, `ch6/test_string_atoreal_2`,
`ch6/test_string_conversion` and `ch6/test_string_realtoa`: observed negative
string-to-real conversions return zero. They already fail with the old pin on
kirimasim `13fe2b65`; they are not attributed to this update or counted as passed.
Earlier population measurements on earlier kirimasim commits remain historical.

The initial comparison harness assumed there were zero baseline failures and
stopped after saving all measurements. Its [initial record](../measurements/2026-10-27/slang-e2d1021-initial-comparison.json)
is retained. The follow-up compares every case and explicitly retains the four
failures, rather than changing an oracle or claiming a zero-failure population.
[Third-party notices](../../THIRD_PARTY_NOTICES.md) also reflect the selected
slang/mimalloc revisions; the existing common MIT body was checked against both
official distributed license files ([record](../measurements/2026-10-27/slang-e2d1021-licenses.json)).

## Integration with current main before publication

Before the requested commit and push, main advanced by five commits to
`563241b9`. The dependency update was rebased onto that main; generated README
counts were reconciled through `tools/readme_counts.py`. This dependency commit
changes no `src/`, detailed specification or existing test oracle relative to
that main.

[Integration validation](../measurements/2026-10-27/slang-e2d1021-main-validation.json)
reruns all 21 local CI checks successfully: dev/ASan/UBSan pass 8,150/8,152,
release passes 8,151/8,153, with zero failures and the same two known skips.
The selected slang source remains unmodified in all four presets. Fresh
[population](../measurements/2026-10-27/slang-e2d1021-main-population.txt) and
[sv-tests](../measurements/2026-10-27/slang-e2d1021-main-svtests.json) runs retain
1,452 passes, four baseline failures and ten unsupported cases, and 244/344
sv-tests passes without previous-pass losses. The earlier measurements above
remain records of their original `13fe2b65` baseline.

## Limits

This update does not complete P1 or all class execution. A recovered syntax
error remains an error. Linux results remain unconfirmed; run the same gates there. The four baseline
population failures and complete P1 parity remain work. Existing expected values
must not be changed to hide a regression. Earlier dated reports retain their
measurement-time dependency pins.
