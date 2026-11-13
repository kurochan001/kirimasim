# Claude Code handoff, 2026-11-14

[日本語](claude-code-handoff-2026-11-14.ja.md) · 2026-11-14. Written by Claude Code (claude-opus-5-5) on the macOS
machine, for the Claude Code session that continues on the maintainer's WSL2 Linux machine. The maintainer's own list
is [maintainer-todo-2026-11-14](maintainer-todo-2026-11-14.md).

Claude Code's memory on the Mac does not move with the work. Everything a new session needs is in this note,
[CLAUDE.md](../../CLAUDE.md) and the issues. Earlier handoffs stayed private and untracked; this one is public at the
maintainer's request, so it names no private paths, machines or people.

## 1. How to work with the maintainer

These are standing instructions from the maintainer, on top of CLAUDE.md.

- **Language.** Reply in Japanese. Commits, issues and repository documents are in English, with a Japanese version
  for every document (`X.md` and `X.ja.md`, edited together; English is authoritative).
- **"おすすめで".** When the maintainer answers "おすすめで" or "おすすめでOK", proceed with the recommendation you
  stated, then report. It approves the proposal it answers, and nothing else: never take it as approval of spec
  text, expected values, slang posts or deletions that you did not put to the maintainer.
- **Who decides.** Specifications (`docs/spec/`), existing expected values, public wording, CI policy, new
  dependencies and new tools are decided by the maintainer. Propose first; change after approval.
- **Commits.**
  - One purpose per commit; English messages, the first line at most 72 characters.
  - Changes of existing expectations and implementation changes go in separate commits.
  - Never weaken an existing `EXPECT_*`/`ASSERT_*`.
  - The trailer is `Assisted-by: Claude Code:<model id>`. Never add `Co-Authored-By` or `Signed-off-by`.
  - `Approved-by:` only on commits the maintainer approved. A commit touching `docs/spec/` or an existing
    expectation needs it (`check_expected_change.py --require-approval` enforces it).
- **Git.**
  - Stage files by name. Never run `git add -A`, `git stash`, `git reset` or `git clean`.
  - Push only after `tools/local_ci.py` passes on the commits to push. Report its summary and check `gh run list`.
- **Documents.** Add one CHANGELOG line (English and Japanese) for each user-visible change. Write "not confirmed"
  (未確認) for anything not checked.
- **Bugs.**
  - A fix states the rule, the one place that implements it, and the other entries through that place.
  - Bug tests are generator-made matrices with mutants (`tools/mutants.py`).
  - Close an issue only when the matrix covers its symptoms. The closing comment states the results for the
    neighbouring shapes, and anything left over becomes a new issue.
