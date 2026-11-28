# P4 fixed-range equal stores in generated bodies

[日本語](p4-fixed-variable-w.ja.md) · 2026-11-28 · [Raw runs, identities, counters and estimates](../measurements/2026-11-28/ibex-fixed-variable-w.json) · [Approved design and original predictions](p4-variable-write-sites.md)

## Ibex/CoreMark

The final self-assignment fix (`74623858`, executable stamped `18e183b808de`) was measured in five interleaved runs per condition. Two-state pre-W/W were **9.8152/9.6235 s** (1.95% less wall time). Four-state W/entry were 9.7958/10.2579 s (0.955×). Verilator was 0.1061 s and the two-state gate ratio was **1.103%**. The 1.40% gate requires 7.580 s, another 21.2% reduction. The accepted endpoint loads ranged from 3.77 to 8.92; 3 above-limit runs were repeated.

A separate series before the self-assignment fix showed service 20 v1 reducing two-state warm execution from **9.763 to 9.490 s** (2.80% less time, 7.26% fewer host instructions). The bypass removed **125,481,155** kernel calls from 169,336,977 admitted attempts. Each changed write still runs at its original position. The two-state result was **1.073%** of the single-thread Verilator speed. At this reference time, 1.40% requires **7.276 s**, a further 23.3% reduction. Four-state runtime was 0.971× the P4 entry build (limit 1.05×), and four-state speed was 0.981× two-state speed.

| Build | 2-state seconds | Host instructions/cycle | 4-state seconds |
|---|---:|---:|---:|
| P4 entry • `b1ad0cb9` | 9.6736 | 503,128 | 9.9604 |
| `4fa11d58` | 9.7458 | 509,309 | — |
| W off • `378d7889` | 9.7634 | 509,311 | 9.8966 |
| W on • `34ed53e8` | 9.4903 | 472,319 | 9.6725 |
| Verilator 5.054 (1 thread) | 0.1019 | 6,491 | — |

Apple M5 Max, 18 logical CPUs, 128 GiB, macOS 27.2.1, Apple Clang 21.0.0. Host Release `-O3 -DNDEBUG -ffp-contract=off`; modules `-O2 -ffp-contract=off -fno-fast-math`. Identical saved SIR and RV32IM image, 433,198 cycles, firmware exit 0 and CRCs e9f5/e714/1fd7/8e3a/e714 on every full run. Five warm rotating/interleaved runs per arm. The accepted 1-minute loads before and after were 2.20–5.34. All 55 samples in this series met the load limit. The JSON contains all samples, build/input SHA-256, all three loads and checks. Timed and diagnostic passes ran separately. Other interactive activity on this shared host was not controlled.

Cold compiler plus linker time (three fresh caches per case) went from 1.506 to 2.267 s for the two-state module and from 2.100 to 2.872 s for the two four-state modules combined. The two-state loadable file grew from 407,224 to 662,760 bytes. These builds use a single-cycle limit to isolate compilation.

### Why the pre-W record moved from 9.707 to 9.865 s

The 9.706963 s series used separate output pipes and blocking completion. The later 9.864789 s series used the exit-notified harness from `62da5eb0`. That change removed timeout polling in `coremark_profile.py`; the first series already used a separate runner. On this replay, the rebuilt `4fa11d58` gave 9.7466 s (pipes) and 9.7458 s (exit). The same pre-W executable gave 9.8593 and 9.7634 s, respectively. On pre-W, the median difference between wrapper wall and the same child's centisecond `time -lp` real was 8.7 ms with pipes, 5.6 ms with exit and 46.9 ms with the old timeout method.

The original pair retired 509,299 and 509,333 instructions/cycle (+0.007%) across the 1.63% wall-time shift. With the same exit timer the 4fa → pre-W retired instruction difference was +2/cycle. The identical-timer wall shift was +17.6 ms across revisions (original gap 157.8 ms). The only file changed under `src/` between 4fa and the pre-W source baseline was front-end randsequence lowering, bypassed by saved SIR. Executable layout, the slang pin, host activity and frequency can still change elapsed time. The P4 entry and Verilator binaries are the originals; 4fa is a rebuild from its pinned sources with a different binary hash. This replay does not isolate the historical 0.158 s to one cause. Compare the historical [4fa runs](../measurements/2026-11-28/ibex-two-islands.json) and [pre-W runs](../measurements/2026-11-28/ibex-pre-w-baseline.json).

