# Class operations in comb and latch bodies

[日本語](class-implicit-execution.ja.md) · 2026-11-05.

Implementation `222f7c47` connects supported class operations in `always_comb`
and `always_latch` under the already approved [class contract 7](class-runtime-proposal.md#7-event-dependencies-and-observation)
and [scheduler 6.3](../spec/SCHEDULER_SPEC.md#63-the-implicit-sensitivity-of-always_comb).
Four newly measured Icarus 13.0 gaps now pass. **Two old unit expectations still
await the separate [migration review](class-implicit-expectation-migration.md).**
The current test binary therefore is not reported as a full passing gate.
Main integration, push and slang posting have not been requested for this change.

## Implicit reads and executable bodies

The shared SIR admission check used to reject an entire comb/latch body if it
used an object, even when its actual implicit triggers were ordinary signals.
The frontend also propagated instance-method reads like ordinary-function reads.

`Lowerer::lowerClassReference` now evaluates class-reference prefixes once while
excluding their reads/call closures from implicit sensitivity. Method arguments
are evaluated normally. The read-call graph excludes instance and constructor
bodies, including virtual overrides; ordinary functions and static methods keep
their normal read closure. Write propagation, assertion-action exclusions,
time-zero execution and the existing scheduler are preserved. A comb/latch body
uses its checked triggers, separately from its object operations, in both frontend
and native SIR loading.

```systemverilog
class Box;
  int value = 7;
  function int read_value(int n); return value + n; endfunction
endclass
module top;
  Box object = new;
  int input_value = 1, observed;
  always_comb observed = object.read_value(input_value);
  initial begin
    #1; $display("%0d", observed);
    input_value = 3; #1; $display("%0d", observed);
    object.value = 20; #1; $display("%0d", observed);
  end
endmodule
```

This original example prints `8`, `10`, `10`. Changing only the property or
receiver does not wake this body; a later argument change executes it using the
then-current receiver/value. A property-only body executes once at time zero
with no implicit property trigger. A disabled latch retains its output and
recomputes when an ordinary input enables it. Allocation, virtual dispatch,
receiver rooting and null errors use the existing object/call engine.

This does not implement object-dependent explicit `@`, wait/iff/watch/monitor
callbacks or `always @*`, nor property refs/selected/NBA targets. A class handle
used as an actual implicit trigger remains rejected. Static numeric reads still
work; unsupported object formatting and the other class boundaries remain.
There is no new instruction, runtime API, dependency or initialization option.

## Evidence and its limits

[Validation metadata](../measurements/2026-11-05/class-implicit-validation.json)
binds source hashes, binaries, test selections, JUnit hashes and compiled controls.

- The same **184 probes** run [before](../measurements/2026-11-05/icarus13-class-implicit-before.json)
  and [after](../measurements/2026-11-05/icarus13-class-implicit-after.json).
  The previous 180 source/oracle pairs and native outcomes are unchanged.
  Four new comb/latch cases pass Icarus 13.0 and move from native unsupported
  to the written expected output.
- There are **183 P1-evaluated inputs and one optional `real_net` exclusion**.
  Required gaps fall from five to one; raw gaps from six to two. The 25 reference
  mismatches remain separate. `dense_udp_coverage` still reaches the frontend
  limit and remains an independent failure. No new P1 exception is introduced.
- [The generator](../../tools/gen_class_implicit_matrix.py) supplies **120 sources**:
  five declaration homes, comb/latch, widths 1/33/65/129, property/method/allocation
  bodies, four-state values, property/receiver changes and disabled latches.
  All sources and the generator check pass.
- Six new unit tests cover ordinary argument-function reads versus static-method
  reads, virtual/receiver side effects with allocation pressure, constructor
  reads, preserved unsupported observers, forged class-handle triggers rejected
  before runtime storage, and null-call argument effects/errors. All pass.
- **12,136 selected dev tests** pass with no failures/skips. The only omissions
  are the two preserved rejection expectations and `tools/readme_counts`, whose
  successful-count publication must wait for an unfiltered passing run.
  The unmodified ClassFront expectations yield 20/22 passing; the separately
  compiled proposed migration yields **23/23**.
- **Nine compiled controls** are detected: seven new class/implicit-read controls
  and two existing task/assertion-action read rules whose anchors changed.
  The latch-only control preserves comb admission and is caught by latch tests.
  No compiler failure or registry self-check counts as detection. Source hashes
  are restored and all **350 focused tests** pass again. All 1,200 registry
  anchors apply; this is not a full-catalogue mutation run.
- Targeted clang-tidy passes five changed translation units. Existing sources,
  oracles, budgets, `counts.json` and private untracked drafts remain intact.
  Diagnostics redact local/install paths and reference-process IDs, including
  paths encoded in hex; classification uses original subprocess bytes.

The fixed population, sv-tests, seven-night fuzzing and external performance
were not remeasured. Full P1 parity is unconfirmed. After the explicit two-location
migration approval, apply it separately, run unfiltered dev and publish counts.
Full local CI remains required before a later authorized push.

```sh
cmake --preset dev && cmake --build --preset dev
ctest --preset dev -R '^(ClassFront\.(Implicit|VMRejectsForgedImplicit)|suite/sched/class_implicit_|tools/gen_class_implicit_matrix)'
python3 tools/gen_class_implicit_matrix.py --check
python3 tools/mutants.py --check
```
