"""Runs a program and keeps a bounded part of what it writes, for the tools
that judge kirimasim's runs (TEST_SUITE.md, "What is judged now").

A run can write without end: a design caught in a loop that prints, as a
mistake planted by tools/mutants.py can make one. A tool that kept all of
it grew until the machine ran out of memory, and took every test running
beside it down too (#27). run() keeps at most LIMIT bytes of each stream,
and stops the program as soon as it writes more. The limit is far beyond
every expected output (the largest is about 120 KB). The program's
standard input is empty, so that a design that reads STDIN does not wait
for the terminal (TEST_SUITE.md, "Where a test runs").
"""

import locale
import subprocess
import threading
from pathlib import Path

LIMIT = 16 << 20  # bytes of each stream


class OutputLimit(Exception):
    """The program wrote more than LIMIT bytes to one stream, and was stopped."""

    def __init__(self, cmd: list[str], stream: str) -> None:
        super().__init__(f"{Path(cmd[0]).name} wrote more than {LIMIT >> 20} MiB to {stream}; "
                         "it was stopped")


def run(cmd: list[str], *, cwd=None, timeout: float | None = None,
        merge_stderr: bool = False) -> subprocess.CompletedProcess:
    """As subprocess.run(cmd, cwd=cwd, timeout=timeout, capture_output=True,
    text=True, check=False) with an empty standard input, with standard
    error in standard output when
    merge_stderr is set. Raises OutputLimit when a stream goes past LIMIT,
    and subprocess.TimeoutExpired, after stopping the program, as
    subprocess.run does."""
    proc = subprocess.Popen(cmd, cwd=cwd, stdin=subprocess.DEVNULL, stdout=subprocess.PIPE,
                            stderr=subprocess.STDOUT if merge_stderr else subprocess.PIPE)
    over: list[str] = []

    def read(stream, name: str, parts: list[bytes]) -> None:
        size = 0
        while chunk := stream.read1(1 << 16):
            size += len(chunk)
            if size > LIMIT:
                over.append(name)
                proc.kill()
                break
            parts.append(chunk)
        stream.close()

    out: list[bytes] = []
    err: list[bytes] = []
    readers = [threading.Thread(target=read, args=(proc.stdout, "standard output", out))]
    if not merge_stderr:
        readers.append(threading.Thread(target=read, args=(proc.stderr, "standard error", err)))
    for r in readers:
        r.start()
    try:
        proc.wait(timeout=timeout)
    except subprocess.TimeoutExpired:
        proc.kill()
        proc.wait()
        raise
    finally:
        for r in readers:
            r.join()
    if over:
        raise OutputLimit(cmd, over[0])

    def text(parts: list[bytes]) -> str:
        # What text=True gives: the locale's encoding, and universal newlines.
        s = b"".join(parts).decode(locale.getpreferredencoding(False))
        return s.replace("\r\n", "\n").replace("\r", "\n")

    return subprocess.CompletedProcess(cmd, proc.returncode, text(out),
                                       None if merge_stderr else text(err))
