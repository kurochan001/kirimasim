# Codex handoff: current P1 status and restart instructions (2026-11-02)

[日本語](CODEX_HANDOFF_2026-11-02.ja.md)

Published in English and Japanese at the maintainer's explicit request.
Local main, remote main and GitHub CI were checked at 2026-11-02 06:07 JST.
The earlier local `docs/CODEX_HANDOFF_2026-10-31.md` remains untracked history and
is not included in this publication. Start here rather than treating its older
main, counts or unsupported-feature list as current.

## 1. Read this first

- **Before publishing these documents, the implementation checkpoint was
  main = origin/main = remote main =
  `866abf4d86d11130a9946b79ea5fd19e2913b19c`.**
  The eight scalar-specify commits were fast-forwarded and pushed normally at
  the maintainer's request. No implementation remains unpushed, and neither
  this implementation nor its expectation migration awaits approval.
  Publishing this handoff adds a documentation commit after that checkpoint;
  inspect Git for the head when resuming. Publication changes no implementation,
  expectations or counts.
- **Posting to slang is on hold by the maintainer's explicit instruction.**
  Do not post until instructed to resume.
- P1 is incomplete. The finite 176-probe inventory has one required-scope gap,
  `dense_udp_coverage`; that does not mean the entire P1 backlog has one item.
- Icarus's `wire real` extension is optional for P1. Only the confirmed dependent
  `real_net` case is classified out of scope. Do not extend this exclusion to
  standard `real` functionality or `nettype real`.
- **Public documents are bilingual.** Update this English document and its
  Japanese counterpart together.

Read in this order:

1. This document.
2. If available locally, the untracked planning note
   `docs/P1_PRIORITIES_2026-10-31.md`. It is not included in a fresh clone.
   The public specifications and support inventory below provide the current
   evidence. Its 95-probe counts and descriptions such as unimplemented classes
   are historical. Codex takes over the inventory work, specify/VPI proposals
   and other work previously assigned to Claude Code.
3. [P1_SCOPE](spec/P1_SCOPE.md), [current support](P1_SUPPORT.md) and
   [scalar specify execution](notes/specify-execution.md).
4. Before extending the implementation, the
   [approved specify contract](notes/specify-p1-proposal.md) and
   [first VPI contract](notes/vpi-p1-proposal.md).

Follow the dependency, prohibition, expectation-protection and specification-
approval rules in local `AGENTS.md`. Its old project name, P0 and parse-only
status descriptions are outdated. Actual namespaces and paths use `kirimasim`;
consult README and P1_SUPPORT for implementation status. Also read `CLAUDE.md`
and [AI_POLICY](spec/AI_POLICY.md). Root `RTK.md` was absent; its referenced
contents remain unconfirmed and must not be invented.

## 2. Maintainer instructions, approvals and holds

- **Specify contract sections 1–8 are approved.** Preserve the implementation
  within that scope without requesting the same approval again. This does not
  approve unspecified semantics for conditional paths or other later work.
- **Migration of the existing `specify_path.sv` classification was separately
  approved.** After execution and the exact migration were demonstrated, the
  maintainer replied with approval. Commit `737416ef` keeps it separate from
  implementation. See the [migration record](notes/specify-expectation-migration.md).
  No repeated approval is needed.
- **The implementation's main integration and push were authorized and are
  complete.** That authorization does not cover every future push. Publishing
  this English/Japanese handoff was newly requested explicitly. For a subsequent
  distinct change, confirm the maintainer's request or authorization for it.
- **New tools/dependencies and semantics that conflict with existing
  specifications need prior approval.** Complete a concrete, reviewable proposal
  first, obtain approval, then implement.
- No already-presented concrete proposal currently awaits approval. Further
  specifications still need to be prepared and presented separately.
- Slang posting remains on hold. The presence of an issue draft is not posting
  authorization.
- Keep the maintainer's earlier holds on streaming `with`, string left-hand
  targets and E02 event assignment. If new evidence shows a conflict with
  required P1 coverage, present it concretely instead of changing policy silently.
- Do not restore `wire real` to mandatory P1 scope or add a compatibility mode
  without approval.

## 3. The latest eight commits on main

Their base is `80859f34`. Implementation, approved expectations, counts and
public documents remained separated when pushed.

1. `5baf5b9e` — 102 original specify reference inputs, checker controls and
   preparation of the detailed contract.
