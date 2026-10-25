# Integral `$fread`: restoring the approved copyback contract

[日本語](fread-copyback-correction.ja.md). 2026-10-24. This restores existing
approved rules; it does not propose a new target-capture exception.

## Contract and mistake

[SIR_SPEC 8](../spec/SIR_SPEC.md) says system-function output frames are
copied back as blocking assignments. Its approved 2026-10-18 argument rule
finds an output/inout actual's indices at copyback; an inout actual also
evaluates them for copy-in. This is the project's interpretation of 4.9.7,
not a claim that the standard explicitly fixes all system-function timing.
[SYSTEM_TASKS 6.3](../spec/SYSTEM_TASKS.md) retains the old bits not supplied
by a partial binary read (21.3.4.4), requiring the integral copy-in value.

`db40e27` correctly added optional integral start/count acceptance but also
froze the output target after copy-in. That target change was not approved
and conflicts with SIR_SPEC 8. The nine new mechanism cases encoded that
incorrect rule; passing them was not evidence of contract compliance.
The original seven-mutation claim also needs correction: evaluating the
target again and following a replaced destination at copyback must not be
classified as semantic defects under the approved rule.

Restore the approved behavior while retaining optional-argument evaluation:

1. Evaluate/read the integral actual for copy-in.
2. Evaluate descriptor and supplied start/count values once, left to right.
   Their values do not limit an integral read (21.3.4.4).
3. Read into the frame, preserving its unfilled old bits.
4. On a positive byte count, evaluate the destination again and copy back.
   On zero bytes, no output was written and no copyback occurs.

This does not change `$fgets`, scans or plusargs. Their output-only actuals
are found when an output is copied back; EOF/no assignment need not evaluate
a target selector. The same copyback rule applies to scalar, packed-part,
concatenated, fixed-array, frame, ref and dynamic-element destinations.
An actual ref retains its existing reference semantics; a direct queue
element expression at copyback selects from the variable's current value.

## Independent evidence and oracle correction

An original automatic-task probe, `change(a[i++])` with an inout formal,
increments i twice and writes the second array element in both installed
Icarus and kirimasim. An original `$sscanf("1 7","%d %d",i,a[i])` probe
also writes `a[1]` after assigning i=1 in both engines. Icarus rejects the
selected integral `$fread`/`$fgets` VPI targets used in the new mechanism
cases, so those direct system-function comparisons are unconfirmed there.
Compiler/runtime both report `14.0 (devel) (s20260301-411-gca6b8d376)`.
No Icarus or Verilator source/tests were read.

The correction retains all 144 width/state/context value expectations.
Only the nine phase-sensitive cases change their incorrect oracle. The
generator version rises to 2, retaining strict positioned warnings and all
existing cases. New checks add valid heap-element copyback at the newly
selected index and preservation of the copy-in value across a later
argument's mutation before partial EOF. No assertion is weakened.

Before fixing native code, all nine corrected mechanism cases fail through
stdout differences with the old binary; the 144 value cases and generator
check pass. This confirms that the corrected oracle detects the mistake.

## Separate review boundaries

CLAUDE.md ("Changes of expected values go in separate PRs") separates
corrections to existing expectations from source PRs, and explicitly permits
new tests alongside implementation. AGENTS.md is not part of the repository. A publication
audit found zero scalar-fread matrix files at `origin/main` (`aae29de`);
all 459 local matrix files (153 cases) are new to that published base.
Thus the unpublished feature's corrected new tests can be included with
its implementation without modifying a published expectation. The complete
diff against the published base passes `check_expected_change.py`.

The local branches retain separate correction commits for review:

- `codex/fread-copyback-expectations`: generator, suite correction and this
  rationale only; no `src/` change. Review against fixed base `70c7fb3`.
- `codex/fread-copyback-implementation`: based on that expectation branch;
  review its source correction against the expectation branch, so no
  existing suite bodies or expected files enter that isolated source diff.

If a PR's base already contains these matrix files, keep their correction
separate from its source PR. Relative to the original published base, the
new-test exception applies. Review the oracle under 4.9.7 and approved
SIR_SPEC 8; the implementation must pass that stronger oracle. The local
review branches do not require a new semantic approval to restore existing
approved rules, and do not block validated integration into local main.
