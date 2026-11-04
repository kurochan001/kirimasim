# Unmet and unconfirmed P1 exit criteria (2026-10-28)

[日本語](P1_CRITERIA_GAPS_2026-10-28.ja.md)

**Current-source pointer (2026-11-04):** This is a dated record. Published main `91139559` and its CI are described in the [integration record](notes/main-integration-2026-11-04.md); use [P1 Support](P1_SUPPORT.md) for present implementation. Measurements and decisions below retain their original scope.

Baseline: main `4c97244784cc107770924c6597e34cbd0186bd24`.
Measurement and nightly dates are UTC unless noted. This document audits the approved criteria;
it does not change specifications or exit criteria.

**2026-10-29 update:** the results below are measurements from October 28.
`--cond-x` and `--init-values` have since been implemented. See
[P1 support](P1_SUPPORT.md) and [initial-value examples](P1_FEATURES.md#source-initial-values)
for current support and regressions. Those additions alone do not complete P1.

**2026-10-30 update:** `--report-pulses` is implemented too. It reports inertially canceled updates
with source positions and times. See the [example](P1_FEATURES.md#report-removed-pulses).
The October 28 measurements below were not repeated.

**2026-10-30 update:** mixed fixed/dynamic-dimension `foreach` and legal dimension omission were added.
See [current support](P1_SUPPORT.md#dynamic-container-foundation) and
[examples](P1_FEATURES.md#mixed-dimension-foreach). The old remaining-work table and probe totals below
were not remeasured. Passing an existing probe that Icarus 13.0/14.0 rejects does not count as
meeting the gate for functionality that Icarus executes.

**2026-10-30 update:** array queries now accept runtime dimensions for fixed dimensions,
the outer dimension of dynamic arrays/queues, fixed element dimensions and strings.
See [examples and remaining limits](P1_FEATURES.md#runtime-array-query-dimensions).
The measurements and overall P1 assessment below are unchanged.

## Conclusion

**2026-10-30 update:** the remaining N01 delayed-NBA paths through fixed-array refs and packed selections
are connected, preserving formal bounds, subarray bases, issue-time indices and parent windows.
See [examples and remaining limits](notes/delayed-ref-selections.md).
The historical measurements and overall P1 assessment below are unchanged.

**2026-10-30 update:** delayed NBA through scalar refs to static actuals (N01's scalar form) is connected.
The destination after return, RHS and NBA/Re-NBA origin are retained.
See [examples and remaining limits](notes/delayed-ref-nba.md).
The historical measurements and overall P1 assessment below are unchanged.

**2026-10-30 update:** nested packed selections through refs are connected to event-NBA (N03).
The static parent window is retained, so an out-of-window child cannot update adjacent storage.
See [examples and remaining limits](notes/event-nba-ref-windows.md).
The historical measurements and overall P1 assessment below are unchanged.

**2026-10-30 update:** event-NBA through fixed-array refs (N02) is connected, including destination/value
retention after return, subarray bases and the shared count of a whole-array operation.
See [examples and remaining limits](notes/event-nba-ref-arrays.md).
The historical measurements and overall P1 assessment below are unchanged.

**2026-10-30 update:** automatic structures' fixed leaf-array members (A03) are connected,
including declaration initialization, structure arguments/returns and retained captures.
See [examples and remaining limits](notes/automatic-struct-array-members.md).
The historical measurements and overall P1 assessment below are unchanged.

**2026-10-30 kernel update:** the [node force and synchronous-release foundation](notes/strength-force-foundation.md)
was added as a prerequisite for force on registered strength nets.
Existing source/SIR rejections remain; this is not counted as completion of source force or `%v`.

**P1 is incomplete. The main exit criterion confirmed unmet is standard functional execution coverage
at least as broad as Icarus Verilog 13.0.**

- Rerunning 95 probes on the current version found six that match the oracle on Icarus 13.0 but fail on kirimasim:
  five standard-side probes and one Icarus-extension probe. The standard side has four unsupported diagnostics and one timeout.
- Those 95 probes omit unimplemented mechanisms such as VPI. Fixing just the five cannot establish P1 completion;
  inventorying necessary mechanisms/combinations against Icarus 13.0 remains work.
- **FST output, source `%p`, resolved-strength `%v`, `--cond-x` / `--init-values` and other behavior explicitly
  required by existing P1 specifications still need implementation.** An unfinished comparison inventory does not remove already specified P1 features.
- The fixed P1 population's 90% gate is met on the current version: **1456 / 1466, 99.3%**.
  Ten unsupported tests are not passes, but do not mean that the 90% condition is unmet.
- The “seven consecutive nights, 100-times fuzzing, no differences” condition has success evidence for
  **seven nightly runs on 2026-10-21–27**. README / `docs/counts.json` saying “five nights through October 21”
  is stale; that value is not evidence that the condition is unmet.
- All 14 ordinary CI jobs and all local CI checks pass. They do not substitute for functional coverage or final M8 assessment.

## 1. Formal criteria and assessment

The authorities are [TEST_SUITE](spec/TEST_SUITE.md) and [P1_SCOPE](spec/P1_SCOPE.md).
The [README P1 exit criteria](../README.md#p1-exit-criteria) summarize them.
“Unmet” means a gap confirmed by execution/implementation; “unconfirmed” means scope or measurement evidence is incomplete.

| Criterion | Assessment | Evidence and remaining checks |
| --- | --- | --- |
| At least as many passes as Icarus in pinned sv-tests simulation tests, on Linux and macOS separately | Met in saved records; both OSes not remeasured on current version | Linux 244 vs 211, macOS 244 vs 210. `docs/counts.json` dates are October 26/27. Current ordinary CI has no step rerunning this external comparison |
| At least 90% of the fixed 1466-test P1 population (at least 1320) | **Met on current version** | Rerun: 1456 pass, 0 ordinary failures, 10 unsupported. Expectations: kirimasim-tests `41085fe3432b`; list unchanged |
| Mandatory test list passes, apart from explicit exemptions | Evidence of meeting it | Current ordinary CI and four local configurations pass; Linux TSan passes. The October 27 nightly's one-process/shuffle/three-repeat step passes in all four sanitizer jobs. Revisions/scopes are separated in section 7 |
| Date-seeded 100-times comparison with `valsem` / reference scheduler, zero differences in seven consecutive nightlies | **Seven nights evidenced; aggregate record not updated** | Commands/results of formal October 21–27 fuzz steps checked; section 6. M8 must check its latest seven runs |
| Standard functional execution coverage at least as broad as Icarus Verilog 13.0 | **Unmet; complete correspondence unconfirmed** | Five standard-side probes fail in the current finite comparison. Concrete class/specify/partial-port gaps and unimplemented/unclassified scope including VPI remain |

Assessment notes:

- The sv-tests numeric gate uses pinned development commit `04cc493506bd4d75c861cfc9961e2a56517d7cc6`,
  not 13.0 stable. Do not confuse it with the 13.0 functional-coverage baseline.
- Pinned sv-tests commit `15490e95fd978ada1c1dc59b7c4b53a7edb23b78` has 344 hand-written simulation tests.
  It is separate from the 95 original probes and fixed 1466-test P1 population.
- The mandatory list includes `context_sequential`, `context_parallel`, `context_churn` and nightly unit shuffle.
  `front_parallel` is not a mandatory P1 criterion.
- `cb/input_sample_1step` is currently exempt from the mandatory list. The other skip,
  `nets/net_port_drivers_part`, is a connection implementation boundary. “Two skips” does not directly mean the mandatory-test criterion is unmet.
- Current ordinary CI, external comparisons on both OSes and nightly results are distinct evidence.

Sources: [mandatory tests and measurement procedures](spec/TEST_SUITE.md),
[fixed population](../tests/gates/p1_population.txt), [allowed skips](../tests/suite/skips.txt), [saved counts](counts.json).

## 2. Five standard-side probes confirmed failing on the current version

All 95 existing [original comparison probes](../tools/icarus_feature_probe.py) were run with the current release binary.
No new reference tool was installed and no other simulator source was read.

| Probe | Icarus 13.0 / 14.0 | kirimasim | Remaining work and verification |
| --- | --- | --- | --- |
| `class_objects` | Both match the oracle | exit 2, class type / `NewClass` unsupported | Connect object construction, instance state and methods; pass this probe and independent lifetime/inheritance checks |
| `class_constructor` | Both match the oracle | exit 2, class type / `NewClass` unsupported | Execute constructor arguments, initialization order and independent instances; non-parameterized static property support is not a substitute |
| `specify_path` | Both match with `-gspecify` | exit 2, specify with path delays / timing checks unsupported | Implement/test module-path delays and scheduler integration; specparams or ordinary gate delays alone are insufficient |
| `mixed_strength_port_bit` | Both match the oracle | exit 2, drivers merging through port connections unsupported | Connect partial-port net identity and multiple-driver strength resolution; test single/multiple-instance combinations |
| `dense_udp_coverage` | Both match the oracle | Reaches 15-second subprocess limit | Localize dense UDP source processing and verify completion; the timeout alone does not confirm cause, an infinite loop or nonconformance |

These are not “five unimplemented features”: classes have several probes, and some mechanisms are not covered.
The 15-second `dense_udp_coverage` bound belongs to this comparison tool; it is not a new numeric P1 performance gate.

Implementation evidence: [class construction/specify rejection](../src/front/lower.cpp),
[rejection of drivers merging through ports](../src/front/lower.cpp), [existing UDP support](P1_SUPPORT.md).

### Exclusions from the standard-side failures

- **`real_net`** is the sixth gap, classified `icarus_extension` (`wire real`). Track extension/compatibility policy
  separately under [P1_SCOPE](spec/P1_SCOPE.md), rather than counting it as missing standard functionality.
- **Reference mismatches:** four for 13.0, seven for 14.0. These mean Icarus differs from the standard-derived probe oracle;
  they are not kirimasim bug counts.
- The four 13.0 cases are `nested_packed_select_bounds`, `class_virtual_dispatch`, `dynamic_array_queries`,
  `blocking_repeated_event_one_bit_unknown`. kirimasim also lacks class dispatch; it matches the oracle for the other three.
- 14.0 adds `dynamic_minimum_duplicate`, `queue_maximum_duplicate`, `dynamic_minimum_with_key`.
  kirimasim matches all three. Existing expectations must not change to match the reference output.

## 3. Mechanisms outside the 95 probes and scope still unresolved

Approved [P1_SCOPE](spec/P1_SCOPE.md) brings necessary parts of former P4/P5 forward into P1.
It does not approve making every syntax form in the following categories unconditionally mandatory.
**The per-form mapping of what Icarus 13.0 executes, standard versus extension status and kirimasim's missing boundary is incomplete.**
“Unconfirmed” does not remove features already explicitly assigned to P1/M7.

| Area | Remaining implementation/specification boundary | P1 treatment |
| --- | --- | --- |
| VPI / external connections | VPI runtime, objects/callbacks, value/time bridges; `VPI_SPEC` lacks detail | Necessary VPI is explicitly P1. Set comparison scope and detailed semantics; approve needed proposals before implementation. Do not add all of cocotb as a separate gate |
| Classes | Object creation, instance properties, dispatch, inheritance/lifetime/collection, parameterized static storage | Basic construction/constructors are measured gaps in section 2; expand comparison for the rest |
| Dynamic containers / array methods | Associative arrays, queue-slice assignment, more locators/shuffle, mixed/partially omitted dynamic foreach, nested/runtime dimension queries, aggregate/class/event elements, implicit sensitivity, effectful ordering keys | Identify necessary parts by comparison. Do not relabel working basic dynamic-array/queue construction, copying, refs, waits, reduction, sort and min/max as unimplemented |
| Events / process control | Scalar event returns/conditionals, automatic structure event fields, rebinding, event containers, event-NBA ref/managed-value boundaries, event-controlled `->>`, named-fork disable, `wait_order` | Legality and Icarus 13.0 support per form unconfirmed; distinguish working automatic capture, input events and basic event-NBA |
| Nets / switches | Remaining partial-port/alias identity, force/release on registered strength nets, complete strength formatting, delayed net declarations without assignment, charge storage | Partial-port gap measured in section 2; compare other forms individually. All six MOS and six passive switches are not wholly unimplemented |
| specify / timing | Module paths, timing checks and interactions | Module-path gap measured; compare/implement needed timing kinds, legality and interactions |
| Aggregates / selection / streaming | Structure refs, structure-array arguments/automatic storage, copyback slices, moving packed parents, streaming `with`/string/nested targets | Preserve existing fixed-aggregate/wide-streaming support and map missing forms to Icarus 13.0 |
| UDP | Unconnected ports, dense-table source-processing boundary | Section 2 records the dense-table timeout; compare unconnected ports and other necessary scope separately. Wide evaluation itself is implemented |
| FST output | Dependency build prepared; backend unimplemented | **Explicit remaining P1 work in existing specifications**: `WAVEFORM` says P1, `CLI` says M7. Do not demote it to unconfirmed comparison scope |
| Formatting / input / more dump scopes | Source `%p`, resolved-strength `%v`, program/interface VCD scopes, remaining `$fread`/scan/packed-format forms | Separate missing existing P1 contracts such as `%p`/`%v` from unconfirmed scope of additional forms; distinguish foundations from source execution |
| Specified CLI behavior | `--cond-x`, `--init-values`, etc. | **Existing CLI M7 behavior unimplemented**; this is missing specified behavior, not a new numeric gate |

Evidence: [support and limits](P1_SUPPORT.md), [remaining README P1 functionality](../README.md#remaining-p1-functionality),
[incomplete VPI spec](spec/VPI_SPEC.md), [existing P1 waveform spec](spec/WAVEFORM.md),
[CLI M7 requirements](spec/CLI.md), [formatting/I/O](spec/SYSTEM_TASKS.md),
[dynamic foreach boundary](../src/front/lower_stmt.cpp), [FST rejection](../src/driver/main.cpp), [dump scopes](../src/front/lower_dump.cpp).

## 4. Ten tests not yet running in the fixed P1 population

The population was rerun with **current main and approved expectation revision `41085fe3432b`**.
The 1466-test list and expectations are unchanged: 1456 pass, 0 ordinary failures, 10 unsupported.

| Test | Stopping construct |
| --- | --- |
| `ch16/test_comb_stable_multiinstance` | `$stable` |
| `ch16/test_sampled_function` | `$sampled` |
| `ch16/test_system_sampled` | `$sampled` |
| `ch18/test_randsequence` | `randsequence` |
| `ch25/test_comb_interface_modport` | Virtual interface |
| `ch25/test_comb_vif_array` | Parameterized virtual interface (rejected at first scalar declaration) |
| `ch25/test_interface_modport_corner` | Virtual interface / modport form |
| `ch7/test_assoc_foreach_struct_key` | Associative array with structure key |
| `ch7/test_multi_dim_assign` | Dynamic array of fixed arrays, `int dyn_of_fixed[][0:3]` |
| `ch7/test_struct_array_sort` | `sort with` on a fixed structure array |

The assessment is **1456 / 1466**, not “1456 / 1456” with unsupported tests removed from the denominator.
These ten do not prevent the 90% numeric gate from passing, but the pass rate does not prove full functionality.
Nor does this table establish every item as required Icarus 13.0 functionality without comparison.

Sources: [pinned external suite](https://github.com/kurochan001/kirimasim-tests/tree/41085fe3432bb093bf52c4f33bc4b44fff1a9401/tests),
[counting rules](../tools/p1_population.py).

## 5. Distinguishing bug ledgers from exit criteria

Passing a rate gate does not exempt IEEE value/scheduling requirements.
Conversely, do not add failures from the entire external suite to failures in the fixed 1466-test population or to the count of unmet P1 criteria.

State including this work:

- [#66](https://github.com/kurochan001/kirimasim/issues/66), integer conversion of queue-method indices, is fixed and its reporting test passes.
  [#70](https://github.com/kurochan001/kirimasim/issues/70), invalid `$timeformat` arguments, is fixed and checked by seven new regressions;
  its external reporting test was not run in this work.
- Runtime fixes exist for [#61](https://github.com/kurochan001/kirimasim/issues/61) shortreal conditionals and
  [#65](https://github.com/kurochan001/kirimasim/issues/65) foreach with all indices omitted.
  Separate problems remain for `localparam real` / constant-function values determined by slang during elaboration.
  Do not mark the entire issues fully resolved.
- For [#69](https://github.com/kurochan001/kirimasim/issues/69), the generated VCD contains named-fork scopes;
  the external test reader skips lines. An [independent regression](../tests/suite/proc/dump_named_fork_scopes.sv) passes.
  Do not count it as a remaining VCD implementation gap.
- “All 25 confirmed” in the [October 28 non-slang ledger](kirimasim_nonslang_triage_2026-10-28.md)
  predates these follow-up investigations/fixes. It is not a current confirmed-remaining count.
- [#58](https://github.com/kurochan001/kirimasim/issues/58)'s config-selection proposal conflicts with the current
  [CLI spec](spec/CLI.md), which rejects multiple configs. It requires an approved amendment, not a silent change to satisfy tests.
- This is not a fresh triage of every public bug. Uninvestigated issues have not been marked complete.

## 6. Seven consecutive fuzz nights: record maintenance, not an unmet gate

The formal [TEST_SUITE](spec/TEST_SUITE.md) “Cross-check with valsem” criterion adds to fixed-seed ordinary CI:
100-times, date-seeded comparisons with no differences in the seven consecutive nightlies preceding assessment.

All seven runs below are `event=schedule` Nightly runs.
The actual logs confirm commands/results for `Fuzz against valsem and the reference scheduler`.
Each sets `KIRIMA_FUZZ_SCALE=100` and `KIRIMA_FUZZ_SEED=$(date -u +%Y%m%d)`.

| Nightly date | commit | Formal fuzz result | Evidence |
| --- | --- | --- | --- |
| 2026-10-21 | `b27968d1f3c1` | 46 tests passed | [p1-gates](https://github.com/kurochan001/kirimasim/actions/runs/39681273172/job/119504065777) |
| 2026-10-22 | `6ec9964c14b4` | 47 tests passed | [p1-gates](https://github.com/kurochan001/kirimasim/actions/runs/39858890595/job/120104127806) |
| 2026-10-23 | `aae29deaed98` | 104 tests passed | [p1-gates](https://github.com/kurochan001/kirimasim/actions/runs/40026756027/job/120664754604) |
| 2026-10-24 | `c507631d2529` | 104 tests passed | [p1-gates](https://github.com/kurochan001/kirimasim/actions/runs/40131731554/job/120984671470) |
| 2026-10-25 | `a4f653c1d0fd` | 108 tests passed | [p1-gates](https://github.com/kurochan001/kirimasim/actions/runs/40232848500/job/121288067792) |
| 2026-10-26 | `1650064d21c7` | 108 tests passed | [p1-gates](https://github.com/kurochan001/kirimasim/actions/runs/40414439435/job/121893084547) |
| 2026-10-27 | `ae5f9dcd1d2a` | 110 tests passed | [p1-gates](https://github.com/kurochan001/kirimasim/actions/runs/40593559897/job/122503629425) |

October 21–22 used filter `W4Ops.*:Valsem.*:Scheduler.*`; October 23–27 added `Heap.*`.
The formal criterion's comparisons are present in all seven. `Wide.*` is a separate step and not included in these counts.

The workflow / p1-gates job failed on October 23–26, but the failed step **inside p1-gates** was `README counts`.
The target fuzz step passed; a red workflow alone is not a fuzz failure or a reset of the consecutive count.
The target steps also passed in all six manual Nightly runs on October 21–27.
For example, the [October 22 manual run](https://github.com/kurochan001/kirimasim/actions/runs/39736534314/job/119683430177)
and [October 27 manual run](https://github.com/kurochan001/kirimasim/actions/runs/40523863226/job/122257954481)
also failed only at `README counts` within p1-gates. Manual runs are not added as separate nights.

This confirms at least seven nights, not the longest streak.
The specification does not require the same commit on all seven nights.
Do not add a new requirement to wait seven nights on the identical HEAD merely because the current revision has no nightly yet.
M8 must check the latest seven runs at assessment and the absence of intervening reference differences.

**Remaining record update:** `docs/counts.json`'s `fuzz.nights=5` / `through=2026-10-21` and the generated README text need updating.
This audit did not change those existing files.

## 7. Measurement revisions and limits

| Subject | Revision/scope checked here |
| --- | --- |
| kirimasim | `0.0.0 (commit 4c97244784cc)`, release, AppleClang `21.0.0.21000334` |
| Stable functional reference | Compiler/runtime both `Icarus Verilog 13.0 (stable) (v13_0)`, distinct from older `v13_0-dirty` records |
| Development functional reference | Compiler/runtime both `14.0 (devel) (s20260301-411-gca6b8d376)` |
| Functional comparison inventory | Existing 95 original probes rerun against both references: six gaps each; stable four / devel seven reference mismatches |
| External P1 population | Isolated expectation commit `41085fe3432bb093bf52c4f33bc4b44fff1a9401`; unchanged 1466-item `tests/gates/p1_population.txt` |
| Current ordinary CI | [Run 40695675823](https://github.com/kurochan001/kirimasim/actions/runs/40695675823), all 14 jobs pass; ordinary tests, sanitizers, minimum supported compiler, lint and Linux TSan checked |
| Local CI | Same current commit: dev/release/ASan/UBSan, PerfCount, format/tidy, expectation protection and every other check pass; two existing skips |
| Nightly shuffle / TSan | [October 27 Nightly](https://github.com/kurochan001/kirimasim/actions/runs/40593559897), commit `ae5f9dcd1d2a`; shuffle passes under Linux/macOS ASan/UBSan; both OS TSan jobs pass |
| Both-OS sv-tests comparison | Saved aggregates and nightly records; **not remeasured for current `4c972447` by this audit** |

Remeasurements of the current original probes/fixed population and evidence from nightlies at different revisions remain separate.
This is not the final M8 assessment report.

Rerun outputs are local under `build/p1-criteria-audit-2026-10-28/`.
Tracked [October 27 stable](measurements/2026-10-27/slang-e2d1021-new-icarus13.json) and
[devel](measurements/2026-10-27/slang-e2d1021-new-icarus14.json) comparisons use kirimasim `13fe2b65c817`,
so they are distinct from this rerun.

## 8. Checklist through P1 completion

### Implementation and functional coverage

- [ ] Pass the five standard-side probes in section 2; add independent regressions, interaction and lifetime checks.
  First localize the dense UDP timeout.
- [ ] Specify needed VPI features, callback regions and external boundaries; implement unspecified semantics only after a proposal.
- [ ] Fill existing P1 implementation gaps such as the FST backend, source `%p`, resolved-strength `%v`,
  specified `--cond-x` / `--init-values`. Propose unspecified details first.
- [ ] Compare remaining forms in section 3 with Icarus 13.0; record standard requirements / extensions / exclusions /
  reference defects / unconfirmed forms individually. Do not defer necessary work solely because it was formerly P4/P5.
- [ ] Triage unresolved value, region-ordering and static-evaluation bugs and fix missing required standard semantics.
  Keep #61/#65 constant-evaluation work distinct from runtime fixes.
- [ ] Inventory extensions and external interfaces separately. Prepare approved compatibility policy/amendments for extensions
  conflicting with strict acceptance or changes conflicting with existing specs.

### Assessment and records

- [ ] At the M8 revision, compare sv-tests with its pinned reference on Linux and macOS separately and record the fixed 1466-test
  population and mandatory-list results. Do not replace the population or expectations for convenience.
- [ ] Check the latest seven formal nightly fuzz runs before assessment. This seven-night evidence is usable;
  ordinary CI, one local fuzz run or separate libFuzzer runs are not substitutes.
- [ ] Update stale fuzz-night counts and ledgers missing later investigations such as #69.
  This maintains records; it is not another unmet fuzz requirement.
- [ ] Prepare the M8 report with revisions, reference versions, population, unsupported/reference differences,
  seeds, consecutive-success evidence and the functional correspondence inventory.
- [ ] The responsible people must reconcile human full/sample reviews and generator-rule checks.
  Completion is unconfirmed from public information. Publish summaries of counts, sampling seeds and corrections,
  not standard text or private review material.

The final human review is required for the M8 report by [TEST_SUITE](spec/TEST_SUITE.md), separately from its pass-rate gates.
This document does not newly require SIR v1 freeze (P2), C++ generation / Tier 2 (P3), or UVM/constrained randomization
beyond the Icarus comparison scope in P1. Performance-regression issues matter too, but no independent numeric performance
gate absent from the formal P1 criteria is added.

## 9. How to remeasure

Use existing matching compiler/runtime pairs and verify versions first with `--version` / `-V`.
The variables below denote locally verified executables and the pinned suite.
Installing dependencies/tools follows repository approval rules.

```sh
build/release/src/driver/kirimasim --version
"$IVERILOG_13" -V
"$VVP_13" -V
python3 tools/icarus_feature_probe.py \
  --kirimasim build/release/src/driver/kirimasim \
  --iverilog "$IVERILOG_13" --vvp "$VVP_13" --extended \
  --output build/p1-criteria-audit-2026-10-28/icarus13-current.json \
  --require-parity
```

Gaps remain, so `--require-parity` currently returns 1, after saving its JSON.
Run 14.0 separately with matching binaries and another output file.
This exit code includes all 95 gaps, including extensions such as `real_net`;
it is not itself an independent gate for standard-side P1 completion.

```sh
python3 tools/p1_population.py run \
  --tests-repo "$P1_TESTS_REPO" \
  --kirimasim build/release/src/driver/kirimasim --jobs 12
```

`P1_TESTS_REPO` denotes `41085fe3432bb093bf52c4f33bc4b44fff1a9401`.
Do not run `select` to replace the denominator in this rerun.
Ordinary CI uses `python3 tools/local_ci.py`; formal external/nightly procedures are in
[nightly.yml](../.github/workflows/nightly.yml) and [TEST_SUITE](spec/TEST_SUITE.md).
