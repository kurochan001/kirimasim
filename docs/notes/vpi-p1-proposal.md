# P1 VPI execution proposal

[日本語](vpi-p1-proposal.ja.md)

Status: **approved (2026-11-01)**. The maintainer explicitly approved this
complete proposal before runtime implementation. Sections 1–6 are the
operative P1 VPI amendment. The original proposal wording below is retained
as the decision record; it does not claim implemented support.
The maintainer requested VPI as the next priority. That direction does not
supply the missing detailed contract in VPI_SPEC. This draft makes those
choices reviewable before runtime code. It adds no implemented VPI support.

## 1. Scope and existing rules

P1_SCOPE advances the Icarus-required VPI subset into P1 and requires review
of missing details. VPI_SPEC currently contains headings only. Preserve the
already approved SCHEDULER_SPEC 7/17 callback regions and RUNTIME_ABI 5/9.6:
Context ownership, one synchronous simulator thread, startup once per Context,
sequential VPI Contexts, and restoration of the current-Context pointer after
foreign calls. No kernel dependency on slang, SIR or LLVM is introduced.

The first delivery covers the [23 original reference probes](../measurements/2026-11-01/vpi-reference-inventory.json):
registration, call arguments, integer/sized functions, static object lookup,
wide four-state values, fixed memory words, real/packed-string values, 64-bit
time, seven callback reasons, callback cancellation/reentry, delayed variable
writes, variable force/release, iteration, argv, finish and error queries.
All 23 pass on the explicitly selected Icarus 13.0/v13_0 compiler/runtime.
These are standard VPI uses, not a new Icarus extension or the `wire real`
exception. The original finite 176-probe inventory and fixed P1 population
are unchanged. This small inventory does not define the whole P1 VPI scope,
prove parity or make complete cocotb support an additional P1 exit criterion.
Further Icarus-supported forms remain inventory work, not exclusions.

## 2. Proposed loading and registration contract

Add a repeatable `--vpi=<file>` CLI option for an explicit shared-library path.
Relative paths are relative to the invocation directory. Load libraries in
command-line order, call their `vlog_startup_routines` entries in table order,
and retain them until all callbacks, handles and foreign calls are finished.
No environment search, implicit plugin directory, `-M`/`-m` aliases or default
plugin is introduced. A missing/unloadable library, missing startup symbol,
empty path, invalid registration or duplicate/built-in system name is an
input error (exit 2). The implementation may use the platform loader already
available on the supported Linux/macOS hosts; it installs no new package.

Provide an independently written C-compatible `vpi_user.h` for the supported
standard ABI declarations. Do not vendor another simulator's header or
implementation. Verify sizes, offsets, signedness and exported C signatures
with separately compiled plugins; do not substitute the kernel's existing
64-bit W4 for VPI's 32-bit aval/bval words. Supporting this external plugin ABI
does not change the existing kirimasim C ABI version.

Support `vpi_register_systf` for tasks and integer, sized four-state and real
functions. Register signatures before slang binds source calls, through its
existing custom-system-subroutine API, without modifying slang. An integer
function returns 32 signed bits; `sizetf` determines a positive sized-function
width during elaboration; a real function returns `real`. Capture that result
per registration, rather than asking `sizetf` on each runtime call.
`compiletf` runs once for each elaborated call site after the supported static
objects and call-argument descriptors exist, before any `calltf` or simulation
callback. It does not run once for every internal slang type-check visit.

`parse --vpi=...` and `--dump-sir --vpi=...` perform registration, signature and
compile-time checks, but never `calltf` or simulation callbacks. Registered
functions in constant-expression contexts remain a positioned unsupported
boundary in this first delivery; no arbitrary C execution is added to constant
folding. Unknown tasks/functions keep their existing diagnostics.

## 3. Functions, objects and values

The first host supplies these standard entry points:

- Registration/calls: `vpi_register_systf`, `vpi_handle(vpiSysTfCall, ...)`,
  `vpi_iterate(vpiArgument, ...)`, `vpi_scan`.
- Static discovery: `vpi_handle_by_name`, `vpi_handle_by_index`, `vpi_handle`,
  `vpi_iterate`, `vpi_scan`, `vpi_get`, `vpi_get_str`, `vpi_free_object`.
- Values/time: `vpi_get_value`, `vpi_put_value`, `vpi_get_time`.
- Callbacks: `vpi_register_cb`, `vpi_remove_cb`.
- Diagnostics/control: `vpi_printf`, `vpi_get_vlog_info`, `vpi_chk_error`,
  `vpi_control(vpiFinish, ...)` and `vpi_control(vpiStop, ...)`.

