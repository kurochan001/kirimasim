# Concrete examples of P1 combination gaps (2026-10-30)

[日本語](P1_COMBINATION_GAPS_2026-10-30.ja.md)

**Current-source pointer (2026-11-04):** This is a dated record. Published main `91139559` and its CI are described in the [integration record](../notes/main-integration-2026-11-04.md); use [P1 Support](../P1_SUPPORT.md) for present implementation. Measurements and decisions below retain their original scope.

Baseline: main `51cda3143aed9db377b4ea570f6b8ec0bf8bc513`.
This investigation makes item “8. Combination gaps” in the previous remaining-work explanation concrete.
Independent large items such as classes, VPI, specify, FST and `%p` / `%v` are handled separately.
It changes neither specifications, existing expectations nor P1 exit criteria.

**Same-day update:** the measurements below belong to `51cda314`. Subsequent work implemented E01
[automatic structure events](../notes/automatic-struct-events.md), A03
[fixed-array members](../notes/automatic-struct-array-members.md), N02
[event-NBA through fixed-array refs](../notes/event-nba-ref-arrays.md), N03
[retained packed parent windows](../notes/event-nba-ref-windows.md), N01
[delayed NBA through scalar refs](../notes/delayed-ref-nba.md) and
[delayed NBA through fixed-array refs and packed selections](../notes/delayed-ref-selections.md).
It also implemented N04 string-character NBA ([SCHEDULER_SPEC 13](../spec/SCHEDULER_SPEC.md)),
A01 unpacked structure ref arguments ([SIR_SPEC 8](../spec/SIR_SPEC.md), member-wise references),
A02 structure-array arguments, returns and automatic storage (SIR_SPEC 4.2/8),
A04 fixed-array slices ([VALUE_SEMANTICS 3.13](../spec/VALUE_SEMANTICS.md)),
S01 writes to bits/elements within a runtime-selected packed parent, the nested streaming targets of S02,
and F02 interface/program VCD scopes. S02 `with` and string targets, and E02 event assignment, remain P4.
See the linked documents for each remaining boundary. The original results in this text and the measurement JSON are retained.

## 1. What a combination gap means

Features that work individually can fail when **types, storage lifetimes, argument passing,
scheduled updates and execution regions are combined, because lowering and retention paths are not connected**.

For example, an automatic scalar event and a static structure's event member work,
but an event member in an automatic structure is unsupported.
Similarly, ordinary delayed NBA and ref arguments work independently, but delayed NBA through `ref static` is unsupported.

The categories below help organize the work. Several forms may stop for the same reason;
the item count is not a count of required PRs, implementation effort or mandatory P1 features.

## 2. Scope of this investigation

Using `kirimasim 0.0.0 (commit 51cda3143aed)` built from the current sources,
34 original inputs were checked with `parse` and `--dump-sir`.
The six inputs that produced SIR were also executed normally.

- **25 inputs: executable SIR lowering unsupported.** `parse` returns 0; `--dump-sir` returns 2 with `unsupported`.
- **5 inputs: working controls.** Events, ordinary delayed NBA, a fixed packed parent, queue monitoring and similar controls.
- **1 input: missing waveform scope.** Execution succeeds, but program/interface signals are absent from VCD.
- **3 inputs: rejected by the front end.** These are not counted as native implementation gaps.

The sources of every input, exit codes, diagnostics and control stdout are saved in the
[measurement JSON](../measurements/2026-10-30/p1-combination-gaps.json).
A `probe` below denotes its JSON `id`; the same JSON contains the complete sources for the snippets.

**No Icarus 13.0 comparison was run in this investigation.** A successful `parse` does not independently
establish IEEE conformance or P1 necessity for each form. These observations record implementation boundaries;
they do not make all 25 inputs new mandatory P1 conditions.

## 3. Combinations of events, event controls and processes

### E01: automatic structure × event member

An automatic standalone event and a static structure's event member work.
The missing form creates the following structure in an automatic task.

```systemverilog
module top;
  typedef struct { event go; int n; } item_t;
  task automatic check_value();
    item_t item;
    -> item.go;
  endtask
  initial check_value();
endmodule
```

`probe: event_struct_auto` reports `unsupported: automatic event variable`.
`event_struct_static_control` and `event_scalar_auto_control` both print `1` and finish normally.

The structure's event leaf needs to be connected to automatic-frame initialization, typed roots and capture.
Tests must cover independence between calls, retention across fork/waits and collection after completion.
Evidence: [structure-frame lowering](../../src/front/lower_struct.cpp),
[automatic capture contract](../notes/automatic-capture-proposal.md).

### E02: event value × initialization from a function return or conditional

Direct waits/triggers of named events, supported declaration-initialization copies and input arguments work.
Copying an event from a computed expression lacks the following paths.

