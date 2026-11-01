# Static partial inout net ports

[日本語](partial-inout-port-execution.ja.md)

IEEE 1800-2023 7.4/23.3.3.7 and approved MOS/port contract 4 retain static
net-net inout bits as identity connections, without attenuation. `lowerPorts`
now records those connections without constructing either directed value
carrier. `connectPartialNetPorts` validates and activates them even when no
driver, overlap or existing strength observer would otherwise activate a
component. Invalid forms keep their positioned declaration diagnostic.

Both directions use the existing physical-bit mapping, Table 23-1 kind merge,
implicit-root ownership, initialization and origin import paths. Whole aliases
retain source bounds; static array elements and packed selections map to
physical ordinals before connection. A port path never becomes a physical
root or a cached resolved-value driver. Supply remains rank 7 across identity;
only an actual switch applies its reduction. No new SIR/ABI version, VM rule,
kernel public header or dependency is introduced.

This checkpoint covers same-width/signedness/four-state static integral bits,
parts and concatenations, including fixed-array elements and compatible
wire/tri, wired, pull and supply kinds. Inout conversions, moving/invalid
selectors, delayed net terminals, force-observable views and uwire boundaries
remain positioned errors or strict slang errors. Existing input/output identity
and strong conversion carriers remain separate and unchanged.

The original generator adds 221 source designs: all ordered kind pairs,
implicit-only roots, mixed-kind components, module/generate/instance, packed
widths 1/65/129, ascending/descending bounds, parts/concatenations, fixed-array
ranks and whole-array alias views. It reuses independent per-bit kind/strength
oracles and checks every untouched bit/element. Eight units inspect rootless
and single-source metadata, absence of inout carriers, alias ordinals, initial
supply, implicit-root deduplication, real passive cycles and rejected mappings.
Detached-source execution uses the existing shared VM/kernel.

The [two-classification migration proposal](partial-inout-expectation-migration.md)
is separate from implementation. The CLI source and embedded unit SV remain
unchanged; other expectations and performance budgets stay protected. No
Icarus/Verilator or GPL/LGPL source was read. Slang posting remains on hold.
Finite comparisons do not establish P1 completion.
