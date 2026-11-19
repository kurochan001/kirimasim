# slang e81bcd25 dependency update

[日本語](slang-e81bcd25-update.ja.md) · 2026-11-19.

The unmodified frontend is pinned to `e81bcd2560d54dfd17524df70c0f297e759509da`, the upstream `master`
head checked on 2026-11-19, 68 commits after [`8ffdcdd8`](slang-8ffdcdd8-update.md). Upstream CI passed on
that commit (19 checks). It includes the fix of
[slang #2117](https://github.com/MikePopoloski/slang/issues/2117), reported from kirimasim (issue #110):
`Token::operator==` gave a wrong answer when only one of the two tokens had an info pointer, which made some
builds (the dev and ASan presets on macOS) reject a subroutine formal argument written on its own line after
enough spaces, and with it the UVM package. The other changes that kirimasim's tests or users can see are:

- The UDP edge-coverage check scales to UDPs with many inputs, so a 33-input UDP no longer runs into the
  time limit of the front end.
- Class type names are printed with their defining scope, as 20.6.1 says for `$typename`. kirimasim's
  diagnostics that name a type take that text from slang, so the unsupported built-in classes are now
  `std::process`, `std::mailbox#(untyped)` and `std::semaphore`.
- Methods that modify their object (`push_back`, `pop_front`, `insert`, `sort` and the like) require a
  modifiable lvalue and count as writes in slang's analysis. Two such calls on one queue within one argument
  list now give `-Wread-write` (13.5, 11.4.2); the 80 queue tests that did this were rewritten in `bd81035d`
  (issue #138).
- When several defparams target one parameter, the last one in the source text is used, with a note that
  points at it (23.10.1; issue #59 asks for this). The warning `duplicate-defparam` stays, with new wording.
- A `new` expression is accepted only as a variable initializer or as the right side of a blocking
  assignment (`MisplacedNew`); the generated tests that wrote `array[index] <= new(99)` were changed in
  `2f67b81f`.
- New errors for a module with a wildcard port list that also has a parameter port list or header imports,
  for missing values of unnamed subroutine arguments, and for non-static class members used from a class
  that does not derive from theirs. The messages of `ConstEvalNoIfItemsMatched` and
  `ConstEvalIfItemsNotUnique` changed, and an x or z index into an associative array in constant evaluation
  is now the warning `invalid-assoc-index` instead of an error.
- Fixes of internal errors and of constant evaluation (if-else-if chains, negative repeat counts, packed union
  members, `inside` with unbounded ranges, queue `delete` with a non-constant index), and of elaboration
  (parameters declared in generate regions, nested modules instantiated in generate blocks, wildcard
  exports, nettype aliases, enum operators).

slang's distributed MIT text is unchanged and its own dependencies are not
([license check](../measurements/2026-11-19/slang-e81bcd25-licenses.json)). No new dependency or installed
tool is introduced.

## Integration

- No change of kirimasim's source was needed: the front builds against the new commit as it is.
- The flags of the compatibility profiles and the diagnostics that are errors by default (the `compat`
  blocks of slang's diagnostic list) do not change. The diagnostic list itself gained the errors and changed
  the wordings above. The [current pin inventory](../measurements/2026-11-19/compatibility-e81bcd25-profiles.json)
  is the one of the previous pin with the new commit.
- `tools/gen_unsupported_matrix.py` (version 21) expects the scoped names of the three built-in classes, in
  `tests/cli/unsupported_matrix.stderr`. This existing expectation has to change with the pin, since each
  slang prints only its own form.

## Before/after comparisons

Both binaries build the same kirimasim code (`bd81035d`) on macOS arm64 with AppleClang 21, the new one
against the new slang ([comparisons](../measurements/2026-11-19/slang-e81bcd25-comparison.json)):

- Own tests (dev): **13,858** with both; all pass with the old pin, and with the new slang all but
  `cli/options`, whose unsupported matrix expected the unscoped class names. This change updates it.
- The frozen P1 population (kirimasim-tests `41085fe3`, 1,466 cases, dev): **1,456 passes, no failures and
  ten unsupported** with both, the same verdict for every case and the same judge output for the ten.
- sv-tests at `15490e95`, release builds as the nightly uses: **253 of 344** with both, and **593 of 593**
  read by `kirimasim parse`, the same result for every test. With dev builds the old slang rejects the UVM
  package (issue #110): 529 of 593 are read, and the 37 UVM simulation tests stop with an error rather than
  at an unsupported construct. The new slang reads all 593 with dev builds too; the simulation count stays 253.
- The 188 extended probes (dev, against Icarus 13.0): no required gap and no independent failure with either,
  and the same 25 reference mismatches. One verdict changes: `dense_udp_coverage`, which reached the time
  limit in slang's UDP edge-coverage check, now passes, so 187 are evaluated and only `real_net` is out of
  scope ([probes](../measurements/2026-11-19/slang-e81bcd25-probes.json)).
- The new tests `tests/suite/syntax/subroutine_formal_layout_*` (made by
  `tools/gen_subroutine_formal_layout_matrix.py`) declare 40 functions and tasks each, in a package, the
  module, an interface and a class: string and int formals, with an implicit or a written `input` direction,
  the second formal on its own line after 1, 13, 14, 18 or 40 spaces. With the old slang the dev build fails
  all four, at the string formals with an implicit direction and 14 or more spaces (in the package, the
  module and the interface the 14-space function passes); the old release build and both builds of the new
  slang pass all four.

## Not confirmed

- CI's Linux jobs on the new pin: they run when the change is pushed. The nightly `slang-main` job has
  tested upstream `5216a3d37` (2026-11-16), not `e81bcd25`.
- Which other open issues marked "(slang)" the new pin resolves. Issue #59 behaves as it asks in one check
  (the second of two defparams is used); it and the rest need their own matrices before they are closed.
- Issue #137 (a signed packed array that loses the bounds of its remaining dimension) reproduces with the
  new slang as well.
