# Scalar specify expectation migration

[日本語](specify-expectation-migration.ja.md). 2026-11-02.

The maintainer separately approved this exact migration after native execution
was demonstrated. The [approved scalar contract](specify-p1-proposal.md) sections
1–8 covers the module path; chapter 30 (in particular 30.5–30.7) is the relevant
IEEE 1800 reference. The edition limitation in that contract remains recorded.

Keep `tests/cli/unsupported_matrix/specify_path.sv` byte-for-byte unchanged
(SHA-256 `3f827e060bc17d4aa07ef1ce4876afccaeb4b601ad0de294aa87cb10f1b792f3`).
Move its classification to `unsupported_matrix.accepted` and remove only its
old rejection line from `unsupported_matrix.stderr`. Update the generator's
`ACCEPTED` set, retaining generation version 19 because the source rules and
inputs did not change. The CLI runner requires exit 0 and empty stdout instead
of exit 2; all other retained rejection checks remain active. This input's
path settles at tick 1. It is not an XFAIL, ignored test or fabricated PASS.

Timing behavior is covered separately by the new 121-case native matrix and
shared kernel/SIR/observation tests, including the frozen P1 probe's early 0 and
late 1. Keep this metadata/expectation migration in a separate commit from `src/`.
