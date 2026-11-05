# Next-time VPI callbacks: ignored payloads and read-only registration

[日本語](vpi-next-time-callback.ja.md) · 2026-11-05.

A next-time callback observes the next time slot the simulation actually reaches;
the [approved VPI contract 4](vpi-p1-proposal.md#4-writes-and-callbacks) distinguishes
this observation from requesting a delay.

Implementation `fc2650be` corrects `cbNextSimTime` registration under that existing
contract and SCHEDULER_SPEC 7/17. It adds no callback reason, API, dependency or
semantic amendment. This is a branch implementation record, not a main publication
or a claim that P1 is complete.

## Rule and correction

The time descriptor must select `vpiSimTime` or `vpiScaledRealTime`. Its numeric
payload is ignored: zero or maximum integer ticks and finite, negative, infinite
or NaN real fields do not request a delay. The descriptor selects the time format
reported when the callback runs. Only the selected format's fields are meaningful.
Missing, suppressed and unknown time formats retain their existing API errors.

The old shared branch incorrectly converted this payload as a delay, so negative
and nonfinite real values could end execution with a false time-overflow error.
It also rejected a zero-valued next-time registration from Postponed as though
it requested same-slot work. The callback actually waits for a later time slot,
so read-only callbacks may register it without introducing a same-slot event.

`Vpi::Impl::timeFormat` now owns descriptor validation. Ordinary delayed callbacks
and writes additionally use `Vpi::Impl::delay`, retaining numeric validation,
rounding and overflow behavior. `vpi_register_cb` uses only format validation for
next-time observers. Task calls, timer callbacks and read-only callbacks all use
this one entry point. Delivery remains one-shot in Pre-Active of the next actual
slot; the observer creates no future time slot and does not prevent quiescence.
Registration descriptors are copied and cancellation remains effective.

## Independent verification

The [comparison](../measurements/2026-11-05/vpi-next-time-comparison.json) uses
144 original sources: six source contexts × three registration phases × eight
payloads. The contexts are a module, child module, generate block, interface,
program and static task; phases are a task call, read-only callback and timer
callback. Each written oracle requires exactly one callback at actual tick 3,
correct time reporting and no API error.
The plugin uses a null callback object and `1ns/1ns`; object-scoped scaling is
checked separately in the Context tests.

An immutable release executable at `92001039` passes **60/144**; the implementation
passes **144/144**. Explicitly selected Icarus compiler and runtime **13.0 stable
(`v13_0`)** also pass **144/144**, using the same original C plugin and kirimasim's
independently written header. Source hashes and expected outputs are identical
before and after. A successful process exit alone is not counted as a match.
These cases are separate from the unchanged 188-probe inventory and fixed P1
populations; none is excluded or reclassified as a pass without matching its oracle.

Five new kernel unit tests using Context cover ignored values, pre-runtime/runtime
registration, copied descriptors, scope-scaled time, malformed formats, ordinary timer validation
and rounding, read-only registration, cancellation and quiescence. All **61 related
CTests** pass in dev, release, ASan and UBSan. Unfiltered dev passes **12,251/12,251**,
with no failures or skips. The first full run found only the new generator's count
sync; counts were updated separately and the complete dev run was repeated.
The [validation metadata](../measurements/2026-11-05/vpi-next-time-validation.json)
records the exact scope, source hashes and configuration results.

All [three compiled controls](../measurements/2026-11-05/vpi-next-time-controls.json)
are caught: converting the ignored payload, rejecting read-only registration and
moving delivery one tick past the actual next slot. Each builds, fails semantic
tests and restores its source SHA; the restored six-test selection passes again.
The registry's 1,206 applicable anchors are a separate check, not 1,206 compiled
controls. Clang-tidy reports no diagnostics in four affected translation units.

No existing test body, expected output, assertion or performance budget changes.
Full local CI remains required before an authorized push. Selected/delayed net
writes, implicit container sensitivity and the other [P1 boundaries](../P1_SUPPORT.md)
remain work; upstream slang posting remains on hold.

## Reproduce

```sh
ctest --preset dev -R '^(VpiNextTime[.]|Vpi[.]|VpiNet[.]|VpiFront[.]|cli/vpi)' --output-on-failure
python3 tests/cli/run_vpi_next_time_tests.py --kirimasim build/dev/src/driver/kirimasim --report build/vpi-next-time.json
python3 tools/gen_vpi_next_time_matrix.py --output build/vpi-next-time-sources
python3 tools/mutants.py --check
```

For the reference comparison, add `--iverilog=<compiler-path>` and
`--vvp=<runtime-path>` to the runner and verify both version outputs. Neither
simulator's source nor an external issue's reproducer was read or copied.
