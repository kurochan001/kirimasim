#!/usr/bin/env python3
# Copyright (c) 2026 The kirimasim Authors
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

"""Keep append output usable on non-seekable files (SYSTEM_TASKS.md 6.3)."""

import argparse
import os
from pathlib import Path
import select
import subprocess
import tempfile
import threading


def source(mode):
    return f'''module top;
  int fd, status;
  string message;
  initial begin
    fd = $fopen("append.pipe", "{mode}");
    if (fd == 0) $fatal(1, "cannot open FIFO");
    $fwrite(fd, "first");
    status = $ferror(fd, message);
    $display("first error=%0d", status);
    $fwrite(fd, ":second");
    status = $ferror(fd, message);
    $display("second error=%0d", status);
    $fflush(fd);
    status = $ferror(fd, message);
    $display("flush error=%0d", status);
    $fclose(fd);
    $finish(0);
  end
endmodule
'''


def read_fifo(fd, finished, received, failures):
    try:
        while True:
            ready = select.select([fd], [], [], 0.1)[0]
            if ready:
                try:
                    chunk = os.read(fd, 4096)
                except BlockingIOError:
                    # A writer can connect after select saw EOF, before it
                    # has supplied any bytes to this non-blocking reader.
                    if finished.is_set():
                        return
                    continue
                if chunk:
                    received.extend(chunk)
                    continue
            if finished.is_set():
                return
            # A FIFO can report EOF before its first writer opens it.
            if ready:
                finished.wait(0.01)
    except OSError as error:
        failures.append(f"FIFO reader: {error}")
    finally:
        os.close(fd)


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--kirimasim", type=Path, required=True)
    exe = str(ap.parse_args().kirimasim.resolve())
    if not hasattr(os, "mkfifo"):
        print("append FIFO: skipped (os.mkfifo is unavailable)")
        return 77

    failures = []
    cases = 0
    expected = "first error=0\nsecond error=0\nflush error=0\n"
    with tempfile.TemporaryDirectory() as tmp:
        root = Path(tmp)
        fifo = root / "append.pipe"
        os.mkfifo(fifo)
        path = root / "append.sv"
        for mode in ("a", "ab"):
            path.write_text(source(mode))
            received = bytearray()
            finished = threading.Event()
            # Open the reader without waiting for the simulator to open its
            # writer, and keep every read bounded even if compilation fails.
            fd = os.open(fifo, os.O_RDONLY | os.O_NONBLOCK)
            reader = threading.Thread(target=read_fifo,
                                      args=(fd, finished, received, failures), daemon=True)
            reader.start()
            try:
                result = subprocess.run([exe, str(path)], cwd=root, capture_output=True,
                                        text=True, timeout=20)
                if result.returncode != 0 or result.stdout != expected:
                    failures.append(f"{mode}: code {result.returncode}, output {result.stdout!r}, "
                                    f"error {result.stderr!r}")
            except subprocess.TimeoutExpired:
                failures.append(f"{mode}: simulator timed out")
            finally:
                finished.set()
                reader.join(timeout=2)
            if reader.is_alive():
                failures.append(f"{mode}: FIFO reader did not finish")
                break
            if received != b"first:second":
                failures.append(f"{mode}: FIFO data {bytes(received)!r}")
            cases += 1
    print(f"append FIFO: {cases} checks, {len(failures)} failures")
    for failure in failures:
        print(failure)
    return bool(failures)


if __name__ == "__main__":
    raise SystemExit(main())
