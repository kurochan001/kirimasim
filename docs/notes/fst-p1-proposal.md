# First P1 FST waveform contract

[日本語](fst-p1-proposal.ja.md) · 2026-11-03.

Status: **sections 1–8 approved by the maintainer on 2026-11-03; implementation in progress**. Existing rejection expectations need their separate migration review. This is the detailed amendment required before changing
[WAVEFORM sections 1, 5 and 7](../spec/WAVEFORM.md), which currently keep FST
unimplemented and reject command-line filenames other than `.vcd`.
[CLI 2.2](../spec/CLI.md) already lists `.fst`; this proposal resolves its
implementation boundary. Dependency setup alone did not approve these semantics.

## 1. First delivery and selection

Add binary FST output selected explicitly by `--wave=<file>.fst`, with a
nonempty basename. Keep `.vcd` selection and every existing VCD golden trace.
Other extensions and bare `.fst`/`.vcd` remain input errors. The invalid-extension
diagnostic may list both accepted suffixes while retaining its existing prefix.
`parse` and `--dump-sir` accept the options but create no waveform file.

Without `--wave`, the design's existing `$dumpfile`/`$dumpvars` path continues
to write VCD, even if its filename happens to end in `.fst`. No implicit format
switch, default FST mode, new option, environment knob or external converter is
introduced. As with command-line VCD, design `$dumpfile` and `$dumpvars` calls
under `--wave` retain their existing ignored-call warnings.

## 2. One selection and timing mechanism

Share the existing numeric dump table, scope selection, alias ownership,
control queue and scheduler notifications with VCD. FST is a serialization
backend; it does not introduce a new simulation observer or alternate scheduler.

Record the static values/scopes already permitted by WAVEFORM 2, in deterministic
declaration/depth-first order. Preserve module/interface/program instance levels,
named blocks, generate/task/function scopes and unpacked-structure member scopes.
Reuse the existing VCD-kind mapping for interface/program/generate/structure
scopes. Exclude automatic variables, unpacked arrays, dynamic/class values,
parameters and private functional carriers exactly as the existing dump does.
`--wave-scope` uses the same patterns and enclosing-scope rules.

The default records the initial end-of-slot snapshot and later changed values
at slot end. `--wave-glitches` records each effective change as it happens, in
existing notification order, including several changes at one timestamp.
NBA/Re-NBA, specify publication, force/release, final procedures and interrupted
slots use the existing dump notifications and finish behavior.

## 3. FST data representation

- One logical slot has one FST handle; other declared names become aliases.
  Do not collapse distinct signals merely because their values coincide.
- Integral values use the full declared width, MSB first, retaining 0/1/X/Z
  above 64 bits. The dump contains resolved public net values, not new drivers
  or a claim of extended strength-waveform support.
- Real/realtime values use FST's binary double representation; shortreal values
  are widened from their stored float. Preserve the numeric value and signed
  zero rather than round-tripping through VCD's decimal text.
- Named events have the FST event type, no invented time-zero trigger, and one
  recorded occurrence per trigger under glitch recording. Default recording
  retains the existing once-per-slot event policy.
- Time units are the simulation precision, with monotonic 64-bit ticks. Preserve
  the run date/version metadata; retain the selected command line as a comment.
  Metadata or compression bytes need not match another writer byte for byte.

Use the already pinned standalone libfst `17f6a5d27b2f576d65b550114fbde3c035cc74a8`,
with its existing zlib pack mode and compressed hierarchy. Compression is not a
new user option. Do not update the dependency or enable writer worker threads.

## 4. Dump controls

Apply `$dumpoff`, `$dumpon` and `$dumpall` at slot end, after normal changes,
using the existing queue and no-op rules. Off writes X for integral signals,
omits real unknowns and events, and records inactive dump status. On records
active status and the current snapshot. Dump-all records the current snapshot
without inventing event triggers. An FST writer may coalesce repeated identical
non-event values; decoded state and event occurrences are the contract, not a
VCD section's textual spelling. No changes are recorded while dumping is off.

`$dumpflush` requests a writer flush; a subsequent close finalizes a readable
file. It does not promise a concurrently open FST reader sees a complete file.
Finish closes/finalizes the writer on all existing termination paths, including
finish, quiescence, runtime limits, interruption and final procedures, without
adding a fabricated final transition.

