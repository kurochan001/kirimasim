#!/usr/bin/env python3
"""Keeps the counts in README.md and README.ja.md up to date.

The counts live in docs/counts.json: this repository's tests (CTest, dev
configuration), the tests under tests/suite/ and their generators, the
sv-tests result against the Icarus build of the P1 criterion, and the P1
population of the own test suite (TEST_SUITE.md), with the fuzz runs and
the mandatory list of the P1 criteria, and the numbers of the other
simulators. Each README holds a range between <!-- counts:begin --> and
<!-- counts:end -->, which this script writes from that file.

  tools/readme_counts.py update [inputs]   # take the inputs, write counts.json and the READMEs
  tools/readme_counts.py check [inputs]    # exit 1 if either is out of date

The inputs, each optional:

  --build DIR        a configured build tree: its number of CTest tests, and
                     of each kind by their labels
  --junit FILE       the JUnit file of a run of those tests (ctest
                     --output-junit): how many passed and were skipped
  --svtests FILE     results.json of tools/svtests.py: kirimasim's count,
                     and Icarus's when it ran
  --population FILE  the output of tools/p1_population.py run

The tests under tests/suite/ and the generators are counted from the
repository every time. check fails when a count an input gives differs
from counts.json, or when a README's range is not what counts.json gives.
update also sets the date of the measurement when sv-tests or the
population is given (--date, today in UTC by default). The fuzz runs and
the mandatory list are edited by hand in counts.json, then written out by
update.
"""

import argparse
import datetime
import json
import re
import subprocess
import sys
import xml.etree.ElementTree as ET
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
DATA = ROOT / "docs" / "counts.json"
READMES = {"en": ROOT / "README.md", "ja": ROOT / "README.ja.md"}
BEGIN = "<!-- counts:begin -->"
END = "<!-- counts:end -->"
FUZZ_NIGHTS = 7  # in a row, for the P1 criterion (TEST_SUITE.md)


def n(v):
    return f"{v:,}"


