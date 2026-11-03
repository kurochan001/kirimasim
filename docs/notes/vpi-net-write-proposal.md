# VPI whole-net writes: proposed P1 integration contract

[日本語](vpi-net-write-proposal.ja.md) · 2026-11-03.

**Status: sections 1–8 approved on 2026-11-03; [native subset implemented](vpi-net-write-execution.md) on 2026-11-04.**
The maintainer explicitly approved these sections after disclosure of section 9
and the Strong-6 deposit presentation choice. This is the operative amendment
to the initial net-write rejection boundary; approval does not complete the
direct 2023 edition audit or authorize an existing expectation migration.
This proposes an amendment to [initial VPI contract 4](vpi-p1-proposal.md#4-writes-and-callbacks)
and VPI_SPEC's net-write boundary. That contract explicitly rejects net writes
in its first delivery. The existing approved scheduler, physical-network and
value rules remain authoritative; missing integration choices are made explicit
below. This proposal does not change implementation status or P1 criteria.

The [original 61-input comparison](../measurements/2026-11-03/vpi-net-write-inventory.json)
records 54 Icarus 13.0 oracle matches, four reference differences and three
observations without an oracle. At the pre-implementation checkpoint, native rejects all 61 API uses as unsupported,
although each process exits zero. The new runner does not count those as passes.
The separate [three compiled controls](../measurements/2026-11-03/vpi-net-write-checker-controls.json)
validate this external checker, not an unimplemented native feature.

## 1. First integration boundary

Admit `vpiNoDelay`, `vpiForceFlag` and `vpiReleaseFlag` for complete static
integral four-state net declarations already represented by verified numeric
reflection. Include scalar/packed declarations, collapsed port aliases and the
supported wire/tri/wired/pull/supply kinds. Reuse supported physical drivers;
do not restrict this to one continuous assignment or invent a variable alias.
A name handle and a captured argument handle must address the same physical net.

Force/release includes the already supported registered passive networks, using
[MOS/switch contract 5](mos-switch-proposal.md#5-force-release-and-transit).
Deposit initially covers only point nets with no passive connection in their
physical component; reject a passive-connected deposit before changing state.
The passive deposit observation remains required follow-up inventory, not a
new P1 exemption. Whole-net force does not imply bit/part-handle writes.

Keep net-bit/part handles, unpacked-net array words, user-defined/real nettypes,
strength-valued puts, delayed net puts, returned event handles and cancellation
outside this first integration. Existing variable writes/delays retain their
contract. These are implementation boundaries, not removals from required P1.

## 2. Validated physical capability

A net flag on an arbitrary slot must not authorize a write. Front/SIR must bind
eligible reflected nets to their validated public physical storage and registered
network, including undriven nets; VM forwards that numeric capability. Keep
language net references read-only and the existing verifier's foreign-net escape
checks. Do not allow a VPI binding to become a language variable reference.

Validate object kind, complete range, element, layout, live Context, physical
registration, capability and permitted operation before mutation. Logical aliases
share the same deposited/forced state; names never select runtime driver state.
Malformed native IR fails verification before allocating runtime storage.

The existing unit tests that fabricate unregistered net slots must remain
rejections, including rejected release preserving the caller's value. No existing
expectation migration is requested by this proposal. If integration exposes an
actual conflict, show the exact source/oracle/clause and seek separate approval.

## 3. No-delay deposits

A deposit overrides the point net's public resolved snapshot without changing
any physical driver's committed contribution or adding a persistent driver.
Retain that override until an actual committed physical driver contribution on
the net changes, even when ordinary resolution before/after that change would
produce the same result. Reexecuting a driver with an identical normalized
contribution does not clear it. Neither an unrelated signal update nor evaluation
alone clears a deposit. A pending distributed/module-path update clears it only
when its driver contribution actually commits, not when it is first scheduled.

On that invalidation, publish current ordinary resolution, not a pre-deposit
cache. Undriven nets retain a deposit until a supported replacing operation.
An existing force remains dominant: a deposit made under force has no retained
post-release effect. A successful deposit adds no process, RNG owner or timer.

**Presentation choice for review:** an ordinary logic-only deposit uses Strong
6 for its temporary public strength interval, X as `[-6,6]` and Z as `[0,0]`,
through the existing value/strength conversion. This is a public snapshot, not a
physical root. Keep `%v`, integral reads, VPI and waveform observations coherent.
Strength-format VPI input remains rejected. Direct 2023 wording for this
presentation choice has not been confirmed; approval must address that limit.

## 4. Force/release and shared ownership

A VPI force installs a held value through the same checked kernel mechanism as
HDL force, using the existing VPI owner rather than a fabricated HDL process.
Per-bit ownership remains common: a new HDL or VPI force replaces the current
owner on its range, and release removes the currently held range independently
of which entry installed it. A displaced HDL force evaluator cannot overwrite
its replacement on a later RHS change. Partial HDL forces still interact with
a whole-net VPI operation under the existing ownership rules.

Preserve physical-network contract 5: the forced node's local roots are
suppressed, the virtual Strong-6 force contribution is transported/attenuated
through existing edges, and the target's public snapshot is clamped. Remote
roots may still traverse that node. Forcing Z is not a connectivity barrier.
Do not clamp every node of the component or bypass the network solver.

Ordinary physical drivers, conduction and pending delayed/module-path work
continue behind a force. Release removes the hold and immediately resolves
current contributions before the caller resumes; it does not accelerate future
arrivals. Whole-net aliases observe the same commit. A non-null release value
receives the released snapshot in its requested supported format using existing
owned conversion buffers; a null value pointer is allowed. Do not return the
last forced value or read an input payload for release.

## 5. Publication, callbacks and regions

Use one shared kernel commit/publication path. Stage a complete multi-bit update
before notifications, including cross-node force effects. Preserve strength-only
internal notifications and ordinary four-state value-change equality. Identical
visible values do not generate ordinary value-change callbacks. Reuse source
clock capture, value/edge waiters, VPI, VCD/FST, monitor and postponed observation;
never write raw net storage behind those services.

Preserve the approved synchronous VPI callback/reentry contract and captured
subscriber batches. An effective no-delay update is visible to a callback before
`vpi_put_value` returns. Reentrant writes/cancellation cannot invalidate outer
buffers, repeat an identical notification or expose half a vector. Source
processes awakened by the commit retain their normal region. There is no new
scheduler region or hidden user process.

Compile/parse-only phases reject writes. Runtime source/start/time/read-write
callbacks use existing writable-region rules. Postponed/read-only writes fail
with the existing runtime-error policy and leave net state unchanged. For these
three no-delay modes, ignore `time_p`, including a non-null time descriptor;
do not interpret it as a delay or dereference it to validate a delay.

## 6. Failure, teardown and architecture

Preflight flags, capability, scope and input/output formats before applying an
operation. Invalid or unsupported requests retain a stable `vpi_chk_error`
diagnostic and leave drivers, pending generations, holds, deposited values and
caller release-output storage unchanged. Allocation/overflow/interrupt failures
use the existing explicit resource/end paths, retaining the first error and
never reporting success after a partial mutation.

Context owns deposit metadata, force ownership, snapshots and pending work.
Configuration and range publication must preflight allocation before commit;
no borrowed C value buffer survives a call. Reuse kernel error boundaries and
synchronous current-Context restoration. Teardown detaches bindings before
library unload; handles from another/ended Context cannot reach new state.

Keep kernel public headers C++20, main code C++23, dependency pins and the public
C ABI. Add no library, installation, runtime name lookup, mutable global or
allowlist entry. Front decides static capability, kernel decides physical
publication/ownership and value rules, and VM transports typed operands.
No source-shape shortcut or second net-resolution implementation is permitted.

## 7. Acceptance after approval

Retain all 61 original sources/oracles/observations. Expand native tests across
widths 1/32/33/65/129/257, all four states, source containers and argument/name
handles, aliases/partial HDL forces, net kinds, multiple/undriven roots,
unchanged and same-result driver updates, force/deposit precedence, pending
distributed/module paths and passive/resistive networks. Check public `%v`,
VPI callback snapshots/reentry, VCD/FST and inside/outside reads. Exercise current
and future driver arrivals while forced, no-op release and repeated force.

Add direct Context/SIR tests for forged capability, invalid range/kind, readonly
phases, allocation retry/atomicity, teardown and independent Contexts. Retain all
existing variable writes, release-result formats, scheduler and force tests.
Use independent reference expectations; the three checker controls above do not
replace native tests or compiled semantic controls.

Compile native faults for persistent-driver deposits, invalidation based only on
resolved output, invalidation on identical contributions, force-as-deposit,
cached release, lost force owner/aliases, lost pending arrivals, duplicate or
partial notifications and forged-capability acceptance. Require semantic
assertion failures after successful compilation; restore source hashes and
rerun baselines. Run affected complete profiles/static checks and, before push,
full local CI with tracked files frozen. Refresh counts separately only from a
fully successful dev JUnit. Push still needs the maintainer's request.

## 8. Reference differences and completion

Icarus 13.0 disagrees with four written oracles: a driver change whose resolved
result stays X does not clear its deposit; reexecuting an unchanged contribution
does clear it; a displaced HDL force reclaims the value; repeated identical
force/release produces extra callbacks. Preserve these as reference differences.
The proposed native behavior follows the inspected deposit rule and the
already approved owner/notification rules, not those outputs.

Two passive force observations agree with native HDL force analogues: `tran`
shows the unforced peer X and forced target 1, whereas `rtran` shows 0 and 1;
both release to 0/0. The passive deposit observation is not a portable oracle
and remains outside this first deposit implementation. All 61 native API writes
remain unsupported until implementation; process exit zero is not a pass.

Do not alter the fixed 176 probes, P1 denominators, historical population or the
optional `wire real` exception. This finite inventory neither completes P1 nor
adds cocotb as a new exit condition. Upstream slang posting remains on hold.

## 9. Primary evidence, edition limit and reproduction

The inspected primary text is IEEE **1800-2017 38.34 and 10.6.2**, from the
[IEEE publication hosted by MIT](https://rfsoc.mit.edu/6S965/_static/F24/documentation/1800-2017.pdf).
No standard text/example or other simulator source/test is copied. Existing
project rules also come from SCHEDULER_SPEC 14 and MOS/switch contract 5.

The [official 2023 page](https://standards.ieee.org/ieee/1800/7743/) and
[Accellera distribution](https://www.accellera.org/downloads/ieee) were checked
on 2026-11-03. The IEEE GET link returned HTTP 418. **Direct 2023 clause-by-clause
confirmation remains unconfirmed.** Before implementation, the maintainer must
explicitly review sections 1–8 with this limitation; approval does not itself
complete the edition audit. Check the official 2023 text when accessible and
amend the contract first if it requires different behavior.

```sh
python3 tools/vpi_net_write_probe.py \
  --iverilog "$VPI_PROBE_IVERILOG" --vvp "$VPI_PROBE_VVP" \
  --iverilog-vpi "$VPI_PROBE_COMPILER" \
  --kirimasim build/dev/src/driver/kirimasim --output build/vpi-net-inventory.json
```

Select all three installed Icarus 13.0 executables explicitly. Default exit zero
means the inventory was recorded without an independent failure. Adding
`--require-reference-oracles` exits nonzero for the four retained differences.
`--case` and `--plugin-source` reproduce bounded checker controls without editing
the baseline plugin. The native binary/source checkpoint and actual versions,
hashes, outputs and classifications are recorded in the linked JSON.

Preparation checks: all 33 existing VPI/CLI checks pass at the unchanged runtime
checkpoint. The original plugin also compiles against the independently written
project header with C11 and `-Wall -Wextra -Werror`. All 61 source/oracle/outcome
records are identical before and after the three compiled checker controls.
No `src/`, existing test expectation, dependency or count file is changed.
These checks validate the proposal and inventory, not native net-write support.
