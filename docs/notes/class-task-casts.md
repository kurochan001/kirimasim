# Class task casts

[日本語](class-task-casts.ja.md)

Class `$cast(destination, source);` now executes under the
[approved class contract, section 5](class-runtime-proposal.md#5-calls-inheritance-and-casts)
(IEEE 1800-2023 6.24.2 and 8.16). This completes the task-form boundary for the
existing supported class destinations, not the entire class contract or P1.

`Lowerer::lowerClassCast` captures the destination reference once, before source
effects, for both function and task forms. Static slots, fixed-array elements
and automatic frame storage share this path across modules, packages,
interfaces, programs and generate scopes. Instance-property refs remain unsupported.

The task emits typed `object.cast.required`, followed by a store only on
success. The shared kernel's `Heap::castObject` keeps the existing compatibility
rules: assignment-compatible views and literal null succeed; narrowing requires
a compatible dynamic object. A typed null retains its static type. A failed
task cast leaves the destination unchanged, reports a positioned design runtime
error, exits with code 2 and runs final procedures. Function-form failure still
returns zero and continues without storing. VM dispatch does not decide class
compatibility. No external C ABI or dependency changes are needed.

## Validation and limits

The original generator [gen_class_task_cast_matrix.py](../../tools/gen_class_task_cast_matrix.py)
adds 180 cases: six declaration containers, scalar/static-array/automatic-array
targets and ten source categories. These include base/interface views, typed
and literal nulls, mismatches, source effects changing the target index, and
4096 temporary allocations during source evaluation. Two further cases cover
virtual functions and waiting virtual tasks with frame-local destinations. Three new unit tests
check the kernel error distinction, SIR validation/text and destination/error
position behavior. Existing function-cast tests remain unchanged.

The isolated successful-cast probe compiles with Icarus Verilog 13.0, but
its runtime rejects `$cast` as undefined. This implementation is grounded in the already approved contract;
it does not close a measured Icarus-supported gap. An exploratory empty UDP
input probe ran in Icarus, but 29.8/A.5.4 require expressions at input terminals;
it is not evidence of missing standard functionality and is not implemented here.
Slang upstream posting remains on hold. Dense UDP compilation and the remaining
class/property/observer, specify and VPI boundaries remain open.

[Reference probe](../measurements/2026-11-03/class-task-cast-reference.json) and
[four compiled controls](../measurements/2026-11-03/class-task-cast-controls.json)
retain the original source and restoration checks. All controls are caught;
the restored focused baseline passes.

The [complete local CI acceptance](../measurements/2026-11-03/class-task-cast-validation.json)
on `f23c6202` passes all 21 checks: dev/ASan/UBSan each 11,780, release 11,781,
without failures or skips, plus clang-tidy over 191 files.
