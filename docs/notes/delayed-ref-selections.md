# Delayed NBAs to selected reference destinations

[日本語](delayed-ref-selections.ja.md). 2026-10-30.

The [scalar-ref delayed NBA](delayed-ref-nba.md) path now extends to fixed-array
reference elements, whole fixed arrays through refs, and packed bit/part
selections through scalar or array refs. This closes those remaining N01
delayed-reference forms using the existing static-target, capture and clipping
rules of [SIR_SPEC 5.3 and 8](../spec/SIR_SPEC.md) and
[SCHEDULER_SPEC 5 and 13](../spec/SCHEDULER_SPEC.md).

```systemverilog
`timescale 1ns/1ns
module top;
  logic [15:0] values[2];
  task automatic issue(ref static logic [15:0] r[2]);
    int index=1, position=8;
    r[index][position+:8] <= #2 8'h5a;
    index=0; position=0;
  endtask
  initial begin
    values='{default:'0};
    issue(values);
    #3; $display("%h %h", values[0], values[1]);
  end
endmodule
```

This prints `0000 5a00`. The reference retains the actual subarray base from
the call, and issue-time indices use the formal array bounds. Forwarding,
return and issuer disable do not redirect or discard the update. RHS values
are captured before delay and target-selector side effects. Pending updates
use the existing kernel's deadline, issue order, NBA/Re-NBA group, notification
and collection roots; they are not children observed by `wait fork`.

Diagnostic SIR format 0 adds `nba.ref.elem.delay` and `nba.ref.bits.delay` at
the end of the opcode enum, preserving existing numbers. Their first operand
is the ref frame variable, whose descriptor retains element type and formal
dimensions. The packed form also carries the existing static parent range.
The verifier checks arity, indices, delay, value domain/width and parent bounds.
The VM checks the concrete static destination, remaining array extent and
compatible element type before adding the selected offset. Automatic,
heap-cell and readonly destinations remain errors.

Packed selections clip inside their parent before adding its storage offset.
Negative positions skip the corresponding low RHS bits. X/Z, unrepresentable
and wholly invalid selections write nothing; partial selections retain only
their valid bits. Other bits keep their current values when the NBA applies.
Bounds warnings use the parent and report once per source place. Whole
queue/dynamic-array elements copy into the actual target's descriptor and
remain collection roots while pending. No kernel API, dependency or value rule
is added; the SIR v1 grammar freeze remains separate.

## Validation and remaining boundaries

The new [CLI matrix](../../tests/cli/run_delayed_ref_selection_tests.py) runs
402 checks on 67 original sources: parse, SIR and four initial-value modes.
It covers 1/32/65/129-bit two/four-state array elements, ascending/descending
and multidimensional arrays, differing formal bounds and subarray forwarding,
1/8/65/129-bit packed parents, positive/negative/partially valid and X/Z/wide
positions, both indexed-part directions, packed structures, whole-array
capture, containers, real/shortreal/string/null chandle, side effects,
issuer disable and Reactive-origin Re-NBA.

[Eight unit tests](../../tests/unit/delayed_ref_selection_test.cpp) run after
source/AST destruction. They include 91 independent `valsem` comparisons,
returned subarray refs, current-neighbour preservation, independent Contexts,
container retention through 4,000 allocation-producing calls, malformed SIR,
native lifetime/type/extent checks, unknown/oversized delays, parent-aware
warning deduplication and Postponed write rejection. Completion releases
activation leases and pending payloads.

Nine compiled faults test lost delay, actual base, formal index, parent offset
and low-RHS skip; omitted actual-extent and parent-range checks; Postponed
writes; and missing frontend parent metadata. Restored sources pass the
focused checks. Existing output/SIR expectations and PerfCount budgets are
preserved.

Runtime-position packed parents and constant parents outside storage retain
their previous frontend boundary. String-character delayed targets (N04),
broader event-value forms and the existing undelayed ref-array/packed-NBA
boundary are unchanged. This does not enable NBA to automatic or dynamic
container-cell storage. Pinned slang source restrictions remain in force.
The historical N01 audit at `51cda314` is retained; complete P1 and external
simulator parity remain unconfirmed and require their separate acceptance checks.
