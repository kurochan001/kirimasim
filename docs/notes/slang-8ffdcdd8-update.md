# slang 8ffdcdd8 dependency update

[日本語](slang-8ffdcdd8-update.ja.md) · 2026-11-13.

The unmodified frontend is pinned to `8ffdcdd8ad237823fbfd72d495082d76e92d4015`, the upstream `master`
head checked on 2026-11-13, 6 commits after [`62affeb0`](slang-62affeb0-update.md). Upstream CI passed on
that commit (19 checks). It includes the fix of
[slang #2116](https://github.com/MikePopoloski/slang/issues/2116), reported from kirimasim (issue #54):
in `a ? b : c -> d` the conditional operator now binds tighter than the implication, as Table 11-2 of 11.3.2
orders them, where slang had put the implication into the false branch of the conditional operator. The
other changes are:

- a use-after-free in slang's diagnostic visitor for nested generic classes is fixed;
- the constant evaluation of selects of large local variables is faster;
- the options that write dependency files move from slang's driver library to its own command-line
  tool, which kirimasim does not use; kirimasim passes none of those options (CLI.md 2.1);
- a utility to capture diagnostics and a change of slang's syntax rewriter, which kirimasim does not
  call.

slang's distributed MIT text is unchanged and its own dependencies are not
([license check](../measurements/2026-11-13/slang-8ffdcdd8-licenses.json)). No new dependency or installed
tool is introduced.

## Integration

- No change of kirimasim's source was needed: the front builds against the new commit as it is.
- The flags of the compatibility profiles and the list of diagnostics that are errors by default do not
  change, and slang's list of diagnostics is the same; the
  [current pin inventory](../measurements/2026-11-13/compatibility-8ffdcdd8-profiles.json) is the one of
  the previous pin with the new commit.

## Before/after comparisons

Both binaries are dev builds of the same kirimasim code (`f3ae2b37`), the new one against the new slang
([comparisons](../measurements/2026-11-13/slang-8ffdcdd8-comparison.json)):

- Own tests: **13,830** with both; all pass with the old pin, and with the new slang all but
  `tools/readme_counts`, which counts the files of the working tree: it held the new tests of this change,
  which the recorded counts do not include yet. It does not depend on slang. The commit that moves the pin
  is checked again by `tools/local_ci.py`.
- The frozen P1 population (kirimasim-tests `41085fe3`, 1,466 cases): **1,456 passes, no failures and ten
  unsupported** with both binaries, the same result for every case.
- sv-tests at `15490e95`: **253 of 344** with both, the same result for every test.
- All **188** extended probes keep their native status, exit code and output
  ([probes](../measurements/2026-11-13/slang-8ffdcdd8-probes.json)).
- With the old pin, `a ? b : c -> d` and `a ? b : c <-> d` gave the values of `a ? b : (c -> d)` and
  `a ? b : (c <-> d)`, which differ from those of `(a ? b : c) -> d` and `(a ? b : c) <-> d` in 8 of the 16
  values of the four bits each; with the new pin they give the latter. The new tests `tests/suite/values/conditional_implication_*` (made by
  `tools/gen_conditional_implication_matrix.py`) hold eight forms of `?:` beside `->`, `<->` and `||`
  for all 16 values, in five places (a procedural assignment, a continuous assignment, the return value
  of a function, a localparam of a generate loop, the condition of an `if`). All five fail with the old
  pin, in 13 of their 16 lines each, and pass with the new one.

## Not confirmed

- CI's Linux jobs on the new pin: they run when the change is pushed.
- Release builds were not compared; the comparisons above are dev builds.
