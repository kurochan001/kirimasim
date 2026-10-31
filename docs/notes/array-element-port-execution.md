# Fixed-array element ports with physical strengths

[日本語](array-element-port-execution.ja.md)

`Lowerer::staticNetBits` now accepts a complete integral element of a fixed
unpacked array. It maps the source declaration's constant indices to one
checked declaration-order ordinal before canonicalizing a whole-array alias.
Packed bit/part selections and concatenations recurse through that same
mapping. This implements IEEE 1800-2023 7.4 and 23.3.3.7 under the approved
2026-10-25 array contract 1–2 and MOS/port contract 4.

The existing partial-port producer connects eligible input/output net bits
through identity edges and retains their physical origins. It imports neither
a resolved readout nor a fabricated strong driver. The existing array resolver,
compact driver ordinals, strength origins, initial states, wait subscriptions
and shared kernel execute the connection. No new SIR instruction, public
header, VM rule, dependency or ABI version is introduced.

Supported connections have the same aggregate width, signedness and state
domain on both sides and plain same-kind four-state wire/tri nets. The formal
is an integral scalar/packed net; its structural actual can select array
leaves, packed portions or concatenate such leaves. Whole-array alias views
retain their own bounds; physical ordinals stay in declaration order, including
opposite bounds and multiple unpacked dimensions. All indices must be known
compile-time integers in range and fit the physical ordinal representation.

This checkpoint retains positioned boundaries for explicit partial inout,
width/signedness/state conversions requiring strength transport, wired and
pull/supply partial-net forms, moving or invalid indices and force-observable
connections. Those approved contracts still have remaining implementation
work. Array-backed clocking sources remain unsupported: the shared mapping
returns an element ordinal, while the existing clocking producer only accepts
scalar packed storage and now checks that boundary explicitly.

The mechanism generator has 208 original sources: module, generate and instance
placements; input/output; ascending/descending packed and unpacked bounds;
multiple unpacked dimensions; whole, bit, part and concatenation mappings;
1/65/129-bit port values; equal/opposite strength and high impedance; alias
bounds; parameter/genvar/hierarchical spellings and packed dimensions. Its
independent per-bit strength oracle checks the formal and every array element,
including untouched bits. Seven unit tests inspect physical ordinals and
origins, detached-source execution, alias bounds, directed variable carriers,
clocking, explicit inout and invalid/moving-index boundaries.

Six mutations were built and caught: omitted mappings, zeroed ordinals, lost
strides, canonical bounds replacing source bounds, zeroed packed positions and
an omitted clocking boundary. Each mutation also passed 79 related existing
units and 361 existing source cases. Sources and builds were restored; all
1,059 mutation anchors apply. The approved historical three-input expectation
migration is isolated in `df1f8912`; no existing source body or performance
budget was changed.

Finite comparisons do not establish P1 completion. The comparison inventory
records optional collapse separately from missing support; different inner
port readouts from an implementation that retains directed carriers are not
silently substituted for the approved identity semantics. Icarus/Verilator
sources were not read. Slang posting remains on hold.

## Executed comparison (2026-11-01)

[Implementation edf36098, 145 probes](../measurements/2026-11-01/icarus13-array-element-ports.json)
was run against installed Icarus Verilog 13.0 (v13_0). The previous 131 source
hashes, oracle bytes and native result statuses are unchanged. Fourteen new
sources and oracles were frozen in the [baseline inventory](../measurements/2026-11-01/icarus13-port-driver-inventory-baseline.json)
before implementation. Native now passes scalar and rank-two array-element
ports. Reference-passing gaps in the new 14 fall from three to two (supply0/1
inside a partial port). Together with the earlier specify/dense-UDP/real-net
cases, the expanded 145 contain five passing-reference gaps. Eleven new
reference mismatches concern optional inner noncollapse; they remain visible
separately from the previous four reference mismatches. Source/oracle bytes
were not adjusted to the measured output. Installation paths and shell process
IDs are removed only from saved diagnostics. Icarus 14.0, population, sv-tests
and seven-night fuzz evidence were not remeasured.

The full dev run passed all 10,205 cases. Follow-up expectation/harness commit `55196d38` preserves the original port-coercion warnings and notes with exact adjacent `.warnings` expectations for the three approved inputs; other accepted inputs remain diagnostic-free and every error remains rejected. Counts are updated in a separate commit after this JUnit result.
