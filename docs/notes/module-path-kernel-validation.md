# Scalar module-path kernel validation

[日本語](module-path-kernel-validation.ja.md). 2026-11-01.

The first native increment of the [approved specify contract](specify-p1-proposal.md)
adds the independent endpoint reference, twelve-transition normalization, and
Context-owned scalar scheduling through the original physical strength origin.
This increment alone does **not** admit source-language specify execution or
complete P1. SIR and frontend integration follow separately. Existing rejection
expectations and the frozen external inventory are unchanged.

The 78 new semantic tests cover all 60 transition/arity combinations, latest
source and same-time minimum selection, epoch-zero and qualified edges, wide
source addresses, max(distributed arrival, path deadline), Active zero/overdue
publication, full-contribution inertial replacement, force across pending
updates, metadata-only changes, independent Contexts, malformed registrations,
unrepresentable delays and allocation-failure retry. The complete shared unit
executable passes 1,018 tests; public headers compile as C++20.

[Nine compiled native controls](../measurements/2026-11-01/module-path-kernel-native-controls.json)
all built and failed semantic tests (not the mutation registry checker): wrong
X departure, oldest source, wrong same-time choice, additive delay, zero in
Inactive, stale generation, missing ordinary source capture, missing strength
source capture, and suppression of functional arrivals under force. Sources
were restored byte-for-byte and the 78-test baseline passed again. The records
contain relative source hashes. The controls are registered in `tools/mutants.py`;
`--only 'scalar module path:'` selects them for the ordinary full-suite runner.

Targeted clang-tidy (LLVM 23.1.4), banned-construct and mutation-anchor checks
pass. Full dev/count refresh and cross-profile integration validation follow
after the source bridge; this record makes no full-local-CI claim.