## 5. FST size limits and errors

FST `$dumplimit(n)` uses libfst's writer-accounted compressed-data limit and
its reached-limit query. Zero disables the limit. Stop permanently once the
writer reports the limit reached; do not resume if a later task changes it.
This is a **soft compressed-block limit**, not the VCD text-byte counter or a
hard final filesystem-size cap. Hierarchy, the current block and finalization
can exceed the requested size. Record the limit stop as FST comment metadata
and finalize a readable file. Do not write VCD `$comment` text into the binary
stream. This is the proposed format-specific amendment to WAVEFORM 4.

Reuse the existing dump-file warning/continue policy for creation failures and
reported I/O errors, then disable further dump writes. Never present an invalid
writer/handle as a successful dump. Check available writer error/status results.
Crashes, hangs and internal errors remain independent defects. Do not add an
exception crossing the kernel boundary or bypass existing runtime limits.

## 6. Ownership and dependencies

The Context owns one writer and numeric handle table per dump. No mutable
global/default Context, runtime environment read, signal-name lookup during
simulation, hidden process or background writer is added. Only private kernel
implementation sees the external C API. Public kernel headers stay C++20 and
the C ABI stays unchanged. The prepared dependency is reused; no new tool,
library, installation or simulator implementation source is needed.

## 7. Acceptance

Use original tests and libfst's reader API to decode hierarchy, aliases, full
four-state values, native real values, times and event occurrences. Require
written expected traces independently of the writer. Compare common scheduling
cases with existing VCD semantics, normalizing dates, identifier codes, repeated
identical snapshots and real presentation only. Do not compare binary hashes
or treat a merely openable file as correct output.

Cover 1/32/65/129/257-bit values; 2-/4-state domains; declared ranges and packed
structures; module/interface/program/generate scopes; aliases and filters;
initial/delayed/zero-time/NBA changes; events; default/glitch modes; dump controls;
soft limits/flush; force/specify visibility; all termination paths; empty selected
sets and file-open failure; sequential/parallel independent Contexts. Existing
VCD cases and PerfCount expectations must remain unchanged.

Compile semantic mutations for lost X/Z or high bits, broken aliases/filters,
wrong slot timing, lost same-tick changes/events, missing off/on snapshots and
missing finalization. Restore source hashes and rerun baselines. Run full dev,
relevant release/ASan/UBSan, the public-header check and full local CI before an
authorized push. Refresh counts only from complete successful dev JUnit.

## 8. Specification and expectation discipline

On approval, record sections 1–8 as the operative FST amendment to WAVEFORM
1–7 and CLI 2.2; the existing VCD contract remains in force. Update public
manuals/support tables in both languages only when execution is proved.

`tests/cli/run_cli_tests.py` currently expects `--wave=w.fst` with the unchanged
`display_finish.sv` to fail with exit 2. The eventual migration is exit 0, exact
stdout `hello\na`, and the ordinary finish-at-zero message, plus separate decoded
FST checks. Preserve this input and every unrelated expectation. **That migration
is not approved by approval of this contract:** demonstrate native execution and
request its separate review, then keep it separate from simulator-source changes.
Until then, the native backend can be verified through the existing public
RunOptions/DumpSetup interface while the CLI rejection stays intact.

## 9. Preparatory evidence and limits

Independent API-only probes of the existing dependency preserved 0/1/X/Z,
same-time 1→0 changes, three event occurrences (two at one time), one alias,
and off/on activity metadata. A native-double probe decoded three real values.
A requested 32-byte limit stopped at tick 3 and finalized a readable 543-byte
file, demonstrating why the compressed limit cannot promise final size ≤ n.
These are library capability checks, **not native kirimasim FST execution or
viewer-interoperability evidence**. Local probe sources/logs are ignored under
`build/fst-*` in the dedicated worktree; no dependency body or upstream test was
used as an implementation reference. Viewer interoperability and performance
remain unconfirmed until separately measured.

The class packed-property write originally considered next was rejected by
Icarus 13.0 with a property-index diagnostic. It remains standard-feature work,
but the priority policy puts confirmed reference-supported P1 work first.
