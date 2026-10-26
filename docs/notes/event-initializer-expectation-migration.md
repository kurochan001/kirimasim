# Event initializers: migration of three historical rejection checks

[日本語](event-initializer-expectation-migration.ja.md). Status: **approved by the
user and applied (2026-10-27)**. Only the three requested oracle classifications
change, separately from implementation. The original 2026-10-26 proposal below
is retained as the decision record; its requests/future tense describe that
earlier state. Implementation semantics were already approved by
automatic-capture contract 5.

The [applied-source audit](../measurements/2026-10-27/event-initializer-approved-oracles.json)
executes the old/new checker versions and verifies that all 24 original inputs
are byte-identical and only the approved three oracles change.

## Exact decision requested

In `tests/tools/check_event_input_boundaries.py`, preserve all 24 original
source programs byte for byte. Three of its 22 rejection cases initialize an
automatic event from a static event. Reclassify only those three as accepted:
require exit 0, empty stdout, no error diagnostic and no internal error, instead
of exit 2 with a positioned `unsupported: event used as a value` diagnostic.
Keep the other 19 rejection cases and both existing aggregate-base acceptance
cases with their current strict checks. The updated checker would execute
19 rejected and five accepted programs, retaining the same 24 inputs.

The three cases share `module top;event a,b;` on line 1 and `endmodule` on
line 3. Line 2 is exactly one of the following original lines:

```systemverilog
initial begin automatic event shared=a; end
task automatic run();automatic event shared=a;endtask initial run();
function automatic void run();automatic event shared=a;endfunction initial run();
```

Do not remove the initializer statement or substitute another program. Keep
the original loop's construction of every source string; place these three
assembled strings in a checked accepted collection. Change only their oracle
classification/assertions and the final informational summary. Change no other
expectation, dump, suite body, unit assertion or performance budget. Apply
the migration in a commit separate from `src/`, with the human Approved-by
trailer. This proposal itself neither migrates an oracle nor authorizes push.

## Grounds and observations

The approved [automatic-capture contract 5/6](automatic-capture-proposal.md)
requires supported initialization values to share the event object before
the new variable's waits. IEEE 1800-2023 6.8/6.21/15.5 and SIR_SPEC 7.4/8 are
the grounds; no standard wording or example is copied. RUNTIME_ABI 6 provides
the existing typed publication/collection contract. General rebinding and
comparisons remain unsupported.

[Before/after evidence](../measurements/2026-10-26/event-initializer-oracle-review.json)
records the full original inputs and their digests. The prior release binary
(runtime source at `63c64d83`) rejects all three with exit 2; the working
implementation accepts all three with exit 0, empty stdout and no error.
New source coverage includes 208 generated designs and a new checker with
14 strict rejection cases and four constructor-count designs; all 210 new
CTest entries pass. The historical checker still fails on the first newly
supported initializer because its unchanged oracle requires rejection.

All source programs, implementation and controls are original. No external
simulator source/test, private material, dependency or installation is used.
Complete P1 parity is not established by this migration or finite matrix.
