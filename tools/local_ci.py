#!/usr/bin/env python3
"""Runs the CI checks on the local machine, before a push.

CI runs them on GitHub's Linux and macOS runners after a push; main takes
direct pushes, so this script is the check that comes first. It runs, in
order:

1. configure, build and ctest for each preset (default: dev, release,
   asan, ubsan),
2. tools/check_release_flags.py (when the release preset is included),
3. tools/check_banned.py and tools/check_suite_files.py,
4. clang-format --dry-run --Werror on all tracked .cpp and .h files,
5. clang-tidy on all tracked .cpp files under src/ and tests/ (using the
   dev build's compile_commands.json), with the custom checks of
   .clang-tidy, and tools/check_global_state.py with its clang-tidy probe,
6. tools/check_expected_change.py on each commit that is not on the
   upstream branch yet, as CI checks each pushed commit (nothing to check
   without an upstream branch),
7. tools/readme_counts.py check on the dev preset's tests (when it is
   included), as CI does.

clang-format and clang-tidy are taken from --llvm-bin, then from PATH,
then from Homebrew's LLVM (/opt/homebrew/opt/llvm/bin). A missing tool is
reported as "not run" and makes the result fail unless --skip-lint is given.

Prints a summary to attach to the report of a push and exits with 1 if
any step failed.

The builds with the oldest supported compilers (the min-compilers job of
ci.yml) are left to CI, as a development machine seldom has those
compilers; so are the nightly jobs.
"""

import argparse
import platform
import shutil
import subprocess
import sys
import time
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
DEFAULT_PRESETS = ["dev", "release", "asan", "ubsan"]
HOMEBREW_LLVM = Path("/opt/homebrew/opt/llvm/bin")


class Step:
    def __init__(self, name: str, status: str, seconds: float, detail: str = ""):
        self.name = name
        self.status = status  # "ok", "FAILED" or "not run"
        self.seconds = seconds
        self.detail = detail


def run(cmd: list[str], log: Path) -> bool:
    log.parent.mkdir(parents=True, exist_ok=True)
    with log.open("w", encoding="utf-8") as f:
        f.write("$ " + " ".join(cmd) + "\n")
        f.flush()
        proc = subprocess.run(cmd, cwd=ROOT, stdout=f, stderr=subprocess.STDOUT, check=False)
    return proc.returncode == 0


def timed(name: str, cmd: list[str], log: Path) -> Step:
    start = time.monotonic()
    ok = run(cmd, log)
    return Step(name, "ok" if ok else "FAILED", time.monotonic() - start,
                "" if ok else f"see {log.relative_to(ROOT)}")


def find_tool(name: str, llvm_bin: Path | None) -> str | None:
    if llvm_bin and (llvm_bin / name).exists():
        return str(llvm_bin / name)
    found = shutil.which(name)
    if found:
        return found
    if (HOMEBREW_LLVM / name).exists():
        return str(HOMEBREW_LLVM / name)
    return None


def tool_version(cmd: list[str]) -> str:
    try:
        out = subprocess.run(cmd, capture_output=True, text=True, check=False).stdout
    except OSError:
        return "unavailable"
    return out.splitlines()[0].strip() if out else "unavailable"


def git_files(*patterns: str) -> list[str]:
    out = subprocess.run(["git", "ls-files", *patterns], cwd=ROOT, capture_output=True,
                         text=True, check=True).stdout
    return out.split()


