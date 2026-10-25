# Directed MOS sources and ordinary physical contributions

[日本語](source-mos-execution.ja.md). Status: implemented partial checkpoint,
2026-10-26; full MOS/switch contract and P1 remain incomplete.

## Grounds and scope

`e7e2ec59` connects six directed MOS primitives (`nmos`, `pmos`, `rnmos`,
`rpmos`, `cmos`, `rcmos`) from slang's elaborated model to the existing
[strength-network SIR/shared-kernel path](strength-network-execution.md).
Grounds: the approved [MOS/switch contract 1/6/7](mos-switch-proposal.md),
its [normative mapping](p1-execution-contracts-2026-10-25.md), SIR_SPEC
3.5/5/7/10 and IEEE 1800-2023 4.9.5/28.7/28.8/28.10/28.13/28.16.
This is original implementation and original testing against those contracts;
no external simulator implementation or tests were read, and no standard text
or examples were copied. No dependencies, external tools, ABI, GC safepoints,
PerfCount budgets or format-version changes are introduced. Tier 1 retains
its direct kernel C++ calls.

Supported here are eligible four-state integral structural data nets and
ordinary data expressions, scalar fixed output terminals (including the
scalar elements slang elaborates for primitive arrays), captured data
selectors/controls, and scalar MOS delays. Input selection includes packed
windows, fixed unpacked arrays and existing eligible whole-net port aliases.
A source expression uses an ordinary strong-strength carrier; a structural
net selection reads the published complete interval. Invalid captured
selectors retain the existing full-width X semantics.

This is not complete MOS/switch support: passive connectivity/world solving,
partial/retained port identities, registered-net force/release, full-interval
`%v` and future VPI strength observation still require work. Existing compact
fixed-array driver-import restrictions, including three-state side resolution
on unpacked array targets, remain explicit. Source terminal legality still
comes from pinned slang; metadata does not loosen its diagnostics.

## One lowering rule and one publisher

`Lowerer::lowerMos` separates two continuous processes. The capture process
evaluates ordinary control expressions, data expressions and selectors under
the existing source implicit-sensitivity rules. It stores owned typed values
in hidden variables. A separate pure device process reads those captured
values and the structural data interval, executes `strength.mos`, and writes
its owned scalar origin through `strength.drive` or `strength.drive.delay`.
A strength-only data change therefore cannot repeat control functions, their
side effects or RNG calls. Ordinary dependencies still recapture as required;
called-function implicit-sensitivity rules are unchanged.

`prepareStrengthNets`, the existing scalar/fixed-array resolver construction,
and `finishStrengthNets` import actual ordinary contributions. The temporary
legacy resolver's typed input tuples identify private driver values, static
ranks and optional side fields. Its recorded process ID becomes a pure
contribution adapter; no published resolved net is fed back as a physical root.
Untouched nets keep their previous path. Each marked physical slot gets full
bit/element node coverage; one component groups that net's complete publication.
This grouping is not passive connectivity or world correlation.

Implicit pull/supply roots are materialized exactly once and the effective
core net kind is normalized. Existing driver coverage excludes unwritten
bits from physical-origin counts. New MOS origins start Z; ordinary origins
retain their initial X/static ranks. Previously unresolved single three-state
drivers need a separate validity bit: before first propagation an unused side
field must not erase their initial X. For delayed gates that bit commits with
the complete existing value/side pending tuple. Already-resolved legacy tuples
retain their previous zero side-field initialization. Loading node metadata
still does not perform observable resolution; initial work uses the shared
scheduler's Active queue. Only its component publisher writes registered nets.

Force-marked registered nets, including force through aliases, are reported
as positioned unsupported input until the hold bridge exists. The kernel and
verifier continue to forbid bypass writes. No symptom/filename recognition or
source-string rewriting is introduced.

## Tests and reference observations

The original generator `tools/gen_source_mos_matrix.py` supplies **154 source
designs**. Its expected values enumerate signed strength-point choices under
the approved contract; they are AI-drafted oracles, not human-reviewed test
samples. Axes include all six kinds, weak/pull/strong/supply roots and opposing
loads, module/generate/interface ownership, widths 1/9/65/129, primitive
arrays and whole-net ports, descending/rank-two unpacked data, captured
129-bit selectors with X/Z/overflow, expression carriers, all four three-state
gates with/without existing resolution, initial side fields, inertial timing,
implicit roots, and strength-only changes with control call/event counters.
Six additional tool probes check positioned force/alias-force, passive and
formatting boundaries and strict slang terminal diagnostics. All 278 focused
checks pass, including the unchanged 29 dump goldens and PerfCount.

