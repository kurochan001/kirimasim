# Main integration: implicit containers and VPI argument declarations

[日本語](main-integration-2026-11-06.ja.md) · 2026-11-06.

Main integrates ten commits from `e6fd5a97` through `c47cc767`, after
`6f5c6984`. This publication record changes documentation only. P1 remains
in progress; finite source matrices do not establish complete Icarus coverage.

## Integrated behavior and approvals

- [Typed implicit container observers](implicit-container-execution.md) cover
  eligible static integral arrays/queues. Contract sections 1–8 were approved
  in `30d45404`; implementation is `8873c89a`. The eleven existing rejection
  judgments were approved and migrated separately in `b3d20b8b`.
- [Automatic integral VPI argument declarations](vpi-argument-declarations.md)
  retain source ranges and scalar/vector metadata. Selected parents capture
  numeric bindings, and expired ordinary-frame views cannot access a reused
  activation. Implementation `c0b047e3` follows the existing approved VPI contract.

Existing expectation changes remain separate from runtime changes. The documented
type, selection, lifetime, callback and net-write boundaries remain in effect.
No upstream slang report was sent. Five externally owned untracked files were
preserved and excluded from these commits.

## Validation results and limits

The earlier [implicit-container checkpoint](../measurements/2026-11-06/implicit-container-validation.json)
records 596 source checks, twelve compiled faults and related release/ASan/UBSan
runs of 1,003 tests each. The later [VPI checkpoint](../measurements/2026-11-06/vpi-argument-declaration-validation.json)
records unfiltered dev **12,268/12,268**, zero failures/skips, 243 source cases,
eight compiled faults and 69 related tests in each of release/ASan/UBSan.
The VPI companion closes 81 reference-passing cases. These are feature
checkpoints, not a claim that the combined local CI passed.

Full `tools/local_ci.py` was run on payload `c47cc767` with all four presets and
lint. All configure/build steps passed with warnings treated as errors. Release
flags, banned constructs, SPDX, suite-file pairing, global-state checks, all ten
per-commit expectation checks, clang-format and clang-tidy on 197 translation
units passed. **The combined local CI failed:** dev had 31 timeouts, release 39,
ASan 59 and UBSan 55. All failing CTest cases were classified as timeouts.
Host sleep transitions occurred during execution; idle-sleep inhibition alone
did not prevent interruptions. The first attempt's seven dev and thirty release
timeouts all passed when retried. No source was edited during either CI attempt.

The README-count check also failed against the interrupted dev JUnit. Published
counts retain the earlier successful full run; timeout results were not relabeled
as passing and counters were not reduced to hide failures. Dev has 12,268 tests;
release adds `cli/slot_event_default` for 12,269. The
[machine-readable record](../measurements/2026-11-06/main-integration-validation.json)
preserves each outcome separately. It was written before publication;
[GitHub CI](https://github.com/kurochan001/kirimasim/actions/workflows/ci.yml)
records results for the pushed commit independently.

## P1 scope and unchanged external evidence

The [fresh 188-probe checkpoint](../measurements/2026-11-06/icarus13-vpi-argument-checkpoint.json)
retains all existing source/oracle pairs: 187 P1-evaluated cases, the sole optional
`real_net` case, the required `dense_udp_coverage` timeout and 25 separate reference
mismatches. Crashes, hangs and independent faults remain failures. The optional
Icarus `wire real` extension does not exclude standard real variables, their
supported continuous assignment, or user-defined net types.

The fixed population remains 1,456/1,466, with zero failures and ten unsupported
cases. sv-tests, fuzz, performance and other external observations keep their
recorded dates in [validation counts](../VALIDATION_COUNTS.md). They were not
remeasured for publication. Broader Icarus-required VPI, specify, class and
container boundaries remain P1 work.
