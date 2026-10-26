# Live runtime-process limits

[日本語](process-limit-execution.ja.md). Status: implemented and locally validated,
2026-10-27. Implementation `624123b2` is integrated into main. The
[integration record](../measurements/2026-10-27/process-limit-main-integration.json)
confirms unchanged validated code/tests and commit-level expectation protection.
The original validation record retains its measurement-time publication state.
P1 remains incomplete.

## Approved rule and implementation

Grounds: CLI 2.7 and SCHEDULER_SPEC 10.8/16.2. The numerical limit and its
failure outcome are approved project policy, not a requirement attributed to
IEEE. IEEE 1800-2023 9.3/9.5/9.6/24.3 supplies the process/fork/termination
context. No external simulator source, reproducer, standard wording/example,
new dependency or installation is used. All tests and faults are original.

`--max-procs=<n>` limits unfinished processes created at run time. The default
is 1000000; 0 disables it. Static processes created by elaboration, program
initials and internal callback processes do not consume this language-child
budget. A queued or waiting child counts from registration until it ends.
An ended child returns capacity immediately even if its record remains for
live descendants or pending reports. The limit is not a cap on retained-record
memory, static process count or function/task call depth.

`Scheduler::addChildProcess` is the single admission gate for every child.
It compares the Context-local live count before registering a process, and on
failure returns `kNone`, requests `ResourceLimit` and records the CLI 2.7 note.
Rejected registrations do not increment `proc.created`. The existing group and
program-origin inheritance is unchanged. The configuration default lives in
`kernel::Limits`; the driver obtains its default from there and passes the
selected value through the existing scheduler configuration.

`Scheduler::markProcessDone` returns capacity exactly once. Normal completion,
kill/disable, program completion and final-callback completion use that entry.
Physical record release does not change the live count. Reused records reset
the runtime-child marker, so a later static/helper process cannot return a
live child's capacity by mistake. No mutable global state is introduced.

The VM checks only the registration result. Its existing fork preparation
rollback handles `kNone`: accepted prefix children have not started, are killed,
and prepared typed frame values/environments use the existing failure
cleanup. The rejected child does not enter the scheduler. Source effects already
performed by fork-block declarations in the parent remain performed; this is
not rollback of the user's statements. The driver prints the resource-limit
note without the unrelated zero-time-limit prefix, runs `final` and returns
code 6. `--max-time-ok` does not turn it into success.

No SIR instruction/version, C function-table revision, dependency, value
semantics or performance budget changes. The public kernel header still targets
C++20. The future execution tiers use the same admission/completion operations;
there is no separate VM calculation of capacity or source-shape condition.

## Verification

The [generator](../../tools/gen_process_limit_matrix.py) adds **90 original
source designs** with AI-drafted oracles: fork joins across module, static and
automatic tasks, functions, interfaces, programs and generate scopes; limits
0/1/2 and large/default values; sequential churn; live descendants of ended
ancestors; disable; time/named/guard waits; pending reports; integral, string,
queue and event captures; parent initializer effects; and static-process
exclusion. Program report cases keep the initial alive through time 1 so that
Postponed reports drain before normal program completion.

The [CLI runner](../../tests/cli/run_process_limit_tests.py) executes those
90 designs and thirteen extra contracts: seven invalid numbers, two independent
limit-option interactions, parse/SIR-only execution and last-option precedence.
It checks full stdout, exit status, end reason, failure time, exact limit note,
absence of internal errors and the help entry. These are one CLI CTest entry
plus one generator check, not ninety additional CTest entries.

Ten independent [kernel tests](../../tests/unit/process_limit_test.cpp) cover
default/unlimited policy, static/program exclusion, rejected admission,
completion before reclamation, repeated kill/end, reused records, retained
ancestors, waiting children, program cancellation and separate Contexts.
The existing sequential/parallel/churn Context tests add one design run with
three different capacities (0/1/2). All **16 focused checks** pass, including
the existing CLI option tests. These additions create **12 new CTest entries**.

[Eight compiled faults](../measurements/2026-10-27/process-limit-controls.json)
are caught: bypassed admission, an off-by-one threshold, broken zero/unlimited
handling, counting static registrations, retaining completed capacity, double
completion, omitted driver configuration and use of failed registration by the
VM. Each fault builds; executed unit/CLI/Context results detect it. Mutation
anchor checks are excluded as detectors. Source bytes are restored exactly,
and the sixteen focused checks then pass.

[Five before/after observations](../measurements/2026-10-27/process-limit-before-after.json)
record actual binary versions and SHA-256. The old binary rejects the new option;
the new one has the expected bounded outcomes. Without the flag, and with the
new explicit unlimited value, complete design stdout and all counters match the
pre-change run for all five sources. This is behavioral/counter evidence,
not a wall-clock comparison.

The [final validation record](../measurements/2026-10-27/process-limit-validation.json)
records full local CI and refreshed population/sv-tests/original probes.
Full local CI passes **21/21 steps**: dev/ASan/UBSan each pass 6,637/6,639,
release passes 6,638/6,640, with zero failures and the same two known skips each.
PerfCount, all 812 mutation anchors, formatting, C++20 public headers and all
116 clang-tidy translation units pass. The refreshed fixed population remains
1,456/1,466 (zero failures, ten unsupported); native sv-tests remains 244/344
with no former pass lost. Both 87-probe Icarus 13/14 comparisons retain seven
gaps, four/seven reference mismatches and unchanged native statuses/stdout.
The runtime-process limit is a resource policy and does not close those
language-feature gaps.

No existing expected value, suite body, specification or PerfCount budget is
changed. Published older measurements retain their own dates and source revisions.

## Remaining P1 work

This completes the specified live-runtime-process option. It does not add class
objects, parameterized class static storage, event-controlled NBA, the remaining
container/net/timing mechanisms, VPI, FST output, or the other unfinished CLI
options. No wall-clock speed improvement, Linux run, hosted CI result or complete
Icarus parity is claimed. See the [remaining inventory](../../README.md#remaining-p1-functionality-2026-10-26).