[Eleven compiled faults](../measurements/2026-10-26/source-mos-controls.json)
are caught by actual new test failures: lost provenance/sensitivity, wrong
MOS kind, discarded selectors, wrong imported ranks/sides/startup and discarded
delays. Two first attempts failed compilation due to unused loop variables;
they are not detections. Their corrected controls compile and fail tests.
Every mutated source was restored byte for byte; all 281 restored checks
(including three additional storage-width checks) passed before full CI.

The [Icarus comparison](../measurements/2026-10-26/source-mos-icarus.json)
executes all 154 original sources with **Icarus Verilog/runtime 13.0
`v13_0-dirty`**, recording the executable hashes. All compile; 113 complete
stdout results match and 41 differ:

- 36 selected-data cases differ at the overflowing 129-bit packed position:
  this implementation returns X; the reference returns the selected bit-zero
  value. Full-width index validation is retained under the approved contract.
- Three delayed startup cases differ under the explicitly approved new-MOS-Z,
  ordinary-initial-X and legacy-side-mask startup choices. The proposal records
  those choices separately from reference observations.
- Two `notif0` transition cases differ after a disabled-to-L-to-full-X sequence.
  The [reduced original observation](../measurements/2026-10-26/source-mos-transition.json)
  records unchanged X projection with a changed interval: the approved rule
  updates the opposed MOS output to X while the reference keeps 0.
  The reference implementation cause is **not confirmed**; reproduce the
  recorded source and vary the preceding states/fanout to investigate further.

These differences are preserved as observations, not erased by changing
oracles. This finite matrix does not establish whole-tool Icarus parity or
complete P1. No refreshed external sv-tests/population baseline, Linux/minimum
compiler run or five-simulator speed comparison is claimed.

## Validation and pending review

The [full local CI record](../measurements/2026-10-26/source-mos-validation.json)
completes all 21 checks: **17 pass and four preset test steps fail**. Dev, ASan
and UBSan pass 5,361/5,364; release passes 5,362/5,365. Each has exactly one
failing CTest case, `cli/options`, and two known skips
(`cb/input_sample_1step`, `nets/net_port_drivers_part`). All 154 new designs and
six boundaries pass in every preset. Werror builds, existing dumps/PerfCount,
policy checks, generated README counts, clang-format and all 107 clang-tidy
sources pass. The automatic expectation check has no upstream to compare;
the manual `e7e2ec59^..e7e2ec59` check passes with existing expectations kept.

The historical nmos rejection input now succeeds silently; its three obsolete
rejection diagnostics are unchanged pending the exact
[expectation migration](mos-expectation-migration.md). A checked, unapplied
patch preserves all 75 source fixtures and changes only acceptance and three
diagnostics, plus the generator's acceptance set. It requires separate human
approval and a commit without `src/`. **CI is not green** until that review is
resolved. No automatic-event instruction-safepoint approval is inferred.

The [focused ordinary-path timing](../measurements/2026-10-26/source-mos-speed.json)
compares release `faadf4b7` and `e7e2ec59` after all CI work, with five alternating
200,000-iteration pairs and small-run startup subtraction. Frame access is
0.385017→0.372855 s (−3.2%); event bounce is 0.077907→0.078514 s (+0.8%). All
counters match exactly. Median peak RSS is 9.203125→9.265625 MiB and
9.203125→9.1875 MiB, respectively. The machine was loaded; these are observations,
not a stable speedup/regression proof or seven nightly baselines. Neither design
uses source MOS, so new strength-network/source-MOS traffic remains unmeasured.
No budget or warning threshold changed. Earlier timing records are separate runs.

## Subsequent passive-kernel checkpoint (2026-10-26)

[Passive graph execution](passive-network-execution.md) now adds exact worlds
and scalar conduction timers to the shared origin bank. This note records the
earlier source-MOS checkpoint; passive SIR/source production, port identity,
registered-net force and full formatting remain required.
