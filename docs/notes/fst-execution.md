# Native P1 FST waveform execution

[日本語](fst-execution.ja.md) · 2026-11-03.

Native binary FST is available through `--wave=<file>.fst`. The maintainer
approved [contract sections 1–8](fst-p1-proposal.md), then separately approved
[one existing CLI expectation migration](fst-expectation-migration.md). Backend,
driver activation and expectation migration are separate commits. No new
library, tool installation or dependency revision was needed.

## Behavior and limits

The Context owns a private libfst writer. FST shares VCD's numeric selection,
physical aliases, scope filters, control queue and scheduler notifications.
Public kernel headers remain C++20 and do not expose the dependency's C API.
The existing standalone libfst pin is
`17f6a5d27b2f576d65b550114fbde3c035cc74a8`, using zlib compression and compressed
hierarchy without writer worker threads. No scheduler or runtime name lookup
was added.

Integral values retain the full declared width and 0/1/X/Z. Real/realtime
values use native doubles, with shortreal widened from its stored float.
Default recording observes slot-end values; glitch recording retains effective
same-time changes and event occurrences. Off/on/all, flush, termination and
final procedures use the existing shared dump paths. Private specify carriers
and unsupported dynamic/class storage remain excluded.

FST `$dumplimit` is a **soft compressed-block limit**, not a hard final file-size
cap. Hierarchy, the current block and finalization can exceed it. Zero disables
the limit; reaching it stops recording permanently and adds FST comment metadata.
`$dumpflush` requests a flush; close finalizes the readable file. Creation and
reported writer errors retain the warning/continue policy. Crashes, hangs and
internal errors remain defects.

Only explicit CLI `.fst` selection activates FST. Without `--wave`, a source
`$dumpfile("name.fst")` continues to write VCD. `parse` and `--dump-sir` accept
the waveform options without creating files. Empty basenames and other suffixes
are input errors. Viewer interoperability and performance remain **unconfirmed**;
check them separately with an actual viewer and measured workloads before making
claims about them.

## Execution evidence

The [validation record](../measurements/2026-11-03/fst-native-validation.json)
binds the tested source hashes and commit boundaries. It records:

- **138 native tests**, including a 120-case matrix: widths 1/32/65/129/257,
  two-/four-state domains, slot-end/glitch modes and six source contexts
  (module, interface, program, generate, named block and static task).
- Independent reader checks of values, times, aliases, metadata, reals including
  signed zero, events, filters/empty selections, controls, limits, force/specify,
  finalization, errors and independent parallel Contexts. Common scheduling
  cases are also compared with VCD; existing VCD expectations stay unchanged.
- Complete dev **11,585/11,585**, zero failures/errors/skips, before the separate
  CLI migration. Release/ASan/UBSan each passed **178 focused tests**, covering
  the new FST tests and existing dump/VCD checks. These are focused profile
  results, not claims of full sanitizer-suite runs.
- **10/10 compiled semantic controls caught**, with source hashes and the
  138-test baseline restored. The [control record](../measurements/2026-11-03/fst-native-controls.json)
  separates successful compilation from failing assertions; build failures
  are not counted as detections. All 1,136 mutation catalogue anchors apply.
- C++20 public-header build, banned-token check and focused clang-tidy passed.
  A single documented test-only suppression preserves the reader's signed
  numeric timescale exponent; it does not disable the check globally.
- Actual CLI execution of the unchanged `display_finish.sv` returned 0, exact
  stdout `hello\na`, ordinary finish-at-zero diagnostics and readable FST with
  zero variables. Added CLI checks decode written 129-bit traces under default
  and glitch modes, and check non-execution modes and invalid filenames.
  A valid zero-variable file need not produce VCD hierarchy text in the decoder.

Reproduce after building with `ctest --preset dev -R '^(Fst|WidthsDomainsModesAndScopes/FstMatrix|cli/options|tools/fst_dependency)'`.
The reader is test-only; it adds no user-facing converter command. Final local
CI passed all 21 steps on the clean `ccded6b8` worktree: dev/ASan/UBSan each
passed 11,585 tests, release passed 11,586, with no skips, and clang-tidy passed
for all 189 project files. The validation record stores these results under
`final_local_ci`; subsequent updates touch only documentation and records.
The original 176-probe Icarus comparison, external sv-tests measurements, frozen
1,466-entry population, seven-night fuzz evidence and optional `wire real`
exception were not remeasured or reclassified by this delivery. **P1 remains
in progress.**