def render(c, lang):
    """The text between the markers."""
    s, p, f, o, k = c["svtests"], c["population"], c["fuzz"], c["others"], c["kinds"]
    pct = 100.0 * p["passed"] / p["total"]
    sv_met = s["kirimasim"] >= s["icarus"]
    pop_met = pct >= p["gate"]
    fuzz_met = f["nights"] >= FUZZ_NIGHTS
    if lang == "en":
        yes = lambda b: "Yes" if b else "Not yet"  # noqa: E731
        lines = [
            "<!-- Written by tools/readme_counts.py from docs/counts.json; do not edit by hand. -->",
            "",
            "| Criterion | Now | Met |",
            "| --- | --- | --- |",
            f"| [sv-tests](https://github.com/chipsalliance/sv-tests) simulation tests (pinned "
            f"commit, hand-written tests, {s['total']}): at least as many as Icarus Verilog | "
            f"kirimasim {s['kirimasim']}, Icarus Verilog {s['icarus']} ({s['icarus_build']}, the "
            f"build the criterion uses) | {yes(sv_met)} |",
            f"| kirimasim's own RTL test population ({n(p['total'])} tests, fixed list): "
            f"{p['gate']}% pass | {n(p['passed'])} pass ({pct:.1f}%), {p['failed']} fail; "
            f"{p['unsupported']} stop at constructs of later work | {yes(pop_met)} |",
            "| The mandatory test list (except the one that needs P3) | "
            f"{'All pass' if c['mandatory']['met'] else 'Not all pass'} | "
            f"{yes(c['mandatory']['met'])} |",
            "| No disagreement between the fast value operations and the reference "
            "implementation (`valsem`): seven nightly fuzz runs in a row with 100 times the "
            f"rounds | The nightly fuzz runs passed on {f['nights']} nights in a row, through "
            f"{f['through']} | {yes(fuzz_met)} |",
            "",
            f"kirimasim's numbers were measured on {c['measured']}; Icarus Verilog's and "
            f"Verilator's in the last two columns on {o['measured']}, from their main branches "
            "that day. The nightly run measures sv-tests and the population again, and fails "
            "when they differ from these.",
            "",
            "| Test set | Tests | kirimasim | Icarus Verilog | Verilator |",
            "| --- | --- | --- | --- | --- |",
            f"| This repository's tests (CTest, dev configuration) | {n(c['ctest']['total'])} | "
            f"{n(c['ctest']['passed'])}, {c['ctest']['skipped']} skipped | — | — |",
            f"| [sv-tests](https://github.com/chipsalliance/sv-tests) simulation tests | "
            f"{s['total']} | {s['kirimasim']} | {o['svtests']['icarus']} | "
            f"{o['svtests']['verilator']} |",
            f"| Own test suite, P1 population | {n(p['total'])} | {n(p['passed'])} | "
            f"{n(o['population']['icarus'])} | {n(o['population']['verilator'])} |",
            "",
            f"This repository's tests by kind: unit tests (GoogleTest) {n(k['unit'])}, "
            f"`tests/suite/` {n(c['suite_tests'])} (made by {c['generators']} generators under "
            f"`tools/`), tools {n(k['tools'])}, `tests/dump_sir/` {n(k['dump_sir'])}, state and "
            f"Contexts {n(k['context'])}, command line and event counts {n(k['cli'])}.",
        ]
    else:
        yes = lambda b: "達成" if b else "まだ"  # noqa: E731
        lines = [
            "<!-- tools/readme_counts.py が docs/counts.json から書く。手で直さない。 -->",
            "",
            "| 条件 | 今 | 達成 |",
            "| --- | --- | --- |",
            f"| [sv-tests](https://github.com/chipsalliance/sv-tests) のシミュレーションのテスト"
            f"（固定したコミット、手書きのテスト {s['total']} 本）で Icarus Verilog 以上 | "
            f"kirimasim {s['kirimasim']} 本、Icarus Verilog {s['icarus']} 本"
            f"（{s['icarus_build'].replace(', ', '、')}。"
            f"合格条件で使う版） | {yes(sv_met)} |",
            f"| 自前の RTL のテストの母集団（{n(p['total'])} 本、一覧を固定）の {p['gate']}% が通る | "
            f"{n(p['passed'])} 本が通る（{pct:.1f}%）。失敗は {p['failed']} 本で、"
            f"{p['unsupported']} 本は後で作る構文で止まる | {yes(pop_met)} |",
            "| 必須のテストの一覧（P3 が要る 1 本を除く） | "
            f"{'すべて通る' if c['mandatory']['met'] else 'まだすべては通らない'} | "
            f"{yes(c['mandatory']['met'])} |",
            "| 速い値の演算と参照実装（`valsem`）の食い違いが無い：回数を 100 倍にした夜間の"
            f"ファジングを 7 回続けて | 夜間のファジングは {f['nights']} 晩続けて通った"
            f"（{f['through']} まで） | {yes(fuzz_met)} |",
            "",
            f"kirimasim の数は {c['measured']} に測ったもので、右の 2 列の Icarus Verilog と "
            f"Verilator の数は {o['measured']} に、その日の main からビルドして測ったものです。"
            "nightly の実行が sv-tests と母集団を測り直し、これと違えば失敗します。",
            "",
            "| テストの集まり | 本数 | kirimasim | Icarus Verilog | Verilator |",
            "| --- | --- | --- | --- | --- |",
            f"| このリポジトリのテスト（CTest、dev の構成） | {n(c['ctest']['total'])} | "
            f"{n(c['ctest']['passed'])}、{c['ctest']['skipped']} 本は飛ばす | — | — |",
            f"| [sv-tests](https://github.com/chipsalliance/sv-tests) のシミュレーションのテスト | "
            f"{s['total']} | {s['kirimasim']} | {o['svtests']['icarus']} | "
            f"{o['svtests']['verilator']} |",
            f"| 自前のテストスイートの P1 の母集団 | {n(p['total'])} | {n(p['passed'])} | "
            f"{n(o['population']['icarus'])} | {n(o['population']['verilator'])} |",
            "",
            f"このリポジトリのテストの種類ごとの本数：ユニットテスト（GoogleTest）{n(k['unit'])}、"
            f"`tests/suite/` {n(c['suite_tests'])}（`tools/` の {c['generators']} 個の生成器が作る）、"
            f"道具 {n(k['tools'])}、`tests/dump_sir/` {n(k['dump_sir'])}、状態と Context "
            f"{n(k['context'])}、コマンドラインと事象の回数 {n(k['cli'])}。",
        ]
    return "\n".join(lines)


# The CTest labels of each kind of test; a test with none is a unit test.
KINDS = {"Suite": "suite", "Tools": "tools", "DumpSir": "dump_sir", "Context": "context",
         "Cli": "cli", "PerfCount": "cli"}


