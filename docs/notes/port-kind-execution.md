# Effective net kinds for static partial ports

[日本語](port-kind-execution.ja.md)

IEEE 1800-2023 23.3.3.7/Table 23-1 and the approved 2026-10-25
[MOS/port contract 4](mos-switch-proposal.md#4-partial-net-ports-and-observability)
assign effective kinds to eligible collapsed bits before creating implicit roots.
`mergeNetKinds` now supplies the same table to whole-port merging and
`Lowerer::connectPartialNetPorts`. The latter groups connected physical bits,
merges their kinds in port traversal order and records one implicit-root owner
per identity component. `finishStrengthNets` publishes those per-bit kinds,
initial states and roots. Physical explicit drivers remain independent origins.

This extends matching-width/signedness/state, four-state input/output net
connections to wire/tri, wand/triand, wor/trior, tri0/tri1 and supply0/supply1.
Static packed bits/parts, concatenations and fixed-array elements share the
mapping; array alias views retain source bounds before physical normalization.
Implicit-only components are included. A connected bit can acquire a pull,
supply or wired kind while neighbouring bits/elements keep their own kinds.
Multiple ports sharing a bit do not duplicate its implicit root. Independent
nets joined by tran retain independent roots: a tran edge is not identity
collapse. Supply rank 7 crosses identity unchanged and attenuates only through
the appropriate switch operation. Newly supported unwritten bits start at their
effective default; written bits retain initial X before scheduled resolution.
Existing plain wire/tri startup behavior remains unchanged.

No SIR/ABI version, kernel public header, VM rule or dependency changes.
Explicit partial inout, uwire root-count legality through these ports,
width/signedness/state conversions requiring strength transport,
force-observable connections and moving/invalid structural indices retain
their existing boundaries. Full `%v` formatting is still pending.

The new generator creates 238 original sources: all 100 ordered kind pairs,
input/output, module/generate/instance, widths 1/65/129, ascending/descending
packed bounds, parts/concatenations/array elements, all four values, unequal
strengths, withdrawals, implicit-only nets and mixed-kind array bits. Its
independent effective-kind rule uses the existing independent strength-point
oracle; every formal and every untouched physical neighbour is checked.
Eight unit tests inspect SIR kinds, origin ownership/ranks, physical alias
ordinals, detached-source execution, startup, repeated connections, independent
tran roots and switch attenuation.

The [six-input expectation migration](port-kind-expectation-migration.md) was
approved on 2026-11-01 and applied separately from implementation in `3987246b`.
Existing source bytes, other expectations and performance budgets remain protected. Slang posting stays on hold. No source
of Icarus Verilog, Verilator or any other GPL/LGPL project was read.

Finite probes alone do not establish P1 completion.

## Validation (2026-11-01)

Implementation `afebe49f` passes the 238-source matrix and eight new units.
All eight negative controls compiled and were caught, each preserving 655
related existing checks (86 units and 569 sources). The controls omit/reverse
kind merging, replace per-bit kinds with storage kinds, duplicate implicit
roots, choose the wrong implicit point, omit implicit-only activation, omit
effective initialization, or count erased carriers as real initial writers.
Sources/builds are restored after the run; all 1,067 mutation anchors apply.
Targeted clang-tidy, format, banned-feature/SPDX and expectation-separation
checks pass.

The [145-probe rerun](../measurements/2026-11-01/icarus13-port-effective-kinds.json)
uses actual implementation `afebe49f` against Icarus 13.0 (v13_0). Every source
hash and oracle byte matches the preceding array-element record; all reference
statuses remain unchanged. Twelve newly supported native partial-port cases
pass, including both reference-passing supply-inner gaps. Remaining gaps are
`specify_path`, `dense_udp_coverage` (frontend subprocess deadline) and the
Icarus extension `real_net`. Four older and eleven optional-collapse reference
mismatches stay separate. Saved diagnostics omit installation paths and shell
process IDs. No 14.0/population/sv-tests/nightly-fuzz rerun is claimed.

The initial dev run excluding `cli/options` passed 10,450 of 10,451 tests;
the sole failure was the derived-count check before bootstrapping the new
totals. The separate CLI run reported the six historical rejections in the
then-pending proposal; the count check passed after bootstrapping. The
maintainer subsequently approved the six-input migration and main commit/push
on 2026-11-01; `3987246b` applies only that migration.
Final JUnit-derived counts are committed separately, followed by the frozen
all-21 local-CI run before integration and push.

The concrete patch is prepared locally as `build/port-kind-expectations.patch`.
Applying it only to a disposable review copy passes the full CLI runner and
the unsupported-matrix generator check, including exact warnings/notes.
The repository now applies that approved patch, preserving all six input sources.
The measured local implementation `afebe49f` was rebased as `67a93af0` onto
upstream `d0865eb6`; port source/tests are identical, and the upstream SIR text
change preserves emitted output. The comparison record retains the actual
measured local version rather than relabeling an old run.

## Approved integration follow-up (2026-11-01)

With the six-input migration applied, the full dev JUnit run passes all
10,452 tests, with no failures or skips. Counts are derived from that result
and recorded separately. Final pre-push validation runs all four presets and
the static/policy checks through `tools/local_ci.py` on a clean, fixed head;
its outcome is included in the push report. The Icarus comparison and the
compiled controls above retain their actual measurement versions and scope.
