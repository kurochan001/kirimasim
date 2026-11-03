# State-dependent scalar specify paths: next execution contract

[日本語](specify-state-dependent-proposal.ja.md) · 2026-11-03.

**Status: sections 1–8 approved on 2026-11-03; [implemented](specify-state-execution.md).**
The maintainer explicitly approved sections 1–8 after the edition limitation in section 9
was disclosed. This operative contract amends the first specify contract's
sections 1, 3, 6 and 7, SIR_SPEC 6, and the shared scheduler's module-path selection.
It does not change the P1 completion criteria or authorize expectation migrations.
Section 9 remains an evidence limitation, not a completed 2023 edition audit.

The existing contract explicitly requires a follow-up review before implementing
state-dependent paths. The [new original comparison](../measurements/2026-11-03/specify-state-dependent-inventory.json)
has 40 inputs: Icarus 13.0 matches 32 written traces and differs on eight X/Z
conditions; native parse succeeds for all 40 and execution rejects all 40 as
unsupported. These are pre-implementation observations, not 40 new P1 gate cases.
The [execution follow-up](specify-state-execution.md) records all 40 native
oracle matches and the separately approved two-case expectation migration.

## 1. First additional boundary

Add simple state-dependent scalar paths, `if (condition) (a => q)` and their
scalar full-path equivalents, plus simple `ifnone` fallbacks. Retain the first
contract's module-instance, input-net, wire/tri-output and single ordinary
functional-driver requirements. Expand multiple scalar terminals into independent
source/destination pairs before grouping conditions.

Conditions may read integral constants/specparams and static integral variables
or nets local to the defining module, including constant bit/part selections.
Preserve pinned slang's legal module-path expression checks. Lower accepted
bitwise/logical/equality/concatenation/replication/conditional operations with
the existing value rules; the outer path-enable conversion is specified below.
Reject unimplemented condition representations with a positioned source error.
No effectful calls, automatic storage, class/container handles, runtime-selected
storage or hierarchical reads are added to the condition evaluator.

Conditional edge-sensitive paths and families mixing a fallback with
edge-sensitive declarations remain a later boundary. Existing unconditional
edge-sensitive paths keep their behavior. Vector terminals, output variables,
passive or multiple functional drivers, explicit pulse controls, SDF and timing
checks retain their existing unsupported status. These limits do not exempt
any required mechanism from P1.

## 2. Four-state path-enable conversion

Use the condition's least-significant bit. Zero disables the path; one, X and
Z enable it. Higher bits do not make a zero LSB true. Thus `2'b10` disables,
while `2'b1x` enables. This conversion differs from ordinary procedural `if`,
logical reduction and event-control `iff`; none may stand in for it.

Define this conversion once in the value reference implementation and test all
four scalar states and wider values independently. Keep the condition expression's
existing width, signedness and four-state operators until extracting its LSB.
Do not replace X/Z by zero or infer native expectations from Icarus output.
The eight recorded reference differences remain visible as differences.

## 3. Condition snapshot and fallback groups

Evaluate enabled conditions when a changed functional contribution is about to
select its path deadline, from the then-current committed storage. Do not latch
the enable at the input event. Read each condition once per selection, with no
user side effects or interleaved scheduler work. Epoch-zero evaluation occurs
after the existing static initialization, under the same initial path clocks.

A change only to a condition does not create a functional result or input
transition and does not retime an existing pending publication. Strength-only
source changes do not advance source clocks. Identical pending contributions,
returns to the committed contribution and cancellation retain the original
contract's early decisions before new path selection.

For each expanded source/destination pair within the defining instance, collect
its simple conditional paths and optional simple fallback. An `ifnone` path is
eligible only when every corresponding condition is disabled. If no conditional
path corresponds, it acts as an unconditional simple path. A zero LSB on every
condition enables fallback; X/Z on any corresponding LSB suppresses it.

Keep strict errors for illegal fallback combinations, including a fallback and
an unconditional simple path for the same pair. Reject duplicate fallback
bindings and malformed groups. Different conditional declarations for the same
pair are permitted; several may be enabled at once. A legal unconditional path
without a fallback participates alongside enabled conditional paths.

## 4. Choose the deadline using existing timing rules

Filter paths by their current enables/fallback group, then apply the existing
eligible-source clocks: choose the latest eligible source time, and the smallest
transition delay among paths at that time. The shortest path globally does not
override a newer eligible source. If no path is eligible, use ordinary functional
arrival. Conditions do not introduce new source clocks.

Use the existing twelve-transition delay table, all five delay-list forms,
min/typ/max selection, precision rounding and overflow handling. The deadline is
the maximum of functional arrival and selected source-time-plus-delay, rather
than the sum of path and distributed delays. A changed condition during a
distributed delay therefore affects selection at functional arrival.

The first contract's inertial filtering, owned complete pending contributions,
generation cancellation, Active publication, resource limits, interruption and
final cleanup remain. This extension does not add transport timing or a new
region. Selected zero/elapsed delays are visible before a module's Inactive
`#0` observation.

## 5. Ports, force, observations and ownership

Retain the private functional contribution, original physical driver and public
delayed destination. Conditions reading a local destination see its public
committed value, including any force; they do not read the hidden functional
carrier. No extra driver or hidden user-visible signal is created for a guard.