def ctest_kinds(build):
    """The number of CTest tests, and of each kind."""
    out = subprocess.run(["ctest", "--test-dir", str(build), "--show-only=json-v1"],
                         capture_output=True, text=True, check=True).stdout
    tests = json.loads(out)["tests"]
    kinds = {"unit": 0, "suite": 0, "tools": 0, "dump_sir": 0, "context": 0, "cli": 0}
    for t in tests:
        labels = [v for p in t.get("properties", []) if p["name"] == "LABELS" for v in p["value"]]
        kind = next((KINDS[x] for x in labels if x in KINDS), "unit")
        kinds[kind] += 1
    return len(tests), kinds


def junit_counts(path):
    cases = ET.parse(path).getroot().iter("testcase")
    total = passed = skipped = 0
    for case in cases:
        total += 1
        if case.find("skipped") is not None:
            skipped += 1
        elif case.find("failure") is None and case.find("error") is None:
            passed += 1
    return total, passed, skipped


def svtests_counts(path):
    results = json.loads(Path(path).read_text(encoding="utf-8"))
    out = {"total": len(results),
           "kirimasim": sum(1 for r in results if r.get("kirimasim", {}).get("pass"))}
    if any("icarus" in r for r in results):
        out["icarus"] = sum(1 for r in results if r.get("icarus", {}).get("pass"))
    return out


def population_counts(path):
    text = Path(path).read_text(encoding="utf-8")
    total = re.search(r"(\d+) of \d+ simulation tests", text)
    m = re.search(r"passed (\d+), failed (\d+), unsupported (\d+)", text)
    if not total or not m:
        sys.exit(f"readme_counts: {path} is not an output of tools/p1_population.py run")
    return {"total": int(total.group(1)), "passed": int(m.group(1)), "failed": int(m.group(2)),
            "unsupported": int(m.group(3))}


def measure(args, c):
    """The counts the inputs give, as (where, its key path, value)."""
    got = [("tests/suite/", ("suite_tests",), len(list((ROOT / "tests" / "suite").rglob("*.sv")))),
           ("tools/gen_*.py", ("generators",), len(list((ROOT / "tools").glob("gen_*.py"))))]
    if args.build:
        total, kinds = ctest_kinds(args.build)
        got.append(("ctest", ("ctest", "total"), total))
        got += [("ctest labels", ("kinds", k), v) for k, v in kinds.items()]
    if args.junit:
        total, passed, skipped = junit_counts(args.junit)
        got += [("JUnit", ("ctest", "total"), total), ("JUnit", ("ctest", "passed"), passed),
                ("JUnit", ("ctest", "skipped"), skipped)]
    if args.svtests:
        for k, v in svtests_counts(args.svtests).items():
            got.append(("sv-tests", ("svtests", k), v))
    if args.population:
        for k, v in population_counts(args.population).items():
            got.append(("population", ("population", k), v))
    return got


def main():
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("command", choices=["update", "check"])
    ap.add_argument("--build", type=Path)
    ap.add_argument("--junit", type=Path)
    ap.add_argument("--svtests", type=Path)
    ap.add_argument("--population", type=Path)
    ap.add_argument("--date", default=datetime.datetime.now(datetime.timezone.utc).date().isoformat())
    args = ap.parse_args()
    c = json.loads(DATA.read_text(encoding="utf-8"))
    problems = []
    for where, keys, value in measure(args, c):
        node = c
        for k in keys[:-1]:
            node = node[k]
        if node[keys[-1]] != value:
            problems.append(f"{'.'.join(keys)} is {node[keys[-1]]} in docs/counts.json, "
                            f"{value} by {where}")
            node[keys[-1]] = value
    if args.command == "update" and (args.svtests or args.population):
        c["measured"] = args.date
    texts = {}
    for lang, path in READMES.items():
        text = path.read_text(encoding="utf-8")
        if text.count(BEGIN) != 1 or text.count(END) != 1:
            sys.exit(f"readme_counts: {path.name} needs one {BEGIN} and one {END}")
        head, rest = text.split(BEGIN)
        _, tail = rest.split(END)
        new = f"{head}{BEGIN}\n{render(c, lang)}\n{END}{tail}"
        if new != text:
            problems.append(f"{path.name}: the counts range is not what docs/counts.json gives")
        texts[path] = new
    if args.command == "check":
        for p in problems:
            print(f"readme_counts: {p}", file=sys.stderr)
        if problems:
            print("readme_counts: run tools/readme_counts.py update with the same inputs",
                  file=sys.stderr)
        return 1 if problems else 0
    DATA.write_text(json.dumps(c, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
    for path, text in texts.items():
        path.write_text(text, encoding="utf-8")
    return 0


if __name__ == "__main__":
    sys.exit(main())
