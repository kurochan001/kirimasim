# P1: optional Icarus `wire real` extension

[日本語](p1-wire-real-exception.ja.md)

The exclusion requires a confirmed dependency on Icarus’s extension, so a test
containing `real` is not automatically out of scope; the [test-classification
rules](../spec/TEST_SUITE.md#optional-extension-classification) preserve standard real
features and independent failure reporting.

Approved by the maintainer's explicit request on 2026-11-01. The authoritative
criteria are [P1_SCOPE](../spec/P1_SCOPE.md) and
[TEST_SUITE](../spec/TEST_SUITE.md#optional-extension-classification).

## Completion criterion

Support for Icarus Verilog's `wire real` extension is not a mandatory P1
condition. Cases with a confirmed dependency on this extension do not prevent
P1 completion when unsupported. This exception qualifies the Icarus 13.0
coverage target; lack of this extension alone is neither a P1 blocker nor a
missing IEEE 1800 standard feature.

`real` variables and arithmetic, standard-permitted single continuous
assignment to a real variable, and user-defined `nettype real` are outside
this exception. Their existing P1 treatment remains; this decision does not
add `nettype real` as a new P1 requirement. No runtime implementation is added,
removed or disabled, and no existing source test or expected output is changed.

## Confirmed case and reporting

The only excluded feature probe is **`real_net`**. Its original source declares
`wire real r`, assigns 1.5 and displays that value after a delay. Its purpose
requires the Icarus extension; it is not a separate standard-real regression.
The complete source SHA-256, including its final newline, is
`f70a56afc6f239649b7f871333a33d12340a57c1106cc7d77b9ca7364bd0f69f`.
Both name and hash must match the reviewed entry in `P1_OPTIONAL_PROBES`.
Changed or mixed sources require dependency review and remain evaluated by
default. A `real` substring alone never selects this exclusion.

The [new 176-case report](../measurements/2026-11-01/icarus13-wire-real-optional.json)
uses Icarus compiler/runtime **13.0 stable**, native executable `a525b8414e1c`
(AppleClang 21.0.0.21000334), and the revised Python policy tool. It records:

- **176 total, 175 P1-evaluated, one out of scope.**
- Three raw passing-reference gaps in `gaps`; two in `p1_required_gaps`:
  `specify_path` and `dense_udp_coverage`.
- `real_net` stays `run_failed`, exit 2, with its positioned invalid-net-type
  diagnostic. The extra compile-only check also rejects with exit 2 and empty
  stdout. Its single-case `--require-parity` invocation returns 0; the required
  `specify_path` single-case invocation still returns 1.
- `dense_udp_coverage` retains its front-end time limit and is also an
  `independent_failures` entry. It is not excused by this decision.
- The 25 reference output mismatches remain separate; reference crashes/limits
  are also listed separately and are not attributed to kirimasim.

All 176 source hashes, oracle bytes, native results and reference statuses/exit
codes match the preceding combined record. The failed `dynamic_query_dimension`
reference stdout differs only in a temporary path; the new public record redacts
that path and records the redaction. Passing stdout is unchanged. Old reports
are retained byte for byte. The dated 95-case aggregate is separately reclassified
as **94 evaluated + one excluded**, with raw gaps 6/6 and required gaps 5/5
(stable/development). This is a scope reassessment, not a new measurement or PASS;
its incomplete-coverage assessment remains false.

## Population audit and checks

The frozen own-suite population at `41085fe3432b` remains **1,466**. Inspection
of its source designs found no explicit `wire real` declarations. The pinned
sv-tests handwritten source tree at
`15490e95fd978ada1c1dc59b7c4b53a7edb23b78` likewise has no explicit `wire real`
in its **344** selected simulation tests. Comments and strings were ignored;
this is not a claim about every possible macro expansion or dependency.
There are no newly confirmed exclusions from either population. Their existing
pass-rate denominators, runner rules, gate lists and nightly workflows are unchanged.
No new external population run or Linux result is claimed.

The existing test `ports/fixed_array_port_typed_real_net.sv` uses standard
`input real` and `output var real`, with single continuous assignments. It has
no `wire real` declaration and remains evaluated. All **eight** existing selected
real/shortreal value and typed-port tests pass unchanged.

`tools/p1_wire_real_scope` checks **21** behavioral scenarios: ordinary optional
compile rejection, supported/mismatching optional results, changed/mixed sources,
standard real variable/assignment/nettype shapes, crashes, time/output limits,
internal errors, failed compile checks and runtime failures. These standard
shapes check classification only; they do not add new required language features.
All pass. Three Python fault controls are caught: broadening exclusion to names
alone, hiding independent failures from the gate, and fabricating optional PASS.
The original tool is restored and the positive check passes afterward.

Full dev after this policy change passes **11,300/11,300**, with zero failed or
skipped (236.13 seconds); it includes 193 tool tests. Counts are updated only
from that successful full JUnit. Runtime sources are unchanged from `a525b841`.

P1 remains incomplete. This finite inventory does not replace the remaining
coverage review, specify/VPI contracts, existing quantitative gates or human
reviews. No slang report is posted by this change.
