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

Validation and the frozen 145-probe comparison are recorded after execution;
finite probes alone do not establish P1 completion.
