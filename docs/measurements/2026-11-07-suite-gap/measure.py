#!/usr/bin/env python3
# Copyright (c) 2026 The kirimasim Authors
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
"""What tests/suite/ holds that kirimasim-tests does not, measured once.

  python3 measure.py --kirimasim <checkout> --tests <checkout> --out <folder>
      [--iverilog-bin <folder>] [--verilator <program>] [-j N]

Static part: each test of tests/suite/ gets a class by what a port would need,
and is compared with every test of kirimasim-tests by name, by source, by source
with identifiers renamed, by shared token runs, by the clauses it cites and by
the pairs of language words it uses. Words are looked for without comments and
strings, so these counts are rough: text that differs can test one behaviour,
and text that is alike can test two.

Running part (with --iverilog-bin): each simulation test runs with Icarus Verilog
with the options of kirimasim-tests' tools/run_icarus.py, and its standard output
is compared with <name>.expected, normalized as both suites do. The .choice files
are for kirimasim alone (TEST_SUITE.md) and are not used. With --verilator, the
same is done with Verilator, with the options of tools/run_verilator.py. Verilator
has two states, so an output that differs says little for a :fourstate: yes test.

The output holds only paths relative to the two checkouts. It decides nothing:
a test that differs from Icarus Verilog may have the wrong expected value, or
Icarus Verilog may not follow the standard.
"""

import argparse
import concurrent.futures
import json
import multiprocessing
import os
import re
import resource
import shutil
import subprocess
import tempfile
import zlib
from collections import Counter, defaultdict
from pathlib import Path

KEYWORDS = frozenset("""
accept_on alias always always_comb always_ff always_latch and assert assign assume automatic
before begin bind bins binsof bit break buf bufif0 bufif1 byte case casex casez cell chandle
checker class clocking cmos config const constraint context continue cover covergroup coverpoint
cross deassign default defparam design disable dist do edge else end endcase endchecker endclass
endclocking endconfig endfunction endgenerate endgroup endinterface endmodule endpackage
endprimitive endprogram endproperty endsequence endspecify endtable endtask enum event eventually
expect export extends extern final first_match for force foreach forever fork forkjoin function
generate genvar global highz0 highz1 if iff ifnone ignore_bins illegal_bins implements implies
import incdir include initial inout input inside instance int integer interconnect interface
intersect join join_any join_none large let liblist library local localparam logic longint
macromodule matches medium modport module nand negedge nettype new nexttime nmos nor
noshowcancelled not notif0 notif1 null or output package packed parameter pmos posedge primitive
priority program property protected pull0 pull1 pulldown pullup pulsestyle_ondetect
pulsestyle_onevent pure rand randc randcase randsequence rcmos real realtime ref reg
reject_on release repeat restrict return rnmos rpmos rtran rtranif0 rtranif1 s_always s_eventually
s_nexttime s_until s_until_with scalared sequence shortint shortreal showcancelled signed small
soft solve specify specparam static string strong strong0 strong1 struct super supply0 supply1
sync_accept_on sync_reject_on table tagged task this throughout time timeprecision timeunit tran
tranif0 tranif1 tri tri0 tri1 triand trior trireg type typedef union unique unique0 unsigned
until until_with untyped use uwire var vectored virtual void wait wait_order wand weak weak0
weak1 while wildcard wire with within wor xnor xor
""".split())

