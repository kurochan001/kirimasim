# Multidimensional binary memory input

[日本語](multidim-fread-execution.ja.md). Implemented 2026-10-25 under the
[approved contract](fread-multidim-proposal.md), SYSTEM_TASKS 6.3.

`Lowerer::lowerFreadMemory` captures a selected-memory prefix before the other
arguments and decomposes linear scalar addresses into numerical indices, with
the rightmost remaining dimension varying fastest. Single remaining dimensions
keep actual-index addressing. Slot/frame/ref leaves reuse `freadLeaves` and the
kernel's integral byte input; there is no new VM address rule or opcode.

The [original generator](../../tools/gen_multidim_fread_matrix.py) supplies 156
cases with an independent Cartesian-product/byte-string model: five range
shapes, 1/9/32/65/129-bit leaves, two/four states and slot/automatic/ref contexts.
Nine input scenarios cover defaults, explicit/omitted start, counts, invalid
boundaries and partial EOF. Six extra sources check prefix capture before
descriptor/start/count side effects, one/two remaining dimensions and waits.
All pass, as does the existing Icarus-13.0 `multidimensional_fread` probe.
The full dev suite has 3,657 passes and two known skips out of 3,659.

Three registered controls break coordinate progression, descending traversal
and exhausted counts. Compiled control evidence and four-configuration CI
will be recorded with the array-method validation checkpoint. Existing test
bodies, expectations, 29 SIR dumps and performance budgets are unchanged.
Dynamic memories and multidimensional structure memories remain P1 work.
