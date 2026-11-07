# slang 6fbb158f dependency update

[日本語](slang-6fbb158f-update.ja.md) · 2026-11-08.

The unmodified frontend is pinned to `6fbb158f156436a1cb4d0ce9de5373a416a05d4c`, the upstream `master`
head checked on 2026-11-08, 14 commits after
[`d02b7cda`](slang-d02b7cda-update.md). Upstream CI passed on that commit (19 checks). It includes the fix of
[slang #2093](https://github.com/MikePopoloski/slang/issues/2093), reported from kirimasim: a program's
parameters can now be named from outside it, as by a `defparam`; only its nets and variables cannot. The
other changes are a left-justified format field padded on the right, an enum element range that must be an
integer literal (now an error, as the grammar of 6.19 asks), and fixes of a recursion on a virtual interface,
a scoped self-reference and an iterator invalidated by a lazy class specialization.

slang's distributed MIT text is unchanged and its own dependencies are not
([license check](../measurements/2026-11-08/slang-6fbb158f-licenses.json)). No new dependency or installed
tool is introduced.

## Integration

- `GenericClassDefSymbol::specializations()` now returns a copy of the list of specializations, so
  `Lowerer::hasGenericClassStatics` takes them as type pointers. kirimasim already copied the list before
  querying members; nothing else of the front changes.
- slang's "no global clocking" (14.14) is now the diagnostic `missing-global-clocking`, a warning with error
  severity by default. kirimasim still rejects such a design with the same message and exit code 2, also
  with `--compat=vcs`; only `-Wno-missing-global-clocking`, which slang accepts, turns it off (checked on
  2026-11-08). The [current pin inventory](../measurements/2026-11-08/compatibility-6fbb158f-profiles.json)
  records it in the default and vcs error lists; the compilation and analysis flags of the profiles do not
  change.

## Before/after comparisons

Both binaries are dev builds of the same kirimasim source (main `33b93822`), the new one with the pin and the
follow-up above ([comparisons](../measurements/2026-11-08/slang-6fbb158f-comparison.json)):

- Own tests: all **13,617** pass with the new pin, as with the old.
- The frozen P1 population (kirimasim-tests `1fd46b2a`, 1,466 cases): **1,456 passes, no failures and ten
  unsupported** with both binaries, the same status for every case.
- sv-tests at `15490e95`: **253 of 344** with both, no changed result.
- All **188** extended probes keep their native status and output
  ([probes](../measurements/2026-11-08/slang-6fbb158f-probes.json)).
- The reproducer of #2093 (a program `prg` with `parameter int Q = 1`, `defparam p.Q = 9` in the module that
  instantiates it) was rejected with the old pin and prints `Q=9` with the new one.

## Not confirmed

- CI's Linux jobs on the new pin: they run when the change is pushed.
