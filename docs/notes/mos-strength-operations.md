# Full-interval MOS operations and transfer maps

[日本語](mos-strength-operations.ja.md). Status: pure operations implemented and checked,
2026-10-26. Source MOS/switch execution remains work.

## Contract and implementation

The [approved MOS/switch contract](mos-switch-proposal.md) section 2 and its
[normative specification mapping](p1-execution-contracts-2026-10-25.md) are the
grounds, with IEEE 1800-2023 28.7/28.9/28.13/28.14 and VALUE_SEMANTICS 5.
Implementation and tests are original. No external simulator implementation or
tests were read, dependency added, global state introduced, existing oracle
changed or ABI version selected.

`59522c50` adds a point/control-choice reference in `valsem/mos.cpp` and a
nonallocating C++20 kernel interface in `kernel/mos.h`:

- `transferStrengthPoint` preserves the signed point for an identity edge,
  caps supply at strong for a nonresistive device, or uses the approved
  resistive rank map `[0,1,1,2,2,3,5,5]`.
- `transferStrength` returns the complete image hull, retaining both input
  endpoints. All three transfer maps are monotone.
- `mosStrength` implements all six directed MOS kinds. Scalar control bit zero
  selects on/off/uncertain; X and Z controls admit conducting and nonconducting
  choices. CMOS combines its n/p legs before applying one reduction; a definite
  conducting leg suffices. Z data remains Z with every control combination.

The reference enumerates every admitted control choice independently and every
data point, sharing each data point between CMOS's parallel legs. The kernel
uses a three-state conduction summary and endpoint transformation. These
different implementations are cross-checked. Invalid kinds/points/ranges return
an empty optional, including malformed data presented to a disabled device;
they are not silently accepted as valid zero drives. No source callback,
simulation state, allocation, exception or RTTI is involved in these operations.

Scalar controls use only bit zero; unused W4 bits do not change their meaning.
The standalone header and the project's public-header probe compile as C++20,
including with exceptions and RTTI disabled. The existing C function table and
formatVersion remain unchanged.

## Evidence

Ten original `MosStrength` tests include:

- 48 signed rank checks against literal approved identity/nonresistive/resistive
  tables, and 360 transfer comparisons covering all 120 valid intervals.
- 11,520 independent-reference comparisons: six MOS kinds, all valid intervals
  and all 16 control pairs, including the ignored second control on single-leg
  devices.
- Worked n/p polarity, parallel-leg, uncertain-control, Z-data, sign and
  resistive-chain cases. Unknown CMOS controls are independent choices; equal
  X encodings do not create Boolean correlation.
- Positive same-value intervals and downstream opposition, showing why
  projecting `[3,6]` to a known 1 would lose behavior. Strength-only input changes
  can change downstream resolution while retaining the input's projected value.
- Invalid kinds/endpoints/reversed ranges and extreme point arguments; scalar
  control transport with unused word bits set.

All 24 focused MOS/strength tests pass. Nine compiled faults are detected by
actual tests: supply retention, a wrong high-rank resistive map, endpoint
collapse, uncertain controls treated as off, parallel legs treated as series,
double attenuation, malformed disabled input accepted, identity-port attenuation
and wrong reference p-control polarity. Every mutated full build succeeds,
including the C++20 header probe; compilation failures do not count as detections.
Restored production passes all 24 cases. All 668 registered controls apply.

[Compiled-control evidence](../measurements/2026-10-26/mos-strength-controls.json)
keeps the exact source hashes and failed tests for all nine faults.
The [full local CI](../measurements/2026-10-26/mos-strength-validation.json) has
17 successful checks and four failed CTest steps. Dev/ASan/UBSan pass
5,138/5,141 and release 5,139/5,142, with the same single `cli/options`
failure and two known skips per preset. All MOS/strength cases, existing dumps
and PerfCount pass. The failure is the six historical indexed-event unsupported
oracles awaiting [separate approval](indexed-event-expectation-migration.md),
not a new MOS failure. Existing expectations remain unchanged. README counts
are generated from the fresh full-dev JUnit and retain that one failure.

**Later expectation-only integration:** the
[approved indexed-event migration](indexed-event-expectation-migration.md) in
`28f5c972` resolves the unrelated CLI failure recorded above. The
[post-migration local CI](../measurements/2026-10-26/indexed-event-migration-validation.json)
passes all 21 checks, with the MOS implementation unchanged. The failed
pre-migration run remains recorded evidence.

## Required integration

The [typed strength SSA checkpoint](strength-ssa-execution.md), `0ec3cc57`,
now connects make/MOS/projection through the VM with complete packed values,
typed verification, block transfer and nested calls. The subsequent
[net publication checkpoint](strength-store-publication.md) adds Context interval
storage, internal notices and atomic publication. Strength load/drive connections,
physical driver/origin state, device subscriptions, directed-driver scheduling and
inertial state, the independent
world-preserving passive solver, force/release and ports, formatting and complete
source execution remain work. The point map can support a passive solver's
per-origin rank transfer; this API does not implement that solver or its correlation rules.
Directed MOS contributions must remain physical roots rather than undirected
edges. Ordinary net initialization and the separately approved new-device
startup policy need their real scheduling connection.

No new source-level comparison or Icarus gap reduction is claimed at this pure
operation checkpoint. The existing 21 historical Icarus-13-passing gaps remain
required work, not a complete feature inventory. Population, sv-tests, Linux
and speed comparisons were not refreshed. P1 and full Icarus coverage remain
incomplete. The indexed-event CLI migration is now approved and applied;
the separate managed-event safepoint proposal remains unapproved.

## Subsequent physical-origin checkpoint

[Physical origin execution](strength-driver-execution.md), `f93da11b`, now
connects copied independent-node/origin tables, complete-range resolution,
per-scalar inertial generations and coalesced Active work to the component
publisher. Sixteen tests and thirteen compiled faults cover the new kernel path.
This supersedes the corresponding driver/timing items above; those checkpoint
measurements remain historical. SIR load/drive/static rows, source sensitivity,
passive graphs, registered-net force/release, port handling, formatting and source
MOS execution remain work. The bank does not turn published net values into
physical roots or implement the passive solver by directed readout feedback.

## Subsequent SIR network connection

[Declared-network execution](strength-network-execution.md), `cea83470` and
`faadf4b7`, now connects selected metadata loads, scalar immediate/delayed
origin drives, static normalized node/origin declarations and separate strength
sensitivity through the VM. Shared kernel wait cleanup cancels those subscriptions
before frame teardown. Nineteen new tests and sixteen compiled controls validate
the bridge. This supersedes those corresponding remaining items above, while
preserving the measurements at that checkpoint. Source lowering/ordinary-driver
import/control capture, passive graph/solver, registered-net force/release, ports,
formatting and full source execution still remain.
