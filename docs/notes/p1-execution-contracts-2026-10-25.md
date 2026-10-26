# P1 execution contracts approved on 2026-10-25

[日本語](p1-execution-contracts-2026-10-25.ja.md). Status: **approved (2026-10-25)**.
The user approved the six concrete contracts requested together after the
array-method checkpoint. Their operative clauses below are normative amendments;
evidence and original observations remain historical records, not copied test
oracles. The English proposal texts are authoritative.


**Later publication (2026-10-26):** the separate tran expectation migration was
approved and applied in `77c9d188`; the automatic-event metadata migration was
approved and applied in `4ba6b63d`. Both are in [published main `304e1aec`](automatic-event-main-integration.md).
The implementation/migration status in the final addendum is the status at that
decision, not a current pending request. The approved execution contracts remain authoritative.

- [Fixed array ports and element-wise resolution, sections 1–6](fixed-array-ports-proposal.md).
- [Shared automatic activations, captures and automatic events, sections 1–8](automatic-capture-proposal.md).
- [Event-controlled NBA: scheduler, lifetime and SIR/kernel boundary contracts](event-nba-proposal.md).
- [Delayed three-state side propagation: the Proposed amendments section](delayed-gate-side-proposal.md).
- [Full-strength MOS and passive networks, sections 1–9](mos-switch-proposal.md).
- [Class identity, initialization, ownership and execution, sections 1–10](class-runtime-proposal.md).

These contracts refine the existing P1 scope, not its completion criteria.
Specifications reference the relevant contracts explicitly. Their stated
amendments take precedence over older incomplete/exclusion text in those
sections; unrelated existing rules remain authoritative. Parameter legality,
strict slang diagnostics, clean-room policy, C++20 public headers, tier 1's
direct C++ kernel calls, formatVersion 0 and C ABI 0.3 remain in force.

Implementation proceeds mechanism by mechanism, starting with complete delayed
driver publication. Existing test bodies, expected outputs, SIR dump expectations
and PerfCount budgets stay protected. Any necessary expectation migration gets
its separate reasoned review. No dependency, tool installation or global-state
exception is approved here. Neither this approval nor finite comparison probes
establishes P1 completion.

## Managed-event amendment approved on 2026-10-26

The user separately selected recommended option 1 of the
[managed-event safepoint proposal](managed-event-safepoints-proposal.md).
It amends RUNTIME_ABI 6 and automatic-capture 5/6/8 to permit event-only
collection at completely published VM instruction boundaries, including
synchronous functions and guards, with concrete runtime pins retained.
The container heap retains boundary-only collection; DPI and termination/final
exclusions remain. Complete publication and all listed evidence are required
before enabling the new timing or source automatic-event construction.
This is an implementation authorization, not a completion claim. The separate
[tran expectation migration](passive-expectation-migration.md) remains unapproved
and unapplied; no expectation, dependency or performance budget is changed.
