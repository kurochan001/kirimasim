#!/usr/bin/env python3
# Copyright (c) 2026 The kirimasim Authors
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

"""Keeps bilingual README test counts and docs/VALIDATION_COUNTS.md records up to date.

The counts live in docs/counts.json: this repository's tests (CTest, dev
configuration), the tests under tests/suite/ and their generators, the
sv-tests result against the Icarus build of the P1 criterion on each OS
that CI runs on (TEST_SUITE.md, "As many as Icarus or more"), and the P1
population of the own test suite (TEST_SUITE.md), with the fuzz runs,
the mandatory list and the original probes against Icarus Verilog of the
P1 criteria (P1_SCOPE.md), and the numbers of the other simulators. Each README and each VALIDATION_COUNTS document holds a range between <!-- counts:begin --> and
<!-- counts:end -->, which this script writes from that file. READMEs show repository test counts;
the full dated external/gate evidence remains in docs/VALIDATION_COUNTS.md and
its Japanese counterpart. A newly built repository does not remeasure those external results.

  tools/readme_counts.py update [inputs]   # take the inputs, write counts.json and both document pairs
  tools/readme_counts.py check [inputs]    # exit 1 if either is out of date

The inputs, each optional:

  --build DIR        a configured build tree: its number of CTest tests, and
                     of each kind by their labels
  --junit FILE       the JUnit file of a run of those tests (ctest
                     --output-junit): how many passed and were skipped
  --svtests FILE     results.json of tools/svtests.py: kirimasim's count,
                     and Icarus's when it ran
  --os NAME          the OS that --svtests was measured on: linux or macos
  --population FILE  the output of tools/p1_population.py run

The tests under tests/suite/ and the generators are counted from the
repository every time. check fails when a count an input gives differs
from counts.json, or when a generated document's range is not what counts.json gives.
update also sets the date of the measurement when sv-tests or the
population is given (--date, today in UTC by default). The fuzz runs,
the mandatory list and the Icarus probes (tools/icarus_feature_probe.py
--extended, against Icarus Verilog 13.0 stable and 14.0 devel) are edited
by hand in counts.json, then written out by update.
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


def render_evidence(c, lang):
    """The text between the markers."""
    s, p, f, o, k = c["svtests"], c["population"], c["fuzz"], c["others"], c["kinds"]
    ip = c["icarus_probes"]
    excluded = ip.get("p1_out_of_scope", [])
    evaluated = ip["probes"] - len(excluded)
    exclusion_names = ", ".join("`" + item["name"] + "`" for item in excluded) or "—"
    required_stable = ip["stable"].get("p1_required_gaps", ip["stable"]["gaps"])
    required_devel = ip["devel"].get("p1_required_gaps", ip["devel"]["gaps"])
    lin, mac = s["linux"], s["macos"]
    dates = s.get("measured", {"linux": c["measured"], "macos": c["measured"]})
    failed = c["ctest"]["total"] - c["ctest"]["passed"] - c["ctest"]["skipped"]
    failures_en = f", {failed} failed" if failed else ""
    failures_ja = f"、失敗{failed}件" if failed else ""
    pct = 100.0 * p["passed"] / p["total"]
    sv_met = all(x["kirimasim"] >= x["icarus"] for x in (lin, mac))
    # kirimasim's count, one number when the two OSes agree
    ours = (str(lin["kirimasim"]) if lin["kirimasim"] == mac["kirimasim"] else
            f"{lin['kirimasim']} (Linux), {mac['kirimasim']} (macOS)")
    pop_met = pct >= p["gate"]
    fuzz_met = f["nights"] >= FUZZ_NIGHTS
    if lang == "en":
        yes = lambda b: "Yes" if b else "Not yet"  # noqa: E731
        lines = [
            "<!-- Written by tools/readme_counts.py from docs/counts.json; do not edit by hand. -->",
            "",
            "| Criterion | Recorded result | Met at measurement |",
            "| --- | --- | --- |",
            f"| [sv-tests](https://github.com/chipsalliance/sv-tests) simulation tests (pinned "
            f"commit, hand-written tests, {s['total']}): at least as many as Icarus Verilog, on "
            f"Linux and on macOS | Linux: kirimasim {lin['kirimasim']}, Icarus Verilog "
            f"{lin['icarus']}; macOS: kirimasim {mac['kirimasim']}, Icarus Verilog "
            f"{mac['icarus']} ({s['icarus_build']}, the build the criterion uses, built on "
            f"each) | {yes(sv_met)} |",
            f"| kirimasim's own RTL test population ({n(p['total'])} tests, fixed list): "
            f"{p['gate']}% pass | {n(p['passed'])} pass ({pct:.1f}%), {p['failed']} fail; "
            f"{p['unsupported']} stop at constructs of later work | {yes(pop_met)} |",
            "| The mandatory test list (except the one that needs P4) | "
            f"{'All pass' if c['mandatory']['met'] else 'Not all pass'} | "
            f"{yes(c['mandatory']['met'])} |",
            "| No disagreement between the fast value operations and the reference "
            "implementation (`valsem`): seven nightly fuzz runs in a row with 100 times the "
            f"rounds | The nightly fuzz runs passed on {f['nights']} nights in a row, through "
            f"{f['through']} | {yes(fuzz_met)} |",
            "| The fixed list of probes against Icarus Verilog 13.0 "
            "([P1_SCOPE](docs/spec/P1_SCOPE.md)), with approved exclusions by source hash | "
            f"Historical {ip['probes']}-probe inventory: {evaluated} P1-evaluated, "
            f"{len(excluded)} out of scope ({exclusion_names}). Raw passing-reference gaps "
            f"{ip['stable']['gaps']}/{ip['devel']['gaps']}; required-scope gaps "
            f"{required_stable}/{required_devel}, reference mismatches "
            f"{ip['stable']['mismatches']}/{ip['devel']['mismatches']} (13.0 stable/14.0 devel). "
            "Raw outcomes are unchanged; finite probes alone do not show complete coverage "
            f"| {yes(ip['met'])} |",
            "",
            "The external sv-tests counts for kirimasim were measured on "
            f"{dates['linux']} (Linux) and {dates['macos']} (macOS); the population on "
            f"{c['measured']}; the Icarus probes on {ip['measured']}. Icarus Verilog's and "
            f"Verilator's in the last two columns on {o['measured']}, from their main branches "
            "that day. The nightly run measures sv-tests and the population again, and fails "
            "when they differ from these.",
            "",
            "| Test set | Tests | kirimasim | Icarus Verilog | Verilator |",
            "| --- | --- | --- | --- | --- |",
            f"| This repository's tests (CTest, dev configuration) | {n(c['ctest']['total'])} | "
            f"{n(c['ctest']['passed'])}, {c['ctest']['skipped']} skipped{failures_en} | — | — |",
            f"| [sv-tests](https://github.com/chipsalliance/sv-tests) simulation tests | "
            f"{s['total']} | {ours} | {o['svtests']['icarus']} | "
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
        yes = lambda b: "達成" if b else "未達"  # noqa: E731
        lines = [
            "<!-- tools/readme_counts.py が docs/counts.json から書く。手で直さない。 -->",
            "",
            "| 合格条件 | 記録した結果 | 測定時の達成状況 |",
            "| --- | --- | --- |",
            f"| [sv-tests](https://github.com/chipsalliance/sv-tests) のシミュレーションテスト"
            f"（固定コミットの手書きテスト{s['total']}件）のパス数が、Linux・macOSの両方で"
            f"Icarus Verilog以上 | Linux：kirimasim {lin['kirimasim']}件、Icarus Verilog "
            f"{lin['icarus']}件。macOS：kirimasim {mac['kirimasim']}件、Icarus Verilog "
            f"{mac['icarus']}件（{s['icarus_build'].replace(', ', '、')}。"
            f"合格条件で指定されたバージョンを各OSでビルド） | {yes(sv_met)} |",
            f"| 独自RTLテスト群（{n(p['total'])}件、対象リスト固定）のパス率{p['gate']}%以上 | "
            f"{n(p['passed'])}件パス（{pct:.1f}%）、{p['failed']}件失敗、"
            f"{p['unsupported']}件は未対応構文の診断で終了 | {yes(pop_met)} |",
            "| 必須テスト（P4が必要な1件を除く）の全件パス | "
            f"{'全件パス' if c['mandatory']['met'] else '未パスのテストあり'} | "
            f"{yes(c['mandatory']['met'])} |",
            "| 高速な値演算と参照実装（`valsem`）の比較で不一致がないこと：試行回数100倍の"
            f"ナイトリーファジングを7晩連続でパス | {f['nights']}晩連続でパス"
            f"（{f['through']}まで） | {yes(fuzz_met)} |",
            "| Icarus Verilog 13.0 と比べる固定したプローブの一覧（[P1_SCOPE](docs/spec/P1_SCOPE.ja.md)）。"
            "承認済みの除外はソースのハッシュで指定 | "
            f"過去の{ip['probes']}プローブ：P1評価対象{evaluated}、対象外{len(excluded)}件"
            f"（{exclusion_names}）。参照が通る生の差は{ip['stable']['gaps']}/{ip['devel']['gaps']}件、"
            f"必須範囲の差は{required_stable}/{required_devel}件、参照不一致は"
            f"{ip['stable']['mismatches']}/{ip['devel']['mismatches']}件（13.0 stable/14.0 devel）。"
            "観測結果は不変で、有限プローブだけでは全機能の網羅を示さない "
            f"| {yes(ip['met'])} |",
            "",
            f"kirimasim の外部 sv-tests は Linux が {dates['linux']}、macOS が {dates['macos']}、"
            f"母集団は {c['measured']}、Icarus のプローブは {ip['measured']} に測ったもので、"
            "右の 2 列の Icarus Verilog と "
            f"Verilator の数は {o['measured']} に、その日の main からビルドして測ったものです。"
            "nightly の実行が sv-tests と母集団を測り直し、これと違えば失敗します。",
            "",
            "| テスト群 | テスト数 | kirimasim | Icarus Verilog | Verilator |",
            "| --- | --- | --- | --- | --- |",
            f"| このリポジトリのテスト（CTest、dev の構成） | {n(c['ctest']['total'])} | "
            f"{n(c['ctest']['passed'])}件パス、{c['ctest']['skipped']}件スキップ{failures_ja} | — | — |",
            f"| [sv-tests](https://github.com/chipsalliance/sv-tests) のシミュレーションのテスト | "
            f"{s['total']} | {ours.replace(' (Linux), ', '（Linux）、').replace(' (macOS)', '（macOS）')} | "
            f"{o['svtests']['icarus']} | {o['svtests']['verilator']} |",
            f"| 独自テストスイートのP1評価対象テスト群 | {n(p['total'])} | {n(p['passed'])} | "
            f"{n(o['population']['icarus'])} | {n(o['population']['verilator'])} |",
            "",
            f"テスト種別ごとの件数：ユニットテスト（GoogleTest）{n(k['unit'])}、"
            f"`tests/suite/` {n(c['suite_tests'])}（`tools/` の{c['generators']}個のテストジェネレータで生成）、"
            f"ツール {n(k['tools'])}、`tests/dump_sir/` {n(k['dump_sir'])}、状態とContext "
            f"{n(k['context'])}、コマンドラインとイベントカウント {n(k['cli'])}。",
        ]
    if excluded:
        for item in excluded:
            if lang == "en":
                lines += ["", f"P1 out of scope: `{item['name']}` — {item['reason']} "
                          f"Policy reviewed {ip.get('p1_policy_reviewed', 'not recorded')}; "
                          "this is a scope reassessment of the dated observations, not a new run or PASS."]
            else:
                lines += ["", f"P1対象外：`{item['name']}` — Icarusのwire real拡張への依存を確認。P1では任意。"
                          f"方針確認日{ip.get('p1_policy_reviewed', '未記録')}。過去の観測の範囲分類だけを見直し、"
                          "再測定やPASSへの変更はしていません。"]
    return "\n".join(lines)


def render(c, lang):
    """Current repository test counts, without historical external comparisons."""
    total, passed, skipped = (c["ctest"][k] for k in ("total", "passed", "skipped"))
    failed = total - passed - skipped
    k = c["kinds"]
    if lang == "en":
        return ("<!-- Written by tools/readme_counts.py from docs/counts.json; do not edit by hand. -->\n\n"
                f"The recorded dev run has **{n(passed)} / {n(total)} tests passing**, "
                f"{skipped} skipped and {failed} failed. "
                f"It contains {n(k['unit'])} unit tests, {n(c['suite_tests'])} suite cases "
                f"({c['generators']} generators), {n(k['tools'])} tool tests, "
                f"{n(k['dump_sir'])} SIR-dump tests, {n(k['context'])} Context tests "
                f"and {n(k['cli'])} CLI/event-count tests.")
    return ("<!-- tools/readme_counts.py が docs/counts.json から書く。手で直さない。 -->\n\n"
            f"記録した dev 全体は **{n(total)} 件中 {n(passed)} 件成功**、"
            f"スキップ {skipped} 件、失敗 {failed} 件です。"
            f"内訳は単体 {n(k['unit'])}、suite {n(c['suite_tests'])}（生成器 {c['generators']}）、"
            f"ツール {n(k['tools'])}、SIR 出力 {n(k['dump_sir'])}、Context {n(k['context'])}、"
            f"CLI・イベント回数 {n(k['cli'])} 件です。")


def render_record(c, lang):
    """The full dated evidence, with links relative to docs/."""
    return render_evidence(c, lang).replace("](docs/", "](")


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
            keys = ("svtests", k) if k == "total" else ("svtests", args.os, k)
            got.append((f"sv-tests on {args.os}", keys, v))
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
    ap.add_argument("--os", choices=["linux", "macos"])
    ap.add_argument("--population", type=Path)
    ap.add_argument("--date", default=datetime.datetime.now(datetime.timezone.utc).date().isoformat())
    args = ap.parse_args()
    if args.svtests and not args.os:
        ap.error("--svtests needs --os, the OS it was measured on")
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
    if args.command == "update":
        if args.svtests:
            dates = c["svtests"].setdefault("measured", {"linux": c["measured"],
                                                       "macos": c["measured"]})
            dates[args.os] = args.date
        if args.population:
            c["measured"] = args.date
    texts = {}
    targets = [(lang, path, render) for lang, path in READMES.items()]
    targets += [(lang, ROOT / "docs" / f"VALIDATION_COUNTS{suffix}.md", render_record)
                for lang, suffix in (("en", ""), ("ja", ".ja"))]
    for lang, path, renderer in targets:
        text = path.read_text(encoding="utf-8")
        if text.count(BEGIN) != 1 or text.count(END) != 1:
            sys.exit(f"readme_counts: {path.name} needs one {BEGIN} and one {END}")
        head, rest = text.split(BEGIN)
        _, tail = rest.split(END)
        new = f"{head}{BEGIN}\n{renderer(c, lang)}\n{END}{tail}"
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
