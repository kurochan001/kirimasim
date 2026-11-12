# slang 62affeb0 dependency update

[日本語](slang-62affeb0-update.ja.md) · 2026-11-12.

The unmodified frontend is pinned to `62affeb02b95df96a69c36ee5a7c3d6dc8878554`, the upstream `master`
head checked on 2026-11-12, 16 commits after
[`6fbb158f`](slang-6fbb158f-update.md). Upstream CI passed on that commit (19 checks). It includes the fix of
[slang #2104](https://github.com/MikePopoloski/slang/issues/2104), reported from kirimasim: a UDP table row
with no fields is reported as a syntax error and no longer reaches an assertion. The other changes that
kirimasim sees are:

- a `defparam` that targets a parameter with no type takes the type of the new value
  ([slang #2105](https://github.com/MikePopoloski/slang/issues/2105)), and one that assigns a NaN or `$` is
  handled;
- the items of a `case` that a constant case expression does not select are treated as unevaluated, as the
  branch of an `if` that is not taken (#2109), so they give no warnings;
- a function call that is evaluated speculatively reports no diagnostics;
- a reversed range select no longer makes an invalid node;
- inline enum values are declared from the syntax and enum types are made on demand.

slang's distributed MIT text is unchanged and its own dependencies are not
([license check](../measurements/2026-11-12/slang-62affeb0-licenses.json)). No new dependency or installed
tool is introduced.

## Integration

- No change of kirimasim's source was needed: the front builds against the new commit as it is.
- The flags of the compatibility profiles and the list of diagnostics that are errors by default do not
  change; the [current pin inventory](../measurements/2026-11-12/compatibility-62affeb0-profiles.json) is the
  one of the previous pin with the new commit. slang gains two diagnostics, both plain errors
  (`DefParamImplicitType`, `RecursiveEnum`).

## Before/after comparisons

Both binaries are dev builds of the same kirimasim code (`e2f6fc41` on main; they were built one commit of
documentation earlier in the history, with the same sources), the new one against the new slang
([comparisons](../measurements/2026-11-12/slang-62affeb0-comparison.json)):

- Own tests: **13,750** with both; all pass with the old pin, and with the new slang all but
  `tools/readme_counts`, whose recorded counts were being refreshed for that source and do not depend on
  slang. The commit that moves the pin is checked again by `tools/local_ci.py`.
- The frozen P1 population (kirimasim-tests `41085fe3`, 1,466 cases): **1,456 passes, no failures and ten
  unsupported** with both binaries, the same status for every case.
- sv-tests at `15490e95`: **253 of 344** with both, no changed result.
- All **188** extended probes keep their native status and output
  ([probes](../measurements/2026-11-12/slang-62affeb0-probes.json)).
- A UDP with the table row `: 1 ;` ended the dev build of kirimasim with slang's assertion
  `range.start()` with the old pin, and gives `error: expected UDP symbol` with the new one.
- `module m; parameter W = 8'shFF; ... endmodule` with `defparam a.W = 16'sh7FFF;` printed `W=-1` and
  `$bits(W)=8` at run time with the old pin, though a generate condition on `W` saw 32767; with the new pin
  it prints `W=32767` and `$bits(W)=16`, as an override in `#(...)` does. The new test
  `tests/suite/hier/defparam_untyped_matrix` (made by `tools/gen_defparam_untyped_matrix.py`) holds this for
  four values, three places of the instance and both ways of overriding; twelve of its lines differ with the
  old pin.

## Not confirmed

- CI's Linux jobs on the new pin: they run when the change is pushed.
- Release builds were not compared; the comparisons above are dev builds.