STRIP = re.compile(r'/\*.*?\*/|//[^\n]*|"(?:\\.|[^"\\\n])*"', re.S)
TOKEN = re.compile(
    r'"(?:\\.|[^"\\\n])*"'
    r"|\$[A-Za-z_][\w$]*"
    r"|`[A-Za-z_]\w*"
    r"|[A-Za-z_][\w$]*"
    r"|\d[\d_]*(?:\.\d+)?(?:'[sS]?[bBoOdDhH]?[0-9a-fA-FxXzZ_?]+)?"
    r"|'[sS]?[bBoOdDhH][0-9a-fA-FxXzZ_?]+"
    r"|\S"
)
TIMESCALE = re.compile(r"^\s*`timescale[^\n]*$", re.M)
HEADER_ITEM = re.compile(r"^:([a-z_]+):[ \t]*(.*)$", re.M)
SEVERITY = re.compile(r"^(?:(?:INFO|WARNING|ERROR|FATAL): \S+:\d+: |\s+Time: \d+\s+Scope: )")
VCD_BANNER = re.compile(r"^VCD info: dumpfile .+ opened for output\.$")
FINISH = re.compile(r"^\S+:\d+: \$finish(?:\(\d\))? called at \d+ \(\d*\w*s\)$")
VERILATOR_LINE = re.compile(
    r"^(?:- (?:\S+:\d+: Verilog \$finish|S i m u l a t i o n|Verilator: )|%Warning-"
    r"|\[\d+\] (?:-Info|%Warning|%Error|%Fatal): "
    r"|(?:-Info|%Error): \S+:\d+: Verilog \$stop|Aborting\.\.\.$)"
)
# An internal check of Icarus Verilog that failed, not a construct it refuses.
COMPILER_ABORT = re.compile(r"failed assertion|Assertion `.*' failed|internal error", re.I)
SHINGLE = 5
COMMON_SHINGLE = 500  # in more tests than this, a token run says nothing about which is alike
OUTPUT_LIMIT = 16 << 20
SIM_TIMEOUT = 60


def split_test(text: str) -> tuple[dict[str, str], str]:
    end = text.find("*/")
    head = text[: end if end >= 0 else 0]
    return dict(HEADER_ITEM.findall(head)), text[end + 2 :] if end >= 0 else text


def code_tokens(body: str) -> list[str]:
    """The tokens of the source, comments removed, strings kept as tokens."""
    no_comments = re.sub(
        r'/\*.*?\*/|//[^\n]*|("(?:\\.|[^"\\\n])*")',
        lambda m: m.group(1) or " ",
        body,
        flags=re.S,
    )
    return TOKEN.findall(TIMESCALE.sub(" ", no_comments))


def renamed(tokens: list[str]) -> list[str]:
    """Identifiers numbered by first use, strings made one token."""
    names: dict[str, str] = {}
    out = []
    for t in tokens:
        if t.startswith('"'):
            out.append('"s"')
        elif re.fullmatch(r"[A-Za-z_][\w$]*", t) and t not in KEYWORDS:
            out.append(names.setdefault(t, f"v{len(names)}"))
        else:
            out.append(t)
    return out


def features(body: str) -> frozenset[str]:
    words = re.findall(r"\$?[A-Za-z_][\w$]*", STRIP.sub(" ", body))
    return frozenset(w for w in words if w in KEYWORDS or w.startswith("$"))


def clauses(meta: dict[str, str]) -> list[str]:
    return [w for w in meta.get("lrm", "").split() if not w.startswith("1800-")]


def pairs_of(items) -> set[tuple[str, str]]:
    ordered = sorted(items)
    return {(a, b) for i, a in enumerate(ordered) for b in ordered[i + 1 :]}


def stderr_class(test: Path, meta: dict[str, str]) -> str:
    value = meta.get("expected_stderr", "")
    if not value.startswith("@"):
        return "none" if not value else "inline"
    lines = [
        line
        for line in (test.parent / value[1:]).read_text(errors="ignore").splitlines()
        if line.strip()
    ]
    if not lines:
        return "empty"
    if any(": error: " in line or ": fatal: " in line for line in lines):
        return "error_or_fatal"
    return "warnings_only"


def port_class(meta: dict[str, str], stderr: str) -> str:
    """What a port to kirimasim-tests would need, the first that applies."""
    if "--" in meta.get("args", ""):
        return "kirimasim_option"
    if "should_fail_because" in meta:
        return "illegal_design"
    if meta.get("expected_exit", "0") != "0" or stderr == "error_or_fatal":
        return "stops_with_an_error"
    if meta.get("nondeterministic") == "yes":
        return "several_allowed_outputs"
    return "stdout_only"


def read_suite(folder: Path, pattern: str, root: Path) -> list[dict]:
    tests = []
    for path in sorted(folder.rglob(pattern)):
        text = path.read_text(encoding="utf-8", errors="ignore")
        meta, body = split_test(text)
        if "type" not in meta:
            continue
        tokens = code_tokens(body)
        same = renamed(tokens)
        top = meta.get("top_module", path.stem).split()
        tests.append(
            {
                "path": str(path.relative_to(root)),
                "name": re.sub(r"^test_", "", path.stem),
                "meta": meta,
                "plain": hash_of(" ".join("TOP" if t in top or t == path.stem else t for t in tokens)),
                "same": hash_of(" ".join(same)),
                "shingles": {
                    zlib.crc32(" ".join(same[i : i + SHINGLE]).encode())
                    for i in range(max(1, len(same) - SHINGLE + 1))
                },
                "features": features(body),
            }
        )
    return tests


