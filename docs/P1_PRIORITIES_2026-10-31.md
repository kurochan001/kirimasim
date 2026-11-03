# P1 completion priorities (2026-10-31)

[日本語](P1_PRIORITIES_2026-10-31.ja.md) · This English text is the main version.

A planning note, not a specification. It orders the remaining work by what
the P1 exit criteria need. The criteria themselves are in
[TEST_SUITE](spec/TEST_SUITE.md) and [P1_SCOPE](spec/P1_SCOPE.md); the
last audit is [P1_CRITERIA_GAPS_2026-10-28](P1_CRITERIA_GAPS_2026-10-28.md).
Measurements below carry their dates; anything not re-measured since is
marked so.

**2026-11-04 follow-up:** [whole-net VPI writes](notes/vpi-net-write-execution.md) now execute the approved immediate subset. Selected/delayed net writes and passive deposits remain work. The dated planning measurements below are retained; current scope is in [P1 Support](P1_SUPPORT.md).

**Class follow-up (2026-11-04):** [explicitly clocked object bodies](notes/class-clocked-execution.md) close three further measured Icarus gaps under the already approved class contract. Object-dependent trigger evaluation remains work.

## 1. Where the criteria stand

| Criterion | State | Evidence |
| --- | --- | --- |
| sv-tests (344): at least Icarus's count on Linux and macOS | Met | 244 against 211 (Linux) and 210 (macOS), the pinned 14.0 devel `04cc493`; measured 2026-10-26/27 |
| Fixed P1 population (1,466): 90% pass | Met | 1,456 pass, 0 fail, 10 unsupported; re-measured 2026-10-31 on `7eab335e` |
| Mandatory test list | Met | CI and local CI |
| Fuzzing against `valsem`: seven nightly runs in a row at 100 times | Evidence for 2026-10-21 to 27 | README and counts record it; to be checked again for the M8 version |
| Functional coverage at least as broad as Icarus 13.0 | **Not met** | 5 of 95 original probes that 13.0 runs fail in kirimasim (re-measured 2026-10-31 on `7eab335e`; 6 on 2026-10-28); mechanisms outside the probes not mapped |

Only the last criterion is open. Two kinds of work block it: gaps already
measured, and the comparison inventory that would say what else is
required. The M8 report (TEST_SUITE) also needs the human review records,
which are the maintainer's.

Since the audit, main gained ref structure arguments, arrays of
structures, array slices, runtime packed parents, nested streaming
targets and interface/program VCD scopes (2026-10-30/31). Of the 88
combination tests of kirimasim-tests, 59 pass on `29ad7315` against 39 on
`f303beca`, and the two VCD tests pass on `7eab335e` with the
interface/program change. Icarus 13.0 fails most of the newly passing ones, so this
widened the lead where kirimasim was already ahead and did not close the
measured gaps.

## 2. What remains, by kind

1. **Measured gaps (re-measured 2026-10-31, `7eab335e`, against 13.0 and
   14.0 devel alike):** `class_objects`, `class_constructor`,
   `specify_path` and `dense_udp_coverage`. `mixed_strength_port_bit`, a
   gap on 2026-10-28, now passes. `real_net` (`wire real`) is an Icarus
   extension, counted apart. Reference mismatches stay four (13.0) and
   seven (14.0). `dense_udp_coverage` does not end in slang: the pinned
   slang checks that an edge-sensitive UDP gives outputs for all edges of
   all inputs (29.6) by enumerating every possible row
   (`checkPrimitiveEdgeCombinations`), about 6 × 3^32 states for the
   probe's 33 inputs, unconditionally; `kirimasim parse` alone runs past
   120 seconds. kirimasim keeps slang unmodified, so this gap closes only
   through slang.
2. **Already-specified P1 features not implemented:** the FST backend
   (WAVEFORM, dependency prepared), source `%p` and resolved-strength `%v`
   (SYSTEM_TASKS), and the M7 options `-W<name>` and `--compat=<name>`
   (CLI.md 2; checked 2026-10-31: both are unknown options).
   `--cond-x`, `--init-values`, `--bounds-check`, `--autoflush`, `--stop`
   and `--libdir` are accepted.
3. **Areas the 95 probes do not map:** VPI, class behaviour beyond the two
   probes, dynamic containers (associative arrays and the remaining
   forms), event and process control (named-fork disable, `wait_order`,
   event-controlled `->>`), nets and switches, specify timing checks, UDP
   unconnected ports, formats and input. Which forms Icarus 13.0 runs is
   **not confirmed** for these.
