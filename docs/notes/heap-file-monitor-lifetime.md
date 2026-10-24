# Heap-backed file monitors and callback termination

[日本語](heap-file-monitor-lifetime.ja.md). Implemented and checked locally,
2026-10-25. This extends the [typed-pattern execution checkpoint](pattern-sir-execution.md)
under existing SYSTEM_TASKS 4/6.2 and SCHEDULER_SPEC 10.7/10.8 rules.

## Correctness fix

`Scheduler::kill` marked a display callback as ended but left its display/file
monitor registration and heap read dependencies attached. A later cell write
could call that ended callback again, and the heap change hook remained
installed after the last callback was killed. `Scheduler::kill` now calls
`detachDisplayMonitor` before canceling ordinary waits and marking completion.
The shared kernel owns the fix; it applies to both display and file callbacks
regardless of the source context that created them. It cancels the callback's
own registration, not a monitor merely issued by an ending source process.
No output, value or scheduling policy is changed.

## Independent checks

The original generator [gen_file_heap_monitor_matrix.py](../../tools/gen_file_heap_monitor_matrix.py)
adds 20 source cases: queue/dynamic storage × mcd/fd descriptors × initial,
task, function, fork and generate call contexts. Expected lines follow the
scripted values and SYSTEM_TASKS 4/6.2, independently of simulator output.
They exercise selected-cell and size dependencies, an unrelated cell write,
two writes before Postponed, selector changes, whole-container replacement,
two independent file monitors, `$monitoroff`, closure, descriptor reuse and
termination after the last output closes. All 20 pass locally.

Two new scheduler tests cross display/file callbacks. One kills a callback,
changes both its old cell and another live callback's cell, and checks that
only the live callback runs; after it ends, the previous change hook is
restored and unrooted containers can be collected. The other injects allocation
failure during Postponed read capture, checks ResourceLimit and the correct
callback origin, and verifies read/change hook restoration. Both tests fail
against the old `kill` behavior and pass with the fix. The registry includes
the corresponding omission control; 514 controls apply. Seven selected
heap-monitor/pattern scheduler tests and all 29 selected CTest entries pass.

Existing test bodies, expected outputs and PerfCount budgets are unchanged.
Callback-driven reentrant registration/reallocation and all possible source
contexts still need broader evidence. Source `%p` lowering remains unfinished.
The 87-probe Icarus inventory retains its prior provenance and gap counts;
these monitor checks do not establish complete P1 coverage.
