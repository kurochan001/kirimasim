# Implicit container expectations: proposed migration

[日本語](implicit-container-expectation-migration.ja.md) · 2026-11-06.

**Status: approved separately on 2026-11-06; migration applied on its own branch as `b3d20b8b`.**
The [approved execution contract, section 7](implicit-container-observers-proposal.md#7-existing-rejection-expectations-to-review-after-implementation)
identifies eleven old rejection judgments in two validators. The implementation
now passes the new 596-source matrix and eight independent kernel/SIR tests,
plus two new frontend tests. These independent checks preceded the separately approved migration.

## Approved change

- In [run_dynamic_container_tests.py](../../tests/cli/run_dynamic_container_tests.py),
  seven sources change from positioned unsupported rejection to successful,
  silent execution: direct queue size in comb/latch/wildcard/continuous forms,
  comb calling `read_size()`, and direct/indirect comb reads of
  `top.holder.q.size()`. Keep all source strings byte for byte. Check exit zero,
  empty stdout, absence of errors, and exactly one typed dependency in SIR.
- In [ImplicitFront.ActualImplicitContainerReadsRemainDiagnosed](../../tests/unit/implicit_front_test.cpp),
  the first four sources change to successful compilation and exactly one
  typed container binding with no token-word trigger. They are wildcard direct
  size, wildcard `read_argument(q)`, comb `read_global()` and latch
  `read_global()`. Keep the test name, all five source forms, and its fifth
  `assign observed=read_global()` rejection and diagnostic checks.

No source inputs, tests, budgets or assertions unrelated to these eleven
judgments are removed, renamed or weakened. The three invalid-size CLI errors
and the captured automatic-queue check remain. Contributing selects, unsupported
layouts and ref bindings, and continuous called-container bodies remain covered
by negative cases in the new matrix. P1 scope, pass gates and external counts
are unchanged.

## Reason and verification

The approved contract supplies typed whole-value observation after static
dependency selection (IEEE 1800-2023 9.2.2.2.1/9.4.2.2; the detailed value rule is
the approved project interpretation). The old static-range-only limitation no
longer applies to these inputs. This changes acceptance, not a stored simulation
value or a comparison simulator's result.

All eleven unchanged input strings were separately executed: each exits zero
with no stdout and emits exactly one `implicit-container` SIR binding. The
retained continuous-function-body source still exits 2 with its original
unsupported diagnostic. Apply only this approved migration separately
from runtime changes, rerun both validators and the unfiltered dev suite, and
update measured counts separately from successful JUnit. Do not represent a
filtered or pre-migration run as a complete passing dev gate.
