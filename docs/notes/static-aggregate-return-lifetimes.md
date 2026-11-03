# Aggregate function-return variable lifetimes

[日本語](static-aggregate-return-lifetimes.ja.md) · 2026-11-03.

The documented P1 correctness gap in static unpacked-array and structure return
values is fixed. IEEE 1800-2023 6.21 and 13.4.1, already reflected in SIR_SPEC
3.3/8 and the approved scalar-return migration, give the named return variable
the function's lifetime. A static return variable keeps its assigned value
between calls; an automatic one starts anew for each invocation.

## Implementation

`Lowerer::subroutineOf` now uses the existing `Storage` descriptor for the named
aggregate return variable: ordinary declaration slots for static storage and
frame variables for automatic storage. `lowerReturn` uses the common aggregate
assignment/copy path. The hidden `$return` output transfers a value to the caller
separately from the persistent declaration. Structure-member initializers use
the existing static initialization path once, or the automatic entry path on
each call. No new SIR instruction, runtime API or dependency is required.

Package/unit declarations with static return-member defaults reserve their
static-init process before instance initialization (SIR_SPEC 7.1). The same
default values are therefore available when a static declaration initializer
calls the function. Function bodies retain their existing lazy lowering.

This applies to fixed arrays, nested structures and arrays of structures,
including wide four-state values, two-state values, real and string leaves.
Returned values remain independent copies. Module-instance storage is separate;
package and compilation-unit storage follows the existing declaration owner.
Event-containing structure returns keep the same positioned unsupported error.

## Evidence and reproduction

The [source-bound record](../measurements/2026-11-03/static-aggregate-return-lifetimes.json)
retains the failed baseline: **960 static checks fail and 480 automatic controls
pass** on the preserved old binary. The repaired binary passes all **1,440 checks
from 360 original sources**: six declaration scopes, five aggregate shapes,
three lifetime declarations, two return forms, two call-entry points and four
initialization policies. Calls run in initial procedures and static declarations.
The checks cover partial writes, copied values, member defaults, recursion and
independent module instances. The earlier sixteen lifetime probes also pass.

Three compiled controls restore the old frame allocation, repeat static member
initializers, or delay package initialization until after its caller. They fail
the semantic assertions, excluding catalogue self-checks; restoring the source
hashes restores the complete passing matrix.
The full dev suite passes **11,829/11,829**, including all existing SIR snapshots
and PerfCount expectations. Existing test inputs and oracles are unchanged.

```sh
python3 tests/cli/run_aggregate_return_lifetime_tests.py \
  --kirimasim build/dev/src/driver/kirimasim
```

This closes the recorded lifetime defect, not the overall P1 coverage criterion.
The external populations and finite Icarus-probe records were not remeasured.
The pinned slang remains unmodified, and upstream posting remains on hold.
