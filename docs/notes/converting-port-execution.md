# Directed value conversion through net ports

[日本語](converting-port-execution.ja.md)

IEEE 1800-2023 4.9.6/23.3.3.7, SIR_SPEC 3.5 and approved MOS/port contract 4
retain directed implicit assignments for converting input/output ports. The
carrier converts the value at ordinary strong strength; it is not a physical
identity connection and does not transport the original strength backwards.

`Lowerer::lowerPorts` now distinguishes that carrier before registering
connections that require physical strength identity. `sameIntegralType` compares
width, signedness and state domain; `hasNetConversion` walks representation
wrappers, selections and concatenations iteratively. Explicit casts remain
value operations even when their final representation matches. Implicit
wrappers preserving the representation remain transparent. Existing lowering,
coercion and ordinary net resolution perform the assignment; no VM special
case, kernel/SIR instruction, public header, dependency or ABI changes.

Supported actuals include whole nets, static packed parts, concatenations and
fixed-array elements. Signed widening, unsigned padding, truncation and casts
that convert X/Z to a two-state value use existing value rules. This does not
permit a two-state *net declaration*: strict slang still rejects that illegal
domain. Equivalent same-type structural ports retain identity and their
physical origins. Explicit partial inout, remaining force/uwire identity
restrictions and invalid/moving identity mappings retain their boundaries.

The generator adds 324 original designs across input/output, widths 1 through
131, whole/part/array/concat actuals, module/generate/instance, ascending packed
bounds, signedness and explicit/nested/two-state casts. Its independent oracle
converts source bits, then resolves a strong carrier against a weak local
contribution. It checks both port readouts and every untouched physical bit or
array element after all four values and withdrawals, including nonuniform data.
Eight units inspect ordinary strong driver slots, directionality, casts,
supply attenuation at a conversion, strength-observed destinations, preserved
identity and strict net/inout boundaries. Source files are removed before the
units execute their SIR.

The [three-input expectation migration](converting-port-expectation-migration.md)
was approved on 2026-11-01 and is committed separately. Sources and the nine
existing warning goldens are unchanged; three new exact warning/note goldens
retain strict diagnostics. Existing suite expectations and performance budgets
are protected. No Icarus/Verilator or GPL/LGPL source was read, and slang
posting remains on hold. Finite probes do not establish P1 completion.

## Validation (2026-11-01)

The full dev JUnit run passes all 10,785 tests, with no failures or skips.
The new 324-source matrix and eight units pass. Five compiled controls are
caught: requiring identity for converting carriers, ignoring width, ignoring
signedness, making explicit casts transparent, and losing nested concat casts.
Each control preserves 901 related checks (94 units and 807 sources). The
sources/build are restored and all 1,072 mutation anchors apply. Targeted
clang-tidy, format and expectation-separation checks pass. Counts are updated
separately from the successful JUnit result; final validation runs all four
presets and static checks with `tools/local_ci.py` on the frozen head.

The [156-probe baseline](../measurements/2026-11-01/icarus13-converting-ports-baseline.json)
uses native `ddf48e2b` and Icarus 13.0 (v13_0). The
[post-implementation run](../measurements/2026-11-01/icarus13-converting-ports.json)
uses actual native `632b7ecf`. Every source/oracle byte and reference status
matches the baseline, and the previous 145 native statuses are unchanged.
All eleven new native forms pass. Five newly measured passing-reference gaps
close: output truncation/widening/signed widening and nested/two-state input
casts. The baseline's eight gaps become three: specify paths, dense UDP's
frontend subprocess deadline and the Icarus real-net extension. Six new
optional-collapse readout differences remain separate from the preceding
fifteen reference mismatches. They do not justify changing the approved
carrier policy. Diagnostics omit installation paths and shell process IDs.
No 14.0, population, sv-tests or nightly fuzz rerun is claimed. No main
integration or push is authorized by the three-input expectation approval.