```systemverilog
function automatic event identity(input event e);
  return e;
endfunction
// In a context with event source_event:
// static event copied = identity(source_event);
// static event selected = choose ? first_event : second_event;
```

`probe: event_return, event_conditional` both pass parse and stop at
`unsupported: assignment to this kind of target`.
Because copying the function/conditional event value stops first, these results do not show that
lowering reached the function body's return handling.

Function-return transfer, event identity, single evaluation and GC roots need to be connected.
Meanings not settled by the existing limited event contract, including an unknown conditional condition,
require review before implementation. Evidence: [event copying](../../src/front/lower_sub.cpp),
[expression lowering](../../src/front/lower_expr.cpp), [event transfer](../notes/event-transfers.md).

### E03: `always_ff` × computed edge expression

Ordinary signal edges and expression observation in general event controls are supported.
`always_ff @(posedge (a ^ b))` lacks the connection to the expression form.

`probe: always_ff_expression` reports
`unsupported: always_ff with an event control on an expression`.
The same event control must remain in effect; expression changes, edges and scheduling regions need verification.
Evidence: [procedure/sensitivity lowering](../../src/front/lower.cpp), [event controls](../../src/front/lower_stmt.cpp).

### E04: `iff` guard × function that writes a variable

Guards that read values work. A guard calling a function that writes a variable is unsupported.

```systemverilog
function automatic bit bump();
  count++;
  return 1;
endfunction
// @(posedge clk iff bump());
```

`probe: guard_writes_value` reports `unsupported: iff condition that writes a variable or net`.
Evaluation count/timing and notification/reentry caused by the write need a defined and tested contract.
This differs from the directly written `count++` guard that slang rejects, described later.
Evidence: [guard lowering](../../src/front/lower_stmt.cpp).

### E05: nonblocking event trigger × event control

Ordinary `->`, `->>`, supported delayed triggers and event-NBA work.
`->> @(posedge clk) go;` is a separate unsupported path.

`probe: event_trigger_control` reports `unsupported: ->> with an event control`.
Registration, retention of the event value, unregistration and process-exit cleanup need to be connected.
Evidence: [event-trigger lowering](../../src/front/lower_stmt.cpp).

### E06: named fork × disable by name

`fork` / `join` and supported `disable fork` work.
Stopping `fork : workers ... join` using `disable workers;` is unsupported.

`probe: named_fork_disable` reports `unsupported: disable of this block`.
The fork activation must be associated with its named scope so that child processes, waits and pending updates
are terminated correctly. It cannot simply be treated like disabling a sequential block.
Evidence: [disable handling](../../src/front/lower_stmt.cpp), [capture handling](../../src/front/lower_capture.cpp).

## 4. Scheduled assignments × refs and selected targets

### N01: `ref static` argument × delayed NBA

`value <= #1 5;` to an ordinary static variable works; the control printed `5`.
Its ID is `nba_slot_delay_control`. Scheduling through a ref to the same static variable is missing.

```systemverilog
module top;
  int value;
  task automatic put(ref static int r);
    r <= #1 5;
  endtask
  initial begin put(value); #2; end
endmodule
```

`probe: nba_ref_delay` reports `unsupported: delayed nonblocking assignment through a ref`.
The value, time and actual static target at scheduling must be retained so that the correct destination is
written after the task returns. This does not relax the prohibition on NBA to automatic variables.
Evidence: [ref stores](../../src/front/lower_expr.cpp), [SIR reference contract](../spec/SIR_SPEC.md).

### N02: event-NBA × element of a ref fixed array

Event-NBA to a direct static array element is supported.
`r[1] <= @(posedge clk) 5;` through `ref static int r[2]` is unsupported.

`probe: nba_ref_event_array` reports `unsupported: event-NBA to an array element through a ref`.
The referenced array and the scheduling-time index must survive the event wait.

### N03: event-NBA × nested packed selection through a ref

This form uses `r[0][i] <= @(posedge clk) 1;` through `ref static logic [1:0][7:0] r`.
A simple reference and supported bit selections alone cannot represent a selection with this parent window.

`probe: nba_ref_event_window` reports
`unsupported: event-NBA to a nested packed selection through a ref`.
Capture the parent's width, position and bounds and the child's position, and schedule only writable bits.
Evidence for N02/N03: [event-NBA target capture](../../src/front/lower_nba.cpp),
[event-NBA contract](../notes/event-nba-proposal.md).

### N04: string-character selection × delayed NBA

Ordinary string and character assignments work.
`text[i] <= #1 8'h41;` to a static string is unsupported.

`probe: nba_string_char_delay` reports
`unsupported: delayed nonblocking assignment to a string character`.
The destination/update rules when the string's value, length or index changes while pending must connect to
existing string semantics. Evidence: [character stores](../../src/front/lower_expr.cpp).

