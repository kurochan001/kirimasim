# VPI whole-net write execution

[日本語](vpi-net-write-execution.ja.md) · 2026-11-04.

A deposit temporarily changes a point net’s public value, while force holds an override
until release; the [VPI net-write
FAQ](../FAQ.md#how-do-vpi-net-deposits-differ-from-forcerelease) explains the differing
lifetimes.

**Published integration (2026-11-04):** Main `91139559` includes this implementation. Its [full local CI and successful GitHub workflow](main-integration-2026-11-04.md) supplement the earlier checkpoints below; dated raw evidence is unchanged.

The approved [contract sections 1–8](vpi-net-write-proposal.md) now execute
immediate whole-net operations through front/SIR, tier 1 and the shared kernel.
This closes the measured ordinary whole-net write gap in the first VPI subset;
it does not establish full VPI or P1 completion. No dependency or public C ABI
changed. The separately approved [physical-origin count migration](vpi-net-write-expectation-migration.md)
is committed without `src/` changes as `1f33ba51`.

## Supported operations

Eligible targets are complete static integral four-state scalar/packed net
declarations, including undriven nets, collapsed port aliases and the supported
wire/tri/wired/pull/supply kinds. Argument handles and name handles use the same
verified numeric physical capability. A net flag on arbitrary storage is
insufficient; malformed SIR is rejected before runtime storage allocation.

- `vpiNoDelay` deposits cover point nets without passive edges in their physical
  component. A deposit changes the public snapshot at Strong 6, with X `[-6,6]`
  and Z `[0,0]`, without modifying drivers or adding a persistent root.
- Any actual committed driver-contribution change clears the deposit across
  the **whole physical declaration**, even if ordinary resolution is unchanged.
  Identical contributions and canceled pending work retain it. Distributed and
  module-path delays invalidate at actual commit, rather than scheduling.
- `vpiForceFlag` and `vpiReleaseFlag` share HDL per-bit ownership and the existing
  registered passive-network rules. Later force replaces the owner; an older
  HDL force cannot reclaim ownership just because its RHS changes. Drivers
  continue updating while forced, and release publishes their current resolution.
  A deposit made under force has no retained post-release effect.
- Complete vector/network snapshots and ownership are committed before ordinary
  callbacks. Synchronous callback reentry may install another force; the callback
  retains its original value snapshot and the replacement retains ownership.
  An unchanged visible value does not generate a duplicate value callback.
- Supported release output formats return the current post-release value.
  A null output pointer is allowed. Invalid output formats are rejected before
  clearing the force or changing caller storage. All three immediate modes
  ignore the time descriptor; read-only-region writes retain the runtime error.

VPI, integral reads, strength `%v`, VCD aliases and decoded FST observe the same
public snapshot. Internal physical notifications remain protected against
recursive publication; ordinary value-change callbacks run after that stage.
Context owns the runtime state. No hidden HDL process, timer or RNG owner is
created for an immediate operation. Public kernel headers still build as C++20.

## Evidence and interpretation

[Validation metadata](../measurements/2026-11-04/vpi-net-write-execution-validation.json)
binds the implementation checkpoint `c21d1e96`, source hashes and complete dev
JUnit. The count-only commit is `c61b55df`.

- The immutable [61-input pre-implementation inventory](../measurements/2026-11-03/vpi-net-write-inventory.json)
  still records 54 Icarus 13.0 oracle matches, four reference differences and
  three observations without oracles. The 58 original written native oracles
  now all pass. The two passive force observations have separate new oracles
  grounded in the existing physical-network contract. Passive deposit remains
  an explicit unsupported API case.
- The original four reference differences remain differences: clearing a
  deposit on a same-result driver change, retaining it on an identical
  contribution, displaced HDL ownership, and duplicate unchanged callbacks.
  Native follows the approved contract; these are not rewritten as reference
  passes. The earlier three compiled external-checker controls remain separate
  from the native controls below.
- `tools/gen_vpi_net_matrix.py` supplies **194 source inputs**. There are 166
  normal executions, 24 expected API rejections, one expected runtime error,
  two compile-phase checks and one dump-phase check. Four additional public
  VCD/FST observations bring the runner to **198 successful checks**. A successful
  rejection check does not imply that the rejected feature is supported.
- Coverage includes widths 1/32/33/65/129/257, four-state values, declaration
  contexts, tasks/functions/forks, name lookup, package/class static callers,
  aliases, driver changes, force ownership, callbacks, pending delays, phases
  and rejected selections/flags. Waveforms check distributed and module-path
  pending arrivals, alias identity and absence of hidden driver declarations.
- Fourteen new unit tests cover physical state, allocation failures, Context
  ownership, release/driver callback reentry and typed capabilities. Fourteen
  malformed SIR variants fail before runtime allocation. The shared observation
  assertion now requires exactly two/three physical origins and exactly one
  destination-q origin; original source and visible oracles remain unchanged.
- All [13 compiled native controls](../measurements/2026-11-04/vpi-net-write-native-controls.json)
  were caught by semantic assertions, with compiler failures and registry
  self-checks excluded. A driver-publication callback test was added when the
  initial reentry control exposed a coverage gap. After source hash restoration,
  all **80 focused CTests** pass again. Registry validation checks **1,191
  anchors**, none inapplicable; this does not claim all anchors were compiled.
- Complete dev configure/build/test passes **11,862/11,862**, with no failures
  or skips. The new source matrix is one CLI CTest. The fixed P1 population,
  historical external measurements and frozen 176-probe comparison are unchanged.

## Final local configuration checks

At checkpoint `5dae9e23`, standalone dev configure/build/test and the count check
pass, followed by all 17 steps of `python3 tools/local_ci.py --presets release,asan,ubsan`.
Release passes **11,863/11,863**; ASan and UBSan each pass **11,862/11,862**, with
no failures or skips. The release-only extra test is `cli/slot_event_default`.
Release flags, banned APIs, SPDX, fixture pairing, global-state checks,
clang-format and clang-tidy on all **194 translation units** pass.

The fresh work branch initially had no upstream tracking, so all **18 commits**
through that checkpoint were separately checked with `check_expected_change.py`
against `origin/main`; all pass. Tracking is now configured for subsequent checks.
This is local validation, not a new GitHub CI/Nightly or frozen-population run.

## Remaining scope and reproduction

Net-bit/part handles, unpacked-net array words, user-defined/real nettypes,
strength-valued puts, delayed net puts, returned event handles and passive
deposits remain outside this delivery, with API errors where exposed. These
boundaries create no new P1 exclusion. The approved optional Icarus `wire real`
exception remains limited to confirmed extension-dependent cases.

Direct inspection of the IEEE 1800-2023 text remains **unconfirmed**: the official
GET endpoint returned HTTP 418. The 2017 edition's 38.34/10.6.2 and existing
project physical-network rules were inspected. This limitation and the Strong-6
presentation choice were disclosed before approval; the edition audit is still
follow-up work. Upstream slang posting remains on hold.

```sh
cmake --preset dev && cmake --build --preset dev
ctest --preset dev -R '^(NetWrites|Vpi|ModulePathObservation|cli/vpi_net)'
python3 tests/cli/run_vpi_net_tests.py --kirimasim build/dev/src/driver/kirimasim \
  --fst-reader build/dev/tests/tools/kirimasim_fst_dependency
python3 tools/mutants.py --check
```