Describe supported module scopes, static integral nets/variables, real
variables, constants, fixed unpacked memories/words and system call/argument
objects. Preserve declaration names, hierarchical/generate/escaped names,
width, signedness, declared ranges, file/line and parent scope. Numeric SIR
bindings select the runtime storage. Name lookup is confined to the VPI
reflection service and never added to ordinary signal execution. An integral
net is observed through its resolved value, not an arbitrary physical driver.

Support the corresponding object types and the `vpiType`, `vpiName`,
`vpiFullName`, `vpiSize`, `vpiSigned`, `vpiFile`, `vpiLineNo`, `vpiTimeUnit`
and `vpiTimePrecision` properties where applicable. Memory/packed indices use
declared indices, including descending and negative bounds. The root-module,
child-module, net, variable, memory and argument iterators have stable source
order. An exhausted iterator returns null and releases its iteration state;
explicit release before exhaustion is also supported. A released iterator is
not reused while an outstanding handle could still identify it. Failed name
lookup or ordinary end-of-iteration returns null, not a fabricated object.

The supported value formats are scalar, 32-bit integer, vector, binary/octal/
decimal/hexadecimal/string, real and suppressed-value. Preserve all X/Z bits
and signed/width conversions under the existing value rules. VPI vector words
are 32-bit aval/bval pairs, least-significant word first; mask unused high bits.
Packed-string conversion follows the existing SV byte/NUL rules. This does
not declare dynamic SV string objects or every aggregate object supported.

`vpi_get_time` supports integer simulation ticks and scaled-real time.
Use the enclosing scope's unit for scaled time and the design's precision
for global integer ticks. Preserve the high/low halves beyond 32 bits.
Use SCHEDULER_SPEC 1's existing rounding/overflow rules for scaled delays.
`vpi_get_vlog_info` exposes an owned argv copy in original order, including
runtime plusargs, with version/product strings owned by the Context.

`calltf` runs synchronously where the source call runs. Its argument records
preserve both the argument-time value and eligible assignable object binding;
read-only expressions cannot become writable merely because they have a
numeric value. A function returns through `vpi_put_value` on its call handle.
Do not evaluate a source expression twice to obtain reflection metadata.

## 4. Writes and callbacks

No-delay variable deposits use the existing typed store and change
notification. Inertial and transport delayed writes own their value, time,
target and cancellation generation; no borrowed C buffer survives the API
call. `vpiInertialDelay` cancels superseded pending writes to the same target;
transport requests retain their scheduled updates. Detect time overflow and
invalid flags/target kinds before scheduling. A net deposit does not create
a persistent driver or bypass resolved-net publication. Its next driver
resolution may replace it. Net deposits, full resolved-net force and strength
observation are subsequent integration work; the first delivery rejects these
write modes explicitly. Their eventual implementation must use the approved
physical-network machinery, never a fabricated variable alias. The variable-
force probe does not establish net-write support.

Variable force/release follows the existing force rules: normal assignments
cannot replace the forced value, release retains a variable's last forced
value until an ordinary assignment, and net release re-resolves the network.
Do not replace force with a one-time deposit. `vpiStop` uses the existing
`--stop` policy; `vpiFinish` follows finish/final/end-callback rules. No direct
process exit from the kernel or VPI host is added.

Preserve the already approved time-callback placement:

- `cbAfterDelay` and `cbNextSimTime`: Pre-Active.
- `cbReadWriteSynch`: Post-NBA; writes cause another active-side iteration.
- `cbReadOnlySynch`: Postponed; writes and new same-slot events are forbidden.
- `cbStartOfSimulation`: before the time-zero simulation cycle.
- `cbEndOfSimulation`: once, after final procedures, including finish/quiescence.

The same-region order is registration order. Time-related requests accept
`vpiSimTime` and `vpiScaledRealTime` and reject null/suppressed time as already
specified. `cbNextSimTime` ignores its time value and observes the next time
that actually runs. It must not itself keep advancing an otherwise quiescent
simulation. `cbValueChange` accepts suppressed time/value formats.

Proposed value-change choice: publish each effective value update atomically,
then invoke its subscribers synchronously on the simulation thread before the
writer resumes. A no-delay VPI write can therefore reenter C through a value
callback before `vpi_put_value` returns, as observed in the dedicated reference
probe. Observe changes with four-state equality, not logical equality or just
the rendered value. Writes made while subscriber lists are traversed use the
existing deferred-change walk machinery; reentry must not invalidate an outer
iterator or borrowed value. Source processes awakened by the update still
execute in their existing scheduler region and do not become C callbacks.