def expected_changes(logs: Path) -> Step:
    """check_expected_change.py on each commit not on the upstream branch yet."""
    start = time.monotonic()
    upstream = subprocess.run(["git", "rev-parse", "--verify", "--quiet", "@{upstream}"],
                              cwd=ROOT, capture_output=True, text=True, check=False)
    if upstream.returncode != 0:
        return Step("check_expected_change", "ok", 0.0, "no upstream branch: nothing to check")
    commits = subprocess.run(["git", "rev-list", "--reverse", "@{upstream}..HEAD"], cwd=ROOT,
                             capture_output=True, text=True, check=True).stdout.split()
    log = logs / "check_expected_change.log"
    log.parent.mkdir(parents=True, exist_ok=True)
    ok = True
    with log.open("w", encoding="utf-8") as f:
        for c in commits:
            f.write(f"commit {c}\n")
            f.flush()
            proc = subprocess.run([sys.executable, "tools/check_expected_change.py",
                                   "--base", c + "^", "--head", c], cwd=ROOT, stdout=f,
                                  stderr=subprocess.STDOUT, check=False)
            ok = ok and proc.returncode == 0
    detail = f"{len(commits)} commit(s) not pushed" if ok else f"see {log.relative_to(ROOT)}"
    return Step("check_expected_change", "ok" if ok else "FAILED", time.monotonic() - start,
                detail)


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("--presets", default=",".join(DEFAULT_PRESETS),
                    help="comma-separated presets (default: %(default)s)")
    ap.add_argument("--skip-lint", action="store_true",
                    help="do not run clang-format and clang-tidy")
    ap.add_argument("--llvm-bin", type=Path, default=None,
                    help="directory that contains clang-format and clang-tidy")
    args = ap.parse_args()

    presets = [p for p in args.presets.split(",") if p]
    logs = ROOT / "build" / "local-ci"
    steps: list[Step] = []

    for preset in presets:
        # Warnings in kirimasim's own code are errors here, as in CI's clang
        # builds (KIRIMASIM_WERROR in CMakeLists.txt).
        # The dev preset's results give the READMEs' counts (step 7).
        junit = ["--output-junit", str(logs / "ctest-dev.xml")] if preset == "dev" else []
        for kind, cmd in (("configure", ["cmake", "--preset", preset, "-DKIRIMASIM_WERROR=ON"]),
                          ("build", ["cmake", "--build", "--preset", preset]),
                          ("test", ["ctest", "--preset", preset, *junit])):
            step = timed(f"{preset}: {kind}", cmd, logs / f"{preset}-{kind}.log")
            steps.append(step)
            if step.status != "ok":
                break

    if "release" in presets:
        steps.append(timed("check_release_flags",
                           [sys.executable, "tools/check_release_flags.py"],
                           logs / "check_release_flags.log"))
    steps.append(timed("check_banned", [sys.executable, "tools/check_banned.py"],
                       logs / "check_banned.log"))
    steps.append(timed("check_suite_files", [sys.executable, "tools/check_suite_files.py"],
                       logs / "check_suite_files.log"))
    # Without clang-tidy, the allowlist is checked against src/ only; the
    # probe that shows the custom check runs needs clang-tidy.
    state = [sys.executable, "tools/check_global_state.py"]
    tidy_for_state = None if args.skip_lint else find_tool("clang-tidy", args.llvm_bin)
    if tidy_for_state:
        state += ["--clang-tidy", tidy_for_state]
    steps.append(timed("check_global_state", state, logs / "check_global_state.log"))
    steps.append(expected_changes(logs))
    if "dev" in presets and (logs / "ctest-dev.xml").exists():
        steps.append(timed("readme_counts",
                           [sys.executable, "tools/readme_counts.py", "check", "--build",
                            "build/dev", "--junit", str(logs / "ctest-dev.xml")],
                           logs / "readme_counts.log"))

    clang_format = clang_tidy = None
    if not args.skip_lint:
        clang_format = find_tool("clang-format", args.llvm_bin)
        clang_tidy = find_tool("clang-tidy", args.llvm_bin)
        if clang_format:
            steps.append(timed("clang-format",
                               [clang_format, "--dry-run", "--Werror",
                                *git_files("*.cpp", "*.h")],
                               logs / "clang-format.log"))
        else:
            steps.append(Step("clang-format", "not run", 0.0, "clang-format not found"))
        if not clang_tidy:
            steps.append(Step("clang-tidy", "not run", 0.0, "clang-tidy not found"))
        elif not (ROOT / "build" / "dev" / "compile_commands.json").exists():
            steps.append(Step("clang-tidy", "not run", 0.0,
                              "needs the dev preset (build/dev/compile_commands.json)"))
        else:
            cmd = [clang_tidy, "--experimental-custom-checks", "-p", "build/dev", "--quiet"]
            if platform.system() == "Darwin":
                sdk = subprocess.run(["xcrun", "--show-sdk-path"], capture_output=True,
                                     text=True, check=False).stdout.strip()
                if sdk:
                    cmd.append(f"--extra-arg=-isysroot{sdk}")
            steps.append(timed("clang-tidy", cmd + git_files("src/*.cpp", "tests/*.cpp"),
                               logs / "clang-tidy.log"))

    head = subprocess.run(["git", "rev-parse", "--short=12", "HEAD"], cwd=ROOT,
                          capture_output=True, text=True, check=False).stdout.strip()
    dirty = subprocess.run(["git", "status", "--porcelain"], cwd=ROOT, capture_output=True,
                           text=True, check=False).stdout.strip()

    print("local CI summary")
    print(f"  commit:   {head}{' (uncommitted changes present)' if dirty else ''}")
    print(f"  system:   {platform.system()} {platform.release()} {platform.machine()}")
    print(f"  compiler: {tool_version(['c++', '--version'])}")
    print(f"  cmake:    {tool_version(['cmake', '--version'])}")
    if clang_format:
        print(f"  format:   {tool_version([clang_format, '--version'])}")
    if clang_tidy:
        print(f"  tidy:     {tool_version([clang_tidy, '--version'])}")
    failed = False
    for s in steps:
        line = f"  {s.status:8} {s.name:28} {s.seconds:7.1f}s"
        if s.detail:
            line += f"  ({s.detail})"
        print(line)
        if s.status != "ok":
            failed = True
    print("result: " + ("FAILED" if failed else "all checks passed"))
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
