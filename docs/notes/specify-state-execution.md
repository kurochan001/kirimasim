# Simple state-dependent scalar specify execution

[日本語](specify-state-execution.ja.md) · 2026-11-03.

The approved [contract sections 1–8](specify-state-dependent-proposal.md) now
execute simple scalar `if`/`ifnone` module paths through typed SIR, tier 1 and
the shared kernel. The exact [two-case expectation migration](specify-state-expectation-migration.md)
was separately approved and committed without `src/` changes. No dependency,
public C ABI or P1 completion criterion changed.

## Supported behavior

Simple scalar parallel/full paths retain the first subset's local input-net,
wire/tri-output and single ordinary fixed-strength functional-driver rules.
Conditions may use local static integral values/constants and constant selections.
The condition's **LSB** enables a path for **1/X/Z** and disables it for **0**;
higher bits do not make a zero LSB true. This is distinct from procedural `if`.

Conditions are evaluated once per evaluator when a changed functional
contribution arrives. A condition-only change does not retime pending work.
An `ifnone` fallback is enabled only when all conditions for its declared
source/destination pair are disabled. Different declared ports retain different
groups even when port binding collapses them onto one physical net.

After filtering enables/fallback, the latest eligible source clock wins, with
minimum transition delay for a tie. Functional arrival and absolute path deadline
combine by their maximum. Twelve transitions, five delay-list arities,
min/typ/max, default inertial filtering and Active publication retain the first
subset's rules. Force holds the public net while underlying functional updates
continue. VPI, VCD and FST observe the same public committed storage.

The frontend emits a verified, read-only `path-condition` role. It has no user
frame or arguments and uses only static integral reads/value operations with
acyclic expression control flow. Logical terminal declaration IDs remain distinct
from physical storage IDs; scalar net views share storage without adding drivers.
The VM transports static evaluator results to the kernel, which owns selection
and scheduling. Teardown detaches evaluator metadata; already-owned pending
values can finish without invoking a destroyed interpreter. Configuration and
callback failures retain existing error/resource boundaries and Context isolation.
Public kernel headers remain C++20.

## Evidence

[Validation metadata](../measurements/2026-11-03/specify-state-execution-validation.json)
binds the implementation checkpoint, source manifests, restored native controls
and complete dev JUnit. The original reference records are unchanged.

- The original 40 inputs now satisfy all 40 native written oracles. Their
  pre-implementation comparison still records 32 Icarus 13.0 matches and eight
  X/Z-condition differences. These differences are not reference passes.
- `tools/gen_specify_state_matrix.py` supplies **322 unique source inputs**:
  317 normal executions and five expected errors (four positioned compilation
  errors and one configured runtime condition error). Two additional decoded
  FST observations bring the runner to **324 checks**, all successful.
- The source matrix includes four condition states × five delay-list sizes ×
  twelve destination transitions, eleven stimulus contexts, local public-output
  reads, delayed arrival, pending cancellation, force, aliased logical ports,
  independent instances, scalar full paths and remaining diagnostic boundaries.
- Seventeen new unit tests cover kernel/SIR/VM and public VPI/VCD observations;
  the SIR test rejects 33 malformed variants before runtime allocation. The
  focused existing/new module-path and array-port checks passed 43/43.
- [Nine compiled native controls](../measurements/2026-11-03/specify-state-native-controls.json)
  all compiled and were caught by value, time or ownership assertions: ordinary
  truth replacing LSB enabling, fallback beside an enabled condition, stale
  snapshots, identical-value retiming, evaluator-ID collapse, illicit evaluator
  frames, lost declaration identity, reversed latest-source selection and lost
  forced arrivals. Source hashes were restored and all 18 new CTests passed
  again. Registry checks and compiler failures are not counted as caught controls.
- The mutation registry checks **1,178 anchors**, with none inapplicable. This
  anchor count does not mean all 1,178 mutations were compiled in this task.
- All **121 original generated source names and bytes** are unchanged. Exactly
  the approved two classifications moved to execution: 107 normal executions
  and 14 expected positioned rejections now pass.
- Complete dev configure/build/test passes **11,847/11,847**, zero failures or
  skips. The count-only commit is `cca9e733`; historical population, sv-tests,
  Icarus 14.0 and the fixed P1 comparison were not remeasured.

The remaining release/sanitizer and static-check results are recorded after the
full local CI run in the validation metadata.

## Remaining boundaries and reproduction

Conditional edge-sensitive paths, fallback families mixed with edge-sensitive
paths, vector/select/variable/inout terminals, passive or multiple functional
drivers, explicit pulse controls, SDF and timing checks remain unsupported.
These are remaining scope boundaries, not additional P1 exceptions. The optional
`wire real` exception and all P1 gate denominators remain unchanged. This finite
matrix does not establish P1 completion.

Direct IEEE 1800-2023 clause-by-clause text confirmation remains **unconfirmed**
as disclosed in contract section 9; the inspected primary text was 1800-2017
30.4.4.1–30.4.4.4 and 30.5.3. Approval did not close that edition audit. No upstream
slang post was made and the posting hold remains in effect.

```sh
cmake --preset dev && cmake --build --preset dev
ctest --preset dev -R 'ModulePath|cli/specify' --output-on-failure
python3 tools/gen_specify_state_matrix.py --output build/specify-state-inputs
python3 tools/mutants.py --check
```

The CLI CTest runs the source matrix and decoded FST observations. Native controls
use the named mutation catalogue entries and ordinary semantic tests, with source
restoration and a baseline rerun. Before push, run `python3 tools/local_ci.py`;
keep tracked files frozen throughout builds, tests and static checks.
