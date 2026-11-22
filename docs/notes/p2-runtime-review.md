# P2 runtime boundary review

[日本語](p2-runtime-review.ja.md)

The P1 baseline is `241e6aaf`. The implementation reviewed here is `1fdd5b35`.
The stable inputs for P3 are a verified Design, its frame and slot descriptors,
optional analysis tables, and the kernel API. RUNTIME_ABI 4 gives the C layout,
lifetime rules and service signatures. ABI 0.5 exposes 18 versioned services.
ABI 1.0 is a P3 decision after evaluation of an emitted model.

## Calls from the VM

[The operation ledger](p2-runtime-calls.txt) contains the union of 537 qualified
kernel operation names. At P1 exit the VM used 461 names at 1,494 locations.
At the reviewed implementation it used 517 at 1,659 locations. Of the current
names, 326 have a C-table path, 22 belong to the host and 169 have a public
inline C++20 implementation. The ledger gives the entry or rationale for each
name, a source location and both counts. A zero current count identifies a
former direct call. For example, the old initialization call to `Rng::next`
now runs inside the initialization service.

The inventory uses Clang's resolved call, member-reference and declaration-reference
expressions from each `src/vm/*.cpp` translation unit, including VM headers and
included implementations. It keeps references to kernel functions, methods,
constructors and conversions, and counts each qualified name and file/line pair
once. Overloads share a row. File and ordinary strobes have separate C entries.
An explicit file display uses its captured targets. The host provides Contexts,
physical storage, run-loop entry and profiling. Inline entries cover pure value
operations, borrowed descriptors and process decisions compiled into the model.
Each host or inline choice is revisited when integrating the P3 emitter.

## Decisions shared by execution tiers

The rows follow section 2-d of the [P2 proposal](sir-v1-proposal.md#2-d-semantic-decisions-now-inside-vmcpp).
The final column names an executable witness or test family in the repository.

| Mechanism | Owner and entry used by an execution tier | Witness |
| --- | --- | --- |
| Numeric conversions, truth and time spelling | Kernel scalar_value, wide conversions and static text formatting | w4ops, wide, format |
| Initial storage and drivers | SIR frame layout, kernel initialization, slot store and signal/strength services | init_values, activation, strength |
| Time-zero order and sensitivity | SIR processPolicy and sensitiveDuringExecution, kernel startTimeZero | ff_expression, clocking, scheduler |
| Fork, join, descendants and retention | Kernel ForkState, suspendForJoin and frameRelease | C++20 model, named-fork suite |
| Postponed writes | SIR postponedAccess and effect metadata, resolved with target provenance | verify_matrix, scheduler |
| Delay, unknowns and overflow | Kernel delayValue and scheduler time limits | delay overflow and rounding suite |
| Indices, clipping and warning counts | SIR declared ranges, kernel position, inside and run diagnostics | bounds_check, wide |
| Call binding, activation and copy-out | SIR formal directions and shared frame layout, kernel FrameValue and activation store | vm_frame_value, activation_environment |
| References and roots | Kernel place, reference, container and event services. Ordinary VM access validates its private generation before use | scoped_reference, vm_event_gc, C place model |
| Containers and language diagnostics | Kernel heap operations, heapDiagnostic and objectDiagnostic | heap, class_vm_execution, C run policy |
| Step/call limits and cancellation | Kernel stepsFit, takeStep, callDepthFull, stack margin and scheduler checkpoint | C++20 model, driver limits |
| Guard/watch reads and subscriptions | SIR monitorReads and effectiveFrame, kernel bind/wait/monitor services | nba_monitor, implicit_container, monitor suite |
| Monitors and held values | Native observation and signal services and scheduler hold/release | C observation and signal models |
| Scope and task disable | Kernel planDisable, disableActivity and mayUnwind, using SIR scope and exit IDs | independent C++20 fixture, named-fork and disable suite |
| Defaults and strength resolution | Kernel fillDefault, strength graph and native resolution | strength_force_runtime, MOS, C net model |
| Wide operations | Kernel wide primitives, logicalValue, shared truth and selection | wide and valsem differential tests |
| Strings, reals and four-state selection | Kernel scalar and string services, selectString, selectReal, logicalValue | real/shortreal tests and C execution model |
| Reference writes, NBA and deferred events | SIR operand/capture descriptors, native place, process, event and observation services | event_nba_ref_window, vm_nba_event |
| Scanning and memory files | Native IO, presentation and run-policy services, typed destinations and memory order | scan, memory-file suite and C IO model |
| Built-in methods | SIR method signatures, kernel string operations and managed-event identity | string methods and managed_event |
| Aggregate presentation and display arguments | SIR projectPattern, kernel formatPattern, formatDisplayOperands and scalar formatter | pattern_sir, pattern_front and format |
| Warnings and RNG ownership | Kernel RunDiagnostics and RNG functions, SIR processPolicy selects procedure RNG | cond_x, init_values, C scalar/run policies |
| Diagnostic stack, waveforms and system calls | Front assigns scope, dump and routine metadata. Kernel formatStackTrace, task, execution and VPI services implement runtime actions | stacktrace suite, C signal/task and foreign models |

The VM still translates its private register and frame indices, moves values,
and supplies typed operands and callbacks. A saved Design carries logical
declarations, source locations, clocks, trigger/capture ranges, nominal types
and waveform metadata. Physical offsets and optimization facts are recomputed.
Analysis keys cover canonical Design bytes, producer, layout and pass/options.
Absent or mismatched tables yield unknown facts.

## Source to SIR

| Front input and lowering entry | Serialized representation and boundary |
| --- | --- |
| Elaborated definitions, names and hierarchy, lower.cpp and lower_sub.cpp | Instance, scope, name, slot, subroutine, frame and owner rows. Runtime instructions carry IDs. Unresolved or unsupported constructs produce frontend diagnostics. |
| Typed expressions, selects, aggregates and streaming, lower_expr, lower_select, lower_struct and lower_stream | Typed SSA operations, constants, ranges and permutations. Flattened aggregate storage retains source dimensions and nominal declarations where the verified operation requires them. |
| Assignment patterns and display, lower_pattern and lower_system | Construction lowers to value operations. Presentation uses PatternLayout, format.pattern and explicit typed operands, member order, enum values and tags. |
| Statements, controls and lifetime, lower_stmt, lower_capture and lower_nba | Basic blocks and typed continuations, explicit ownership/capture, event controls and named scopes. |
| Arrays, containers, classes and interfaces, lower_array, lower_container, lower_class and lower_interface | Type and class tables, allocation and reference operations, explicit bindings for the implemented interface subset. |
| Nets, ports, switches, clocks and sampled values, lower_net_ports, lower_strength, lower_module_path, lower_clocking and lower_sampled | Physical source/driver ranges, strength edges, module paths, clocking rows and sample slots. |
| System tasks, files, random sequences, waveforms and VPI, lower_system, lower_scan, lower_randsequence, lower_dump and lower_foreign | Verified task/method signatures and typed operands, lowered control flow, dump rows and foreign call sites. |

The [contract inventory](../spec/sir-v1-contract.txt) lists every serialized
record, variant, ordered field, opcode and accepted system routine. The
[P1 support inventory](../P1_SUPPORT.md) gives the supported source subsets and
positioned rejections. Broader language features remain available for later
versioned extensions.
