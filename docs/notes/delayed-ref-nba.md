# Delayed NBAs through scalar references

[日本語](delayed-ref-nba.ja.md). 2026-10-30.

A delayed nonblocking assignment to a whole value through a scalar reference
now captures its static destination and RHS before the issuing call returns.
This implements the scalar form of combination gap N01, using the existing
static-target rules in [SIR_SPEC 5.3 and 8](../spec/SIR_SPEC.md) and delayed-NBA
ownership and timing in [SCHEDULER_SPEC 5 and 13](../spec/SCHEDULER_SPEC.md).

```systemverilog
`timescale 1ns/1ns
module top;
  int value;
  task automatic issue(ref static int r);
    r <= #2 7;
  endtask
  initial begin
    issue(value);
    #3; $display("%0d", value);
  end
endmodule
```

This prints `7`. The issuing task does not wait, and the scheduled update is
not a child observed by `wait fork`. Returning or disabling the issuer does
not discard it. A scalar reference can name an eligible selected element of
a static fixed array, and can be forwarded through another reference argument.
The selected actual is bound at the call; later selector changes do not redirect it.

The frontend snapshots a container RHS before evaluating the delay expression.
The VM resolves the static slot and element once, checks its value type and
copies a container into the actual destination's descriptor. The shared kernel
then owns the pending value, deadline and NBA/Re-NBA origin. Its existing
notification, collection roots, issue order and overflow handling apply.
Zero and X/Z delays use the existing zero-delay rule. A known delay above the
representable time range reports a positioned design error.

Diagnostic SIR format 0 adds `nba.ref.delay ref, [leaf,] value, delay`.
The optional leaf is a nonnegative constant within 32 bits. The verifier checks
arity, reference/value/delay types and typed container access. The opcode is
appended to preserve existing opcode numbers; this is not the separate v1
grammar freeze. No kernel API, dependency or value-semantics rule is added.

## Validation and remaining boundaries

The new [CLI matrix](../../tests/cli/run_delayed_ref_nba_tests.py) runs 180 checks
on 30 original sources: parse, SIR generation and four initial-value modes.
It covers 1/32/65/129-bit two/four-state values, a 137-bit packed structure,
real/shortreal, string, null chandle and whole queue/dynamic-array values.
Other cases cover selected static array actuals, function/interface/program/
generate contexts, forwarded refs, returned retained refs, issuer disable,
same-deadline order, Reactive-origin Re-NBA and RHS/delay side effects.

[Seven unit tests](../../tests/unit/delayed_ref_nba_test.cpp) run after the source
and AST are destroyed. They check returned wide actuals, independent Contexts,
container snapshots through 4,000 allocation-producing calls, malformed SIR,
unknown/oversized delays, actual-type and leaf bounds, and rejection of writes
from Postponed callbacks. Native injected instructions also check automatic,
heap-cell and readonly actual rejection. Completion releases activation leases
and pending NBA storage.

Seven compiled faults check lost delays, wrong actual elements, omitted actual
lifetime/type checks, Postponed writes, missing RHS snapshots and unchecked
delay types. Restored sources pass the focused tests. Existing expectations,
SIR dumps and PerfCount budgets are preserved.

Direct delayed writes to elements of a ref-array formal or to packed bit/part
selections through refs remain unsupported, as do string-character delayed
targets. Automatic/heap-cell/readonly destinations remain errors; this does
not enable event-value assignment. The pinned slang source restrictions on
NBA automatic-variable references and `ref static` actual forms are preserved.
The historical N01 measurement at `51cda314` is retained. Full P1 completion
and external-simulator parity remain unconfirmed and require their separate
acceptance checks.
