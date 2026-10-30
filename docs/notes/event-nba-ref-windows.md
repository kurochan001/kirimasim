# Bounded packed parents in event-NBA reference destinations

[日本語](event-nba-ref-windows.ja.md). 2026-10-30.

Event-controlled nonblocking assignments now accept runtime child selections
inside a statically selected packed parent through an eligible reference. This
implements combination gap N03 under [SIR_SPEC 5.3](../spec/SIR_SPEC.md) and the
approved [event-NBA contract](event-nba-proposal.md). It connects the existing
packed-parent clipping semantics to reference payload capture; it adds no kernel
API, value rule, instruction or dependency.

The reference bit-target descriptor optionally retains the parent's absolute
least-significant-bit offset and width. A zero offset/width keeps the existing
whole-element form and text. A bounded target prints `parent=offset+width`.
The verifier rejects a nonzero offset with zero width, out-of-storage ranges
and overflowing sums. This remains diagnostic SIR format 0, before the separate
v1 grammar freeze.

At registration, the VM clips the captured child position and value inside the
parent before adding its storage offset, using the shared kernel helpers.
Negative positions skip the corresponding low RHS bits. Positions above the
parent, X/Z and unrepresentable positions contribute no write. A control with
no effective destination still owns its countdown and evaluates its guards.
Bounds warnings use the parent range and retain once-per-place reporting.

```systemverilog
module top;
  logic [1:0][7:0] value='0;
  event go;
  task automatic issue(ref static logic [1:0][7:0] r);
    int index=3;
    r[1][index] <= @(go) 1'b1;
    index=0;
  endtask
  initial begin
    issue(value);
    ->go;
    #1;
    $display("%h", value);
  end
endmodule
```

This prints `0800`. A child outside `r[1]` cannot modify `r[0]`. The captured
payload owns only the valid destination bits, so neighbouring bits changed
after issue retain their then-current values when the NBA applies. Issuer
return, shared repeat counts, pending issue order and NBA/Re-NBA origin reuse
the existing event-NBA machinery.

Packed structure members and nested packed dimensions use the same parent
metadata. [Fixed array references](event-nba-ref-arrays.md) can combine their
captured unpacked indices and subarray base offsets with this packed parent.

## Validation and remaining boundaries

The new [CLI matrix](../../tests/cli/run_event_nba_ref_window_tests.py) has
234 checks on 39 original sources. It checks two/four-state parents of width
1/8/65/129, positive/negative/partially valid positions, ascending/descending
and negative packed ranges, upward/downward parts, X/Z and wide positions,
structure offsets, nested dimensions, forwarded array refs, shared countdowns
and issue order. Module, interface, program, generate and function contexts
run in four initial-value modes. Source expectations use an independent
bit-by-bit calculation.

[Six unit tests](../../tests/unit/event_nba_ref_window_test.cpp) execute after
source/AST destruction. Ninety-one VM comparisons use `valsem` to change the
selected parent and then insert it into the original root. They include wide
four-state data, cross-word offsets, one-bit parents, partial writes, extreme
positions, X/Z and the legacy whole-element form. Other tests cover current
neighbour preservation, returned array refs, native guard counts for empty
updates, malformed metadata, independent Contexts and parent-aware warning
deduplication. Completion releases payloads, controls and activation leases.

Seven compiled controls expose lost parent offsets, escaped parent widths,
lost low-RHS skips, missing frontend metadata, unchecked parent ranges,
whole-element warning checks and discarded empty controls. Restored sources
pass the focused checks. Existing output, SIR and PerfCount expectations are
preserved.

Runtime-position parents and constant parents outside storage retain their
existing frontend boundary. Strict slang rejects source part widths larger
than their parent; acceptance is not relaxed. [Whole-value delayed scalar-ref NBAs](delayed-ref-nba.md)
are now connected (N01 scalar form). Ref-array/packed-selection delayed writes,
automatic/container-cell NBA targets and broader event-value forms remain work.
The original N03 audit at `51cda314` is historical; this change does not establish
complete P1 or external-simulator parity.