## Binding lifetime and correctness

| Workload | Context binds | Invalidations | Canonical writes after invalidation | Equal kernel writes avoided |
|---|---:|---:|---:|---:|
| `ibex` | 1 | 0 | 0 | 125,481,155 |
| `dyn_queue` | 0 | 0 | 0 | 0 |
| `class_pool` | 0 | 0 | 0 | 0 |
| `dynarray_resize` | 0 | 0 | 0 | 0 |
| `force_release` | 0 | 0 | 0 | 0 |
| `variable_site_heap` | 1 | 0 | 0 | 64 |
| `grow-sites` | 2 | 1 | 4 | 4 |
| `force-sites` | 2 | 1 | 4 | 4 |

Ibex bound 1,136 fixed sites once, entered site-enabled bodies 26,974,775 times, and sent 43,855,822 changes to canonical write. Adding a typed heap object or resizing its dynamic array does not grow the kernel's physical slot table. The daily `dyn_queue` ran 4 million iterations. The pinned daily manifest contains no class case; `class_pool`, `dynarray_resize` and `force_release` are unmodified holdouts. Their ordinary fixed sites had zero executions and all outputs matched VM. The mixed class/array fixture ran 64 allocations with active W. The growth and live-force tests use two Contexts sharing one module. Each invalidated one binding, kept it stale after release and left the second binding active. The JSON includes attempts, canonical changes, exact inputs and output checksums.

Both state modes matched pre-W for every native counter, including 426,247,905 logical two-state writes and 200,803,958 changes. The full `--tier-check` runs matched CRCs, time, scheduling and write counts. The checker runs an independent generated counter and compares each attempted store with the VM's before-image. A deliberately missing charge fails the regression. Execute/check module caches are distinct and tested in both orders. Copying a public storage/counter table leaves the copy detached. Assigning or moving a table revokes affected live borrows. With a live VPI value-change callback, edge/iff, $monitor and normal/glitch VCD, 7/10 equal calls were skipped in two-/four-state runs; callback output, logical counters and VCD transitions matched VM. The ABI retains the service 19 v1 prefix; changing its layout requires a new service 20 version. A v1 binding remains stale until teardown. Per-site invalidation or safe rebind is open before P5.

## Separate generated-code experiments

| Private build | Median s | Range s | Instructions/cycle | Saved vs control |
|---|---:|---:|---:|---:|
| `control` | 9.4688 | 9.295–9.763 | 472,304 | +0 |
| `read` | 9.0856 | 8.990–13.957 | 460,254 | +12,050 |
| `end` | 9.2997 | 9.102–13.342 | 465,485 | +6,819 |
| `read-end` | 8.9350 | 8.886–13.717 | 453,052 | +19,253 |
| `dynamic` | 9.2047 | 9.168–9.686 | 456,541 | +15,764 |
| `inline` | 9.3528 | 9.256–9.489 | 471,009 | +1,295 |

The six private modules cover all generated bodies on this workload; the earlier address/ending estimate counted only ordinary CombAt. They share the W host, fixed sites and otherwise identical compile flags, including `-DNDEBUG` for access to the internal C++ layout. Each used the same five-run procedure and matched output and all native counters. `read` inlines the fixed address lookup, `end` inlines the same termination flag test at its original position, and `read-end` combines them. The probes counted 1066.41 fixed reads and 812.10 post-effect tests per cycle. Remaining module calls were 0.00 indirect reads and 0.00 indirect ending checks per cycle. These experiments use the live Context layout directly; a released implementation would require a reviewed, portable ABI and checker support. The guard that ends simulation synchronously left the following write unexecuted in VM and all three variants, in both state modes. The native zero-delay ring handled SIGINT at 1 ns with the same final output and exit 130.

