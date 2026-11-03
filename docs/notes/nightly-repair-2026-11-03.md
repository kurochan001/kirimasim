# CI and Nightly repair: empty FST and SIGINT controls

[日本語](nightly-repair-2026-11-03.ja.md) · 2026-11-03.

The [CI run at `7554bb47`](https://github.com/kurochan001/kirimasim/actions/runs/41587222529)
failed three Linux/GCC UBSan tests because an empty FST passed a null buffer
to libc. The [FST adapter and regression record](fst-execution.md#empty-fst-and-ubsan-2026-11-03)
cover the repair, with dependency sources, existing expectations and sanitizer
checks retained. Post-repair Linux execution requires the next published CI run.

The [scheduled Nightly](https://github.com/kurochan001/kirimasim/actions/runs/41556717656)
tested the older `786c94bb`. Its `slang-main` failures are the three static-return
expectations already covered by the approved [dependency migration](slang-d02b7cda-update.md).
Earlier macOS failures in [run 41468624550](https://github.com/kurochan001/kirimasim/actions/runs/41468624550)
were hosted-runner acquisition failures. The scheduled run's macOS ASan, UBSan,
TSan and sv-tests jobs all succeeded when checked for this repair.

## A surviving SIGINT mutation

[Mutation shard 16](https://github.com/kurochan001/kirimasim/actions/runs/41556717656/job/125657296788)
reported `kernel: SIGINT is taken again in final procedures` as surviving.
It removes the consumed-request assignment in `Scheduler::takeInterrupt`.
Existing CLI final bodies do not cross a branch safe point; wall-clock tests
set the same latch through the separate `takeWallTimeLimit` assignment.

SCHEDULER_SPEC 16.2 already requires the first request to be taken once so
every final procedure completes. The new kernel test covers Active/Reactive
entries, a cleared/asserted request during final execution, two final procedures
with three safe points each, and independent Contexts sharing an external flag.
It exercises the existing correct implementation without changing runtime code.

The [compiled control record](../measurements/2026-11-03/nightly-sigint-control.json)
binds the source hashes: the exact surviving mutation compiles, fails the new
semantic assertions, then passes again after restoring the production header.
Catalogue self-checks are excluded from that detection. A full dev run then
passed **11,828/11,828**, with zero failures or skips. Generated counts come
from that successful JUnit; fixed RTL populations and existing expectations stay
unchanged. This local result does not replace the next hosted Nightly run.
