# Automatic event: migration of a historical rejection check

[日本語](automatic-event-expectation-migration.ja.md). Status: **proposal (not approved)**,
2026-10-26. Implementation is authorized by the approved automatic-capture and
managed-event safepoint contracts. This separate request concerns one existing
test oracle, under CLAUDE.md and AI_POLICY's protected-expectation rule.

## Exact decision requested

In `tests/tools/check_automatic_capture_metadata.py`, retain every existing
source input byte for byte. The last member of `rejected` declares an unread
automatic event and forks a child that reads another automatic field, `value`.
It currently requires exit 2, empty stdout and a positioned error. Once the
implemented construction and retained-extent support are enabled, require exit
0, exactly `7\n` on stdout and no error diagnostic for this input instead.

Move that input into a separately checked accepted case; keep the four ref,
const and `ref static` rejection cases and their existing checks. Update only
the final informational test summary to say four strict boundaries and one
automatic-event extent case. Change no other expectation, fixture body, dump,
unit assertion or performance budget. Make this migration in its own commit
with no `src/` changes and the human's Approved-by trailer.

## Grounds

IEEE 1800-2023 6.8 gives a default automatic event its own object on declaration
execution. Sections 6.21 and 9.3.2 govern the automatic scope retained by spawned
activity. The approved [automatic-capture contract 1/2/5/6](automatic-capture-proposal.md)
requires every typed field in that retained extent, including an unread event,
to remain valid. The child reads the initialized numeric value once and prints
7. The former rejection was an implementation boundary, not a language error.
This reasoning and all tests are original; no external implementation or tests
were read, and no standard wording or examples were copied.

## Evidence before approval

The working implementation passes 176 new generated original source cases:
32 default-construction cases across eight containers and four shapes, and
144 retained-task cases across module/program origins, all three joins, four
shapes, named/mixed/triggered waits and immediate/delayed-NBA triggers.
The existing last metadata input itself now exits 0 and prints exactly `7\n`.

[Full local CI](../measurements/2026-10-26/instruction-events-validation.json) now
passes 17/21 steps; the four failing preset test steps all have this same one
historical oracle failure. Dev/ASan/UBSan pass 5,740/5,743 and release 5,741/5,744,
with two known skips each. Builds, all policies, full format/tidy and generated
README counts pass. All 12 compiled faults are detected, and 288 related checks
pass after restoration. Ordinary-path counters match, and both fresh 87-probe
comparisons reduce gaps eight to seven. The implementation note records the
cost observations and limitations. Approval here does not establish P1
completion, authorize event rebinding or approve any other oracle change.
