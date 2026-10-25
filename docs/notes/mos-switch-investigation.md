# MOS and bidirectional-switch investigation

[日本語](mos-switch-investigation.ja.md). Investigated 2026-10-24.
Status: historical investigation (2026-10-24); the follow-up contract and the
five pending proposals were approved on 2026-10-25 (see the
[P1 execution contracts](p1-execution-contracts-2026-10-25.md)).

MOS and bidirectional-switch source execution remains **unsupported**. The
existing strength-range kernel API and wide three-state side masks do not
implement switch networks. This note records the latest original observations
and remaining design questions; it does not amend the approved specifications.

## Evidence and limits

[Portable sources and results](../measurements/2026-10-24/mos-switch-design-observations.json)
contain 28 original programs and 56 compiler/runtime observations. Each program
compiled and ran successfully with both installed reference pairs:

- Icarus `13.0 (stable) (v13_0)`.
- Icarus `14.0 (devel) (s20260301-411-gca6b8d376)`.

All 28 stdout pairs agree. The data records source hashes, versions, commands,
exit codes and exact stdout. No other simulator source or tests were read.
Kirimasim execution is not measured in this exploratory dataset. These programs
are separate from the [79-item feature inventory](icarus-13-comparison.md),
whose measured gaps at the original `57a64ef` baseline were 20 against 13.0
and 26 against development. The later [fork comparison](fork-local-declarations.md)
expanded the inventory of that time to 81 probes with 21/27 gaps; current
87-probe results are in [the P1 progress record](p1-icarus-progress.md).
Reference agreement alone does not establish IEEE conformance or an approved
test oracle. Performance and Linux behavior were not remeasured.

## Mechanisms the implementation will need

- **Strength changes without value changes.** In `strength_only`, the source
  remains logical 1 while changing from weak to strong. Its downstream MOS
  output changes from 0 to 1 against a pull-0 driver, while an ordinary
  `@(source)` process records zero changes. Internal strength dependencies
  must therefore be distinct from ordinary value-change subscriptions.
- **Device-specific truth and attenuation.** `mos_z_unknown` produces Z for
  all six MOS/CMOS forms with high-impedance data and uncertain controls.
  This needs the MOS rules of 28.7/28.9. The observed resistive chain is
  supply → pull → weak → medium → small, then remains small. The nonresistive
  path changes supply to strong. Strength reductions are specified in
  28.13/28.14; applying ordinary three-state gate truth rules is insufficient.
- **Passive networks without artificial drivers.** `root_removed_ring`
  changes all three nodes from 1 to Z when its external driver is removed;
  `floating_self` stays Z. Resolved neighbor values cannot become persistent
  independent sources merely because paths form a cycle. Conversely,
  `mos_directed_feedback_drop` retains 1 after removing its external drive.
  Directed MOS output drivers and passive bidirectional paths need distinct
  state and scheduling contracts (4.9.5, 28.7/28.8).
- **Port identity and channel reduction.** `port_supply_identity` retains
  supply strength through a child output connected to a parent bit, then
  reduces it to strong through `tran`. Strength-preserving net-port identity
  must be distinguished from a switch edge (4.9.6, 28.13).

## Questions still requiring a reviewed contract

The [approved interval model](../spec/VALUE_SEMANTICS.md#5-net-resolution)
already retains both endpoints. At that time, end-to-end propagation, network
ownership, force behavior and delayed full-state updates still needed detailed
contracts under [P1_SCOPE](../spec/P1_SCOPE.md).

`positive_range` combines an uncertain strong-1 gate contribution with weak 1.
The existing interval model gives a positive input range from weak to strong;
propagating that complete range against pull 0 can give X. Both references
instead print an output of 1. The cause and normative interpretation are
**unconfirmed**. Comparing only the largest input strength would discard
information; this observation must not silently replace the approved model.

All five wired-net channel probes print X, including uniform `wand` and `wor`
networks. Local wired resolution and transport through a channel require a
reviewed interpretation; these observations are not adopted as expected values.
The force probes also distinguish a node's forced readout from contributions
that reach other nodes: forcing the transit node to Z still lets a remote 1
reach the far node in `force_highz_transit`. A simple routing barrier would
not reproduce that observation. The exact contract remains unconfirmed.

Delayed startup needs separate review. With a weak input and no background
driver, `mos_delayed_start` initially prints `WeH`, whereas
`rmos_delayed_start` initially prints `StX` and later `St1`. The latter does
not show the weak-to-medium reduction expected from 28.14. Its cause is
unconfirmed; it is not an approved startup oracle. The existing
[delayed three-state side proposal](delayed-gate-side-proposal.md) also remained
pending at that time and does not itself define all MOS/switch timing.

Before implementation, the specification work must settle complete interval
transport, physical driver ownership across multiple paths, uncertain controls,
wired-net combinations, force/release, startup and cancellation, strength-only
notifications, port connections and resource failure. SIR must carry the needed
state into the shared kernel; source lowering and VM transport alone cannot
provide the missing execution semantics. No new semantic amendment is approved
by this investigation, and the five existing pending proposals remained pending
at that time.

## Follow-up contract and loaded observations

The [full execution proposal](mos-switch-proposal.md) now specifies the
network, MOS, port, force, timing, SIR/kernel and formatting decisions for
review. At that time it added no implemented support and left the earlier five
proposals pending.

An [18-source follow-up matrix](../measurements/2026-10-24/mos-strength-matrix-observations.json)
tests each MOS form with isolated outputs and weak/pull opposing drivers.
Each reference has 2,304 scalar samples. All projected values agree with the
truth/reduction/local-resolution calculation. With competing drivers every
strength token agrees too; isolated resistive outputs have 252 strength-token
differences per engine. Thus the earlier single-driver `St1` observation does
not establish that the device fails to attenuate during resolution. The
implementation cause remains unconfirmed. No simulator source was read.

## Reproduce one observation

Extract any named original program from the portable data, then use a matching
compiler/runtime pair. For example:

```sh
python3 -c 'import json; from pathlib import Path; d=json.loads(Path("docs/measurements/2026-10-24/mos-switch-design-observations.json").read_text()); Path("build/mos-switch-probe.sv").write_text(next(p["source"] for p in d["probes"] if p["name"] == "strength_only"))'
iverilog -V
vvp -V
iverilog -g2012 -s top -o build/mos-switch-probe.vvp build/mos-switch-probe.sv
vvp build/mos-switch-probe.vvp
```

The sample assumes the build directory exists. Verify both version banners;
PATH does not select a particular Icarus version automatically.
