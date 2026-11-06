# Glossary

[日本語](GLOSSARY.ja.md) · [Documentation guide](README.md) · [Project README](../README.md)

Terms used in the [project README](../README.md) and the [documentation](README.md).
The [Japanese glossary](GLOSSARY.ja.md) gives the Japanese terms.

**Updated 2026-11-02.** For availability see [P1 Support](P1_SUPPORT.md); for practical questions see [FAQ](FAQ.md).
A definition does not enable an unimplemented interface.

- [Structure](#structure), [values](#values), [storage/lifetime](#storage-objects-and-lifetime), [scheduling](#scheduling), [validation](#testing-and-the-way-of-working)
- Recent terms: [typed watch](#typed-watch), [iff](#iff), [alias view](#alias-view), [physical origin](#physical-origin), [passing-reference gap](#passing-reference-gap)

## Structure

| Term | Meaning |
| --- | --- |
| slang | The SystemVerilog front end library (parsing, elaboration, type checks) that kirimasim uses unmodified, pinned to one commit; its [compatibility profiles](notes/compatibility-execution.md) are explicit options |
| <a id="elaboration"></a>elaboration | Building the design's instance tree, parameters and types from the sources; slang does it, with [frontend evaluation limits](REFERENCE_MANUAL.md#frontend-elaboration-limits) distinct from runtime limits |
| `front` | kirimasim's library that runs slang and lowers its result to SIR |
| <a id="lowering"></a>lowering | Turning slang's elaborated design into SIR instructions |
| <a id="sir"></a>SIR | kirimasim's intermediate representation: storage slots and statically typed instructions, the same whichever tier runs it ([SIR_SPEC.md](spec/SIR_SPEC.md)) |
| `valsem` | The reference implementation of value semantics: bit-by-bit integral operations and an independent container-cell model. It decides what the operators, edges and case matches mean; every fast version is tested against it |
| kernel | The execution kernel shared by every tier: scheduler, value operations, VCD/FST output and the typed heap API; typed SIR/VM heap execution, collection and heap-dependent waits are implemented; the [first VPI subset](notes/vpi-execution.md) and [whole-net writes](notes/vpi-net-write-execution.md) are connected; additional container forms and DPI-C remain pending. It is linked with the user's design, so it depends on neither slang nor LLVM |
| <a id="execution-tier"></a>tier 1 / VM | The interpreter of tier 1 (P1). For now it runs SIR directly; compiling SIR to register-based bytecode comes later |
| tier 2 | C++ generated from SIR and built ahead of time, for speed (P3) |
| <a id="context"></a>Context | The object that owns one simulation's run-time state; independent Contexts can run concurrently |
| `analysis` | Analyses for tier 2 (P3): X-reachability (where x can never arrive, so values can stay two-state), static islands, levelization (ordering combinational logic by dependence) and observability (what VPI or force can see, which must not be folded away) |
| static island | A part of the design that tier 2 is to compile as one block for speed (P3; the first rules are in CODEGEN_SPEC.md) |
| slot | A place in the kernel's store for a static variable, a net or an array of them |
| frame / frame variable | The storage of a process's or a subroutine call's automatic variables |
| W4 | The kernel's four-state 64-bit word: two planes a and b; 0 is a=0 b=0, 1 is a=1 b=0, z is a=0 b=1, x is a=1 b=1 |
| chunk | One W4 of a value wider than 64 bits, least significant first |
| P0 to P5 | The project's phases (see [Phases](../README.md#phases)) |
| <a id="ast"></a>AST | A structured representation of source syntax. Front uses slang’s syntax/semantic objects rather than rewriting source strings. |
| <a id="sir-verifier"></a>SIR verifier | Checks typed operands, ownership, control flow and restrictions before execution; successful parsing alone does not run these checks. |
| <a id="format-version"></a>formatVersion | The SIR text-format version, currently 0. The existing dump is partial and is not a complete portable saved simulation. SIR v1 is P2 work. |
| <a id="abi"></a>ABI | The binary calling/data boundary between components. The documented C ABI remains 0.3; the planned ABI 1.0 is not a current interface. |
| <a id="numeric-id"></a>numeric ID | An identifier for a slot, process, type or other runtime row. Simulation uses numeric identities; names are retained for diagnostics and observation. |


## Values

| Term | Meaning |
| --- | --- |
| <a id="four-state"></a>four-state / two-state | Types whose bits can be 0, 1, x or z (`logic`, `integer`) or only 0 and 1 (`bit`); a two-state target turns x and z into 0 |
| x / z | The unknown value and the high-impedance value of a bit |
| strength | How strongly a driver drives a net (supply, strong, pull, weak, ...), used when several drivers meet |
| <a id="resolution"></a>resolution | Working out a net's value from all its drivers and their strengths ([VALUE_SEMANTICS.md](spec/VALUE_SEMANTICS.md) 5) |
| <a id="effective-net-kind"></a>effective net kind | The net kind after eligible port collapse, decided by Table 23-1 per connected physical bit. Neighbouring bits can retain different kinds. |
| <a id="implicit-origin"></a>implicit origin | A pull/supply contribution from a net declaration. Identity-connected port bits share one such root; distinct nets joined by tran keep independent roots. |
| wired logic | Nets with AND/OR tie rules after strength comparison (`wand`, `wor`); this is not a strength-blind logical fold (VALUE_SEMANTICS 5.4) |
| side slot | A two-state slot that a gate writes beside its output: which output bits it drives on its 1 side and on its 0 side, for resolution |
| UDP | User-defined primitive: a gate described by a truth table (`primitive`) |
| override process | The process that holds a procedural continuous assignment (`assign`, `force`) |
| <a id="variable-net"></a>variable / net | A variable stores a value; a net has drivers whose contributions determine its value. `logic` describes a value type, while `wire` describes a net kind. |
| <a id="packed-unpacked"></a>packed / unpacked | Packed dimensions form one integral value; unpacked dimensions organize separately indexed elements. Supported forms and assignments differ. |
| <a id="array-extent"></a>array extent | The number of elements in a dimension, independent of its numeric bounds or ascending/descending direction. Equal extents do not imply equal source indices. |
| <a id="declaration-order"></a>declaration order | For compatible fixed-array ports, the leftmost declared element corresponds to the other declaration’s leftmost element; the last dimension varies fastest in storage. |
| <a id="alias-view"></a>alias view / physical slot | A view keeps one declaration’s bounds while referring to shared storage. Accesses translate that view to the same physical elements. |
| <a id="element-ordinal"></a>element ordinal | A zero-based position in declaration order, not a source index. Several unpacked indices flatten to one checked physical ordinal. |
| <a id="port-collapse"></a>port collapse | Joining eligible connected nets so their physical drivers resolve together. Whole-net sharing and supported static bit mappings preserve the source names/views. |
| <a id="directed-carrier"></a>implicit continuous assignment / carrier | A directed port connection used for variables, expressions and uncollapsed forms. Integral width/sign changes and explicit casts enter the destination at ordinary strong strength. Reading a resolved value is different from importing every physical driver. |
| <a id="physical-origin"></a>physical origin | One independent drive contribution. Several passive paths from the same origin do not create additional independent drivers. |
| <a id="identity-edge"></a>identity edge | A strength-preserving bit connection. Unlike a strength-reducing switch, it does not attenuate a physical origin. |
| <a id="strength-interval"></a>strength interval | The complete range of possible strength points before projection to 0/1/X/Z. The side states L/H project to X when read as an ordinary four-state value. |
| State-dependent scalar path | A supported specify `if`/`ifnone` path whose condition is sampled at functional arrival. LSB 1/X/Z enables it, 0 disables it; `ifnone` applies only when its declaration group has no enabled condition ([scope](notes/specify-state-execution.md)) |
| Clocked body and trigger evaluator | An explicit clock observes ordinary trigger/guard/watch expressions; the body may execute supported class operations. A property change alone does not trigger that body ([scope](notes/class-clocked-execution.md)) |
| <a id="module-path"></a>module path | A specify timing connection from an input port to a delayed public output, distinct from the module’s functional computation. The first scalar subset retains the original physical driver. |
| <a id="path-deadline"></a>path deadline | Absolute source-event time plus the selected transition delay. Publish at the later of this deadline and functional arrival; cascaded cells use each public arrival as their own input event. |
| <a id="inertial-delay"></a>inertial delay | A delayed driver update can be replaced or canceled by a later input change. `--report-pulses` reports removed transitions without changing the value rules. |


## Storage, objects and lifetime

| Term | Meaning |
| --- | --- |
| <a id="static-automatic"></a>static / automatic lifetime | Static storage is shared across invocations; automatic storage belongs to an activation. This is independent of whether a value has two or four states. |
| <a id="activation"></a>activation | One executing process or subroutine invocation and the lifetime/identity of its automatic storage. Retained children or refs may outlive a returned call. |
| <a id="lexical-capture"></a>shared lexical capture | A child retains access to the enclosing automatic storage rather than making an unrelated value copy. The typed supported subset is documented separately. |
| <a id="ref"></a>ref / const ref | A reference to caller storage, with its type, bounds, lifetime and permitted writes. A const ref forbids writes through that reference; it is not a value snapshot. |
| <a id="class-handle"></a>class handle / object identity | A typed reference to an object. Copying a handle aliases the same object; assigning null removes that handle, not every other reference. |
| <a id="constructor"></a>constructor | `new` initializes a new object through its class/base initialization sequence. The source execution subset supports staged construction. |
| <a id="class-static"></a>static property / instance property | A static property has shared storage for its declaring class; an instance property belongs to each object. Parameterized static storage remains unsupported. |
| <a id="virtual-dispatch"></a>virtual dispatch / receiver | A virtual call selects an override from the dynamic class of its receiver. Retaining the receiver keeps a waiting method’s object alive. |
| <a id="shallow-copy"></a>shallow object copy | Copying an object’s fields into a new object keeps nested object handles pointing at their existing objects. It is different from copying a handle. |
| <a id="gc-root"></a>GC root / retained owner | A live value, frame, pending operation or explicit runtime pin that keeps managed storage reachable. Reachability determines collection, not a source name alone. |
| <a id="safepoint"></a>safepoint | A defined execution boundary where managed values and roots have been fully published and collection is allowed by that storage contract. |
| <a id="generation-token"></a>generation token | A managed identity that includes a generation, so a stale reference cannot silently become a different object after an entry is reused. |
| <a id="managed-event"></a>managed event | An event value held as an opaque, generation-checked token in the Context's registry, as automatic events need; [GC roots](#gc-root) keep it alive while in use ([note](notes/managed-event-registry.md)) |
| <a id="dynamic-container"></a>dynamic container / element cell | Dynamic arrays/queues hold typed elements with their own copy/reference rules. Here “container” means data storage; an execution context is a different use of the word. |

## Scheduling

| Term | Meaning |
| --- | --- |
| <a id="time-slot"></a>time slot | Everything that happens at one simulation time |
| <a id="region"></a>region | One of the 17 queues of a time slot that the standard runs in a fixed order (Active, Inactive, NBA, Observed, Reactive, Postponed and others; [SCHEDULER_SPEC.md](spec/SCHEDULER_SPEC.md) 2) |
| active / reactive region set | The regions that run design code (Active, Inactive, NBA) and those that run program and checker code (Reactive, Re-Inactive, Re-NBA) |
| <a id="nba"></a>NBA | Nonblocking assignment (`<=`): its update happens later in NBA, or Re-NBA for program work |
| subscription | A process waiting for changes of some bits of a slot |
| monitor record | A kernel record that re-evaluates an expression when what it reads changes (`wait (...)`, event controls on expressions) |
| held report / flush point | A unique or priority violation or a deferred assertion is held until the Observed region and dropped if its process resumes from an event control first (the flush point) |
| deferred immediate assertion | `assert #0` or `assert final`: its action runs only once the time slot settles |
| static-init process | The process that sets static variables' initial values before time 0's procedures start |
| quiescent | The end of a run because no event is left |
| zero-time limit | The end of a run because time stopped advancing past a limit (`--max-steps`, `--max-zero-delay`, `--max-slot-events`) |
| record release | Giving back the memory of finished processes ([SCHEDULER_SPEC.md](spec/SCHEDULER_SPEC.md) 10.8) |
| <a id="blocking-nba"></a>blocking / nonblocking assignment | A blocking statement completes its assignment before proceeding; an NBA schedules an update while the issuing process proceeds. Timing controls add capture/lifetime rules. |
| <a id="typed-watch"></a>typed watch | An integral expression evaluated by a borrowed process frame and monitored for relevant dependency changes. Used by expression event controls, including the supported always_ff subset. |
| <a id="arming"></a>arming | Installing a subscription and evaluating its baseline. For supported always_ff expression events this happens before initial activity, without running the ff body. |
| <a id="iff"></a>iff guard | A condition sampled when its event occurs. A guard changing to true by itself does not turn it into a `wait` condition. |
| <a id="snapshot"></a>snapshot / capture | A retained value or target selected at the statement’s defined evaluation point. Later RHS or selector changes do not alter an already captured NBA payload. |
| <a id="event-nba"></a>event-controlled NBA (event-NBA) | An NBA whose retained update waits for an event independently of the issuing process; it is not a blocking event wait in that process. |
| <a id="program-origin"></a>program origin | The originating program and evaluation group carried through calls/forks. Program work uses Reactive/Re-NBA and its program-completion rules. |
| <a id="named-fork"></a>named fork activity | A named parallel scope and its child activity, identified by scope/activation identity. Descendants can remain associated after the parent leaves the scope. |
| <a id="simulation-wall-time"></a>simulation / wall-clock time | Design time advances by scheduled events; wall-clock time is elapsed execution time. Their limits answer different questions. |


## Testing and the way of working

| Term | Meaning |
| --- | --- |
| unsupported | A construct kirimasim does not implement yet: it is meant to stop with its position and exit code 2, not run some other way |
| mechanism matrix | A test that a generator makes over the axes of one mechanism (places, ways of reading and writing, spellings, containers), so that a fix covers the neighbors of a bug, not only its own shape |
| generator | A script `tools/gen_*.py` that writes a mechanism matrix and its expected output from the written rules; `--check` tells whether the committed files are up to date |
| container | Where a statement runs: a module procedure, a task, a function, a fork branch, an interface, a generate block, a package; a statement means the same in all of them |
| <a id="negative-control"></a>mutant / negative control | A known mistake that `tools/mutants.py` plants in the code, one at a time, to prove that a selected test set catches it. Each mutant must build; applicability alone is not detection. The configured tests run: the mutant is *caught* when some test fails, and *survives* when none does, which shows a hole in the tests. The code is put back each time. The nightly workflow runs every mutant |
| <a id="perfcount"></a>PerfCount | Regression tests on event counters (not on time), against `tests/perf_counts/expected.json` |
| `--stats=json` | The option that prints those counters |
| <a id="oracle"></a>oracle | Where a test's expected output comes from (`:oracle:` in its header: the standard, or a named simulator as evidence) |
| `:spec_check:` | Who checked a test against the standard: `ai` (an AI check) or `human` |
| P1 population | The fixed list of 1,466 tests of the own test suite that the P1 criterion counts |
| mandatory tests | A fixed list of tests that must not fail, apart from any pass rate (TEST_SUITE.md); for P1, all but the one that needs P4 must pass |
| sv-tests | The CHIPS Alliance's public SystemVerilog test collection |
| Icarus Verilog, Verilator | Other open-source simulators, run for comparison and as evidence, not as the truth |
| <a id="vpi"></a>VPI | Standard C plugin interface for registered tasks/functions, reflection and callbacks; the [first execution subset](notes/vpi-execution.md) is connected, without full API or cocotb support |
| DPI-C | SystemVerilog/C function and task call boundary; unimplemented, with required Icarus coverage in P1 |
| Whole-net capability | Verified numeric permission to write one complete public physical net declaration; a net flag or bit handle alone grants no write access ([scope](notes/vpi-net-write-execution.md)) |
| Net deposit | Temporary public point-net snapshot that leaves physical drivers unchanged and expires on an actual committed driver-contribution change; distinct from persistent force |
| VPI handles and borrowed buffers | Handles belong to their live Context; returned value/string buffers are borrowed until a related API call or reentry and must be copied for retention |
| <a id="shared-renderer"></a>shared renderer | The kernel's formatter of typed values for `%p` and `%0p`, used through the SIR `pattern.format` instruction ([note](notes/source-pattern-formatting.md)) |
| FST | A compact binary waveform format, selected by `--wave=<file>.fst`; native four-state/real/event output shares VCD selection and timing ([evidence](notes/fst-execution.md)) |
| checked, rule of this document, interpretation, not confirmed | The notes the specifications put on their statements: "checked", read and confirmed in a clause of the standard; "rule of this document", kirimasim's own choice where the standard leaves it to the implementation; "interpretation", how kirimasim reads text that allows two readings; "not confirmed", not checked yet, with how to check it |
| <a id="m8"></a>M8 | The last stage of P1: the judgment that it has ended, with the report on the comparisons, mandatory tests, nightly fuzz and human reviews ([TEST_SUITE](spec/TEST_SUITE.md)). M1 to M7 are the earlier stages. |
| <a id="feature-probe"></a>feature probe | A small original design with a written oracle, source hash and per-engine result. It tests a finite form and does not prove full language parity. |
| <a id="passing-reference-gap"></a>passing-reference gap, raw gap | A probe where the reference matches the written expectation and kirimasim does not. A raw gap is counted before approved exclusions apply. The denominator and source versions must accompany the count. |
| <a id="reference-mismatch"></a>reference mismatch | The reference’s observed result differs from the written oracle. This alone does not decide correctness: optional collapse or unspecified ordering can permit several results. |
| <a id="extension"></a>extension / compatibility mode | A behavior outside the strict standard contract and an explicit policy for accepting it. A feature in a reference tool is not automatically the default here; --compat is not exposed yet. |
| <a id="expectation-migration"></a>expectation migration | A reviewed change to an existing test’s classification/output, isolated from implementation. A newly supported historical input can keep its original source. |
| <a id="local-ci"></a>local CI / nightly | Local CI checks the chosen build/test/static policies; nightly jobs add configured long-running or external checks. Read each run’s scope and result. |
| <a id="source-binary-version"></a>source / binary revision | A checkout may be newer than its executable. Keep both `git rev-parse HEAD` and `kirimasim --version` when reproducing a result. |
| <a id="test-count"></a>test count / coverage | A CTest entry, generated source, external frozen population and feature mechanism are different units. More passing entries do not establish all P1 requirements. |
| captured strength interval / provenance | Signed endpoints before four-state projection, captured with a selected net argument. Runtime `%v` uses these typed values; a name lookup or `%b` value cannot reconstruct them ([scope](notes/strength-format-execution.md)). |
