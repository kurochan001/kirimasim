# One gate for a verified Design

[日本語](verified-design.ja.md) · This English text is the main version.

Status: design approved on 2026-11-07 (P2 review M4; the direction is recorded in
[sir-v1-proposal](sir-v1-proposal.md), "Amendments after the P2 review"). Not implemented yet.

## Why

Once format 1 reads SIR written outside kirimasim, the verifier is the only thing between that text and
execution. Today the front verifies every Design it makes, in every build, and the VM checks parts of it
again on load, up to six times, each time only when the Design has a certain feature. A Design that reaches
the VM by another way, such as a unit test that builds SIR by hand, is caught only by those partial checks.

## The type

`sir::VerifiedDesign` holds a Design that has passed the checks, and only `sir::verifyDesign` makes one:

```cpp
namespace kirimasim::sir {
class VerifiedDesign {
public:
    const Design& design() const noexcept;
    Design release() && noexcept;           // the Design back; it must be verified again to return
private:
    explicit VerifiedDesign(Design d);
    friend VerifyOutcome verifyDesign(Design design);
    Design design_;
};

struct VerifyOutcome {
    std::optional<VerifiedDesign> verified; // set exactly when errors is empty
    std::vector<VerifyError> errors;
};

VerifyOutcome verifyDesign(Design design);
}
```

- Errors are values, not exceptions. The Design moves in and out; nothing is copied.
- Only `const Design&` is reachable from a VerifiedDesign, so it cannot change after the check. To change a
  Design, release it and verify it again.
- `verifyDesign` runs, in one place, the checks the VM runs on load today: `verify`,
  `verifyActivationScopes` and the error part of `validatePatterns`. It adds no new check.

## Who makes it and who takes it

- `front::compile` returns it in `CompileResult`; `success` keeps its meaning, and a failed check stays an
  internal error with the same text.
- parse1 (format 1, later in P2) returns a VerifyOutcome.
- `vm::run` takes `const sir::VerifiedDesign&`, and so will tier 2. `--dump-sir` prints its Design; the text
  does not change.
- The VM's repeated checks go. It still computes pattern shapes with `validatePatterns`; an error there
  cannot happen after the gate and becomes an internal error. The VM's load errors about what this run can
  do (no VPI host; class initialization overrides, observation or storage not connected) stay: they are not
  about SIR's validity.

## Tests

- Unit tests that build SIR by hand go through `verifyDesign` before `vm::run`, through a shared helper that
  fails the test on a verification error. No `EXPECT_*` or `ASSERT_*` is weakened.
- Six unit test files pass an invalid Design to `vm::run` on purpose and expect a load error of the VM
  (`vpi_front_test`, `activation_sir_test`, `implicit_container_test`, `vm_nba_event_test`,
  `ff_expression_test`, `class_vm_guard_test`). Each case moves to a test that `verifyDesign` rejects the same
  Design. That changes existing expectations, so it is its own commit, shown with each old assertion beside
  the new one before it is made.
- New tests: each kind of violation is rejected by `verifyDesign`; `vm::run` does not accept a plain Design
  (checked at compile time); a released Design is verified again.
- Every SIR text of `tests/dump_sir` and every other expected result stays the same.

## The verifier's rules

After the gate, these leave what the verifier accepts, one rule per commit with negative tests: order
comparisons of chandles (6.14 allows chandles only equality, case equality, a truth test and assignment),
`Ceq`/`Cne` on reals (table 11-1), and the looser points of sir-v1-proposal 2-a (Syscall and Builtin, Load,
writes through references). The front makes none of the first two today; slang and `compareValues` reject
them.

## Order of commits

1. `VerifiedDesign`, `verifyDesign` and their tests, in `sir` only.
2. The front returns it; the driver and the tests that compile designs use it.
3. The six files' expectations move to `verifyDesign` (shown first).
4. `vm::run` takes it; the repeated checks go; hand-built SIR in unit tests goes through the gate.
5. The verifier's rules, one per commit.

Not measured: the time the repeated checks cost at load today.
