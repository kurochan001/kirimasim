# Separate unit expectation migration for program strength regions

This migration accompanies the scheduling amendment that the user approved
on 2026-10-29 in section 1 of
[the program-net correction](p1-scheduling-corrections-proposal.md).
It is kept in its own PR, with the implementation reviewed separately.

Two existing unit tests count startup resolution, two process resumptions
and one dirty-component resolution. Their Reactive branch previously
expected two Active events and two Reactive events. Under the approved
amendment, startup is the one Active event; the two Reactive process
resumptions and the Reactive resolution make three Reactive events.
The Active-only branch keeps its existing event counts.

The tests are renamed to describe the new resolution region. Their exact
value, callback-order, Inactive-event and process-count assertions remain.
The region-count assertions retain exact equality and the total event count.
IEEE 1800-2023 4.5 and 24.3.2 provide the scheduling context; the explicit
internal-component scheduling choice is the approved project amendment.

Independent source regressions cover program drive/release, Re-NBA and
delayed net updates, and new kernel checks cover roots, conduction, startup,
inertial retention and committed transitions. Validate the combined
implementation and this migration before integrating them.
