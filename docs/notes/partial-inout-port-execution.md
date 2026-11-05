# Static partial inout net ports

[日本語](partial-inout-port-execution.ja.md)

Sharing selected net identity differs from adding two opposite value assignments; the
[partial-inout FAQ](../FAQ.md#does-a-partial-inout-port-create-two-value-drivers)
explains how this avoids duplicate drivers.

**Approved integration update (2026-11-01):** both old acceptance expectations are now migrated in their own commit. The [combined integration record](p1-approved-class-inout-integration.md) supersedes the pre-approval validation status below.

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

## Pre-approval validation checkpoint (2026-11-01)

The 221-source matrix and eight units pass. Five compiled controls are caught:
omitted inout admission, activation still requiring a carrier overlap, omitted
identity publication, tran attenuation substituted for identity, and accepted
signedness conversion. Every control preserves 1,232 related checks (101 units
and 1,131 sources); the historical unit proposed for migration is excluded from
that legacy group. Sources/builds are restored, all 1,077 mutation anchors apply,
and targeted clang-tidy/format/expectation-separation checks pass.

The dev run excluding only `cli/options` and the historical partial-inout
rejection unit passes all 11,013 selected tests, with zero failures/skips.
The concrete migration patch was applied only to a disposable review copy:
the full CLI runner and generator pass, and the proposed stronger positive
unit was separately compiled against the actual libraries and passed. The
repository's existing expectation files remain unchanged pending approval.
Final JUnit-derived counts and the frozen all-21 local-CI run follow approval.
The published count record retains its earlier scope until then.

The [171-probe baseline](../measurements/2026-11-01/icarus13-partial-inout-baseline.json)
uses native `b56fae24`; the [implementation run](../measurements/2026-11-01/icarus13-partial-inout-ports.json)
uses actual native `85535e72`, both against installed Icarus 13.0 (v13_0).
All sources/oracle bytes and reference statuses are unchanged, including the
previous 156 native statuses. All fifteen new native forms pass. Eleven newly
measured passing-reference gaps close, reducing fourteen baseline gaps to the
same three unresolved cases: specify paths, dense UDP's frontend deadline and
the Icarus real-net extension. Four new wired readout differences remain
separate from the previous twenty-one reference mismatches. Their reference
cause is **unconfirmed**; further original whole-net/bit/part/concat controls
varying only kind and mapping can isolate it without reading simulator source.
No oracle is changed to adopt those readouts. Saved diagnostics omit only
installation paths and shell process IDs. No 14.0/population/sv-tests/nightly
fuzz rerun, main integration or push is claimed. Slang posting remains on hold.
