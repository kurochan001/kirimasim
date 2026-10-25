# Program execution amendment

[日本語](program-execution-proposal.ja.md). Status: **approved (2026-10-23)**.
Incorporated into SIR_SPEC, SCHEDULER_SPEC and SYSTEM_TASKS. The shared kernel now implements program lifetime and scheduling. The
124-case execution matrix and kernel/SIR/CLI checks cover the approved
mechanisms; complete language coverage and full Icarus parity remain unconfirmed.

P1_SCOPE advances programs into P1 but requires review of missing or
conflicting details before implementation. Before this amendment,
SCHEDULER_SPEC 16.1 described `$exit` as waiting for every program, whereas
IEEE 1800-2023 24.7 makes it terminate the originating program. The proposal
below fills that detail and the SIR representation. The clauses were checked
locally; this text paraphrases them without reproducing standard text or
examples.

## Approved semantic amendment

- A named program instance has its own identity in SIR's instance table.
  Its declarations, ports, generate scopes and supported statements use
  the same lowering and value rules as other scopes. slang continues to
  check illegal items and references; no source rewriting or relaxed
  diagnostic mode is introduced (24.3, 24.4, 24.5).
- Program initial procedures and continuous assignments belong to the
  reactive thread set. Event waits and positive delays resume in Reactive,
  zero delays in Re-Inactive, and their NBAs update in Re-NBA. Calls,
  recursive calls and fork descendants inherit the caller's thread set,
  even when a subroutine is declared in another scope (24.3.1).
- Driving and resolving connected program nets happens immediately as
  part of the reactive update. Each sensitive process wakes in its own
  thread set, including module processes in Active. Existing port and
  resolution semantics apply (24.3.2, SIR_SPEC 3.5).
- Count the top-level initial procedures of each program. Once all of a
  program's initial procedures end, immediately terminate their remaining
  fork descendants with the existing kill/disable cleanup rules. Do not
  count continuous assignments or final procedures as initial procedures.
  A program with no initial procedures does not itself cause global
  termination (24.3; SCHEDULER_SPEC 10.7, 10.8).
- If any program has an initial procedure, immediately finish simulation
  after all such programs have ended. Run final procedures once under the
  existing finish rules; pending events do not run after that finish.
  While another program remains, work already queued by ended threads
  follows existing cleanup rules, including captured NBA values (24.3;
  SCHEDULER_SPEC 10.7, 13, 16).
- `$exit` terminates every initial procedure and descendant of the program
  in which its executing thread originated, then applies the same global
  completion rule. This origin follows calls and forks, rather than the
  lexical scope containing `$exit`. With no program-initial origin, ignore
  the call without a warning (24.7 permits ignoring it and makes warning
  optional; choosing silence is a project rule).
- Anonymous programs introduce no new instance: declarations continue to
  belong to the enclosing package or compilation unit. Functions and
  tasks use the calling thread's set and program origin. Other unsupported
  declarations in them keep their own diagnostics (24.6).
- Static initialization still happens before processes start. Deferred
  assertions, postponed output, final procedures and concurrent assertions
  keep their existing specified scheduling; program support does not
  silently enable unsupported assertion or clocking mechanisms.

## Representation and verification

SIR adds a `program` instance kind. Process membership is determined by
its instance and parent; dynamic thread-set and originating-program state
is retained in the execution kernel and inherited by child processes.
The textual SIR form and verifier represent/check the new kind. Runtime
VM/codegen adapters only supply SIR metadata and execute kernel operations;
termination and scheduling decisions belong to the shared kernel. Kernel
public interfaces stay C++20 and introduce no dependency or mutable global.

Original mechanism matrices will check module/program ordering, #0 and
positive delay, blocking/NBA writes, waits, subroutine scope versus caller,
forks and descendants, multiple initials/programs, implicit and explicit
finish, `$exit` from called code and outside a program, ports, generate
and instance arrays, top programs, empty programs, final execution and
anonymous-program subroutines. Keep existing suite bodies and expected
values. Before source changes, any existing rejection expectation that
must change gets a separate specification-backed expectation commit.

Approval preceded implementation. The validation does not claim
complete program coverage or full Icarus parity. Classes, dynamic storage,
VPI and other gaps continue as independent mechanisms under P1_SCOPE.