2. `407888e5` — Record approved specify sections 1–8 as an operative amendment
   to the formal specifications.
3. `fdc88772` — Independent valsem reference and shared-kernel scalar paths.
4. `c56e86d5` — Typed SIR, VM, slang frontend, source matrix and VPI/VCD observations.
5. `737416ef` — Separately migrate the one approved historical rejection expectation.
6. `4aa68356` — Preserve existing diagnostic walks for unrelated unsupported declarations.
7. `b62cf990` — Refresh only counts from a completely successful dev JUnit.
8. `866abf4d` — Refresh public English/Japanese documents, FAQ/glossary and
   comparison/validation records.

All use a noreply author and `Assisted-by: Codex:GPT-6`. Do not rewrite or squash
this history. Record human approval/review trailers according to AI_POLICY;
do not infer them without actual evidence.

## 4. Implemented specify scope and boundaries

The first subset covers module instances with scalar four-state input nets and
scalar wire/tri outputs driven by one ordinary functional driver. It includes
simple `=>` and full `*>` paths, multiple scalar inputs/outputs, unconditional
posedge/negedge qualification, data/polarity annotations, specparams/parameters,
min/typ/max, real/negative/zero delays, hierarchy and generate. Internal procedural
state can feed the output through one continuous assignment.

Key semantics:

- Normalize to twelve transitions in the order
  `01,10,0z,z1,1z,z0,0x,x1,1x,x0,xz,zx`. Support 1/2/3/6/12 delay values.
  Omitted X-arrival transitions use the minimum; X-departure transitions use the
  maximum. Do not substitute the ordinary three-delay gate selector.
- Capture actual four-state input changes before callbacks. Strength-only
  changes do not advance clocks. Choose the latest eligible input; at equal
  times choose the smallest transition delay.
- Within a cell, use the maximum of the absolute path deadline and distributed
  functional arrival, not their sum. Separate cascaded cells clock their paths
  from each preceding public arrival.
- Identical pending work keeps its deadline. Returning to the committed
  contribution cancels the unpublished pulse; stale timer generations cannot
  publish. Own the complete value/strength contribution instead of borrowing a
  VM temporary span.
- Due, zero and overdue path publications are Active. Do not confuse them with
  ordinary `drive.delay #0` in Inactive.
- Retain the original physical origin; never reintroduce a resolved readout as
  a new physical driver. Keep the public output separate from the private
  functional result, and align internal/external reads, monitors, `%v`, VPI and VCD.
- Force clamps the public net immediately while underlying contributions and
  pending updates continue. Release synchronously reads the current committed
  contribution, neither advancing a future deadline nor losing an elapsed one.
- Data/polarity annotations do not execute the functional computation. Do not
  execute an annotation function or consume RNG state.
- Verify numeric SIR bindings and ownership before allocating VM slots. Foreign
  net references may only occupy VPI net-binding positions; they cannot escape
  into ordinary writable references.
- Mutable state belongs to the Context. There is no added kernel dependency on
  slang/SIR, public C ABI change or new dependency. Public kernel headers remain C++20.

**Still rejected with source positions:** vector/bit-select/output-variable/inout
terminals; passive or multiple-driver outputs; tri-state/MOS/UDP functional
output drivers; conditional `if`/`ifnone`; explicit pulse controls; PATHPULSE$;
SDF; and timing checks. These are not new P1 scope exceptions.

### Implementation entry points

- `src/kernel/include/kirimasim/kernel/module_path.h` and `src/kernel/module_path.cpp`.
- Clock/publication integration in `src/kernel/sim.cpp`, `strength_drivers.cpp`
  and `strength_publication.cpp`.
- Independent endpoint reference in `src/valsem/module_path.cpp`.
- `src/sir/include/kirimasim/sir/module_path.h`, `src/sir/module_path.cpp`,
  `design.h`, `verify.cpp` and `text.cpp`.
- `src/front/lower_module_path.cpp`, `lower.cpp`, `lower_stmt.cpp` and the existing
  `lower_strength.cpp` pipeline.
- Numeric transport in `src/vm/vm.cpp`. Do not move timing semantics into
  VM-specific special cases.
- `tests/unit/module_path_test.cpp`, `module_path_sir_test.cpp` and
  `module_path_observation_test.cpp`.
- `tools/gen_specify_matrix.py`, `tests/cli/run_specify_tests.py` and `tools/mutants.py`.

