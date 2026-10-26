# Class static storage and eager initialization

[日本語](class-static-execution.ja.md). Status: implemented and locally validated,
2026-10-27. Implementation `9e6a149d` is integrated into main; the
[integration record](../measurements/2026-10-27/class-static-main-integration.json)
confirms unchanged validated implementation/tests. The validation record retains
its measurement-time uncommitted state. P1 and class object execution remain incomplete.

## Contract and implementation

Grounds: approved [class-runtime contract 2](class-runtime-proposal.md#2-static-properties-and-initialization),
SIR_SPEC 7.1, VALUE_SEMANTICS 1.2/1.4/3.13, and IEEE 1800-2023 6.21/8.9/13.5.
All tests and controls are original. No external simulator source, reproducer,
standard wording/example, new dependency or installation is used. Slang remains
the pinned unmodified frontend; its semantic symbols determine identities and
access permissions. No additional external design source was used.

`Lowerer::declareClassStatics` is the declaration entry for non-parameterized
classes. `declareScope` admits only a class's own static properties into the
ordinary variable/aggregate slot and initializer paths. Instance fields wait
for object construction. A canonical declaring-class set prevents duplicate
visits; it is queried only for membership, never iterated for SIR numbering.
Inherited properties retain the original symbol/slot; hidden redeclarations
have separate storage. Separate elaborated lexical owners keep slang's canonical
identities rather than merging by textual name. Package classes share their
package row across consumers.

`declarePackages` discovers class storage in compilation units, packages and
anonymous programs even when unused. Instantiated modules, programs, interfaces,
generate scopes and nested classes pass through the same declaration entry.
At each declaration position, static properties retain source order in the
existing pre-time-zero initialization process, after ordinary default storage
initialization. Qualified forward reads retain the existing order warning;
instance fields and inherited declarations are not initialized again.

The common `isVariable`/`inSlot` entries now include ClassProperty for typed
loads, stores, ref/output/inout actuals, automatic-lifetime checks and initializer
order warnings. Static methods use existing subroutine lowering. Waiting tasks,
NBA, event waits and combinational dependencies use the same typed slot paths.
No VM shape branches, kernel operation, SIR instruction/version or ABI changes
are needed; mutable simulation state stays in each Context.

Parameterized static storage is deliberately unfinished. Direct access is
positionedly rejected in `newSlot`; `checkGenericClassStatics` also rejects
elaborated but unused specializations with static storage, avoiding a false
successful run with omitted initializer effects. It queries to a fixed point,
snapshots slang's specializations before lazy elaboration, and emits one source-
ordered diagnostic per declaration. Hash discovery never assigns SIR IDs.
Property-free generic static methods remain usable. This guard is a boundary,
not the deterministic generic-specialization execution required by contract 1/2.

A declaration-discovery stack stop is caught at the `compile` boundary, returning
its already recorded diagnostic. The first full test run exposed an escaping
exception on the existing default-thread case; three additional class-declaration
thread cases and a compiled fault cover the corrected boundary.

## Evidence

The [generator](../../tools/gen_class_static_matrix.py) adds **563 designs**:
448 value/access combinations (eight declaration homes, fourteen types, four
spellings), 24 unused/inherited/alias discovery checks, eight hidden-field checks,
24 initialization checks, 20 argument-direction checks, 24 waiting/Active/NBA
checks, twelve combinational sensitivity checks and three canonical-owner checks.
Struct member defaults are constant expressions; initializer side effects are
checked separately in ordinary static initializers. Anonymous-program aliases
are declared in a legal consuming named program, not as illegal anonymous members.

The [contract checker](../../tests/tools/check_class_static_contracts.py) covers
nine positioned boundaries, a property-free generic method, two SIR storage
identity designs, the qualified-forward-read warning and fatal/finish stopping
before module effects. These and the generator make 565 new CTest entries; three small-stack class
checks bring the total to **568 new CTest entries**.
The existing sequential/parallel/churn Context tests also include a new class
static design with integer, string, queue and event state, comparing repeated
compilations/runs against isolated Contexts.

[Before/after observations](../measurements/2026-10-27/class-static-before-after.json)
show three previously silent unused-declaration omissions now producing
`calls=2 trace=12`; a fourth module declaration changes from unsupported to that
output. The saved pre-change binary and current dev binary have recorded SHA-256
and actual version strings. These finite original checks are not class parity.

[Seven compiled faults](../measurements/2026-10-27/class-static-controls.json) are
caught: skipped discovery, omitted scalar initialization, eager instance-field
initialization, duplicate class initialization, accepted generic omission,
missing order warning and an escaping declaration stop. Mutation-anchor checks
are excluded as detectors; every fault builds and is caught by executed
source, contract or Context checks. Source bytes
are restored exactly and all **571 selected tests** then pass (568 new plus the
three existing Context modes).

Full local CI and refreshed external-regression evidence are recorded in the
[validation record](../measurements/2026-10-27/class-static-validation.json).
The final full local CI passes **21/21 steps**: dev/ASan/UBSan each pass
6,625/6,627, release passes 6,626/6,628, with zero failures and the same two known
skips each. PerfCount, 804 mutation anchors, formatting and all 115 clang-tidy
translation units pass. Existing protected test sources/expectations are
unchanged against the base. That run preceded the implementation commit and the
topic had no upstream; the implementation commit now also passes the explicit
commit-level expectation guard, as the integration record shows.

The [fixed population](../measurements/2026-10-27/class-static-population.txt) is
1,456/1,466, zero failures, ten unsupported; native
[sv-tests](../measurements/2026-10-27/class-static-svtests.json) is 244/344,
62 unsupported, with no former pass lost. Both original 87-probe comparisons
([Icarus 13](../measurements/2026-10-27/icarus13-class-static-probes.json),
[Icarus 14](../measurements/2026-10-27/icarus14-class-static-probes.json)) retain
seven gaps, four/seven reference mismatches and all prior native statuses/stdout.
The earlier [class investigation](class-execution-investigation.md) remains a
dated pre-fix record. No existing oracle, specification or performance budget
is changed. No fresh wall-clock speed claim, Linux run or hosted CI is made.

## Limits

This supports static properties of non-parameterized classes using existing
supported slot types: packed integral 2/4-state, real/shortreal, string, event,
fixed arrays, supported unpacked structures and integral queues/dynamic arrays.
The restrictions of each existing operation still apply. Class scope/property
waveform selection is not enabled (WAVEFORM 7).

Class construction, object handles/layout, instance fields, receiver/virtual
dispatch, object RNG/ownership/GC and parameterized static execution remain work.
The approved class-runtime contract is broader than this increment. Existing
87-probe gaps and finite population counts do not establish P1 completion.
