"""The list of tests allowed to be skipped as unsupported (TEST_SUITE.md,
"What is judged now"): tests/suite/skips.txt holds one test per line, its
path under tests/suite/ without .sv, then # and the reason. Blank lines
and lines that start with # are comments.
"""

from __future__ import annotations

from pathlib import Path

SKIPS = Path(__file__).resolve().parent.parent / "tests" / "suite" / "skips.txt"


def read_skips(path: Path = SKIPS) -> tuple[dict[str, str], list[str]]:
    """The tests on the list with their reasons, and the problems of the
    list. A list that does not exist allows no skips."""
    skips: dict[str, str] = {}
    problems = []
    if not path.exists():
        return skips, problems
    for number, line in enumerate(path.read_text(encoding="utf-8").splitlines(), 1):
        text = line.strip()
        if not text or text.startswith("#"):
            continue
        name, _, reason = text.partition("#")
        name, reason = name.strip(), reason.strip()
        if not reason:
            problems.append(f"{path.name}:{number}: {name} has no reason")
        if name in skips:
            problems.append(f"{path.name}:{number}: {name} is listed twice")
        skips[name] = reason
    return skips, problems


def skip_name(test: Path, path: Path = SKIPS) -> str | None:
    """The test's name on the list: its path under the list's directory,
    without .sv; None for a test outside that directory."""
    try:
        return test.resolve().relative_to(path.parent.resolve()).with_suffix("").as_posix()
    except ValueError:
        return None
