# Nightly count and mutation repairs (2026-11-02)

[日本語](nightly-repair-2026-11-02.ja.md)

## Observed failures

[Nightly run 41355215502](https://github.com/kurochan001/kirimasim/actions/runs/41355215502)
tested `2c81a591105b5b364ac9c0f106b99082987b3c51`. The Linux `p1-gates`
and macOS `svtests-macos` jobs failed at `README counts`: each measured
252 kirimasim passes out of 344, while `docs/counts.json` still held 244.
Icarus remained at 211 on Linux and 210 on macOS. The public log summaries
were measured on 2026-11-01 UTC; these are not new local measurements.
The fixed own-suite population still passed 1,456/1,466 (99.3%).

All eight mutation jobs exceeded the hosted six-hour execution limit.
Their 1,126-entry catalogue assigned 140–141 mutations per job. The logs
record partial catches, not a whole-catalogue pass. Shard 4 also reported
`DOES NOT BUILD` for `container VM: uses the formal receiver bound`:
its replacement still referenced the removed VM member `heapTypes_`.
The sanitizer, shuffled-unit, ThreadSanitizer, compiler and arm64 jobs passed.
See the [log-derived audit](../measurements/2026-11-02/nightly-failure-audit.json).
The workflow did not archive per-test sv-tests `results.json`; none is fabricated.

## Repairs

- Synchronize both sv-tests records to 252, with their actual UTC measurement
  date, and update both README progress tables and generated validation records.
  Keep the pinned population, reference commits and strict count comparisons.
  Other historical measurements and P1 completion criteria do not change.
- Divide the complete mutation catalogue into 32 nonempty jobs, with 35–36
  mutations each at this checkpoint. No mutation is omitted or duplicated;
  build failures and survivors still fail. Hosted duration after this change
  remains unconfirmed until the new workflow completes.
- Keep the formal-receiver-bound negative control, replacing its obsolete
  storage access with `heapPlan().containers()`. It still deliberately chooses
  the formal type instead of the receiver's actual type. No simulator source,
  existing test input, expected value or classification changes.

## Verification

The workflow parsed with the existing Ruby YAML library. Matrix checks assigned
all 1,126 entries exactly once to the 32 shards, with at most 36 per shard;
the catalogue's names, order and count are unchanged. Applicability checking
reported 1,126 checked and zero failures.

The actual mutation runner passed the unchanged full CTest baseline, compiled
the repaired control and caught it in
`suite/containers/container_sequence_program_actual_bounds`. Both catalogue
self-checks were excluded from the mutated test run. The runner returned zero
problems, restored the source SHA and rebuilt the unmodified source. The same
test then passed again. Local logs are ignored under `build/nightly-formal-bound-*`
in the dedicated validation worktree.

```sh
python3 tools/readme_counts.py check --build build/dev
python3 tools/mutants.py --check
python3 tools/mutants.py --preset dev \
  --only 'container VM: uses the formal receiver bound'
ctest --preset dev -R '^suite/containers/container_sequence_program_actual_bounds$' \
  --output-on-failure
```

Run mutations only in a dedicated clean-source worktree. A complete repaired
Nightly run must still verify both count jobs and all 32 mutation shards;
re-running the old revision alone does not test these repairs. No full hosted
mutation pass, seven-night fuzz streak or P1 completion is claimed here.
