# Source pattern-format validation

[日本語](source-pattern-validation.ja.md)

The approved SYSTEM_TASKS 3.9 source bridge is implemented at `3b80f17c`.
The [expectation-only migration](source-pattern-expectations.md) is `8e0eedca`;
it retains the original source inputs. Current manuals and counts follow in
`c718251c` and `8ff9e0af`. This is a working-branch result, not a P1-completion
claim. Main integration and push remain separate; slang posting stays on hold.

The [machine-readable record](../measurements/2026-11-01/source-pattern-formatting.json)
contains code revisions, native binary/version and source hashes, configuration
coverage, the source matrix and individual compiled-control failures. All
tested runtime/test/tool code is unchanged from `8e0eedca`.

## Positive checks

- Full dev: **11,321 passed**, zero failures or skips (142.43 seconds including
  CTest invocation overhead). The successful full JUnit is the only input for
  the final passed-count refresh.
- Release, ASan and UBSan: **70 related checks passed per configuration**,
  zero failures/skips. These are the 20 new source tests, 47 existing kernel/SIR
  format tests and three CLI regressions: pattern, runtime string and binary
  formats. All four builds include the C++20 public-header target with warnings
  treated as errors. Full regressions for the other three profiles and the
  complete pre-push local-CI run have not been performed in this batch.
- The original source matrix covers **196 programs / 4,312 conversions** across
  12 contexts and 17 APIs. It compares pattern tokens without requiring field
  whitespace, while retaining quoted string contents and escaped identifiers.
  Each of the three profile cohorts also runs this matrix.
- Both historical CLI runners pass; the runtime-format runner retains 26 cases.
  The unchanged `display_p.sv` has seven executing calls and eight compiled
  contexts. No existing suite source or performance budget changes.
- The runnable example in both language manuals produces the documented output.

## Compiled controls and restoration

Eight registered controls each compile under the dev warning policy and are
detected by semantic tests: runtime presentation mode, packed member offsets,
single argument evaluation, detached monitor snapshots, four-state monitor
comparison, raw scalar preservation, presentation-operand types and unrelated
object coexistence. Neither anchor self-checks nor build failures count as
detection. The focused execution uses 67 kernel/SIR/source tests per control;
the JSON lists every failing detector. Baseline and restored runs pass all 67.

Restored source hashes equal the pre-mutation hashes. A further restored
70-check cohort, including all three CLI regressions, also passes. Reproduction
with the repository's broader default detector set is available through:

```sh
python3 tools/mutants.py --preset dev --only 'source pattern:'
```

All 1,093 registered mutation anchors apply. The eight changed C++ translation
units pass clang-tidy 23.1.4 with zero final source diagnostics; clang-format,
banned APIs, SPDX, expectation-file pairing, global-state analysis/probe,
Python compilation and per-commit expectation separation also pass.

## Remaining scope

See the [supported values and boundaries](source-pattern-formatting.md).
Whole object/event/chandle identities, associative storage, unsupported
tagged/unpacked unions and object-dependent postponed observation remain.
This adds no dependency or new formatting policy. Dated population, sv-tests,
Icarus and fuzz evidence is retained without remeasurement; the finite P1
comparison is not claimed to gain another closed gap.
