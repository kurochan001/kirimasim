# Event-NBA destinations through fixed array references

[日本語](event-nba-ref-arrays.ja.md). 2026-10-30.

Event-controlled nonblocking assignments now accept fixed-array elements through
eligible `ref static` arguments. This implements combination gap N02 under the
approved [event-NBA contract](event-nba-proposal.md). It reuses the existing
kernel payload, countdown, notification and issue-order machinery; it does not
add a kernel API, instruction or dependency.

The RHS, repeat count and destination selectors retain the approved evaluation
order and are evaluated once. An immutable SIR reference target now optionally
contains the formal array dimensions and captured index ordinals. Ordinary
scalar reference targets retain their previous metadata and text output. Array
targets print `shape=[left:right] ... indices=($n, ...)`; the SIR format remains
the pre-v1 diagnostic representation, not a newly frozen parser grammar.

At registration, the VM resolves the captured reference to its writable static
variable and base element, validates that the declared shape fits its remaining
storage, and selects the element in the formal's declared dimension order.
The resulting concrete destination and owned value enter the existing kernel
payload. Forwarded subarray references retain their original base offsets even
when equivalent formal arrays use different bounds.

```systemverilog
module top;
  int values[3:1];
  event go;
  task automatic issue(ref static int r[3:1]);
    int index=2;
    r[index] <= @(go) 7;
    index=1;
  endtask
  initial begin
    issue(values);
    ->go;
    #1;
    $display("%0d", values[2]);
  end
endmodule
```

This prints `7`. The pending update retains neither the task's mutable index nor
its formal-argument frame. Whole-array event-NBAs share one control/countdown
across all element writes. Packed bit/part selections within an array element
retain their captured positions. Fixed arrays of queue values use existing
typed snapshots and pending-payload roots through collection and issuer return.
`wait fork` does not wait for the registration, and issuer disable does not
cancel an already issued control.

An out-of-range or X/Z destination index contributes no write, while the event
control, countdown and guard evaluations still occur. The existing bounds-check
option reports the invalid selection. Frontend restrictions on guards that
write variables remain in force; a native SIR guard test independently observes
evaluations for controls with no effective destination.

## Validation and limits

The new [CLI matrix](../../tests/cli/run_event_nba_ref_array_tests.py) performs
222 checks on 37 original sources. It covers 1/32/65/129-bit two/four-state
values, ascending/descending and negative multidimensional bounds, module,
interface, program, generate and function contexts, returned/retained refs,
whole-array snapshots, packed selections, invalid indices, repeat-zero order,
disable, notification and repeated registration. Four initial-value modes use
the same sources.

[Six unit tests](../../tests/unit/event_nba_ref_array_test.cpp) execute after
source/AST destruction and inspect immutable descriptors. They check independent
Contexts, forwarded base offsets, shared countdowns, queue payload roots across
4,000 intervening declarations, native guard counts, malformed ranks/types/
ordinals/overflow/leaf offsets and runtime static-storage bounds. Completion
releases pending payloads, controls and activation leases.

Six compiled controls expose ignored captured indices, lost reference bases,
writes for invalid indices, missing actual-shape checks, omitted controls with
empty payloads and unchecked selector rank. Restored sources pass focused
checks; existing expectations, SIR dumps and PerfCount budgets are unchanged.

Statically bounded [packed-parent windows through refs](event-nba-ref-windows.md)
are now connected (N03). Delayed NBAs through refs, automatic NBA targets,
container-cell NBA targets and broader event-value forms
retain their existing boundaries. The N02 audit at `51cda314` is historical;
this change does not establish complete P1 or external-simulator parity.
