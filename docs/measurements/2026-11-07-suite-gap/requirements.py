#!/usr/bin/env python3
# Copyright (c) 2026 The kirimasim Authors
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
"""How many requirement rows of kirimasim-tests that no test is linked to lie in a
clause that a test of tests/suite/ cites.

  python3 requirements.py --kirimasim <checkout> --tests <checkout> --out <file>

A row is counted when no check is recorded for it in coverage/behaviors.json. The
count is by clause number alone, so it is the most that tests/suite/ could add
before anyone reads the tests: a test that cites a clause need not observe the
row's behaviour.
"""

import argparse
import json
from collections import Counter
from pathlib import Path

from measure import clauses, split_test


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--kirimasim", type=Path, required=True)
    ap.add_argument("--tests", type=Path, required=True)
    ap.add_argument("--out", type=Path, required=True)
    args = ap.parse_args()
    rows = json.loads((args.tests / "coverage/behaviors.json").read_text())["requirements"]
    cited: Counter[str] = Counter()
    for path in sorted((args.kirimasim / "tests/suite").rglob("*.sv")):
        meta, _ = split_test(path.read_text(encoding="utf-8", errors="ignore"))
        cited.update(set(clauses(meta)))

    def relation(clause: str) -> str:
        if clause in cited:
            return "same_clause"
        if any(c.startswith(clause + ".") for c in cited):
            return "a_subclause_is_cited"
        parent = clause
        while "." in parent:
            parent = parent.rsplit(".", 1)[0]
            if parent in cited:
                return "a_parent_is_cited"
        return "not_cited"

    unlinked = [r for r in rows if not r.get("checks")]
    found = Counter(relation(r["clause"]) for r in unlinked)
    by_chapter = Counter(r["clause"].split(".")[0] for r in unlinked if relation(r["clause"]) == "not_cited")
    args.out.write_text(json.dumps({
        "rows": len(rows),
        "rows_with_no_linked_test": len(unlinked),
        "by_relation_to_tests_suite": dict(found),
        "not_cited_by_chapter": by_chapter.most_common(),
        "same_clause_by_clause": Counter(
            r["clause"] for r in unlinked if relation(r["clause"]) == "same_clause"
        ).most_common(),
    }, indent=1) + "\n")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