def hash_of(text: str) -> int:
    return zlib.crc32(text.encode()) ^ (len(text) << 32)


_INDEX: dict[int, list[int]] = {}
_SIZES: list[int] = []


def best_jaccard(shingles: set[int]) -> float:
    shared: Counter[int] = Counter()
    for s in shingles:
        for other in _INDEX.get(s, ()):
            shared[other] += 1
    best = 0.0
    for other, n in shared.items():
        best = max(best, n / (len(shingles) + _SIZES[other] - n))
    return best


def similarity(mine: list[dict], others: list[dict], jobs: int) -> list[float]:
    global _SIZES
    index: dict[int, list[int]] = defaultdict(list)
    for i, t in enumerate(others):
        for s in t["shingles"]:
            index[s].append(i)
    _INDEX.clear()
    _INDEX.update({s: v for s, v in index.items() if len(v) <= COMMON_SHINGLE})
    _SIZES = [len(t["shingles"]) for t in others]
    with multiprocessing.get_context("fork").Pool(jobs) as pool:
        return pool.map(best_jaccard, [t["shingles"] for t in mine], chunksize=64)


def limits() -> None:
    resource.setrlimit(resource.RLIMIT_FSIZE, (OUTPUT_LIMIT, OUTPUT_LIMIT))
    resource.setrlimit(resource.RLIMIT_AS, (8 << 30, 8 << 30))


def normalized(text: str) -> list[str]:
    return [
        line
        for line in text.splitlines()
        if line.strip()
        and not SEVERITY.match(line)
        and not VCD_BANNER.match(line)
        and not FINISH.match(line)
    ]


def allowed_outputs(test: Path, meta: dict[str, str]) -> list[list[str]] | None:
    value = meta.get("expected_stdout")
    if value is None:
        return None
    text = (test.parent / value[1:]).read_text(errors="replace") if value.startswith("@") else value + "\n"
    if meta.get("nondeterministic") == "yes":
        parts = re.split(r"^---[ \t]*$", text, flags=re.M)
        return [normalized(p) for p in parts]
    return [normalized(text)]


def judged(got: list[str], allowed: list[list[str]], code: int, meta: dict[str, str]) -> str:
    """A run the simulator aborted, or that it ended with an error of its own where
    the test expects none, says less about the expected value than a clean run."""
    if code < 0:
        return "simulator_aborted"
    want_exit = meta.get("expected_exit", "0")
    if got not in allowed:
        return "differs" if code == 0 or str(code) == want_exit else "differs_and_stopped_with_error"
    if want_exit.isdigit() and int(want_exit) != code:
        return "stdout_matches_exit_differs"
    return "matches"


def run_icarus(job: tuple[str, str, dict[str, str]]) -> tuple[str, str, str]:
    path_text, bindir, meta = job
    test = Path(path_text)
    allowed = allowed_outputs(test, meta)
    if allowed is None:
        return path_text, "no_expected_stdout", ""
    tops = meta.get("top_module", "").split()
    with tempfile.TemporaryDirectory() as tmp:
        out = Path(tmp) / "a.vvp"
        compiled = subprocess.run(
            [
                f"{bindir}/iverilog", "-g2012", "-gspecify", "-ginterconnect",
                *[f"-s{t}" for t in tops], "-o", str(out), str(test),
            ],
            capture_output=True, text=True, errors="replace", cwd=tmp, timeout=120,
        )
        if compiled.returncode != 0:
            first = (compiled.stderr.strip().splitlines() or [""])[0]
            shape = re.sub(r"^\S*?:\d+: ", "", first.replace(str(test), "<test>"))
            shape = re.sub(r"\b[\w.$]*(?:top|test)[\w.$]*\b|\d+", "#", shape)[:90]
            kind = "unsupported" if "sorry" in compiled.stderr.lower() else "compile_error"
            if compiled.returncode < 0 or COMPILER_ABORT.search(compiled.stderr):
                kind = "compiler_aborted"
            return path_text, kind, shape
        stdout = Path(tmp) / "stdout.txt"
        args = meta.get("args", "").split()
        try:
            with stdout.open("wb") as sink:
                run = subprocess.run(
                    [f"{bindir}/vvp", str(out), *args],
                    stdout=sink, stderr=subprocess.DEVNULL, stdin=subprocess.DEVNULL,
                    cwd=tmp, timeout=SIM_TIMEOUT, preexec_fn=limits,
                )
        except subprocess.TimeoutExpired:
            return path_text, "timed_out", ""
        got = normalized(stdout.read_text(errors="replace"))
    return path_text, judged(got, allowed, run.returncode, meta), ""