- **Other tools.** Icarus Verilog and Verilator are black boxes: run them, never read their sources.
- **slang.** Upstream posts follow section 5. In this repository, refer to slang issues as plain text ("slang
  issue 2117"), never as a URL or `owner/repo#N`, so that no backlink appears on slang's side.
- **The standard.** A licensed copy of IEEE 1800-2023 is kept privately. Read it locally (for example through
  `pdftotext` into a scratch folder). Never copy its text anywhere: give clause numbers and your own words.
- **Private material.** The private folder outside the repositories holds the standard, the human-review sheets,
  the AI pre-review and the slang drafts. It is never committed. It is on the Mac; whether a copy reaches the WSL2
  machine is the maintainer's step (their list, item A), by rsync and never through the git relay (section 7).

## 2. State on 2026-11-14

- **main.** Includes `0f7092b8` (matrix of shortreal/real conditionals, issue 61), `86bb4b36` (matrix of foreach with
  omitted loop variables, issue 65), `a3b2b6e6` (counts: 13,857 tests, all passing in two dev runs), `a1c8568b` and
  `948cb490` (CI and the nightly moved from LLVM 22 to 23, approved by the maintainer) and the commit that adds this
  note. They reach `main` only after a full local CI on macOS passes, which was still running when
  this note was written; until then the relay branch `from-mac/handoff-2026-11-14` (section 7) holds them. Right
  after that push, the Mac session closes issues 61 and 65 with the matrix results; check that they are closed.
- **P1.** The [closed exit list](../spec/P1_SCOPE.md#closed-exit-list-approved-2026-11-07) asks for no new feature.
  What remains is evidence and review ([M8 draft](p1-m8-report.md)):
  - **Human review, 0 of 145 checks.** This is the maintainer's. An AI pre-review is ready for them in the private
    folder; it is not the human review and must never be recorded as one (section 3).
  - **Measurement on the revision to be judged.** This is Claude Code's.
    - Dry run at `77c27531` on macOS arm64:
      - Fixed probe gate against Icarus 13.0: exit 0; 186 evaluated, 0 required gaps, 25 reference mismatches,
        0 independent failures.
      - Population: 1,456 of 1,466 (0 failed, 10 unsupported).
      - The `fuzz` job of the last eight nightly runs (2026-11-05 to 2026-11-12): all successful.
    - Linux at this revision: not confirmed.
    - sv-tests on both systems: read the scheduled nightly on the pushed head.
- **Open issues.** 42 at the time of writing; most are slang gaps titled "(slang)".
  - Kirimasim-side items that wait for the maintainer:
    - 93: reading of the last rule of 18.7.1.
    - 94: whether a non-random string comparison may appear in a constraint.
    - 124: a declaration left open at the end of one file.
    - 126: a foreach with every loop variable omitted runs its body once (slang's constant evaluation) or not at
      all (run time, Icarus 13.0, Verilator 5.052). The standard does not say which.
  - 28: tier-1 speed. It is not a P1 exit condition.
  - 97 moved to P5.
- **Found and not explained.** No input reaches the conditional case of `untrustedConstant` in
  `src/front/lower_expr.cpp`. A mutant that turns it off changes no result in the new matrix, in
  `values/conditional_shortreal_context`, or in a delay or real product of constant conditionals. Whether it is dead
  code is not confirmed.

## 3. The AI pre-review of the 145 human checks

The maintainer asked Claude Code to review. The rules (TEST_SUITE, "Sampling by people") ask for a person, so the
review was done as a pre-review. It is kept in the private folder under `spec-sampling/2026-11-07/ai_prereview/`:
`SUMMARY.md`, `INSTRUCTIONS.md`, and one CSV per group. The person's columns of `sample.csv` and `generators.csv`
and every `:spec_check:` were left alone.

| Group | Count | Match | Error | Cannot decide |
| --- | --- | --- | --- | --- |
| Tests | 131 | 104 | 23 | 4 |
| Generators | 14 | 3 | 11 | 0 |

- **Test errors.** Most are checks that cannot fail, or clause numbers. One is an expected value: `hextoa` letter
  case.
- **Generator errors.** Most rest on points the standard leaves open while the test is fixed to one result, instead
  of TEST_SUITE's `choice`. Several declare initialized static variables in procedural blocks without `static`,
  which 6.21 forbids (slang only warns).

When the maintainer returns results:

1. Change only the tests they checked to `:spec_check: human`.
2. Fix errors in separate commits, after showing the clause and getting approval. Expected-value changes follow
   CLAUDE.md, "Changes of expected values go in separate PRs".
3. Apply the 20% rule of TEST_SUITE to each chapter with a confirmed error.
4. Fill in the M8 report.

## 4. What to do next, in order

1. **Bring the WSL2 clone up to date.** On 2026-11-14 it was at `77c27531`, clean. After the push, pull `main`;
   configure and build `dev` (slang comes through FetchContent at the pinned commit). Then run
   `python3 tools/local_ci.py --llvm-bin <directory with clang-format and clang-tidy>` once.
   - **clang-format and clang-tidy: LLVM 23 everywhere.** On 2026-11-14 the maintainer decided to align on 23.
     - CI moved from 22 to 23 in `948cb490`: the Linux build, tsan and lint in `ci.yml`, and the nightly's Linux
       jobs. The minimum-compiler jobs keep their versions.
     - The Mac has Homebrew's LLVM 23.1.4. The WSL2 machine moves from 22.1.8 to 23.
     - The first CI run on 23 is the check that lint passes with 23 there; before, the Mac's 23 and CI's 22 had
       accepted the same code. Read that run.
   - Keep the machine awake during local CI: host sleep caused false timeouts before.
2. **Confirm the push of 2026-11-14.**
   - The CI of the pushed head is green.
   - Issues 61 and 65 are closed.
   - The next scheduled nightly on that head: read `svtests`/`svtests-macos`, `p1-gates`, `fuzz` and the mutant
     shards.
3. **Linux measurements for M8, on the same head.**
   - Run the fixed probe gate:
     `python3 tools/icarus_feature_probe.py --extended --gate tests/gates/p1_probes.txt --require-parity`, with
     Icarus Verilog 13.0.
   - Run the population: `python3 tools/p1_population.py run`, with kirimasim-tests at the commit named on the first
     line of `tests/gates/p1_population.txt`.
   - Whether Icarus 13.0 and Verilator 5.052 are installed on the WSL2 machine is not confirmed. In October it had
     Icarus 14.0 devel and Verilator 5.053 devel builds. Ask the maintainer before installing anything.
   - Hand the results to the Mac through the git relay (section 7) if a comparison with the macOS run is wanted.
4. **Wait for the maintainer's review results (section 3).** Do not start the 20%-rule work before they decide how
   to proceed.
5. **slang queue (section 5)**, one post at a time, each after the maintainer's read and go.
6. **Smaller items that need a proposal first.**
   - L5 rest from the 2026-11-11 review: generate Appendix A of `docs/notes/sir-v1-proposal.md` from `ops.def`
     (`tools/sir_schema_drift.py` already parses it), then `--strict` in CI. The CI part needs approval.
   - The unexplained `untrustedConstant` branch (section 2).
   - Issue 28 if the maintainer wants speed work.

## 5. slang reports

These rules hold for every post, comment or reopen on slang:

- **What to post.** Only the title and the body after `---` of a draft. Never name kirimasim, related projects,
  accounts or local paths: write "a program that embeds slang".
- **What to include.** The AI-use disclosure line. State whether Icarus and Verilator accept the case (run them).
- **Review.** Claude Code reviews every post first, including posts written by other agents.

The drafts are in the private folder (`reports/`).

- **Posted:** slang issue 2117 (`Token::operator==` reads the wrong union member when only one token has an info
  block), on 2026-11-14. No reply yet.
- **The maintainer's order:**
  1. After a reply on 2117: the subnormal real literal draft (`slang-issue-draft-subnormal-literal`).
  2. Then the unique value range (`slang-issue-draft-unique-value-range`; issue 118 here). It was rechecked on
     slang `8ffdcdd8` and has a fix and a unit test.
- **Later candidates:**
  - The `ato*` leading underscore draft and the block parameter defparam draft.
  - Not drafted yet: issue 59 (defparam keeps the first, 23.10.1), issue 68 (static locals of constant functions,
    13.4.3), issue 125 (shortreal `?:` in constant evaluation, 11.4.11), and issues 104, 95, 91 and 89.
  - Issue 110 closes when its slang fix is pinned and the dev and asan builds parse UVM.

## 6. Environment notes

- **The pin.** slang `8ffdcdd8` (`cmake/Dependencies.cmake`). After a pin move, any build that takes slang from
  `FETCHCONTENT_SOURCE_DIR_SLANG` must be reconfigured to a checkout of the new pin, or it silently tests the old
  one.
- **Counting tests.** After adding or removing tests:
  1. `ctest --preset dev -j8 --output-junit <file>`
  2. `python3 tools/readme_counts.py update --build build/dev --junit <file>`
  3. A second full run, and `update` again.
  4. Commit.

  Never rebuild `build/dev` while a ctest run is using it.
- **Mutant checks.** Apply the mutant, build, run the targeted tests, restore. Use a build that no running job uses.
- **Shell (zsh).** Unquoted variables are not word-split. A command starting with `=` (for example `echo ==`) fails.
- **CI.** Test jobs check out without history, so a tool test that reads old commits fails there.
- **GitHub runners.** The macOS queue is slow and has cancelled jobs for lack of a runner. Rerun those jobs only if
  no later run covers the same commits.

## 7. The git relay between the Mac and WSL2

A bare repository on the Mac (`Projects/kirimasim-handoff.git` under the Mac user's home) is a private relay
between the two machines. In the Mac clone it is the remote `handoff`.

- **Reaching it from WSL2.** Add it once over SSH, using the Mac's LAN address (`.local` names may not resolve from
  WSL2) and key authentication:

  ```sh
  git remote add handoff <mac user>@<mac LAN address>:Projects/kirimasim-handoff.git
  ```

  The Mac's Remote Login must be on. On 2026-11-14 WSL2 reached it: `git fetch handoff` worked, and WSL2's Claude
  Code answered on `from-wsl/handoff-2026-11-14`.
- **Never push its branches to origin.** GitHub is public.
- **The branch convention** (in use since 2026-10-17):
  - The side that asks pushes a branch `<topic>` holding `handoff/<topic>/README.md`, with the steps and its own
    result to compare. On the Mac, the branch was made from `origin/main` in a scratch worktree, on a local branch
    `wsl/<topic>` deleted after the push.
  - The other side answers on `from-wsl/<topic>` (from WSL2) or `from-mac/<topic>` (from the Mac).
  - Keep home paths and machine names out of the result files.
- **What it is for.**
  - Commits not ready for `main`, for example a branch whose local CI has not passed.
  - Comparisons between macOS and Linux, such as the Linux measurements for M8 against the macOS dry run in
    section 2.
- **What it is not for: private material.** The relay holds kirimasim's history, so a branch with private files
  could reach origin by mistake. Copy the private folder over the same SSH connection instead, for example
  `rsync -a <mac user>@<mac LAN address>:Projects/<private folder>/ ~/Projects/<private folder>/`. The same works
  for Claude Code's memory folder on the Mac. Which folder name Claude Code reads on WSL2 depends on the clone's
  path and is not confirmed.
- **What is on it.** 16 branches from October (`slang-*`, `memsafety-stack`, `from-wsl/*`, `from-mac/*`). They are
  records of earlier checks and need nothing.
