# kirimasim Documentation Guide

**P1 policy update (2026-11-01):** Icarus's `wire real` extension is optional.
Only the reviewed `real_net` source is out of scope: the current 176-probe inventory
has 175 P1-evaluated cases, one exclusion, two required-scope gaps and three raw gaps.
Unsupported results and independent faults remain recorded. Standard `real`
features retain their existing scope. See the [decision and checks](notes/p1-wire-real-exception.md).


[日本語](README.ja.md) · The English version is authoritative when the versions differ.

**Approved combined checkpoint (2026-11-01):** class observers and partial inout ports now have their three approved expectation migrations applied. Full dev passes 11,299/11,299 without exclusions; the frozen 176-probe Icarus 13.0 comparison retains three gaps and 25 separate reference mismatches. See the [integration record](notes/p1-approved-class-inout-integration.md). Earlier checkpoint measurements below retain their dates and populations.


Choose a document by the task you want to complete. kirimasim is in early
development and P1 remains in progress; an interface described in a plan is
not necessarily implemented. Save the executable's `--version` output when
reproducing a result.

**Current implementation checkpoint (2026-11-01):** implementation `85535e72` includes
class objects, named-fork disable, always_ff expression events and static
array-element ports within their listed limits. Start with the [41-question FAQ](FAQ.md)
or [102-term glossary](GLOSSARY.md), then use [P1 Features](P1_FEATURES.md)
for executable examples. Earlier benchmark/population records keep their own dates.
[Effective partial-port net kinds](notes/port-kind-execution.md) now include wired/pull/supply kinds per bit; its six-input historical expectation migration was approved and applied separately in `3987246b`.


## Start using kirimasim

- **Read common questions:** [FAQ](FAQ.md), including current limitations,
  supported interfaces and how to contribute.
- **Look up a term:** [Glossary](GLOSSARY.md), covering architecture,
  values, scheduling and testing.
- **Build and run a first design:** [User Manual](USER_MANUAL.md).
  It covers prerequisites, a counter example, inputs, VCD, limits and
  troubleshooting. The runnable sources are in
  [examples/user_manual](examples/user_manual/).
- **Move an existing simulator command:** [Porting Guide](PORTING.md).
  It explains input options, compile/run workflows, output comparison and
  restrictions when coming from Icarus Verilog, Verilator or Lyra.
- **Look up an option or exit code:** [Reference Manual](REFERENCE_MANUAL.md).
  It includes accepted syntax, defaults, runtime behavior, system-task
  families and unimplemented interfaces.
- **Find a language example:** [P1 Features](P1_FEATURES.md).
  Its examples include original source, commands, expected design output
  and feature-specific restrictions.

Suggested first reading: User Manual → Porting Guide if needed → Reference
Manual for individual questions.

## Check whether a design can run

- **Reproduce and interpret a validation result:** [Validation Guide](VALIDATION.md).
  It distinguishes local CI, reference comparisons, external populations,
  feature probes, negative controls and performance evidence.
- [P1 Support](P1_SUPPORT.md) is the implemented-feature inventory. Check the
  exact source form, type, storage lifetime and execution context; a
  frontend-accepted construct may still lack executable support.
- [P1 Scope](spec/P1_SCOPE.md) defines the required coverage target.
  It is a completion requirement, not a claim that the target has been met.
- [P1 criteria audit, 2026-10-28](P1_CRITERIA_GAPS_2026-10-28.md) records a
  dated assessment of remaining work.
- [Combination gaps, 2026-10-30](P1_COMBINATION_GAPS_2026-10-30.md) records
  34 original probes of event, ref, aggregate, scheduled-assignment, streaming
  and I/O combinations.
- [Icarus comparison](notes/icarus-13-comparison.md) and
  [P1 progress](notes/p1-icarus-progress.md) retain measured versions,
  bounded test populations, gaps and earlier checkpoints. Their results
  apply to the recorded inputs and revisions.
- [Recorded validation counts](VALIDATION_COUNTS.md) separates the repository test counts
  from external measurements, with each date and reference build retained.
- [Status history](STATUS_HISTORY.md) keeps the dated checkpoints, probe
  counts and validation records that the README listed earlier; the README
  itself gives the current status.
