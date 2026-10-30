# Fixed array members in automatic structures

[日本語](automatic-struct-array-members.ja.md). 2026-10-30.

Automatic unpacked structures now support fixed unpacked-array leaves, including
multidimensional and nested-record members. This connects combination gap A03
to the existing dimensioned frame rows and the approved
[automatic-storage contract, sections 2–3](automatic-capture-proposal.md).
It adds no SIR instructions, kernel value semantics, dependencies or VM paths.

The tested element types are integers, wide four-state vectors, reals, strings,
dynamic arrays and queues. Declaration defaults run for every element on every
activation, in declared dimension order. Explicit declaration and member defaults
retain their normal priority. Eligible four-state elements independently use
the existing `--init-values` policy; two-state values keep their usual defaults.

Declaration copies, assignment patterns, aggregate assignments and returned
structures use the existing leaf/element snapshot rules. A structure's input
array leaf is passed as a typed array operand, rather than loaded as a scalar.
Input, output and inout calls retain their shapes and value-copy rules; output
copyback occurs once. Existing array arguments and array refs can name an
eligible array member without enabling references to the whole structure.

```systemverilog
module top;
  typedef struct { int words[2]='{3,5}; } payload_t;
  task automatic show(input int number);
    payload_t value;
    value.words[0] += number;
    #1;
    $display("%0d", value.words[0] + value.words[1]);
  endtask
  initial show(7);
endmodule
```

This prints `15`. Different calls and reentered blocks have distinct storage.
Captured siblings share the original array field, see its committed changes
and can retain it after the owner returns. Typed container elements remain
roots through the existing activation publisher. A blocking intra-assignment
wait preserves its RHS snapshot if another process changes the source.

## Remaining boundaries

Automatic arrays of structures, including record-array members, and arrays of
structures as subroutine arguments retain their existing rejection. Fixed event
array members are also still rejected: this change preserves their reviewed
boundary fixtures. A separate scalar event field can coexist with the supported
array members and use the existing retained-event path. Event rebinding,
event-bearing output/inout records, whole-structure refs and general array-slice
copyback are not enabled.

The original A03 measurement at `51cda314` is historical. This change implements
the fixed leaf-array connection; it does not resolve the whole combination-gap
list or establish complete P1/external-simulator parity.

## Validation

The new [CLI matrix](../../tests/cli/run_struct_array_member_tests.py) performs
150 checks on 25 original sources: eight declaration homes, nested records,
ascending/descending and negative bounds, multidimensional wide values, strings,
reals, defaults, input/output/inout, references to array members, returns,
container copies, invalid indices, capture, blocking snapshots and four initial
value modes. The mode checks also exercise default output formals and input
array marshalling.

[Five unit tests](../../tests/unit/struct_array_member_test.cpp) run after
destroying source files and frontend ASTs. They inspect captured array metadata,
test independent retained iterations, wait notifications, output copyback,
Context isolation and 4,000 intervening declarations while owners' children
retain container arrays and scalar events. Completion releases every activation
lease and managed event; event-row reuse remains bounded. Existing boundary
fixtures, expected output, SIR dumps and performance expectations are preserved.

Five compiled controls are detected: missing array defaults, reused random
defaults across elements, scalar input marshalling, missing member defaults and
omitted array-container snapshots. Restored sources pass the focused checks.
