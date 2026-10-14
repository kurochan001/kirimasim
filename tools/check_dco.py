#!/usr/bin/env python3
"""Checks that every commit in base..head carries the author's Signed-off-by.

Merge commits are skipped, so pass the PR's real head commit
(github.event.pull_request.head.sha), not the merge commit GitHub creates
for testing. A commit passes when one of its Signed-off-by trailers is
exactly "<author name> <<author email>>".

Signed-off-by is added by a human who has checked the change (see
CONTRIBUTING.md). AI tools never add it.
"""

import argparse
import subprocess
import sys


def git(*args: str) -> str:
    return subprocess.run(["git", *args], check=True, capture_output=True, text=True).stdout


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("--base", required=True, help="base commit (excluded)")
    ap.add_argument("--head", required=True, help="head commit (included)")
    args = ap.parse_args()

    commits = git("rev-list", "--no-merges", "--reverse", f"{args.base}..{args.head}").split()
    errors = []
    for sha in commits:
        author = git("log", "-1", "--format=%an <%ae>", sha).strip()
        trailers = git("log", "-1", "--format=%(trailers:key=Signed-off-by,valueonly)",
                       sha).splitlines()
        signoffs = {t.strip() for t in trailers if t.strip()}
        if author not in signoffs:
            subject = git("log", "-1", "--format=%s", sha).strip()
            have = ", ".join(sorted(signoffs)) or "none"
            errors.append(f"{sha[:12]} {subject!r}: needs 'Signed-off-by: {author}' "
                          f"(found: {have})")

    for e in errors:
        print(e, file=sys.stderr)
    if errors:
        print(f"check_dco: {len(errors)} of {len(commits)} commit(s) not signed off",
              file=sys.stderr)
        return 1
    print(f"check_dco: {len(commits)} commit(s) signed off")
    return 0


if __name__ == "__main__":
    sys.exit(main())
