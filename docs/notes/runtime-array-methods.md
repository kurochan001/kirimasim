# Runtime array methods: execution checkpoint

[日本語](runtime-array-methods.ja.md). Status: implemented subset, 2026-10-25.

The human-approved [reduction/ordering](array-methods-proposal.md) and
[extrema](array-extrema-proposal.md) decisions are incorporated into
VALUE_SEMANTICS 7.9, SIR_SPEC 5.8/6 and RUNTIME_ABI 6. These decisions,
IEEE 1800-2023 clauses 7.12.1–7.12.3 and 13.5.2, and this repository's
existing typed value/reference operations are the design sources. No other
simulator's implementation or tests were read. The tests are original;
no new dependency is introduced.

## One lowering and one exchange operation

`Lowerer::lowerArrayMethod` handles reductions, extrema and ordering from
both expression and statement calls. Fixed storage uses declared left-to-right
indices; dynamic arrays and queues use increasing positions. The old
fixed-only reduction path is removed. Traversal, first-value initialization,
stable comparison and key evaluation belong to front. Arithmetic and comparison
use existing shared value instructions, rather than a second policy in VM.

`container.swap.values` is a typed, nonterminating SIR mutation with a captured
container ref and two integral indices. The verifier checks descriptor, ref,
arity and index types. The VM delegates to `Heap::swapValues`, using the actual
receiver type/bound and the existing full-width index decoder. Invalid,
unknown, negative, equal and out-of-range indices do nothing. The kernel swaps
normalized words inside existing cells, with no allocation or replacement of
cell tokens. Only changed cells emit value notifications, after both values
are committed; no structural notification is emitted. Nested owned handles
move intact, retaining refs within their nested values. Fixed storage uses
raw typed element stores for the same move, not language assignment copies.
The reference cell model in valsem independently exchanges referenced values.
Tier 1 continues to call the C++ API directly; C ABI remains 0.3.

Reductions start with the first actual value when nonempty, preserving a
singleton Z for every method. Empty product is one, empty integral and all
ones, and the other identities zero. `with` supplies the result type. Runtime
array call expressions bypass slang constant folding so these approved choices
also hold inside larger runtime expressions. Parameter forms already folded
by slang remain an explicit limitation.

Min/max snapshot the receiver before keys. A fixed receiver is copied into a
temporary typed queue; a dynamic receiver uses the shared snapshot operation.
Each key, including the first, runs exactly once. Only a definitely true
comparison replaces the candidate. The output is an independent unbounded
queue, empty or containing exactly one original element, not the key. The
shared append/copy operation also copies nested selected values independently.

Sort/rsort first capture pure keys, then perform stable adjacent exchanges;
reverse uses pairwise exchanges. X/Z and unordered floating comparisons do
not create a total order. This initial sorting algorithm is quadratic; a
future faster implementation must preserve these comparison and ref rules.

## Independent verification

[The generator](../../tools/gen_array_methods_matrix.py) produces 270 source
programs and expected outputs using Python integer folds, explicit four-state
truth tables and stable sorting of runs separated by unknown keys. Simulator
output is not used as the oracle. The matrix covers:

- 180 integral cases: fixed/dynamic/queue × initial/automatic/ref × widths
  1/9/32/65/129 × signed/unsigned × two/four states. They check all ten methods,
  empty/singleton input, X/Z, tie stability, typed bitwise keys and 32-bit
  `int`/`integer` projections. These projections explicitly distinguish
  two-state conversion from four-state preservation.
- 36 real/shortreal/string cases, including bounded actual queues passed
  through unbounded formal refs, in all three storage contexts.
- 24 held-reference cases, scalar and nested, across fixed/dynamic/unbounded/
  bounded receivers and all three storage contexts. Nested selected results
  are mutated to test independent copies.
- Twelve effectful-key cases: min/max across fixed/dynamic/queue, verifying
  call count, traversal trace, receiver evaluation, source mutation during
  keys, in-place writes to the original elements, and no key calls on empty input.
- Nine NaN cases with payload-bit output, testing first/middle/last unordered
  keys, and nine named/nested `with` cases with independent result copying.

Four direct unit tests check the shared kernel against the independent
valsem reference model, nested ownership/tracing without cloning, malformed
SIR operands/types, and hand-built SIR execution. The kernel matrix uses
150 valid/invalid random index pairs per width/state/container configuration,
retains every element reference, checks notifications and forbids heap
allocation during exchange. The VM matrix includes 129-bit positive, negative,
unknown and out-of-range indices. The 36 additional CLI cases check explicit rejection of effectful and unverified-pure
ordering keys across receiver/storage contexts. All 270 source cases and four unit tests
pass in dev; full configuration and compiled negative-control results will
be recorded in the validation checkpoint.

## Remaining work

This is not complete P1 or full array-method support. Other locators, shuffle,
fixed-aggregate/class/event elements, and unsupported source representations
remain. Effectful ordering keys are rejected; user-function purity is not yet
proved, so those ordering calls are conservatively rejected too. Min/max permit
effectful keys, including nested reductions. Slang still rejects nonintegral
reduction operands; no relaxation of its diagnostics is introduced. Parameter
constant folding, broader performance and full Icarus functional parity remain
unconfirmed; check the corresponding scope gates and original comparisons.