`dynamic` adds equality checks after canonical index/bounds/clipping resolution at the five emitted dynamic partial stores. 160.49 attempts/cycle, 145.14 equal, 15.35 changed, 0 guard declines. Fully invalid selects issue no write. A separate VM comparison covered unknown, negative, clipped and out-of-range selectors in both modes. `inline` separately forces inlining of the existing equality helper. The main build includes only reviewed fixed sites.

### Cost of a changed CombAt write

Matched empty indirect calls on the unmodified W host gave **420.7 instructions/full change and 441.5/slice**, compared with 419.6 and 441.6 before W. Three independent seeds sampled one in about 1,024 canonical writes. In a second temporary host, one in about 2,048 ordinary writes was selected. Three seeds gave these median per-call instruction estimates for ABI/validation, mask/store, observer inspection, subscription matching, wake/queue, guards/monitor and residual, in order.

| Change | Seven categories in order | Instrumented inclusive | Fixed arms visited / wakes |
|---|---|---:|---:|
| Whole | 76.5 / 93.7 / 148.5 / 85.9 / 95.2 / 0.0 / 26.4 | 526.2 | 1.02 / 0.60 |
| Slice | 76.2 / 94.0 / 157.3 / 147.6 / 43.8 / 0.0 / 31.9 | 550.7 | 2.25 / 0.32 |

Stage probes change the generated host machine code. After empty-window correction the instrumented inclusive costs exceed the pristine samples by 105.6/109.2 instructions. Residual reconciles the sum only to the instrumented host. Observer dispatch and repeated matching deserve investigation, especially slice matching. Matching readers, synchronous observers and ordered wakeups remain necessary on real transitions; the measurements do not establish an amount that can be removed safely.

## Remaining gate budget

| Increment / comparison | Low instructions/cycle | High instructions/cycle |
|---|---:|---:|
| W | 36,978 | 36,996 |
| W → W + island 4 WS | 36,848 | 91,980 |
| W + island 4 WS → + island 0 | 19,385 | 48,446 |
| W + island 4 WS + island 0 | 93,212 | 177,422 |
| 1.40% (historical model) | 123,704 | 123,704 |

W is the observed min/max of five full-workload paired instruction deltas. The two island rows are conditional projections using [the earlier exclusive cost model](p4-ibex-island-plan.md). Low credits none of the islands' write savings a second time, then saves 45% of scheduling, 30% of wakes and 15% of NBA issue. High assumes no overlap and fractions of 70%, 55%, 55%, 40% for scheduling, writes, wakes and NBA. The combined projection spans the reference budget. A combined measured delta awaits the island builds. Hold ordinary W fixed in all four island 4 experiments (00/W/S/WS) before deciding on island 0.

Local CI passed all 23 checks (dev 14,045, Release 14,046, ASan 14,045, UBSan 14,045 tests).

## Reproduction

Use [the pinned design, firmware and reference](p4-performance-profile.md) and Release builds identified in the raw record. Each simulated arm uses `--tier=native --state=2|4 --max-scc-iter=65536 +cycles=443198`. One clean output directory keeps caches per binary/state; `--resume` retakes high-load rows. Run on a quiet host. `--stats=json` and `--tier-check` are separate diagnostic runs. Private machine-counter probes use macOS `thread_selfcounts`.

```sh
python3 tools/compare_ibex_w.py --entry "$ENTRY" --historical "$BUILD_4FA" \
  --before "$PRE_W" --after "$W" --reference "$VTOP" --sir "$SIR" \
  --firmware "$RAM" --cxx=/usr/bin/c++ --out build/replay-ibex-w
"$W" --tier-check --state=2 --max-scc-iter=65536 --load-sir="$SIR" \
  --stats=json +image="$RAM" +cycles=443198
"$W" --tier=native --state=2 tests/context/designs/variable_site_heap.sv
ctest --test-dir build/release -R 'static_islands|emitted_island' --output-on-failure
python3 tools/local_ci.py
```
