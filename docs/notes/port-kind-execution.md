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

The [six-input expectation proposal](port-kind-expectation-migration.md) is
separate from implementation. Existing source bytes, other expectations and
performance budgets remain protected. Slang posting stays on hold. No source
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

A dev run excluding `cli/options` passed 10,450 of 10,451 tests; the sole failure
was the derived-count check before bootstrapping the new totals. The separate
CLI run reports the six historical rejection classifications in the pending
proposal; the count check passes after bootstrapping. Approval is still needed
before applying the migration to the repository. Final JUnit-derived counts
and the frozen all-21 local-CI run are deferred until that approval, with no
main integration or push. The existing count record keeps its earlier scope.

The concrete patch is prepared locally as `build/port-kind-expectations.patch`.
Applying it only to a disposable review copy passes the full CLI runner and
the unsupported-matrix generator check, including exact warnings/notes.
Repository expectation files remain unchanged pending approval.
