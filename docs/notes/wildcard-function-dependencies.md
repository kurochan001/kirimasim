# Wildcard calls and function-body dependencies

[日本語](wildcard-function-dependencies.ja.md) · 2026-11-05.

Executing a function body and collecting implicit sensitivity are separate operations;
the [wildcard-function FAQ](../FAQ.md#can-a-wildcard-function-read-a-queue-internally)
explains why a body read need not become a watch.

Implementation `86507400` allows ordinary functions called from `always @*`
to read dynamic arrays and queues in their bodies. The old admission check incorrectly treated those body reads as
implicit observer requirements and rejected otherwise executable designs.

The existing [SCHEDULER_SPEC 6.3](../spec/SCHEDULER_SPEC.md#63-the-implicit-sensitivity-of-always_comb)
already distinguishes wildcard sensitivity from comb/latch function-body
analysis (IEEE 1800-2023 9.2.2.2.2 and 9.4.2.2). This change implements that
approved rule; no new semantic amendment or expectation migration is needed.

```systemverilog
int q[$];
int input_value, observed;
function int read_value(input int n);
    return q.size() + n;
endfunction
always @* observed = read_value(input_value);
```

Only changes of `input_value` wake this procedure. The body reads the current
queue when the argument changes; queue changes alone do not execute it.
Wildcard procedures still wait at time zero. Calls through other ordinary
functions and supported static class methods follow the same rule. A changed
argument executes the body even when its result equals the previous result.
The arming path does not evaluate the function to discover dependencies.
Existing named-event `triggered` function boundaries are preserved.

## Scope and preserved boundaries

The frontend now excludes the called-body closure when checking wildcard
implicit observers, as it already excludes those reads from trigger generation.
Actual expression reads remain checked. Direct queue/array reads, a container variable
read as an actual argument, and container-reading functions under
`always_comb`, `always_latch` or continuous assignment retain their current
unsupported diagnostic. The [two earlier direct queue-size gaps](../measurements/2026-11-05/container-implicit-followup.json)
remain open. Object-dependent wildcard evaluation also remains rejected.

No kernel, VM, SIR representation, external ABI, dependency or initialization
policy changes. The source `wire real` exception is unchanged. This is not
implementation of general implicit heap observers.

## Independent validation

The [188-probe before](../measurements/2026-11-05/icarus13-star-functions-before.json)
and [after](../measurements/2026-11-05/icarus13-star-functions-after.json) use
identical sources and written oracles. Four new cases pass explicitly selected
Icarus compiler/runtime **13.0 stable (`v13_0`)** and move from native rejection
to oracle matches: queue and array function reads, transitive calls and the
no-argument case. The earlier 184 sources, oracles and native results are
unchanged. P1 evaluates **187**, excludes **one** confirmed `wire real` source,
and its measured required gaps decrease **5→1**. The remaining dense UDP
frontend limit is an independent failure. Raw gaps remain 2 and reference
mismatches 25; this finite inventory does not establish P1 completion.

The [generator](../../tools/gen_star_container_function_matrix.py) adds
**96 source tests** across module/unit/package/interface/generate/static-class
declarations, queue/dynamic-array storage, widths 1/33/65/129, and direct/nested
calls. They verify time-zero waiting, current values after growth/change/delete,
no wake on hidden body reads, unchanged-result argument changes and X/Z.
Native improves **0/96→96/96**. The separate
[source comparison](../measurements/2026-11-05/star-function-source-matrix.json)
has 48 reference matches and 48 reference compilation rejections; these
outcomes are kept separate rather than adopted as native oracles.

Four new unit tests inspect the actual numeric trigger binding, preserve
existing event-function diagnostics, retain actual
container-read diagnostics and preserve the class-observer boundary.
All **105 focused CTests** pass, including the unchanged dynamic-container CLI
diagnostics, three unchanged event-boundary checks and the generator check.

Two compiled controls are caught: restoring called-body admission causes
97 positive tests to fail; suppressing actual-container rejection fails two
boundary checks. Both compile successfully, fail semantic/diagnostic tests,
and restore the original source SHA and the 105-test baseline.
[Validation metadata](../measurements/2026-11-05/star-function-execution-validation.json)
records these controls and build/test results. No existing suite body, expected
stdout, assertion or performance budget is changed.

Unfiltered dev passes **12,245/12,245**, with no failures or skips. Release,
ASan and UBSan each pass 105 related tests; clang-tidy reports no diagnostics
in the two affected C++ translation units. Full local CI remains required
before an authorized push.

```sh
ctest --preset dev -R '^(ImplicitFront[.]|suite/sched/star_container_function_|tools/gen_star_container_function_matrix|cli/dynamic_containers$)' --output-on-failure
python3 tools/gen_star_container_function_matrix.py --check
python3 tools/mutants.py --check
```

Published records remove machine paths, including encoded paths in hex output;
classification uses original bytes. External population/sv-tests/fuzz/performance
measurements retain their original dates. Upstream slang posting remains on hold.
