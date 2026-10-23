# Array minimum/maximum: proposed execution contract

[日本語](array-extrema-proposal.ja.md). Proposed 2026-10-24; **awaiting
approval, not implemented**. This is separate from the pending reduction/
ordering and multidimensional binary-input proposals. It adds no dependency.

## Why review is needed

[P1_SCOPE](../spec/P1_SCOPE.md) requires detailed semantic review where
specifications are incomplete or conflicting. The approved container contract
defines copying and identity, but does not decide locator traversal, tied-key
selection, unknown comparisons or effectful `with` evaluation. IEEE 1800-2023
7.12 and 7.12.1 were checked locally; no other simulator's source/tests were
read. The rules below paraphrase the standard and identify project choices.
After approval, incorporate these rules into VALUE_SEMANTICS and link them
from SIR_SPEC 5.8 and the supported-method inventory before enabling lowering.

## Proposed rules

- `min()` and `max()` return an unbounded queue of the source element type.
  Empty input returns an empty queue. Nonempty input returns **one element**,
  including when several elements have equal extreme keys (7.12.1). Do not
  copy the installed Icarus behavior of returning every tied element.
- Without `with`, compare the element values. With `with`, compare its key
  values but return a source element, not a key. Preserve the key expression's
  type, width, signedness and existing value-operation semantics. Slang
  continues to check comparable types and method syntax.
- **Project traversal/tie choice:** visit the first remaining fixed dimension
  from its declared left bound toward the right; visit dynamic arrays and
  queues from index zero upward. Nested arrays remain elements, not flattened
  scalar candidates. Initialize the candidate with the first
  element and replace it only for a strictly smaller/larger key. Equal keys
  retain the first candidate. Locator traversal is unspecified in 7.12.1;
  this chooses deterministic traversal without claiming the standard fixes it.
- **Project unknown/unordered choice:** replacement requires an existing
  relational comparison to be definitely true. An X/Z comparison or false
  real comparison, including NaN, retains the current candidate. Do not
  invent a numerical total order for unknown values or alter shared operators.
- **Project snapshot/evaluation choice:** evaluate the receiver once and
  snapshot its values before key evaluation. Evaluate every element's key
  exactly once in the traversal order, including the first element; no key
  is evaluated for empty input. Keys may have side effects. Those effects
  follow that order but cannot change the captured source length/values or
  make a key run again. Section 7.12 leaves effectful-key results unpredictable;
  this is an explicit deterministic project rule for that otherwise open case.
- Return an independent value copy of the chosen element under the existing
  container copy/bound rules. This includes nested container values; future
  object handles retain their established assignment semantics. Do not
  implicitly mutate the receiver, reorder its cells or invalidate existing
  references. Explicit key-expression side effects on live program state persist.
- Use one front lowering path across slots, automatic frames, refs, returned
  values and execution contexts. Emit existing size/read/snapshot/queue
  operations and shared relational operators. The VM only transports typed
  operands; no source-level selection rule belongs in it. No new kernel
  minimum/maximum API or SIR opcode is proposed.
- Source kinds whose element representation is not yet available, including
  associative/class/fixed-aggregate cases, remain explicit incremental gaps.
  They are not removed from the P1 target. Constant/parameter forms must be
  checked against pinned slang before claiming the same coverage; a discovered
  constant-evaluation conflict needs review rather than silently different
  runtime/constant behavior.

## Original observations and validation plan

The installed compiler/runtime report `14.0 (devel)
(s20260301-411-gca6b8d376)`. Both dynamic-array and queue inputs `[4,7,6]`
return one minimum, 4, and one maximum, 7. Inputs `[4,7,4]` and `[4,7,7]`
return two tied minima/maxima there, contrary to the single-result rule checked
in 7.12.1. Empty dynamic/queue inputs return an empty queue. The pinned slang
MIT implementation returns one element for a nonempty min/max constant
evaluation; this code observation is not a completed constant-case test.
Exact Icarus 13.0 behavior remains unconfirmed.

For the original `[4,7,6]` input, `min() with (-item)` returns 4 in that
Icarus run; using the key requires returning source element 7. Record this
as a reference result mismatch, not as an oracle for key semantics. Original
four-state probes with X first/later return X/1 respectively; these examples
do not settle all unknown-key or side-effect cases.

After approval, add independent model matrices for unique/duplicate extrema,
empty/singleton inputs, signed and unsigned widths across machine-word
boundaries, X/Z positions, real/shortreal/string keys, `with` projections,
effectful keys, nested value copies, fixed/dynamic/queue sources, receiver
calls, selected refs and scheduling contexts. Include compiled mutations that
return all ties, ignore keys, repeat key/source evaluation, lose signedness or
share mutable result values. Preserve existing expectations and separate
Icarus-passing gaps from reference mismatches in the comparison record.