4. **Correctness bugs:** 12 open issues outside the slang and triage
   buckets (#28, #34, #54, #56, #58, #59, #61, #62, #63, #65, #68, #102), 17
   slang buckets and 14 triage buckets (2026-10-31). They are not a gate,
   but the criteria keep IEEE value and scheduling semantics.

## 3. How the order was decided

- **Does it move the open criterion?** Work on a form Icarus 13.0 is
  measured to run comes before forms Icarus also fails.
- **Can "done" be decided?** Without the inventory of item 2.3, finishing
  every measured gap would still not show the criterion met, so the
  inventory is on the critical path.
- **Is the meaning approved?** Approved contracts (classes: P1_SCOPE
  execution amendments, 2026-10-25) can be implemented now; specify and VPI
  need proposals first, which take the maintainer's time, so they start
  early and run beside implementation.
- **Size, risk and dependency:** small diagnoses first, long items
  started early, independent items in parallel.

## 4. Priorities

Sizes are rough (S: about a day, M: several days, L: a week or more) and
are not commitments.

### First (small, they correct the picture)

1. **Re-measure:** done 2026-10-31 for the probes and the population
   (above); sv-tests was not re-run locally (no pinned sv-tests and
   `04cc493` build here), the nightly measures it.
2. **`dense_udp_coverage`'s time-out:** diagnosed 2026-10-31 as slang's
   exhaustive coverage check (2.1). Next: reproduce with the slang CLI
   alone on slang master and, with the maintainer's go, report it
   upstream; move the pin when slang fixes it.

### Critical path

3. **Extend the comparison inventory** to the areas of 2.3 and record, form
   by form, whether Icarus 13.0 runs it, whether it is standard or an
   extension, and what kirimasim lacks. This defines the rest of P1. M,
   continuing.
4. **Class execution:** objects, constructors, properties, methods,
   inheritance, virtual dispatch and lifetime, under the approved contract.
   Two measured gaps, and classes are a large part of the sv-tests gap
   (chapters 7, 8 and 16, README). Icarus 13.0 is wrong
   on virtual dispatch through a base handle; follow the standard, not it.
   L.
5. **Drivers that meet through ports:** the probe `mixed_strength_port_bit`
   passes since 2026-10-28. The shapes the unsupported matrix still
   reports (wired nets, `tri0`/`tri1`, supply nets, another width and net
   arrays meeting through a port) go into the inventory first, and are
   implemented as far as Icarus 13.0 runs them. M.
6. **specify module paths**, then the timing checks the inventory requires:
   a proposal first, then the scheduler connection. M to L.
7. **VPI subset:** fill VPI_SPEC's open details as a proposal, then the
   runtime. Start the proposal now; it has the longest lead. L.

### Already decided, independent of the comparison

8. **FST backend.** M.
9. **Source `%p` and resolved-strength `%v`.** M.
10. **`-W<name>` and `--compat=<name>`**, with the loosening each compat
    mode does listed in CLI.md, strict by default. S to M.

### Remaining forms that Icarus runs

11. **E03** (`always_ff` waiting on a computed edge) and **E06** (disable of
    a named fork block), the two combination gaps Icarus 13.0 passes. S to M
    each.
12. **The population's 10 unsupported cases:** check each against Icarus
    13.0 (virtual interfaces, `$sampled`/`$stable`, `randsequence`,
    associative arrays with structure keys, dynamic arrays of fixed arrays,
    `sort with` on structure arrays) and take those it runs.

### Steady allocation

13. **Correctness bugs** of 2.4, each with a mechanism matrix as CLAUDE.md
    requires; slang buckets as upstream reports. A fixed share of each
    week, so that they do not pile up behind feature work.
14. **#28** (tier 1 slower than at `394717f`) is not a P1 gate; keep it in
    view so that feature work does not make it worse.

### Not needed for the open criterion unless the inventory says so

- Forms Icarus 13.0 also fails: E01, E04, E05, H01 and the `$fread` forms
  of F01. They are standard and stay in the backlog.
- Forms the specifications put in P4: streaming `with` and string targets
  (VALUE_SEMANTICS 8), event assignment and E02 (SIR_SPEC 3).
- `real_net`: an Icarus extension; it needs a compatibility decision, not
  default support.

## 5. Decisions for the maintainer

The maintainer accepted the recommendations below on 2026-10-31. The VPI
and specify texts still need approval when they are written.

- **VPI and specify:** proposals limited to the forms the inventory (3)
  finds Icarus 13.0 running, standard ones only. For specify, module paths
  (simple and edge-sensitive, rise/fall/turn-off delays) first, then the
  timing checks Icarus runs (`$setup`, `$hold` and the like). For VPI, the
  system task/function registration and value/time access and callbacks
  that the probes need; cocotb as a whole is not made a pass condition.
- **`wire real`:** no compatibility option for now; strict acceptance
  stays the default. If a later comparison needs it, it comes as one item
  of a documented `--compat` mode, proposed then.
- **`-W<name>` and `--compat=<name>`:** pass slang's own warning names and
  compatibility modes through, as CLI.md 2 describes, and list in CLI.md
  what each mode loosens; no kirimasim-specific modes now.
- **Work split:** one owner per area, written in the handoff note before
  work starts, and other agents' active branches (`codex/*`) checked
  first. Claude Code takes 1, 2, 3, 5, 8 to 11 and the VPI and specify
  proposals; class execution (4) goes to the agent the maintainer assigns,
  Codex by default since it did the class static-property work. The
  maintainer passes the assignment on. Later on 2026-10-31 the maintainer
  handed Claude Code's items to Codex as well.
- **Human review for M8:** a person does it, as TEST_SUITE and AI_POLICY
  require; Claude Code prepares the sampled case lists, their seeds and a
  review sheet, so that the maintainer checks and records the result.

## 6. Suggested order for the next sessions

1. Re-measure (1) and diagnose the UDP time-out (2).
2. Start the inventory (3) with classes, VPI and process control, the
   areas most likely to hold required forms.
3. Begin class execution (4) while the VPI and specify proposals (6, 7)
   are reviewed.
4. Port drivers (5) and the decided features (8 to 10) as parallel tracks.
5. Keep the bug allocation (13) every week.