## 5. Fixed aggregates × automatic storage, arguments and copyback

### A01: unpacked structure × ref argument

Fixed structure values and supported input/output/inout transfers, and scalar/fixed-array refs work.
Passing `typedef struct { int n; } item_t;` to `task put(ref item_t r);` is unsupported.

`probe: struct_ref` reports `argument 'r' of an array of structures or a ref structure`.
Map the whole structure ref to leaves and retain the actual target across waits/transfers, including const restrictions.
Evidence: [subroutine-argument lowering](../../src/front/lower_sub.cpp).

### A02: structure array × automatic storage or array argument

Supported operations on static structure arrays work.
An automatic task's `item_t items[2];` and `input item_t items[2]` have missing paths.

`probe: struct_array_auto` reports `automatic unpacked array in a structure or of structures`;
`struct_array_input` reports `argument 'r' of an array of structures or a ref structure`.
Automatic leaf-array storage, argument copies and independence between calls need to be connected.
The source also explicitly rejects functions returning structure arrays, but that form was not executed in this investigation.
Evidence: [structure storage](../../src/front/lower_struct.cpp), [arguments/returns](../../src/front/lower_sub.cpp).

### A03: structure's fixed-array member × automatic storage

The form uses this type as a local variable in an automatic task.

```systemverilog
typedef struct { int words[2]; } item_t;
// Inside task automatic: item_t item; item.words[0] = 1;
```

`probe: struct_array_member_auto` reports
`unsupported: automatic unpacked array in a structure or of structures`.
Frames must represent, initialize and copy array leaves as well as scalar structure leaves.
Evidence: [structure frames](../../src/front/lower_struct.cpp).

### A04: fixed-array slice × output/inout copyback

Supported array-slice reads and whole-array arguments work.
A subarray used as an output actual is unsupported.

```systemverilog
module top;
  int values[4];
  task put(output int r[2]); r = '{1, 2}; endtask
  initial put(values[1:2]);
endmodule
```

`probe: array_slice_copyback` reports `unsupported: an output or inout array argument of this actual`.
The original input here tests output only; inout is the same boundary documented in the support table and source.
Actual evaluation time, returned target range, declaration direction and preservation of other elements need to be connected.
Evidence: [array-actual lowering](../../src/front/lower_sub.cpp), [SIR copyback](../spec/SIR_SPEC.md).

## 6. Combinations of selections and streaming

### S01: moving packed parent × runtime child selection

Fixing the parent, as in `values[1][bit_index] = 1;`, works; the control prints `0800`.
Its ID is `packed_parent_fixed_control`. Selecting the parent at runtime is unsupported.

```systemverilog
module top;
  logic [1:0][7:0] values;
  int row = 1, bit_index = 3;
  initial begin
    values = '0;
    values[row][bit_index] = 1;
  end
endmodule
```

`probe: packed_parent_runtime` reports `unsupported: a runtime select of this parent packed range`.
Evaluate the parent and child positions once each and retain both positions and bounds, so an out-of-parent
child selection cannot write a neighboring element. Evidence: [packed write targets](../../src/front/lower_expr.cpp).

### S02: streaming × `with`, string targets or nested targets

Supported fixed-array/structure streaming and delayed NBA are implemented. Three representative remaining forms are:

- `probe: stream_with`: `{>>{values with [1:2]}}`, rejected as `streaming with a with clause`.
- `probe: stream_string_target`: `{>>{text}} = 16'h4142;`, rejected as `unpacking this stream`.
- `probe: stream_nested_target`: `{>>{{>>{value}}}} = 8'h41;`, rejected as `unpacking into this target`.

Selected element count/order, string length, nested bit rearrangement and per-target truncation must be connected
to shared conversion/store operations.
Evidence: [streaming lowering](../../src/front/lower_expr.cpp), [SIR streaming contract](../spec/SIR_SPEC.md).

## 7. Combinations of files, waveforms and observation

### F01: `$fread` × dynamic memory or complex structure memory

Integral scalars, multidimensional fixed integral memories and supported one-dimensional structure memories
with integral leaves work. Four representative remaining forms are:

- `probe: fread_dynamic`: reading into `byte values[]`; the dynamic-memory destination path is unsupported.
- `probe: fread_struct_array_member`: a memory of structures containing `byte values[2]`; array members are unsupported.
- `probe: fread_struct_real_member`: a memory of structures containing `real value`; real members are unsupported.
- `probe: fread_struct_multidim`: structure `rows[2][2]`; multidimensional structure memories are unsupported.

All four pass parse, then stop at `$fread into a memory of this kind` or
`$fread into a structure with a member of type ...`. None reaches file opening.
Traversal order, start/count, partial input, preservation of previous values and copyback-target capture must be integrated.
Evidence: [input targets/layout](../../src/front/lower_expr.cpp), [system-task contract](../spec/SYSTEM_TASKS.md).

