# kirimasim Documentation Guide

[日本語](README.ja.md) · The English version is authoritative when the versions differ.

Choose a document by the task you want to complete. kirimasim is in early
development and P1 remains in progress; an interface described in a plan is
not necessarily implemented. Save the executable's `--version` output when
reproducing a result.

## Start using kirimasim

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
  dated assessment of remaining work; this audit is currently in Japanese.
- [Icarus comparison](notes/icarus-13-comparison.md) and
  [P1 progress](notes/p1-icarus-progress.md) retain measured versions,
  bounded test populations, gaps and earlier checkpoints. Their results
  apply to the recorded inputs and revisions.
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
in [AGENTS.md](../AGENTS.md).

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

Maintain English and Japanese documents together. Link to the authoritative
feature inventory or contract instead of copying its full tables into each
guide. Keep measurement dates and version identifiers attached to evidence.
When a fact is unconfirmed, state that and explain how to verify it. Record
user-facing source or option changes in the relevant manuals; retain older
measurement records with their original scope.
