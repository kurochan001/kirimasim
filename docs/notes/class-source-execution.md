# Class runtime: source execution

[日本語](class-source-execution.ja.md)

The frontend now connects source class objects to the [VM execution layer](class-vm-execution.md),
under the [approved contract](class-runtime-proposal.md). This closes the basic
object/construction lowering boundary, not the entire class contract or P1.

## One canonical graph and one producer

`Lowerer::discoverClasses` discovers the referenced canonical class closure
before executable lowering. IDs follow deterministic encounters, never map
iteration. Placeholder rows precede recursive fields, bases and interfaces.
Aliases share a row; distinct parameter specializations retain distinct rows.
Required constructors/virtual bodies and called methods extend the closure;
static initializers remain discoverable in otherwise unused routines. Static-only
calls and unused automatic signatures do not invent object layouts or force
unsupported instance fields into execution.

`classOf` records declaring fields, dimensions, const/initializer flags and
inheritance. Existing static slots retain their identity. Instance syntax for static properties aliases the original declaring storage, including fixed arrays, records, containers and ref arguments. Receiver effects are evaluated once without dereferencing null. Constructors enter
base construction, their own declaration initializers, their body and stage
completion. Leading local declarations of an explicit `super.new` retain their
place. Fixed-array initializers provide all elements in declaration order.
Once-only instance const stores use the constructor capability, not an ordinary
property write. Runtime enforcement remains in the shared kernel.

`classPlace` captures a receiver and indices once, and the ordinary `placeOf`,
load and store paths use numeric field IDs. Hidden fields retain their declaring
class. Ordinary call lowering binds input/output/inout/ref/const-ref parameters,
returns and copyback; the receiver is retained before argument effects. Forks
import the same implicit receiver binding and can outlive the declaring method.

Virtual calls use global signatures and dynamic tables. Pure interface methods
supply signatures without fabricated executable bodies. `super` and explicit
class qualification select direct calls; package qualification of a variable
does not suppress dispatch. Virtual task slots use a waiting ABI, including
implementations that do not themselves wait; suspension propagates to callers.
Covariant results pass through the slot's declared return view and then the
statically selected result view. Possible implementations contribute to the
ordinary call-effects graph.

Copying uses `object.copy`, preserving the dynamic class without constructors.
Typed null, equality, truth and conditional merging use the existing class
semantics. Function-form `$cast` captures the destination reference before the
source, keeps failed destinations unchanged, and distinguishes typed/literal
null. `srandom` calls the object-seed operation. Class elements in supported
queues/dynamic arrays retain their typed leaves and shared-kernel ownership.

## Verification and preserved limits

`gen_class_object_matrix.py` generates 374 original source programs covering
compilation-unit/module/package/interface/program/generate declarations,
static/automatic handles, widths 1/32/65/129/257 and two/four-state values,
fixed property elements, real/shortreal/string fields, initializer/const stages,
inheritance, hidden fields, interface dispatch, covariant returns, virtual task
suspension (including direct super calls to nonwaiting base bodies), ordinary call transport, casts and target capture, nested object and
container edges, unknown selection, receiver/argument order and GC after
suspension or method/fork termination. Seven frontend unit tests also inspect
canonical metadata, absence of fabricated interface bodies, detached-AST
execution, copy/seed state, null-check timing and pre-allocation boundaries.
Seventeen compiled negative controls target these mechanisms. The older lexical
return-cleanup control retains its fault at the constructor-aware return path.
A control-flow check enforces SIR_SPEC 7.2 named-scope exits before return;
runtime call cleanup alone can mask an omitted producer exit.

One existing static-boundary input was previously expected to reject `b=new`.
The maintainer approved updating that one expectation to exit 0 with no stdout
or error diagnostic (IEEE 1800-2023 8.7). Its source and the other rejection
checks are preserved; the expectation is committed separately. Existing suite
bodies, golden output and performance budgets are unchanged.

Managed event or unpacked-record instance fields, property/container-element
references, selected property writes, property NBA/delayed targets, class
formatting, task-form casts, built-in synchronization/weak classes and
parameterized static storage remain unsupported. Sensitivity-driven processes,
dependency waits, monitors, procedural overrides, event-counted NBA layouts,
pattern formatting and nonstandard initial-value overrides combined with class
objects are rejected before execution. Those boundaries prevent partial object
dependency/private-field/event semantics from silently running. Source support
must not be reported as complete while these mechanisms remain.
