# Source event inputs and runtime-selected input subarrays

[日本語](event-input-execution.ja.md). Status: integrated into main as `63c64d83`;
locally validated, 2026-10-26. P1 remains incomplete.
[Post-commit validation](../measurements/2026-10-26/event-input-committed-validation.json)
passes all 21 local CI steps on the complete committed tests. The earlier
working-tree record below is retained as dated pre-integration evidence;
it does not describe the current branch/publication state.

## Contract and implementation

The grounds are the approved [automatic-capture contract 3/5/6](automatic-capture-proposal.md),
[event publication amendment](managed-event-safepoints-proposal.md),
SIR_SPEC 7.4/8, RUNTIME_ABI 6 and IEEE 1800-2023 6.21/13.5/15.5.
All implementation, source checks and controls are original. No external
simulator source or test was read. No dependency, installation, public API,
SIR instruction/version, ABI version, option or expectation migration is added.

`Lowerer::lowerInput` supplies the event-specific copy-in entry. A named event
or fixed-array element is loaded through `eventOperand` and the existing typed
place/index rules; `null`, including a default input argument, becomes typed
event zero. Supported conversions preserve that same event value. The formal
shares its actual's object, queue and triggered state; no `event.new` is emitted
for the copy. `frameVarOf` admits automatic **input** event formals and retains
the output/inout boundary. Ordinary scalar input conversions retain their
existing path and diagnostic locations.

`lowerCallTo` uses this entry for scalar event inputs. Fixed arrays retain
element-by-element typed transport. Deferred assertion actions snapshot their
event input at evaluation time; the synthetic action uses the existing
substitution rather than reevaluating the original expression after the
issuer's scope ends. Non-event deferred snapshots are unchanged.

The VM and kernel are unchanged. Their previously validated typed fields,
input-transfer pins, retained lexical extents, pending operations and event
SSA roots supply ownership during synchronous calls, waiting tasks, recursion,
fork descendants and deferred callbacks. This change enables source access to
that transport; it introduces no source-context branches in the VM.

The neighboring array-input check exposed an existing runtime-prefix gap.
`arrayArgument` now uses `storageOf` and `copyArrayStorage` for selected input
subarrays not represented by the compact constant-index path. Indices are
evaluated once; remaining dimensions copy from each side's own left bound.
`arrayTarget` checks whether a selector is constant **before** lowering its
base, so discovering a runtime prefix cannot evaluate an effectful base twice.
This shared front path applies to event, integral, real and string elements.

## Validation

The [generator](../../tools/gen_event_input_matrix.py) supplies 104 new designs:

- 44 scalar cases across module/program, automatic/static function/task,
  slot/frame actuals, hierarchical names and constant/runtime selectors.
- 36 whole-array cases across automatic/static formals, slot/frame actuals,
  ascending/descending/multidimensional bounds and synchronous/waiting calls.
- 12 retained-descendant cases across module/program, named/mixed/triggered
  waits and blocking/delayed nonblocking triggers.
- Null/default-null and recursive collection checks, two deferred-action
  checks, four runtime-selected subarray checks and four neighboring element
  checks (integral, four-state, real and string).

The [boundary checker](../../tests/tools/check_event_input_boundaries.py)
requires positioned exit-2 errors, empty stdout and no internal error for
22 cases: general assignment/comparison/initializer aliases in three contexts,
automatic scalar/array output/inout formals, waiting on null, and implicit
triggered sensitivity through direct/nested input calls.

Four compiled controls replace an event value with null, evaluate an actual
twice, construct an object for null, or ignore a deferred snapshot. All are
caught by execution checks, not merely by mutant-anchor matching. Sources are
restored byte-for-byte and all 106 related CTest entries pass afterward.
The [control record](../measurements/2026-10-26/event-input-controls.json)
contains the source digest and detecting tests. Two additional policy-checker
execution cases verify that a function returning the selected array is called
once, for both constant and runtime prefixes.

[Full local CI](../measurements/2026-10-26/event-input-validation.json) passes
21/21 steps. Dev/ASan/UBSan each pass 5,847/5,849; release passes 5,848/5,850,
with zero failures and the same two known skips. PerfCount, 795 mutant anchors,
formatting, all 115 clang-tidy translation units and policy checks pass. After
adding the two stronger test-only aggregate-base checks, all 106 related CTest
entries pass again in all four presets; runtime sources are unchanged.

## Limits and separate observations

This adds input sharing, not general event rebinding, merging, comparisons,
initializer aliases, event-return expressions, event-valued conditional
expressions, event patterns, automatic structure event fields or event-containing
dynamic containers. No new output/inout rebinding semantics are supplied.
Null triggers remain no-ops; waiting on null retains the approved runtime error.
The conservative event-SSA lifetime and the existing final/heap collection
boundaries remain unchanged.

The real/string neighboring checks use ordinary equality. A separate original
probe found that Icarus 13 rejects real/string `===`/`!==`, while the pinned slang
accepts the source and native lowering emits a `ceq`/`cne` rejected by the SIR
verifier. The required source-language treatment is **not confirmed**: check
IEEE 1800-2023 11.4.5 and slang's documented type rules before choosing a
diagnostic or lowering change. This work changes neither of those operators
nor their expectations; no speculative semantics are enabled.

Fresh runs of the fixed P1 population remain 1,456/1,466 with zero failures
and ten unsupported; native sv-tests remains 244/344, with 62 unsupported and
no prior pass lost. Both Icarus 13/14 comparisons run all 87 original probes:
seven gaps and four/seven reference mismatches remain, with no native stdout
change or prior passing probe lost. The [population](../measurements/2026-10-26/event-input-population.txt),
[sv-tests](../measurements/2026-10-26/event-input-svtests.json),
[Icarus 13](../measurements/2026-10-26/icarus13-event-input-probes.json) and
[Icarus 14](../measurements/2026-10-26/icarus14-event-input-probes.json) records
preserve the evidence. These finite populations do not establish complete parity.
No hosted CI or fresh Linux comparison is claimed.
No fresh wall-clock benchmark or performance threshold is asserted here;
PerfCount budgets are unchanged.
