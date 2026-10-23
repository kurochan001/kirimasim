# Blocking assignments with repeated event controls

[日本語](repeated-event-controls.ja.md). Implemented at `87ae3a2`, 2026-10-24.
This implements the existing approved SCHEDULER_SPEC 8 and IEEE 1800-2023
9.4.5 rules; it adds no SIR instruction or scheduling policy.

## Execution and limits

`target = repeat(count) @(event_expression) value` now executes through the
existing blocking intra-assignment path:

1. Evaluate and retain the right-hand value.
2. Evaluate the count once, retaining its width, signedness and state domain
   in a frame variable. A nonpositive or X/Z count waits for no events.
3. Wait for the specified number of occurrences using the existing named
   event, slot subscription or expression monitor. Mixed items and `iff`
   retain their existing rules; a false guard does not satisfy a wait.
4. Select the blocking target after the last wait and assign the retained
   value. With no waits this happens immediately, after count evaluation.

Front lowers the repetition to comparisons, branches, frame reads/writes
and ordinary wait instructions. It does not narrow the count to a host
word or reevaluate the source count. Each iteration registers a new wait;
an event that happened before that registration does not satisfy it.
Kernel and VM implementations are unchanged, and temporaries do not cross
waits. Container snapshots and scalar/string values use their existing
typed frame storage and collection roots.

The tested targets include fixed-array elements, packed concatenations,
automatic variables, real/string values and whole dynamic containers.
Whole fixed-array/unpacked-structure timing was unsupported at this revision;
[subsequent work implements blocking aggregate timing](blocking-aggregate-timing.md).
Event-controlled **nonblocking** assignments remain a separate
gap: they must register without stopping the issuing thread. This blocking
loop does not provide that asynchronous registration.

## Evidence

The [original matrix generator](../../tools/gen_repeated_event_matrix.py)
adds 75 source cases and a generator check. The 64 main cases cross eight
execution contexts, 1/32/65/129-bit payloads and two/four-state payloads.
They check RHS-before-count effects, a count changed during the wait, late
target selection, X/Z payloads, real/string snapshots, named/mixed/watch
events, packed concatenations and skipped counts. Eleven additional cases
cover 65/129-bit positive counts that expose truncation or a changed sign, both state
domains, narrow signed/unsigned/X/Z counts, mixed `iff`, and independent
container values across collection-safe waits.

The first 70 source cases all stop as unsupported on the previous release
binary reporting `51cafd204560`. Five further count-type cases were added
after that baseline audit. The implementation passes all 75 and the generator
check. Full dev CTest runs 2,907 tests: 2,905 pass and the two existing skips
remain. Full local CI at `87ae3a2` passes all four presets and static checks,
including clang-format, clang-tidy and expectation protection. ASan/UBSan
also run 2,907 tests (2,905 pass); release runs 2,908 (2,906 pass), with the
same two skips. Existing suite bodies, expectations and performance counts
are unchanged.

Three intentional mutations compile and are caught by actual standard-output
differences in the new source cases: narrowing the count to a signed low
word, waiting once on zero, and capturing the blocking target before the
control. Each also fails the suite with anchor/judging self-checks excluded.
The unchanged baseline passes, and restored sources rebuild before full CI.
These three executed controls are separate from the 483-anchor application check.

The installed Icarus compiler/runtime both report
`14.0 (devel) (s20260301-411-gca6b8d376)`. Of the same 75 source cases,
48 match the written output. Twenty-six do not compile there: automatic
task expression controls, package-variable assignments, `iff`, and a
container compilation failure. One compiled narrow-count case reaches its
watchdog while waiting on a one-bit X count. These failures are not treated
as independent validation of the affected forms. No other simulator source
or tests were read.

Four additional original comparison probes expand the finite inventory
from 68 to 72. The first three pass Icarus and fail the old kirimasim binary;
they pass after implementation. On the same 72 probes, Icarus-passing gaps
therefore fall from 29 to 26. The original 68 retain their 26 gaps. The fourth
probe independently starts one-bit X and Z counts without ever triggering
their event. Clause 9.4.5 requires immediate assignments, giving `7 9`;
Icarus prints `0 0`, while kirimasim prints `7 9`. This increases separately
recorded reference mismatches from six to seven; it is not an Icarus-passing
execution gap or a reason to weaken the oracle. Exact-13.0 and full parity
remain unconfirmed.

## Reproduce

```sh
ctest --preset dev -R repeated_event --output-on-failure
python3 tools/gen_repeated_event_matrix.py --check
python3 tools/icarus_feature_probe.py --kirimasim build/dev/src/driver/kirimasim \
  --extended --output build/repeated-event-icarus-probes.json --require-parity
```

The comparison still exits 1 because required execution gaps remain. Its
JSON separates versions, individual results, gaps and reference mismatches.