While an output is forced, functional updates and their selected pending path
deadlines continue. Later condition-only changes do not retime them. Release
observes the currently committed contribution; it does not re-evaluate a past
condition or accelerate future work. VCD/FST, monitor and VPI see the same public
snapshot. Instance and Context state, including identical module definitions
with different conditions, remain independent.

## 6. Typed SIR and kernel bridge

Extend each static path row with its enable kind (unconditional, conditional or
fallback), a numeric condition-evaluator identity where applicable, and a numeric
fallback-group binding. Groups describe an owner/source/destination pair; do not
group by source/destination text, declaration order or delay. Keep original path
identities stable through sorting and port normalization.

Front lowers a condition to a read-only static evaluator returning a four-state
one-bit LSB. It may use the existing watch/subroutine infrastructure with a
dedicated verified role. It has no user frame, arguments, writes, waits, heap
allocation, RNG or event scheduling. SIR verification checks evaluator signature,
allowed operations and local bindings, group ownership/membership, duplicate
bindings and the existing functional-driver/terminal invariants before execution.
Unsupported source input remains a frontend error, not malformed-IR failure.

The VM supplies a mechanical evaluator callback bridge for typed SIR. The shared
kernel performs enable conversion, group filtering, clock selection and timing.
Callback metadata and its storage must outlive configuration and all pending
work; configuration validates and allocates transactionally. Report failures
through existing positioned/resource error paths. Mutable state belongs to
Context, without a callback after teardown or crossing into another Context.

Retain C++20 kernel public headers, the public C ABI and dependency graph. No
runtime name lookup, new dependency, mutable global, hidden user process or RNG
owner is introduced. Designs without conditional paths retain their existing
path and performance-count behavior.

## 7. Implementation evidence required after approval

Implement in reviewable layers: independent enable/group reference, shared
kernel selection, SIR verification/text, VM bridge, then frontend and observation
integration. Test mechanisms through the same shared algorithm, including:

- Four scalar states; wider zero/one/X/Z LSBs; pure expressions and constant
  selections; local variables, nets and parameters; independent instances.
- No enabled path, orphan fallback, overlapping enabled paths, fallback
  suppression, distinct sources, latest-source selection and simultaneous ties.
- Delayed functional arrival with a condition changed before/after that arrival;
  condition-only changes during pending work; identical contributions and pulses.
- Twelve destination transitions, five delay-list sizes, zero/overflow, min/typ/max,
  initial evaluation, normal and forced public output, observations and teardown.
- Malformed evaluator/group/owner bindings, read/write and lifetime violations,
  allocation failures and Context isolation. Preserve all unaffected diagnostics.

Compile native semantic controls that use ordinary truth instead of the LSB
rule, enable fallback beside a true condition, latch the guard too early, select
the global minimum before the latest source, retime on condition-only changes,
or lose force/instance ownership. Require value/time/ownership assertions to fail;
a compiler error or mutation-registry self-check does not count. Restore source
hashes and rerun the baseline and full local CI before push.

The [three recorded controls](../measurements/2026-11-03/specify-state-checker-controls.json) only validate the external comparison
checker. They are not native mutations and do not satisfy the above requirement.

## 8. Existing expectations and completion

The original 102-case inventory, 176-probe comparison, fixed P1 population,
repository fixtures and all gate denominators remain unchanged. The new tool
mode is an independent inventory, not an additional CI pass-rate gate. A
successful inventory exit means recording completed without an independent
failure; it does not turn native unsupported cases or reference differences into
passes. Strict oracle-checking mode fails for the eight reference differences.

No existing diagnostic or timing expectation migration is approved here. Before
changing one, identify its exact input, reason, clause and proposed replacement
and obtain separate review under AGENTS.md/AI_POLICY. Keep migrations separate
from `src/` changes. Update bilingual support documents and counts from a fully
successful dev JUnit only after native verification; no implementation status or
test count is advanced by this proposal.

## 9. Edition review and reproduction

The inspected primary IEEE text remains **1800-2017**, 30.4.4.1–30.4.4.4 and
30.5.3, from the [IEEE publication hosted by MIT](https://rfsoc.mit.edu/6S965/_static/F24/documentation/1800-2017.pdf).
No text, diagram or example is copied. The sources above are original.

On 2026-11-03 the [official IEEE 1800-2023 page](https://standards.ieee.org/ieee/1800/7743/)
and [Accellera distribution page](https://www.accellera.org/downloads/ieee) were
checked again. Their GET link returned HTTP 418 and the document endpoint
requested a JavaScript robot check. **Direct 2023 clause-by-clause confirmation
remains unconfirmed.** The first contract's edition-review requirement is not
reported as complete. Maintainer review must explicitly address this limitation
before authorizing the next implementation; approval does not itself establish
that the 2023 text has been inspected. If that text requires different semantics,
amend the concrete contract before implementing the affected behavior.

```sh
python3 tools/specify_feature_probe.py --inventory state-dependent \
  --iverilog <icarus-13-compiler> --vvp <icarus-13-runtime> \
  --kirimasim build/dev/src/driver/kirimasim \
  --export build/specify-state-inputs --output build/specify-state-inventory.json
```

The default `--inventory baseline` preserves the original inputs. `--case`
selects a case within the chosen inventory. The measurement includes sources,
hashes, written oracles, actual versions and all observed outcomes. The native
binary's embedded prior commit is retained as printed; its hash and the tested
source checkpoint are recorded separately. No upstream report is posted.
