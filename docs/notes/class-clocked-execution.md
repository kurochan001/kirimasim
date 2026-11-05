# Class operations in explicitly clocked bodies

[日本語](class-clocked-execution.ja.md) · 2026-11-04.

A process body executes after a trigger fires, while its trigger evaluator decides when
to wake it; the [clocked-class
FAQ](../FAQ.md#can-an-explicitly-clocked-process-use-class-objects) explains why their
support limits differ.

**Published integration (2026-11-04):** Main `91139559` includes this implementation. Its [full local CI and successful GitHub workflow](main-integration-2026-11-04.md) supplement the earlier checkpoints below; dated raw evidence is unchanged.

Ordinary `always @(...)` and `always_ff` bodies now execute supported object
operations when their explicit trigger evaluation is object-free. This closes
three measured Icarus 13.0 gaps under the already approved
[class contract 5–7](class-runtime-proposal.md). No new semantic contract,
instruction, kernel dependency or public ABI is introduced. Existing suite
sources, expectations and assertions are retained.

## Trigger evaluation and body execution

The shared `sir::classExecutionShapeSupported` check previously treated the
entire `ff`/`ff-plain` body as a dependency evaluator. As a result, a property
read or method call in a body caused rejection even with a simple ordinary
clock. Front and the native SIR loader now check trigger slots and the
transitive guard/watch call closures independently of the executable body.

Supported bodies use the existing object/property/call and activation paths:
property reads and whole-property blocking writes, direct/virtual calls,
allocation, automatic local handles and receiver evaluation once. Ordinary
NBA writes to eligible static scalar/vector variables can capture a property
value; this does not add property NBA targets. Receivers remain rooted through
allocating calls. Null access retains the positioned design runtime error.

```systemverilog
class Box;
  int value = 0;
  function void bump(); value++; endfunction
endclass
module top;
  Box object;
  bit clock = 0;
  always_ff @(posedge clock) object.bump();
  initial begin
    object = new;
    #1 clock = 1; #1 clock = 0;
    #1 clock = 1; #1 $display("%0d", object.value);
  end
endmodule
```

This original example prints `2`. Changing a property alone does not trigger
an explicitly clocked body. Ordinary signal-based iff guards and expression
events retain their existing rules. Implicit comb/latch/`@*` object dependencies,
object-dependent trigger/guard/watch evaluation, observer refs, property ref/
selected/NBA targets, object formatting and the other existing class boundaries
remain unsupported. Functions used in a trigger cannot hide object reads through
ordinary calls. Waiting-method control flow keeps the existing general-process
path and is not a newly enabled mechanism here.

## Evidence

[Validation metadata](../measurements/2026-11-04/class-clocked-execution-validation.json)
binds implementation `7d5bdfac`, the additional NBA unit check `775d99a4`, source
hashes and full dev JUnit. Count-only publication is `193373d2`.

- The same **180 original probes** run [before](../measurements/2026-11-04/icarus13-class-clocked-before.json)
  and [after](../measurements/2026-11-04/icarus13-class-clocked-after.json). All
  previous 176 sources/oracles and native results are unchanged. The four added
  probes cover ordinary always method calls, always_ff method calls, clocked
  property reads and a waiting-method control. All four pass Icarus 13.0; the
  first three move from native unsupported to passing. Compile-time synthesis
  warnings on Icarus's always_ff example are retained separately from execution.
- The expanded inventory has **179 P1-evaluated cases and one exclusion**,
  `real_net`. Required gaps fall from four to one, raw gaps from five to two;
  the 25 reference mismatches remain separate. `dense_udp_coverage` still times
  out in the pinned frontend and remains an **independent failure**, not a pass
  or P1 exemption. The fixed 1,466-case population and external sv-tests/fuzz
  gates are not remeasured; finite probes do not establish full P1 coverage.
- `tools/gen_class_clocked_matrix.py` supplies **144 new source tests**: five
  declaration homes, two clocked process forms, widths 1/33/65/129, four-state
  field/method/allocation paths, ordinary iff/expression events, automatic locals
  and side-effecting receiver selection. All pass.
- Five new unit tests check numeric trigger metadata, virtual dispatch and
  receiver lifetime under allocation pressure, four malformed callback closures
  rejected before runtime allocation, null errors and owned 129-bit NBA payloads.
  No existing expectation migration is needed.
- [Five compiled native controls](../measurements/2026-11-04/class-clocked-native-controls.json)
  are caught: two admission regressions and three existing rejection/closure
  invariants. Compiler failures and registry self-checks are not counted.
  Sources are hash-restored and all 160 controls' focused CTests pass again;
  the subsequent additional NBA unit also passes. Registry validation checks
  **1,193 anchors**, none inapplicable; only these five controls were compiled
  in this task.
- Full dev configure/build/test passes **12,012/12,012**, zero failures or skips.
  Published diagnostics redact machine-specific paths, including paths inside
  hex output, and reference-process IDs. Classification uses original bytes;
  redaction changes no oracle or status.

## Final local configuration checks

At `4c2ecce1`, standalone full dev and the count check pass, followed by all
17 steps of `python3 tools/local_ci.py --presets release,asan,ubsan`. Release
passes **12,013/12,013**, and ASan/UBSan each pass **12,012/12,012**, with no
failures or skips. Release flags, banned APIs, SPDX, fixture pairing, global
state, per-commit expectation checks for all 23 unpublished commits, formatting
and clang-tidy on all **194 translation units** pass. These are local results,
not a new GitHub CI/Nightly or fixed-population measurement.

The final test-only follow-up `269e9c7b` moves the NBA source initialization
into the clocked writer to meet 9.2.2.4. All assertions and implementation
sources are unchanged. After rebuilding, all **16 ClassFront tests** pass in
each of dev/release/ASan/UBSan, and targeted clang-format/clang-tidy pass again.
The metadata distinguishes the full-suite checkpoint from this fixture follow-up.


```sh
cmake --preset dev && cmake --build --preset dev
ctest --preset dev -R '^(ClassFront|suite/sched/class_clocked_|tools/gen_class_clocked)'
python3 tools/gen_class_clocked_matrix.py --check
python3 tools/mutants.py --check
```

The probe runner accepts explicit `--iverilog` and `--vvp` paths; use installed
13.0 executables with `--extended` to repeat the saved inventory. No reference
simulator source or external tests were read. Slang posting remains on hold.