A time/synchronization callback is one-shot. A value-change callback persists
until removed. Copy registration records, time/value-format descriptors and
scalar fields at registration; `user_data` remains the plugin's pointer.
Cancellation is effective before a pending invocation starts, including
cancellation inside another callback or self-removal. New registrations do
not join an already captured notification batch. Keep callback record storage
alive through an in-flight callback, then retire it. No hidden user process,
fork child or RNG owner is created. Callback work uses existing scheduler
counters, stop checks and resource limits.

Other callback reasons, handle kinds and API entry points remain explicitly
unsupported until inventoried and implemented. This does not relax any
already specified callback semantics or remove required P1 work.

## 5. Ownership, errors and library boundaries

The Context owns registration tables, static reflection records, iteration
state, copied write payloads, callbacks, error information and result buffers.
Opaque handles are meaningful only in their owning Context's lifetime. Check
kind, generation and current-table membership before using a live token;
never dereference plugin-supplied pointers as internal slot addresses. Releasing
a handle does not delete its HDL object. String/value return buffers are
borrowed until the next relevant VPI call/reentry; a plugin retaining them must
copy them. No automatic migration of handles across Contexts is supported.

Use the existing RUNTIME_ABI 9.4/9.6 exception for exactly one guarded
`thread_local` current-Context pointer. Restore its previous value on every
return and nested entry; there is no global default Context. The concrete
allowlist entry proposed for implementation is:
`src/kernel/vpi.cpp | currentVpiContext | 9.4 item 4 | synchronous VPI entry and restoration`.
All other mutable state remains in the Context. Startup tables in externally
compiled user fixtures follow the public C ABI; they are not host singletons.

Invalid kinds, formats, flags, phases or unsupported operations return the
API's failure/suppression result and set Context-owned `vpi_chk_error` data
with a stable diagnostic code. They never produce a successful zero result
that hides an unsupported operation. Existing forbidden scheduler writes,
time overflow and resource errors retain their current termination rules;
internal invariants remain internal errors. No exception crosses the C ABI.
The plugin's C callbacks cannot block the simulator or advance simulation
time by suspending a native stack.

Front sees plain registration/type/call-site descriptors through a narrow
adapter, not kernel headers or a kernel link. Driver connects these to the
Context. Kernel contains neither slang nor SIR dependencies. SIR carries typed
numeric external-call IDs, argument values/bindings and source positions;
verification rejects mismatched result widths and malformed writable
bindings before VM execution. The host rejects attempted writes to readonly
argument objects at the API call. VM delegates foreign calls to the common kernel
service. Loading paths and native addresses are not embedded into SIR text as
executable pointers. Future tier 2 uses the same service.

## 6. Acceptance and remaining checks

Before marking this first subset implemented, require:

- The original reference inputs to execute natively with independent semantic
  checks; do not turn loader errors or unsupported cases into PASS.
- Source-context matrices for module/interface/program, named/generate scopes,
  automatic functions/tasks and nested calls; wide signed/four-state values,
  declared memory bounds, time units and high/low rollover.
- NBA/read-write/read-only placement, effective value changes, no-change X/Z,
  reentry, callback registration/removal during dispatch, delayed cancellation,
  final/end ordering and quiescence/next-time behavior.
- Context A/B reuse, loader teardown, callback/handle lifetimes, copied C-buffer
  ownership, readonly/invalid requests, TLS restoration and C++20 headers.
- Compiled controls for these mechanisms, plus dev and targeted release/ASan/
  UBSan checks. Existing suite/performance expectations remain unchanged;
  any obsolete rejection migration is a separate review/change.

This draft refers to IEEE 1800-2023 chapter 38, 38.36 and 4.10/Table 4-1, and
the existing checked scheduler contract. Individual non-callback API subsection
numbers and behavior of unreferenced/optimized-out reference objects are
**unconfirmed**; verify against the standard during review and use explicit
retained targets in probes. The probe sources are original, and no simulator
implementation or generated simulation bytecode was read. The installed
public VPI header is used only by the external probe compiler and is not
copied here. Current cocotb integration is untested.

The official Icarus [VPI usage](https://steveicarus.github.io/iverilog/usage/vpi.html)
and [loader flags](https://steveicarus.github.io/iverilog/usage/vvp_flags.html)
were used only for invocation, not as implementation oracles.