- [Performance measurements](BENCHMARKS.md) explains the measured designs,
  machines, dates and reproduction commands. Operation counts and elapsed
  time answer different questions.

For a failure, start with the User Manual's
[troubleshooting section](USER_MANUAL.md#troubleshoot-a-design), then reduce
the source with the [Debugging Guide](DEBUGGING.md) and check the support inventory.
The debugging guide includes SIR inspection and intentionally failing examples.
The repository's
[issue forms](../.github/ISSUE_TEMPLATE/) identify the information to report.

## Develop and maintain kirimasim

Read [CONTRIBUTING](../CONTRIBUTING.md) for participation and review rules,
then the [Development Guide](DEVELOPMENT.md) for a worked source-to-kernel
trace and a feature implementation workflow. The
[Architecture](ARCHITECTURE.md) describes the library boundaries, dependency
directions and shared execution model. Repository-specific agent rules are
in [CLAUDE.md](../CLAUDE.md).

Detailed contracts are grouped by the boundary being changed:

- **Language values and scheduling:** [Value Semantics](spec/VALUE_SEMANTICS.md),
  [Scheduler](spec/SCHEDULER_SPEC.md) and
  [Unspecified Behavior](spec/UNSPECIFIED_BEHAVIOR.md).
- **Intermediate representation and runtime:** [SIR](spec/SIR_SPEC.md) and
  [Runtime ABI](spec/RUNTIME_ABI.md).
- **User-facing services:** [CLI](spec/CLI.md),
  [System Tasks](spec/SYSTEM_TASKS.md) and [Waveform](spec/WAVEFORM.md).
- **Validation:** [Test Suite](spec/TEST_SUITE.md),
  [Performance](spec/PERFORMANCE.md),
  [PerfCount maintenance](../tests/perf_counts/README.md) and
  [oracle audit](review/oracle_audit.md).
- **Later or incomplete interfaces:** [Code Generation](spec/CODEGEN_SPEC.md),
  [Randomization](spec/RANDOMIZE_SPEC.md),
  [Assertions and Coverage](spec/ASSERTION_COVERAGE_SPEC.md),
  [Library Export](spec/LIB_EXPORT_SPEC.md) and [VPI](spec/VPI_SPEC.md).
  Consult implementation status before using an interface from these plans.

## Read evidence and proposals

[notes/](notes/) contains proposals, approval records, implementation notes
and investigation reports. A proposal's status and its link to the adopted
specification matter; its filename alone does not establish approval or
implementation. [measurements/](measurements/) holds dated raw evidence.
Follow the corresponding note for the source revision, options, comparison
versions and interpretation.

Use the current support inventory for available behavior, the detailed
specification for the agreed contract, and measurement records for what a
particular run established. Report a discrepancy between them rather than
silently treating a historical observation as the current contract.
The [phase mapping](notes/phase-renumbering.md) explains older phase labels.

## Project policies and documentation maintenance

[CHANGELOG](../CHANGELOG.md) records user-visible behavior changes.
[Security](../SECURITY.md), [Code of Conduct](../CODE_OF_CONDUCT.md),
[AI Policy](spec/AI_POLICY.md) and the [license](../LICENSE) cover the
corresponding project policies.

Maintain complete English and Japanese versions of all public prose documents,
including CHANGELOG, dated audits and public report drafts. Use `X.md` and
`X.ja.md`, with reciprocal language links; translate tables, examples, footnotes
and reproduction instructions as well as the main text. Public issue forms
provide both languages in the same form. Private untracked handoffs/drafts are
outside this set. Code identifiers, raw diagnostic/output evidence and data
schemas stay verbatim; explain them in both languages. Canonical legal texts
remain unchanged, with clearly identified reference translations.

Keep the README focused on current implemented behavior, limits and build/run
entry points. Move chronology and superseded measurements to bilingual history,
with their original revisions and dates. Keep the generator's current README
summary and [dated records](VALIDATION_COUNTS.md) synchronized through
`tools/readme_counts.py`; do not relabel an old measurement as current.

Maintain English and Japanese documents together. Link to the authoritative
feature inventory or contract instead of copying its full tables into each
guide. Keep measurement dates and version identifiers attached to evidence.
When a fact is unconfirmed, state that and explain how to verify it. Record
user-facing source or option changes in the relevant manuals; retain older
measurement records with their original scope.
