# Blocking aggregate intra-assignment timing

[日本語](blocking-aggregate-timing.ja.md). Implemented 2026-10-24 in `6195846`;
tested with the separate expectation baseline `36aa2ae` in `396131e`.

The event-NBA restrictions below describe this checkpoint. Current support
and remaining boundaries are recorded in
[source event-NBA lowering](event-nba-source-execution.md).

## Contract and implementation

Whole fixed arrays, unpacked structures and arrays of structures now support
blocking intra-assignment delay, event and repeated-event controls. This
implements SCHEDULER_SPEC 8, VALUE_SEMANTICS 3.1/3.13 and IEEE 9.4.5; no new
semantic choice, SIR opcode, kernel dependency or VM path is introduced.

The front captures every RHS leaf into private typed frame storage before
evaluating the control. Container leaves are independent snapshots, including
before later pattern expressions modify their source. These frames remain
typed GC roots across waits. After the control, the actual LHS selectors are
evaluated once and the saved leaves are copied in declaration order. SSA
temporaries do not cross a wait. Concurrent automatic calls have separate
activation storage; eligible whole-array refs use the same path.

Assignment-compatible arrays of structures also now map each side by its own
declared bounds. The prior copy reused the destination index on the source,
producing default values for different ranges. The correction covers ordinary
assignments, immediate/delayed NBAs and the new blocking timing path, including
nested arrays and runtime prefixes (7.6).

Event-controlled NBAs remain unsupported and need the separate
[ownership and ordering proposal](event-nba-proposal.md). Blocking waits cannot
replace that independently pending control. Explicit automatic structures with
fixed-array members and automatic arrays of structures retain their declaration
restriction. Streaming targets and unsupported array slices are not enabled.

```systemverilog
`timescale 1ns/1ns
module top;
  int a[2], b[2];
  initial begin
    b = '{7, 9};
    a = #2 b;
    $display("%0t %0d %0d", $time, a[0], a[1]);
  end
  initial begin #1; b = '{40, 41}; end
endmodule
```

Output: `2 7 9`.

## Original source evidence and expectation review

[`gen_blocking_aggregate_timing_matrix.py`](../../tools/gen_blocking_aggregate_timing_matrix.py)
generates **74 original sources** and their stdout/stderr expectations:

- 64 cases: initial, static/automatic task, fork, interface, package, program
  and generate; widths 1/32/65/129, two/four-state payloads. Each checks `#d`,
  `#0`, edge events, mixed named/signal repeats, zero counts, runtime targets,
  array members, real/string leaves and invalid targets.
- Ten mechanism cases: concurrent automatic ref calls, nested differing bounds,
  structure arrays, nested structure arrays, ordinary/NBA bound mapping,
  container roots and pattern effects, mixed `iff`, narrow/wide nonpositive
  counts, a pending 129-bit positive count and unknown target indices.

The retained pre-change binary reports revision `8616ee56b510`: 72 new sources
are unsupported and two report incorrect values. All 74 pass with the new
implementation. All 29 existing SIR dump expectations still pass.

The existing CLI input `unsupported_matrix/intra_delay_array.sv` contains six
legal delayed assignments and previously expected unsupported diagnostics.
Under 9.4.5 and the already approved scheduler contract, execution is correct
and those historical rejection expectations are obsolete. **The expectation
change is isolated in `36aa2ae`, without `src/` changes**, for a separate PR.
It preserves the source byte for byte, moves it to the existing accepted-input
manifest and requires exit zero with empty stdout and diagnostics. The following
implementation PR must use that review branch as its base; it must not include
the old expectation changes. Its new value-checking sources prevent acceptance
alone from counting as execution correctness. Until implementation is present,
the expectation-only branch is expected to reject that historical input.

## Verification

Dev testing on the combined review stack passes 3,053 of 3,055 tests, with the
same two known skips. Five deliberate mutations compile and are caught by
actual new stdout expectations: capturing after the control, selecting the
target before it, rereading the RHS on resume, losing the first resumed leaf
and using destination bounds on the source. Anchor/judging self-checks are
excluded from the judging run; all five also fail the focused new source set.
The sources are restored and rebuilt afterwards. Full configuration results
are recorded in the [current progress record](p1-icarus-progress.md).
Three new feature probes exercise whole-array delay, repeated-event capture
and structure-array bound mapping. The versioned results distinguish reference
compile failures from Icarus-passing gaps; these additions do not establish
complete P1 coverage. Historical performance results are not remeasured here.

```sh
python3 tools/gen_blocking_aggregate_timing_matrix.py --check
ctest --preset dev -R blocking_aggregate_timing --output-on-failure
python3 tools/mutants.py --only 'blocking aggregate timing:'
python3 tools/mutants.py --only 'structure array copy:'
```
