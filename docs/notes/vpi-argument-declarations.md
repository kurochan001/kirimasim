# VPI declaration metadata for automatic arguments

[日本語](vpi-argument-declarations.ja.md) · 2026-11-06.

Implemented in `c0b047e3` and included in the [2026-11-06 main integration](main-integration-2026-11-06.md). The measurements below are the earlier feature checkpoint.

A declaration such as `logic signed [-3:61] value` uses source indices, not
normalized storage offsets. Passing this whole automatic variable to a C plugin
now preserves its `-3:61` range. `vpi_handle_by_index(argument, -3)` reads and
immediately writes the correct bit, subject to the existing access permissions.
This implements the already approved declaration-preservation and lifetime rules
in [VPI contract sections 3–6](vpi-p1-proposal.md), under
[VPI_SPEC](../spec/VPI_SPEC.md); it does not introduce a new VPI API or C ABI.

## Metadata and numeric capabilities

Front records an optional SIR `foreign.argument` declaration for a supported
whole integral automatic actual. Bounds, declared scalar/vector status and the
existing integer-object classification are associated with a call-site ID and
argument ordinal. The existing syscall argument tuple and numeric reference
remain unchanged. Static reflection uses the same declaration-shape helper.
SIR verifies bounds/width, flags, integral type, whole offset, variable binding,
unique association and a valid call/argument before VM allocates runtime storage.
Older SIR without these optional descriptors retains its existing behavior.

VM passes the verified declaration to the shared host before `compiletf`.
Runtime values still come from the original evaluation and checked binding;
source expressions are not evaluated again. Scalar `logic` and a packed
`logic [7:7]` remain distinct even though each has one bit. A selected bit is
unsigned, and the already approved nearest-format policy selects scalar for it.

A selected argument's parent is a snapshot of the numeric binding, not a call
descriptor that can switch to the next invocation. Ordinary automatic frames
also carry an owner-checked generation, so a reused frame number cannot revive
an expired view. Scoped references retain their existing resolver checks.
These views grant no ownership or new write permission: const-ref writes and
expired reads/writes fail, simultaneous invocations stay independent, and
transient parents/bits are not exported into global name lookup.

## Scope and verification

The [original source matrix](../measurements/2026-11-06/vpi-argument-declaration-matrix.json)
contains **243 cases**: twenty declaration shapes in twelve contexts plus
activation reuse, overlapping activations and a const-ref control. Shapes cover
scalar/one-bit packed declarations, int/integer, two/four states, signed/unsigned
values, widths 33/65/129/257, ascending/descending negative bounds and
left/middle/right bit access. HDL checks the modified bit and unchanged neighbors.
Contexts include static controls, automatic tasks/functions, nesting, ref
forwarding, named blocks, fork, interface/program/generate/package and class methods.
Identical inputs improve from **20/243 to 243/243** without changing existing
test expectations. Five new frontend/host unit tests cover declaration records,
nested call-site association, malformed descriptors before allocation, legacy
transport and invalid direct host declarations. The matrix is one CLI CTest.

The reference companion uses explicitly selected Icarus compiler/runtime
**13.0 stable (`v13_0`)**. It checks metadata and numeric 0/1 reads/writes in
240 cases, retaining stronger native parent, nearest-format, X/Z and lifetime
checks separately. **90/240** match the companion oracle; **81** were failing
natively before this change and now pass. The other observations remain recorded:
ref ports/package calls are rejected; one-bit vector flags, int object kind and
integer/two-state bit access differ. They are not converted into P1 exclusions
or passing reference observations. No other simulator's source/header was read.

The [validation record](../measurements/2026-11-06/vpi-argument-declaration-validation.json)
contains configuration results, actual executable versions/hashes and eight
compiled faults. Every fault is caught by behavioral tests; the applicability
self-check is not a detector. Sources are restored and clean tests pass again.

Unfiltered dev passes **12,268/12,268** with no failures/skips; release, ASan
and UBSan each pass all **69 related VPI CTests**. All 1,224 mutation anchors
apply. Format checks on 281 files and clang-tidy on seven affected translation
units pass. The first full dev run exposed only the outdated count self-check;
after reconciling it, the successful full rerun supplies the separately published
repository counts. External evidence and its dates remain unchanged.

This correction does not add arbitrary expression/part-select, multidimensional
packed/aggregate argument metadata, automatic global discovery, transient bit
callbacks, new net-write forms or full VPI/cocotb support. Remaining Icarus-required
forms remain P1 work. The 188-probe inventory, fixed external populations and
the sole optional `wire real` exception retain their scope; this matrix is not
a P1 completion percentage. The fresh [188-probe checkpoint](../measurements/2026-11-06/icarus13-vpi-argument-checkpoint.json)
retains 187 P1-evaluated cases, the optional `real_net` case, the one required
`dense_udp_coverage` timeout and 25 separate reference mismatches. Existing
source/oracle pairs are unchanged. P1 is not complete. No upstream report was sent.

```sh
python3 tests/cli/run_vpi_argument_declaration_tests.py --kirimasim build/dev/src/driver/kirimasim
ctest --preset dev -R 'Vpi|cli/vpi' --output-on-failure
python3 tools/mutants.py --check
```

For the companion, supply the explicit `--iverilog` and `--vvp` executables and
inspect the separate `reference_matches` fields in `--report`. Command success
denotes native oracle matches, not complete reference parity. The later combined local-CI attempt, including its host-sleep timeouts, is recorded in the [main integration](main-integration-2026-11-06.md).