def run_verilator(job: tuple[str, str, dict[str, str]]) -> tuple[str, str, str]:
    path_text, verilator, meta = job
    test = Path(path_text)
    allowed = allowed_outputs(test, meta)
    if allowed is None:
        return path_text, "no_expected_stdout", ""
    env = {k: v for k, v in os.environ.items() if k != "VERILATOR_ROOT"}
    if shutil.which("ccache"):
        env["OBJCACHE"] = "ccache"
    tops = meta.get("top_module", "").split()
    with tempfile.TemporaryDirectory() as tmp:
        obj = Path(tmp) / "obj"
        try:
            built = subprocess.run(
                [
                    verilator, "--binary", "--timing", "--coverage-user", "-Wno-fatal",
                    "-Wno-lint", "-Wno-style", "--x-assign", "unique", "--x-initial", "unique",
                    *(["--top-module", tops[0]] if len(tops) == 1 else []),
                    "-Mdir", str(obj), "--prefix", "Vt", str(test),
                ],
                capture_output=True, text=True, errors="replace", cwd=tmp, timeout=600, env=env,
            )
        except subprocess.TimeoutExpired:
            return path_text, "timed_out", ""
        if built.returncode != 0:
            return path_text, "build_failed", ""
        stdout = Path(tmp) / "stdout.txt"
        try:
            with stdout.open("wb") as sink:
                run = subprocess.run(
                    [
                        str(obj / "Vt"), "+verilator+rand+reset+2", "+verilator+seed+1",
                        "+verilator+error+limit+1000000", *meta.get("args", "").split(),
                    ],
                    stdout=sink, stderr=subprocess.DEVNULL, stdin=subprocess.DEVNULL,
                    cwd=tmp, timeout=SIM_TIMEOUT, preexec_fn=limits,
                )
        except subprocess.TimeoutExpired:
            return path_text, "timed_out", ""
        lines = stdout.read_text(errors="replace").splitlines()
    got = normalized("\n".join(line for line in lines if not VERILATOR_LINE.match(line)))
    return path_text, judged(got, allowed, run.returncode, meta), ""


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--kirimasim", type=Path, required=True)
    ap.add_argument("--tests", type=Path, required=True)
    ap.add_argument("--out", type=Path, required=True)
    ap.add_argument("--iverilog-bin", default=None)
    ap.add_argument("--verilator", default=None)
    ap.add_argument("-j", "--jobs", type=int, default=16)
    args = ap.parse_args()
    root_k, root_t = args.kirimasim.resolve(), args.tests.resolve()
    args.out.mkdir(parents=True, exist_ok=True)

    mine = read_suite(root_k / "tests/suite", "*.sv", root_k)
    others = read_suite(root_t / "tests", "test_*.sv", root_t)
    names = {t["name"] for t in others}
    plain = {t["plain"] for t in others}
    same = {t["same"] for t in others}
    other_clauses = {c for t in others for c in clauses(t["meta"])}
    other_pairs = {p for t in others for p in pairs_of(t["features"])}
    other_clause_pairs = {p for t in others for p in pairs_of(set(clauses(t["meta"])))}
    other_chapter_pairs = {
        p for t in others for p in pairs_of({c.split(".")[0] for c in clauses(t["meta"])})
    }
    other_features = {f for t in others for f in t["features"]}
    jaccards = similarity(mine, others, args.jobs)

    new_pairs: Counter[tuple[str, str]] = Counter()
    new_clauses: Counter[str] = Counter()
    new_chapter_pairs: Counter[tuple[str, str]] = Counter()
    new_features: Counter[str] = Counter()
    for t, j in zip(mine, jaccards):
        meta = t["meta"]
        path = root_k / t["path"]
        t["jaccard"] = j
        t["stderr"] = stderr_class(path, meta)
        t["class"] = port_class(meta, t["stderr"])
        t["generator"] = meta.get("generator", "(hand-written)").split("@")[0]
        pairs = pairs_of(t["features"]) - other_pairs
        t["new_pairs"] = len(pairs)
        new_pairs.update(pairs)
        cited = set(clauses(meta))
        t["new_clause"] = bool(cited - other_clauses)
        new_clauses.update(cited - other_clauses)
        t["new_clause_pair"] = bool(pairs_of(cited) - other_clause_pairs)
        new_chapter_pairs.update(pairs_of({c.split(".")[0] for c in cited}) - other_chapter_pairs)
        new_features.update(t["features"] - other_features)

    outcomes: dict[str, str] = {}
    shapes: Counter[str] = Counter()
    version = None
    if args.iverilog_bin:
        version = subprocess.run(
            [f"{args.iverilog_bin}/iverilog", "-V"], capture_output=True, text=True
        ).stdout.splitlines()[0]
        jobs = [
            (str(root_k / t["path"]), args.iverilog_bin, t["meta"])
            for t in mine
            if t["meta"].get("type") == "simulation"
        ]
        with concurrent.futures.ThreadPoolExecutor(args.jobs) as pool:
            for path_text, outcome, shape in pool.map(run_icarus, jobs):
                outcomes[str(Path(path_text).relative_to(root_k))] = outcome
                if shape:
                    shapes[shape] += 1

    v_outcomes: dict[str, str] = {}
    v_version = None
    if args.verilator:
        env = {k: v for k, v in os.environ.items() if k != "VERILATOR_ROOT"}
        v_version = subprocess.run(
            [args.verilator, "--version"], capture_output=True, text=True, env=env
        ).stdout.strip()
        jobs = [
            (str(root_k / t["path"]), args.verilator, t["meta"])
            for t in mine
            if t["meta"].get("type") == "simulation"
        ]
        with concurrent.futures.ThreadPoolExecutor(args.jobs) as pool:
            for path_text, outcome, _ in pool.map(run_verilator, jobs):
                v_outcomes[str(Path(path_text).relative_to(root_k))] = outcome

    bands = ((0.9, "0.9+"), (0.7, "0.7-0.9"), (0.5, "0.5-0.7"), (0.3, "0.3-0.5"), (0.0, "<0.3"))
    def band(j: float) -> str:
        return next(name for floor, name in bands if j >= floor)

    by_generator: dict[str, dict] = defaultdict(
        lambda: {
            "tests": 0, "classes": Counter(), "icarus": Counter(), "similarity": Counter(),
            "with_new_word_pair": 0, "with_new_clause": 0, "clauses": Counter(), "issue": 0,
            "verilator": Counter(), "either_matches": 0, "fourstate": 0,
        }
    )
    for t in mine:
        row = by_generator[t["generator"]]
        row["tests"] += 1
        row["classes"][t["class"]] += 1
        row["similarity"][band(t["jaccard"])] += 1
        row["with_new_word_pair"] += t["new_pairs"] > 0
        row["with_new_clause"] += t["new_clause"]
        row["issue"] += "issue" in t["meta"]
        row["clauses"].update(clauses(t["meta"])[:1])
        if t["path"] in outcomes:
            row["icarus"][outcomes[t["path"]]] += 1
        if t["path"] in v_outcomes:
            row["verilator"][v_outcomes[t["path"]]] += 1
        row["fourstate"] += t["meta"].get("fourstate") == "yes"
        row["either_matches"] += "matches" in (outcomes.get(t["path"]), v_outcomes.get(t["path"]))
    for row in by_generator.values():
        row["clauses"] = [c for c, _ in row["clauses"].most_common(3)]

    summary = {
        "tests_suite": len(mine),
        "kirimasim_tests_suite": len(others),
        "same_name": sum(t["name"] in names for t in mine),
        "same_source": sum(t["plain"] in plain for t in mine),
        "same_source_with_identifiers_renamed": sum(t["same"] in same for t in mine),
        "similarity_bands": dict(Counter(band(t["jaccard"]) for t in mine)),
        "generators": len({t["generator"] for t in mine} - {"(hand-written)"}),
        "generated_tests": sum(t["generator"] != "(hand-written)" for t in mine),
        "port_classes": dict(Counter(t["class"] for t in mine)),
        "stderr_classes": dict(Counter(t["stderr"] for t in mine)),
        "types": dict(Counter(t["meta"].get("type", "") for t in mine)),
        "fourstate": dict(Counter(t["meta"].get("fourstate", "") for t in mine)),
        "oracles": dict(Counter(t["meta"].get("oracle", "") for t in mine)),
        "tests_with_issue": sum("issue" in t["meta"] for t in mine),
        "tests_with_new_word_pair": sum(t["new_pairs"] > 0 for t in mine),
        "tests_with_new_clause": sum(t["new_clause"] for t in mine),
        "tests_with_new_clause_pair": sum(t["new_clause_pair"] for t in mine),
        "new_clauses": new_clauses.most_common(),
        "new_chapter_pairs": [[f"{a}+{b}", n] for (a, b), n in new_chapter_pairs.most_common()],
        "new_words": new_features.most_common(),
        "new_word_pairs_total": len(new_pairs),
        "new_word_pairs_top": [[f"{a}+{b}", n] for (a, b), n in new_pairs.most_common(40)],
        "icarus_version": version,
        "icarus": dict(Counter(outcomes.values())),
        "icarus_by_class": {
            c: dict(Counter(outcomes[t["path"]] for t in mine if t["class"] == c and t["path"] in outcomes))
            for c in sorted({t["class"] for t in mine})
        },
        "icarus_by_fourstate": {
            f: dict(Counter(outcomes[t["path"]] for t in mine if t["meta"].get("fourstate") == f and t["path"] in outcomes))
            for f in ("yes", "no")
        },
        "icarus_compile_messages": shapes.most_common(40),
        "verilator_version": v_version,
        "verilator": dict(Counter(v_outcomes.values())),
        "verilator_by_class": {
            c: dict(Counter(v_outcomes[t["path"]] for t in mine if t["class"] == c and t["path"] in v_outcomes))
            for c in sorted({t["class"] for t in mine})
        },
        "verilator_by_fourstate": {
            f: dict(Counter(v_outcomes[t["path"]] for t in mine if t["meta"].get("fourstate") == f and t["path"] in v_outcomes))
            for f in ("yes", "no")
        },
        "both": dict(Counter(
            f"icarus_{outcomes.get(t['path'])}+verilator_{v_outcomes.get(t['path'])}"
            for t in mine if t["path"] in outcomes and t["path"] in v_outcomes
        )),
        "either_matches_by_class": {
            c: sum("matches" in (outcomes.get(t["path"]), v_outcomes.get(t["path"])) for t in mine if t["class"] == c)
            for c in sorted({t["class"] for t in mine})
        },
    }
    (args.out / "outcomes.tsv").write_text(
        "".join(
            f"{t['path']}\t{t['class']}\t{t['generator']}\t"
            f"{outcomes.get(t['path'], '-')}\t{v_outcomes.get(t['path'], '-')}\n"
            for t in mine
        )
    )
    (args.out / "summary.json").write_text(json.dumps(summary, indent=1) + "\n")
    (args.out / "by_generator.json").write_text(
        json.dumps(dict(sorted(by_generator.items())), indent=1, sort_keys=True) + "\n"
    )
    if outcomes:
        differs = sorted(p for p, o in outcomes.items() if o == "differs")
        (args.out / "icarus_differs.txt").write_text("".join(f"{p}\n" for p in differs))
    if v_outcomes:
        (args.out / "both_differ.txt").write_text("".join(
            f"{p}\n" for p in sorted(outcomes)
            if outcomes[p] == "differs" and v_outcomes.get(p) == "differs"
        ))
        (args.out / "icarus_differs_verilator_matches.txt").write_text("".join(
            f"{p}\n" for p in sorted(outcomes)
            if outcomes[p] == "differs" and v_outcomes.get(p) == "matches"
        ))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
