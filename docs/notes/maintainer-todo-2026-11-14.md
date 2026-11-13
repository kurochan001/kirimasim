# Maintainer to-do list, 2026-11-14

[日本語](maintainer-todo-2026-11-14.ja.md) · 2026-11-14. Prepared by Claude Code for the maintainer, as the work moves
from the Mac to the WSL2 Linux machine for a while. Claude Code's side is in
[claude-code-handoff-2026-11-14](claude-code-handoff-2026-11-14.md).

Items are grouped by what they wait for. Within a group, the first is the most useful to do first.

## A. Moving to the WSL2 machine

1. **Set the committer of the WSL2 clone to the GitHub noreply address.** WSL2's Claude Code found the clone at
   `77c27531` and clean on 2026-11-14, so no fresh clone is needed. Its global git identity is not the noreply
   address; Claude Code passes it on each commit for now.
2. **Install the tools Claude Code will need there.** Any new installation is your decision.
   - A compiler from the README's "Building" list, CMake, Ninja and Python 3.
   - clang-format and clang-tidy of LLVM 23 (your decision of 2026-11-14; WSL2 had 22.1.8). The Mac already has
     Homebrew's 23.1.4, and CI moved to 23 in the same push.
   - Icarus Verilog 13.0, for the fixed probe gate. WSL2 has `v13_0-25-g07dd50d37`, 25 commits after the 13.0 tag.
     Whether it behaves like the 13.0 release is not confirmed. Decide whether to install the release or to use
     this build.
   - Verilator 5.052, for comparisons. WSL2 has 5.050.
   - `gh`, logged in, so Claude Code can read CI and issues.
3. **Copy the private folder to WSL2 if Claude Code should use it there.** It is the folder outside the
   repositories, holding the standard, the review sheets, the AI pre-review and the slang drafts. It must stay out of
   every repository. Without it, Claude Code on WSL2 cannot read the standard or prepare slang posts.
   - Copy it from WSL2 with rsync over SSH (the Mac's Remote Login must be on). Do not use the git relay: a branch
     holding private files could reach GitHub by mistake.
   - If you want Claude Code on WSL2 to start with the Mac's memory, copy Claude Code's memory folder for this
     project the same way. Which folder name it reads on WSL2 is not confirmed.
4. **The git relay works from WSL2** (checked on 2026-11-14). How Claude Code uses it is in section 7 of the
   handoff note.
5. **Keep the machine awake during a full local CI** (about 1.5 hours on the Mac). Sleep caused false timeouts
   before.

## B. The human review for M8 (the main thing P1 waits for)

P1's exit list needs no new feature. It waits for the review by people: **0 of 145 checks**, the
[M8 draft](p1-m8-report.md).

1. **Read the AI pre-review.** It is in the private folder, under `spec-sampling/2026-11-07/ai_prereview/`; start with
   `SUMMARY.md`. It covers all 145 checks:

   | Group | Match | Error | Cannot decide |
   | --- | --- | --- | --- |
   | 131 tests | 104 | 23 | 4 |
   | 14 generators | 3 | 11 | 0 |

   It is Claude Code's reading, not yours. It does not count as the human review.
2. **Record your own result.** Write each one in the `result`, `error_kind` and `note` columns of `sample.csv` and
   `generators.csv` (or at the end of each sheet). Your result may differ from the pre-review.
3. **Decide how to proceed before Claude Code fixes anything.** Confirmed errors trigger the 20% rule of TEST_SUITE:
   the chapter's sampling rises to 20%, and every test of the same mechanism is reviewed. If the pre-review's
   errors all hold, that covers about ten chapters and most of the sampled generators. Points to settle:
   - **Points the standard leaves open.** Tests and generators fixed to one result where the standard leaves
     evaluation order, process order or initialization order open. Either move them to `choice` (with
     `:nondeterministic: yes` and `.choice`), or change the tests.
   - **Lower-case hexadecimal.** `%h`, `%x` and `hextoa` print a to f in lower case. The standard's examples do,
     but its text does not say so. Either accept it as `spec`, or treat it as a choice.
   - **Initialized static variables without `static`.** Several generators declare such variables in procedural
     blocks, which 6.21 forbids; slang only warns. Decide whether to fix them.
4. **Judge M8.** Do it after the review and Claude Code's re-measurement on the revision you choose (Claude Code's
   list, item 3).

## C. Decisions on issues

| Issue | Question | Claude Code's note |
| --- | --- | --- |
| 126 | Every foreach loop variable omitted: does the body run once, or not at all? | Run time, Icarus 13.0 and Verilator 5.052 give 0; slang's constant evaluation gives 1. 12.7.3 does not say. If 0, it is a slang report; if 1, the run-time fix and a test change. |
| 93 | 18.7.1, last rule: does `local::r` name the class's static `r` or the handle `r`? | Two readings fit the text; slang takes the handle. |
| 94 | May a constraint compare a non-random `string`? | slang now accepts it. 18.3 speaks of integral types. |
| 124 | A declaration left open at the end of one file, finished in the next | Passes with `--single-unit`, fails by default; slang reads each file separately. |
| 34 | Ask slang to place the fork-in-function error at `fork`? | Optional; the tests no longer check the place. |

## D. slang posts (each one needs your read and "go")

Claude Code reviews each draft first, then shows it to you. A post never names kirimasim.

1. **Wait for a reply on slang issue 2117** (posted 2026-11-14, no reply yet).
2. **Then the subnormal real literal report.**
3. **Then the unique value range report** (issue 118).
4. **Later:**
   - The `ato*` leading underscore draft and the block parameter defparam draft.
   - Reports for issues 59, 68 and 125, which Claude Code has not drafted yet.

## E. Approvals Claude Code may ask for

- **L5 (from the 2026-11-11 review).** Generate Appendix A of the SIR v1 proposal from `ops.def`, then turn on
  `--strict` in CI. The CI change needs your approval.
- **Issue 28 (tier-1 speed).** Whether to spend time on it now.

## F. Housekeeping on the Mac (whenever convenient)

- **Untracked handoff documents in the Mac's main worktree:** `docs/CLAUDE_CODE_HANDOFF_*`,
  `docs/CODEX_HANDOFF_2026-10-31.md` and `docs/notes/class-property-nba-proposal*`. They were kept out of git on
  purpose. Keep them or delete them.
- **Three detached worktrees under `kirimasim-tests-work/`:** `cross-20261030-latest`, `cross30-event-iff-core` and
  `issues40-44-core-latest`. They are clean and hold no branch. Delete them if they are no longer needed.
- **The two unmerged branches deleted on 2026-11-14** are saved as a git bundle in the private folder (`archive/`).
  Keep it until you are sure they are not needed.
