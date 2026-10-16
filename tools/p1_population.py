#!/usr/bin/env python3
"""The second P1 gate of TEST_SUITE.md: 90% of the own-suite tests in P1 scope.

  tools/p1_population.py select --tests-repo DIR > tests/gates/p1_population.txt
  tools/p1_population.py run --tests-repo DIR --kirimasim BIN [--list FILE]

select prints the population: the simulation tests of the test-suite
repository that use no P3 feature. A test is out when its source, without
comments and string literals, contains one of the words of TEST_SUITE.md
("自前のテストの 90%"). The first line records the repository commit.

run runs every test of the frozen list (tests/gates/p1_population.txt) with
tools/run_suite_test.py and reports the pass rate. A test skipped as
unsupported counts as failed. The exit code is 1 below 90%.
"""

import argparse
import concurrent.futures
import os
import re
import subprocess
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
DEFAULT_LIST = HERE.parent / "tests" / "gates" / "p1_population.txt"
GATE = 0.90

# P3 features (TEST_SUITE.md); a match takes the test out of the population.
P3_FEATURES = {
    "class": r"\bclass\b",
    "constrained random": r"\brand\b|\brandc\b|\brandomize\b|\bconstraint\b",
    "coverage": r"\bcovergroup\b|\bcoverpoint\b",
    "concurrent assertion": r"\bproperty\b|\bsequence\b|\bexpect\b",
    "clocking": r"\bclocking\b|##",
    "program": r"\bprogram\b",
    "mailbox or semaphore": r"\bmailbox\b|\bsemaphore\b",
    "process class": r"\bprocess\b",
    "dynamic data": r"\[\$\]|\[\]\s*[;=,)]|\bnew\s*\[|\[\*\]|\[string\]|\[int\]",
    "DPI-C export": r'\bexport\s+"DPI',
    "checker": r"\bchecker\b",
    "let": r"\blet\b",
}


def header(text: str) -> dict[str, str]:
    m = re.search(r"/\*(.*?)\*/", text, re.DOTALL)
    meta: dict[str, str] = {}
    for line in (m.group(1) if m else "").splitlines():
        fm = re.match(r"^:([A-Za-z_-]+):\s?(.*)$", line.strip())
        if fm:
            meta[fm.group(1)] = fm.group(2).strip()
    return meta


def p3_features(text: str) -> list[str]:
    body = re.sub(r"/\*.*?\*/", "", text, flags=re.DOTALL)
    body = re.sub(r"//[^\n]*", "", body)
    body = re.sub(r'"(?:\\.|[^"\\])*"', '""', body)
    return [name for name, pat in P3_FEATURES.items() if re.search(pat, body)]


def select(repo: Path) -> int:
    commit = subprocess.run(["git", "-C", str(repo), "rev-parse", "--short=12", "HEAD"],
                            capture_output=True, text=True, check=False).stdout.strip()
    names = []
    total = 0
    for path in sorted((repo / "tests").rglob("*.sv")):
        text = path.read_text(encoding="utf-8", errors="ignore")
        meta = header(text)
        if "name" not in meta or "simulation" not in meta.get("type", "").split():
            continue
        total += 1
        if not p3_features(text):
            names.append(str(path.relative_to(repo / "tests").with_suffix("")))
    print(f"# P1 gate population (TEST_SUITE.md): kirimasim-tests {commit}, "
          f"{len(names)} of {total} simulation tests; made by tools/p1_population.py select")
    for n in names:
        print(n)
    return 0


def run(repo: Path, kirimasim: str, listing: Path, jobs: int) -> int:
    lines = listing.read_text(encoding="utf-8").splitlines()
    names = [ln for ln in lines if ln and not ln.startswith("#")]

    def one(name: str) -> tuple[str, int]:
        p = subprocess.run([sys.executable, str(HERE / "run_suite_test.py"), "--kirimasim",
                            kirimasim, str(repo / "tests" / (name + ".sv"))],
                           capture_output=True, text=True, check=False)
        return name, p.returncode

    with concurrent.futures.ThreadPoolExecutor(max_workers=jobs) as pool:
        results = list(pool.map(one, names))
    passed = sum(1 for _, rc in results if rc == 0)
    skipped = sum(1 for _, rc in results if rc == 77)
    failed = len(results) - passed - skipped
    rate = passed / len(results) if results else 0.0
    print(f"{lines[0] if lines else ''}")
    print(f"passed {passed}, failed {failed}, unsupported {skipped} (counted as failed): "
          f"{100 * rate:.1f}% (gate {100 * GATE:.0f}%, "
          f"{int(-(-GATE * len(results) // 1))} of {len(results)})")
    for name, rc in results:
        if rc not in (0, 77):
            print(f"FAIL {name}")
    return 0 if rate >= GATE else 1


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("command", choices=["select", "run"])
    ap.add_argument("--tests-repo", required=True, type=Path)
    ap.add_argument("--kirimasim")
    ap.add_argument("--list", type=Path, default=DEFAULT_LIST)
    ap.add_argument("--jobs", type=int, default=os.cpu_count() or 4)
    args = ap.parse_args()
    if args.command == "select":
        return select(args.tests_repo.resolve())
    if not args.kirimasim:
        ap.error("run needs --kirimasim")
    return run(args.tests_repo.resolve(), str(Path(args.kirimasim).resolve()), args.list,
               args.jobs)


if __name__ == "__main__":
    sys.exit(main())