## 5. Validated checkpoint and CI

**All 21 local CI steps passed for immutable `866abf4d86d1`.** Source and tracked
specifications were frozen during validation; the dedicated worktree was clean
again afterward.

- Dev: 11,447/11,447, 200.84 seconds.
- Release: 11,448/11,448, 133.92 seconds.
- ASan: 11,447/11,447, 212.13 seconds.
- UBSan: 11,447/11,447, 181.50 seconds.
- No failures/skips in any profile. Release's extra case is the existing default
  event-limit test.
- Clang-tidy 23.1.4 produced no diagnostics for all 188 translation units.
  Format, banned constructs, global-state probe, SPDX, release flags, suite files,
  per-commit expectation separation for eight commits, and counts also passed.
- Measured tools: Apple clang 21.0.0, CMake 4.4.4 and clang-format/clang-tidy 23.1.4.

The dedicated worktree is `../kirimasim-p1-class-runtime`. Its local logs/helpers:

- `build/specify-local-ci.log` and
  `build/local-ci-before-handoff-publication-20261102/`: full local CI for the
  implementation checkpoint.
- `build/handoff-publication-local-ci.log` and `build/local-ci/`: validation of
  the bilingual documentation publication candidate. Freeze that candidate and
  run full local CI; consult the push report for its final result.
- `build/specify-full-dev.xml` and `build/specify-full-dev.log`: the independent
  complete dev pass used for counts, 130.23 seconds. It is separate from the
  local CI dev rerun above.
- `build/specify-native-controls.json` and `build/specify-native-controls.log`:
  final execution of 15 mutations.
- `build/check_specify_native_controls.py`: helper that built and ran just the
  relevant targets.
- `build/specify-push-preservation.json`: protection audit before/after main
  integration and push.
- `build/local-ci-before-specify-20261102/`: the preceding local CI logs.

Files under `build/` are ignored and local to this checkout, not guaranteed in a
fresh clone. Tracked evidence is in the
[validation metadata](measurements/2026-11-02/specify-execution-validation.json)
and [15-control record](measurements/2026-11-02/specify-native-controls.json).
Every mutation compiled and was detected by semantic execution or ownership
checks. Build failures and registry self-check failures do not count. Original
source hashes were restored and both native baselines passed again.