### F02: program/interface variables × VCD scope

Module VCD output works. This input instantiates an interface and a program inside a top module,
updates their internal variables and calls `$dumpvars(0, top)`.

`probe: vcd_program_interface` passes parse, SIR generation and normal execution, but its VCD contains only
`$scope module top` and `module_bit`; interface `bus_bit` and program `program_bit` are absent.
Exit codes alone do not expose this gap.

Hierarchy traversal, scope kinds and corresponding signal registration must connect to waveform output.
Evidence: [dump-hierarchy construction](../../src/front/lower_dump.cpp), [waveform specification](../spec/WAVEFORM.md).

### H01: dynamic-container values × implicit sensitivity

Explicit waits on dynamic arrays/queues and supported heap-dependent observation work.
Implicit sensitivity such as `always_comb result = values.size();` has a missing path.

`probe: heap_implicit_sensitivity` reports
`unsupported: implicit sensitivity to dynamic container values`.
Container mutation, replacement and element changes must connect to sensitivity beyond static slot ranges.
The support table gives the same boundary for `always_latch`, `always @*` and continuous assignment;
only `always_comb` was executed here. This overlaps the previous large category “4. Remaining dynamic-container work”;
do not count it twice. Evidence: [implicit-sensitivity checks](../../src/front/lower.cpp),
[container support](../P1_SUPPORT.md).

### U01: heap-reading file monitor × all contexts or reentrant registration (unconfirmed)

Basic monitoring of queue-element changes, file-monitor closure, descriptor reuse and callback termination are already tested.
The `postponed_heap_control` here also printed `1` and `2` as the same queue element changed.

Reentrant registration from callbacks and coverage across all source contexts remain **unconfirmed** in the support table.
This does not mean this investigation found an unimplemented feature or defect.
Registration changes, file closure, replaced heap values, callback ownership and cleanup need combined verification.
Evidence: [file-monitor lifetime checks](../notes/heap-file-monitor-lifetime.md), [support table](../P1_SUPPORT.md).

## 8. Forms excluded from the implementation-gap count

The following three inputs fail parse under the strict front-end settings used here.
Their diagnostics are not relaxed to enable native lowering.

- `automatic_nba_illegal_control`: NBA to an automatic local,
  `nonblocking assignment to automatic variable ... is not allowed`.
  N01 uses `ref static` receiving a real static target to avoid this restriction.
- `strobe_auto`: direct reference to an automatic variable from `$strobe`,
  `automatic variable ... cannot be traced`. This is a documented boundary, but the diagnosis here comes from slang.
- `packed_parent_outside`: a constant packed index outside the declaration bounds,
  `cannot refer to element 2 ... [-Windex-oob]`, promoted to an error.
  This records strict diagnostic behavior; it does not generalize that every constant out-of-range selection is illegal under the standard.

A directly written `count++` guard was also rejected by slang.
E04 instead checks a variable write inside a called function.

## 9. Implemented work and how to use this record

Already implemented features distinct from these gaps include mixed fixed/dynamic-dimension `foreach`,
supported runtime dimension queries, automatic scalar/fixed-array events, basic event-NBA,
delayed NBA to fixed aggregates and basic heap-reading monitors.
The recent strength-force work is a kernel foundation; its source connection belongs separately to
“5. Remaining net/switch work”.

To use this record for the next implementation, select one form and:

1. Determine whether it is required by [P1_SCOPE](../spec/P1_SCOPE.md) or a candidate needing comparison/specification review.
2. Check existing semantics and accept/reject expectations; separate any necessary specification amendment or expectation migration first.
3. Retain a standalone passing control and a failing combination; verify values, evaluation counts, times, regions and lifetimes.
4. After support is added, keep these dated observations and update the new revision's support table and regressions.

## 10. Reproducing the saved inputs

For example, extract E01's complete input from JSON. Run at the repository root.

```sh
mkdir -p build/combination-audit
python3 - <<'PY'
import json
from pathlib import Path
data = json.loads(Path('docs/measurements/2026-10-30/p1-combination-gaps.json').read_text())
probe = next(item for item in data['probes'] if item['id'] == 'event_struct_auto')
Path('build/combination-audit/probe.sv').write_text(probe['source'] + '\n')
PY
build/dev/src/driver/kirimasim parse build/combination-audit/probe.sv
build/dev/src/driver/kirimasim --dump-sir build/combination-audit/probe.sv
```

At the recorded revision, the first command returns 0 and the second 2.
Reformatting the input changes diagnostic positions. To switch probes, change `id` and inspect saved `parse`/`sir` results.
The waveform example creates `probe.vcd` in the working directory, so use an isolated directory.
Future implementations may make these inputs succeed. This document is neither an automatically registered test suite
nor additional pass/fail counts for the then-current 8,311 tests.
