# Physical VPI net registration: observation-count migration

[日本語](vpi-net-write-expectation-migration.ja.md) · 2026-11-03.

**Status: exact one-assertion/two-case migration separately approved on 2026-11-03 and applied.**
The [approved whole-net contract 1–2/4](vpi-net-write-proposal.md) requires
complete physical net capabilities, including input port nets reachable by
name. Their existing continuous drivers must be registered rather than
pretending these are writable variables or omitting their physical origin.

`tests/unit/module_path_observation_test.cpp::observePath` currently requires
`ctx.strengthDrivers().originCount() == 1` for both source designs. With complete
physical registration, the unconditional case has two origins: the existing
weak output-q driver and the existing implicit input-a port driver. The
conditional case has three, adding the existing input-b port driver. No new
source driver is created. Previously only q required physical registration.

Propose changing the shared assertion to:

```cpp
EXPECT_EQ(ctx.strengthDrivers().originCount(), conditional ? 3U : 2U);
```

Retain the complete original source bytes and every value/time/strength,
VPI-registration/callback, VCD-alias and malformed-IR assertion. Add an explicit
assertion that exactly one physical origin targets the module-path destination
q, retaining the old check's protection against duplicated q drivers. Neither
case is skipped, deleted, renamed or loosened to a minimum/range.

The affected CTests are:

- `ModulePathObservation.VpiBothPortNamesFormattingAndWaveformSeeCommittedPathSnapshot`:
  old count 1, actual/new count 2.
- `ModulePathObservation.ConditionalPendingPublicationKeepsVpiFormattingAndWaveformCoherent`:
  old count 1, actual/new count 3.

Both currently fail only this registration-count assertion. The 58 frozen
native written oracles pass and the other 133 selected kernel/VPI/path checks
pass. These selected results are not a full regression success while the two
old count assertions remain. The [record](../measurements/2026-11-03/vpi-net-write-origin-count-migration.json)
retains the unchanged test-file hash and exact counts.

Grounds: the approved whole-net physical-capability contract, inspected
1800-2017 38.34/10.6.2 and the existing source port-driver model. Direct 2023
text audit remains unconfirmed. This updates a physical-registration count,
not a visible language oracle or P1 gate. After separate approval, put the
assertion migration in a commit with no `src/` changes and rerun both cases.
