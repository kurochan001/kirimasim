# Shared strength-interval resolution

[日本語](strength-range-foundation.ja.md). Source `30a6ebc`, 2026-10-24.
This implements the already approved [VALUE_SEMANTICS 5.2–5.4](../spec/VALUE_SEMANTICS.md)
interval result in the shared kernel. MOS/switch source execution and `%v`
formatting remain unsupported.

The source-support limits here describe the foundation revision above.
Later [directed MOS](source-mos-execution.md) and
[passive switch execution](source-passive-execution.md) use this foundation;
the [P1 inventory](../P1_SUPPORT.md#strength-resolution-kernel) lists current limits.

## Representation and APIs

The kernel previously computed the full strength hull internally but exposed
only its four-state projection. Distinct ranges such as `[3,6]` and `[6,6]`
both read as 1; losing their endpoints prevents faithful strength propagation.
The new APIs expose the approved result before that information is discarded.

[`kernel/strength.h`](../../src/kernel/include/kirimasim/kernel/strength.h) provides:

- `StrengthRange`: two signed endpoints, with the valid-input contract
  `-7 <= lo <= hi <= 7`. Negative strength points denote 0, positive points
  denote 1, and zero denotes high impedance.
- `resolveStrengths(kind, signals, count)`: resolve arbitrary valid intervals,
  including ambiguous strengths wholly on one side of zero. An empty input
  still includes the net's implicit pull/supply where applicable.
- `resolveStrengthBit(kind, drivers, count, bit)`: retain the complete result
  of masked four-state driver inputs. Strengths are 0–7, bit is 0–63, and
  the side masks retain the L/H distinction.
- `strengthValue(signal)`: project to a four-state bit only when needed;
  L/H read as X, exactly as before. Existing `resolveBit` and `resolve4`
  use the shared interval calculation and retain their value interfaces.

The fast path uses the existing finite point-state set and one shared hull/
wired-tie implementation. It allocates no memory, keeps no global state and
adds no dependency. The public header remains C++20; a standalone caller
compiled with `-std=c++20 -fno-exceptions -fno-rtti` returns `-7 -6 0 0`
(endpoints, then the projected W4 a/b fields) for this input:

```cpp
#include "kirimasim/kernel/strength.h"

namespace k = kirimasim::kernel;
const k::StrengthRange inputs[] = {{-7, -6}, {3, 5}};
const auto signal = k::resolveStrengths(k::NetKind::Wire, inputs, 2);
const auto bit = k::strengthValue(signal);
```

The independent [`valsem::resolveSignals`](../../src/valsem/strength.cpp)
enumerates point choices directly. The existing driver-based reference
delegates to it after mapping driver values and effective side strengths to intervals. The kernel
does not depend on that reference implementation.

## Verification scope

Five added [unit tests](../../tests/unit/strength_test.cpp) check:

- All 120 valid intervals and their four-state projections.
- All 14,400 interval pairs under 11 net kinds: 158,400 comparisons with
  the independent reference, plus empty and single-interval inputs.
- All four input values, eight strengths per side and four side-mask states
  under 11 net kinds: 11,264 masked single-driver cases.
- 512 seeded three-interval inputs in all six permutations (default fuzz
  scale), preserving driver-order independence.
- 256 seeded masked 64-bit words (16,384 bit cases at default scale),
  covering every bit position and zero through three drivers.

All 14 strength tests pass after restoration. Four deliberate mutations
compile and fail actual range assertions: collapsing either same-value
endpoint, replacing each input by its high point, and ignoring later inputs.
The prior nine value-only strength tests pass under all four mutations. In
particular, the two same-value collapses preserve the old four-state result
while corrupting the interval; the new assertions catch that lost information.
Anchor checks and judging-tool self-checks are not used as semantic evidence.

Full local CI at `30a6ebc` passes dev/release/ASan/UBSan and every static
check, including clang-format, clang-tidy and expectation protection.
Dev/ASan/UBSan each have 2,831 tests: 2,829 pass and the two known skips
remain; release has 2,832 tests, 2,830 pass and the same skips. See the
[comparison and validation record](p1-icarus-progress.md) for the unchanged
language gaps and measured revisions.

```sh
ctest --preset dev -R '^Strength\.' --output-on-failure
python3 tools/mutants.py --only 'strength ranges:'
```

## Remaining integration

The current VM `resolve` path still consumes a four-state value. Per-net
interval storage/transport, strength-only propagation, MOS attenuation/control,
bidirectional network resolution and strength-aware formatting remain work.
Their SIR/runtime contracts need review where details are missing (as of
2026-10-24; the MOS/switch and delayed-side contracts were approved on
2026-10-25). This API does not enable any of those source forms or close an
Icarus execution gap. The full P1 target and the existing pending semantic reviews remain intact.
