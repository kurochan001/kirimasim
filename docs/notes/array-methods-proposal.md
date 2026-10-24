# Runtime array reduction and ordering amendment

[日本語](array-methods-proposal.ja.md). Status: **approved (2026-10-25)**;
tested implementation subset: [execution checkpoint](runtime-array-methods.md). The proposal below is retained as the decision
record; approval requests and future tense describe its original proposal state.

## Purpose and authority

P1_SCOPE requires the Icarus-supported mechanisms, including the measured
`dynamic_array_sum` and `queue_sort` gaps. Its scope approval does not decide
missing semantic details. The requirement is: "Detailed semantic amendments
must be reviewed before implementation where specifications are incomplete
or conflicting."

Clauses 7.12, 7.12.2, 7.12.3 and 13.5.2 were checked locally. The text below
is a paraphrase and explicit project choices, with original probes only.
Neither Icarus nor Verilator source or tests were read.

## Proposed value rules

- Reduction methods `sum`, `product`, `and`, `or`, `xor` visit an unpacked
  array in increasing cardinality order. The first value initializes a
  nonempty fold, preserving a lone X/Z payload. Later operations use the
  existing arithmetic/bitwise rules of VALUE_SEMANTICS. A `with` expression
  supplies both the value and result type, including width and signedness.
- **Project choice for empty inputs:** return zero for sum/or/xor, numeric
  one for product, and all ones in the integral result width for and. Do not
  evaluate the `with` expression on an empty input. These are empty-result
  choices, not operands prepended to a nonempty fold.
- The empty-input choice needs approval: the checked reduction clause does
  not explicitly specify these return values. On original probes, the
  installed Icarus compiler/runtime report `14.0 (devel)
  (s20260301-411-gca6b8d376)` and return 0 for empty int/logic sum and 1 for
  empty int/logic product. The three bitwise method spellings are rejected
  by that compiler; their empty results are unconfirmed there. The pinned
  slang constant evaluator returns zero for all empty reductions, including
  product. Runtime and constant lowering must not silently disagree: if
  this choice is approved, document and test the differing constant case
  under the existing front constant-evaluation exception policy. Parameter
  forms not yet lowered under this policy remain an explicit limitation.

## Proposed ordering and reference rules

- Implement reverse, sort and rsort for fixed arrays, dynamic arrays and
  queues. Sort uses the existing `<`/`>` value operations, with strings and
  numeric keys governed by the same shared value rules as expressions.
- **Project choice:** stable ordering for equal keys; an unknown relational
  result does not move an item past the item it is compared with. This
  also supplies deterministic behavior for X/Z keys and unordered real
  comparisons without inventing a numeric total order.
- **Project choice:** pure `with` keys are evaluated once per element, in
  increasing cardinality order, before moving values. Capture the receiver
  binding before evaluating keys. The initial delivery diagnoses effectful
  key expressions until their permitted unspecified behavior has a complete
  implementation. This is an incremental limitation, not removal from P1.
- **Project reference interpretation:** ordering exchanges values in
  existing element variables; it does not resize, delete or replace the
  entire variable by assignment. Keep the element cells at their existing
  indices, so a ref to an element observes its reordered value. This keeps
  fixed and dynamic array element references consistent. Nested owned
  values move without a language copy; refs inside those nested values keep
  their cells. The queue movement rules for insert/push/erase remain intact.
  This interpretation and nested-reference behavior require explicit review.

## Shared implementation contract

Add `container.swap.values !T, ref<!T>, first, second` to SIR_SPEC 5.8. The
indices retain their integral widths. It exchanges two values in the actual
receiver; an invalid index leaves both unchanged. Empty or equal indices are
no-ops. Add a C++20 `Heap::swapValues` API, checked against the independent
`valsem` cell model. It validates before committing, retains cell identity,
requires no new heap object, and publishes only committed normalized cell
changes. Nested values update the existing graph/GC bookkeeping. If change
notification metadata allocation fails, preserve the existing positioned
ResourceLimit policy rather than accept an incomplete notice set.

Front emits typed loops for runtime folds and ordering. VM mechanically
transports the new instruction to the shared heap; it contains no sorting or
source-shape policy. Fixed storage uses the same value comparisons and
exchanges through its existing places. No dependency, installation, C ABI
change, generated-code exception or kernel dependency on slang/SIR is added.

After approval, incorporate this contract into VALUE_SEMANTICS, SIR_SPEC,
RUNTIME_ABI and the implementation/status inventories before enabling it.
Shuffle, locator methods and other remaining forms stay on the full P1 work
list; this amendment does not redefine completion around these methods.

## Evidence required

Original matrices cross fixed/dynamic/queue storage, module/automatic/ref
contexts and callers, empty/single/many elements, signedness, two/four states,
1/32/64/65/129-bit widths, nonempty X/Z, real/string keys, named/nested `with`,
actual queue bounds, nested payloads, held refs, exact changes, allocator
failure and both scheduling groups. Compare normal results to original
Icarus executions and keep unsupported/mismatching cases explicit. Add
compiled deliberate mistakes and prove actual value/diagnostic failures,
then restore and run the full local CI. Preserve all existing test bodies,
expected output and performance counts; incompatible expectation changes
require their own reviewed PR under the repository rule.