**Distinguish GitHub CI from local CI.** Read-only API inspection at
2026-11-02 06:07 JST confirmed
[push run 41325154021](https://github.com/kurochan001/kirimasim/actions/runs/41325154021)
was `completed` with `conclusion: success`, for
`866abf4d86d11130a9946b79ea5fd19e2913b19c`. That is the implementation checkpoint;
publishing the handoff triggers a separate workflow run. On resumption inspect
the run associated with the relevant head SHA.

## 6. Comparison status and measurements not repeated

- [The original 176 probes](measurements/2026-11-02/icarus13-scalar-specify.json):
  **175 P1-evaluated, one exclusion, one required gap, two raw gaps and 25
  reference mismatches.** The required gap is `dense_udp_coverage` at the
  15-second frontend deadline. The exclusion is confirmed Icarus `wire real`
  dependency `real_net`.
- All 176 sources, written expectations and P1 classifications match the earlier
  record. Only the native result of `specify_path` changed: early 0 and late 1 as
  originally expected. Diagnostics/results of the other 175 remain unchanged.
- [The 102 specify inputs](measurements/2026-11-02/specify-execution-inventory.json):
  native matches 83, unsupported 16 and execution without a frozen oracle 3.
  Every source/oracle is unchanged. The reference has 87 matches, six differences
  and nine observations without an oracle. Do not relabel observations as native PASS.
- The native-specific matrix has **105 successful executions plus 16 expected
  positioned rejections: 121 cases**. There are 82 new unit tests. Do not confuse
  these case counts with CTest test counts.
- Dev has 11,447 tests: unit 1,217, suite 9,948, Tools 193, DumpSir 29, Context 21,
  and CLI+Perf 39; there are 176 generators. `docs/counts.json` and the generated
  README ranges are authoritative.
- The fixed P1 population (1,456/1,466), sv-tests (244 kirimasim passes out of
  344, versus Icarus 211 on Linux and 210 on macOS), seven-night fuzz evidence
  and Icarus 14.0 results retain their historical dates. They were not remeasured
  for this implementation. Do not silently replace external dates or the old
  95-probe fields in `docs/counts.json` with the current 176-case inventory.
- Total remaining P1 item count/effort and comprehensive Icarus 13.0 parity are
  **unconfirmed**. Final M8 also needs human review records and verification of
  evidence against the version being judged.

### Do not broaden the `wire real` exception

Only confirmed extension-dependent cases are excluded. Standard real variables,
real arithmetic, a single standard continuous assignment to a real variable,
and `nettype real` are not covered by this exception. It also does not newly
require `nettype real` for P1. Do not turn unsupported outcomes into PASS;
report evaluated/excluded counts separately. Crashes, hangs and internal errors
remain independent failures even in excluded cases.

### Unconfirmed IEEE edition audit

The primary timing text actually inspected for specify was IEEE 1800-2017,
chapter 30. The official 2023 body could not be retrieved; **direct clause-by-
clause comparison against the 2023 text remains unconfirmed**. The concrete
project contract in sections 1–8 is approved, but its edition audit is not
complete. Before extending semantics, inspect
[contract section 9](notes/specify-p1-proposal.md) and the official distribution.
Do not place standard text, diagrams or examples in the public repository.
The inspected PDF is local to the dedicated worktree's ignored `build/tmp/pdfs/`.

## 7. Recommended order for the next contributor

1. **Check current GitHub CI, main and the dedicated worktree.** Do not interpret
   historical branches/documents as proof that their work remains unimplemented
   or unpushed.
2. **Continue the coverage inventory.** Probe necessary standard mechanisms
   outside the 176 inputs using original, shape-specific sources. Freeze
   sources/oracles for before/after measurements and separate reference
   differences from native gaps. Do not port other projects' implementation,
   test suites or issue reproducer code.
3. **Prepare the next concrete specify contract.** Inventory conditional
   `if`/`ifnone`, vectors, output variables/multiple drivers and necessary timing
   checks. They are outside the initial implementation. Define conditions,
   X/Z, pulses, notifiers, regions, port/force, ownership, malformed IR and
   acceptance checks before requesting approval. Do not equate path conditions
   with ordinary `if`; X/Z reference differences are already recorded. Reference
   acceptance of `$setup` or another check alone does not prove timing-check execution.
4. **Prioritize needed VPI beyond the first subset, FST and remaining
   class/container/port/input boundaries.** Registration, static numeric
   reflection, variable writes and supported callbacks already work. Net writes,
   unsupported types/APIs/callbacks and cocotb remain. FST has dependencies but
   no backend. Reconcile `--compat` semantics/documentation before proposing it;
   do not invent an undocumented relaxation mode.
5. **Keep dense-UDP posting on hold.** Preserve the existing investigation and
   drafts identifying pinned slang's coverage check. Do not exclude the gap,
   modify slang or advance the dependency pin without authorization. Conduct
   any further reproducer investigation in isolation; wait for instructions on
   resuming posting or changing dependencies.
6. **Inventory correctness bugs and M8 evidence alongside feature work.** Issue
   counts and open/closed states were not refreshed here; do not present old
   counts as current.

Class execution, computed always_ff edges, named-fork disable, partial inout/
effective port kinds, some source `%p`, strength `%v`, warning options, first VPI
and scalar specify are implemented. This does not establish every form of each
feature; read the boundaries in P1_SUPPORT.

The earlier October 28 review items A/B/D–G (negative/wide `%t`, real precision
limits, out-of-bounds refs, huge allocations, `-0.0`/monitors and extra CLI
reports) are historical pending items. They were not all re-audited in this
session. Reconcile current specifications, implementation and approval history
before presenting them collectively as new approval requests.

## 8. Preserve worktrees and drafts

The usual main worktree is `kirimasim`. Preserve these seven existing local files:

- Tracked and modified: `docs/notes/slang-udp-coverage-issue-draft.md` and `.ja.md`.
- Untracked: `docs/CLAUDE_CODE_HANDOFF_2026-10-30.md`.
- Untracked: `docs/CODEX_HANDOFF_2026-10-31.md`, historical notes through section 41.
- Untracked: `docs/P1_PRIORITIES_2026-10-31.md` and `.ja.md`.
- Untracked: `docs/measurements/2026-10-31/slang-udp-coverage-standalone.json`.

Only this handoff's two language files are being published. Do not include the
seven files above or mistake their remaining status for uncommitted implementation.
**Do not use `git add -A docs`, blanket stashing or unrelated reset/clean.**
Stage files by explicit name.

The validated worktree is `../kirimasim-p1-class-runtime`, branch
`feat/p1-specify-runtime`. It was clean at `866abf4d`, matching main during
implementation validation. Publication fast-forwards that validated worktree to
the documentation candidate. Its older `class-runtime` directory name does not
mean it lacks the latest specify work. Check ownership, processes and dirty
state before reusing it; do not share it with concurrent work.

Other worktrees remain for `fix/inside-array-element-extension`,
`fix/p1-core-regressions`, array/pattern/queue expectations and old detached
comparisons. Their existence was checked with `git worktree list --porcelain`,
but current owners/activity are unconfirmed. They were not modified; do not
delete, reset, reuse or re-merge old branches without checking.

## 9. Avoid repeating these mistakes

- Freeze source in a worktree during local CI or compiled mutations. Do not
  build another profile or measure the same binary while its source is mutated.
- An applicable anchor is not a detected fault. Compile each mutation and
  require a semantic test failure, then restore source hashes and baseline runs.
  `--only` is a substring filter, not an OR expression.
- Add new expectations with evidence, but separately review and separate changes,
  deletion or reclassification of existing expectations, with reasons and standard
  clauses. Do not weaken existing assertions to pass.
- A changed total/generator count can make `tools/readme_counts` fail itself.
  If necessary, back up counts, both READMEs and both VALIDATION_COUNTS documents,
  provisionally synchronize totals, run all dev tests, and always restore the
  originals. Apply final pass counts **only from a complete successful JUnit**.
  `build/specify_bootstrap_counts.py` was the local helper; inspect it before reuse.
- Use an absolute JUnit output path. Preset-relative output may appear somewhere
  unexpected below `build/dev/`.
- Do not derive success counts from failed/interrupted runs. This batch stopped
  on a diagnostic regression, fixed it and used only the later full successful XML.
- Clang-tidy runs last in local CI. Checking new translation units early can
  reduce rework.
- Do not confuse `main/build` with another worktree's `build`. CMakeCache belongs
  to its worktree. Inspect actual binary `--version` output instead of treating
  another checkout's results as evidence for the current one.
- Do not read Icarus/Verilator implementation sources or generated bytecode.
  Slang dependency sources may be read but not modified.
- Update public documents in both languages. Keep private material, personal/
  machine data, standard text/figures/examples and commercial simulator output
  out of the public repository. Do not invent unconfirmed versions/semantics/behavior.

## 10. Restart commands

Begin with read-only checks in main:

```sh
git status --short --branch
git rev-parse HEAD origin/main
git ls-remote --exit-code origin refs/heads/main
git log -8 --oneline
git worktree list --porcelain
```

After confirming the dedicated worktree is available, use targeted checks below.
The immutable implementation checkpoint already passed full local CI; reading
this handoff alone does not require repeating it.

```sh
cd ../kirimasim-p1-class-runtime
git status --short --branch
build/dev/src/driver/kirimasim --version
ctest --preset dev -R 'ModulePath|cli/specify' --output-on-failure
python3 tests/cli/run_specify_tests.py --kirimasim build/dev/src/driver/kirimasim
python3 tools/mutants.py --check
```

When actually changing the path mechanism, the mutation entry point is below.
Do not perform other work on the same source while it runs.

```sh
python3 tools/mutants.py --only 'scalar module path:'
```

For a full dev JUnit after changes, fix the output location and establish the
count-synchronization prerequisites first:

```sh
ctest --preset dev -j8 --output-junit "$PWD/build/p1-next-dev.xml"
python3 tools/readme_counts.py check --build build/dev --junit build/p1-next-dev.xml
```

Before a subsequent push, freeze the authorized candidate and validate local CI.
The existing LLVM location used here is below; no new tools were installed.

```sh
CTEST_PARALLEL_LEVEL=8 CMAKE_BUILD_PARALLEL_LEVEL=4 \
  python3 -u tools/local_ci.py --llvm-bin /opt/homebrew/opt/llvm/bin
```

Publishing this handoff changes only its two language files, not implementation,
expectations, counts or the seven pre-existing files. Freeze the publication
candidate and run pre-push local CI. Resume with the evidence and maintainer holds
intact, without staging the other untracked documents.
