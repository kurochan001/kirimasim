#!/usr/bin/env python3
# Copyright (c) 2026 The kirimasim Authors
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

"""Binary captures keep NUL, non-UTF-8 and CR/LF, with the output bound."""

import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / "tools"))
import bounded_run  # noqa: E402


def main():
    data = bytes(range(256))
    program = ("import sys;sys.stdout.buffer.write(bytes(range(256)));sys.stdout.flush();"
               "sys.stderr.buffer.write(b'\\xff\\x00\\r\\n')")
    result = bounded_run.run([sys.executable, "-c", program], text=False, timeout=10)
    assert result.returncode == 0
    assert result.stdout == data
    assert result.stderr == b"\xff\x00\r\n"
    result = bounded_run.run([sys.executable, "-c", program], text=False, merge_stderr=True, timeout=10)
    assert result.returncode == 0
    assert result.stdout == data + b"\xff\x00\r\n"
    assert result.stderr is None
    for stream in ("stdout", "stderr"):
        program = f"import sys;sys.{stream}.buffer.write(b'\\xff' * {bounded_run.LIMIT + 1})"
        try:
            bounded_run.run([sys.executable, "-c", program], text=False, timeout=10)
        except bounded_run.OutputLimit:
            pass
        else:
            raise AssertionError(f"the binary {stream} output bound was not enforced")
    print("bounded binary captures: bytes, merged streams and both bounds checked")


if __name__ == "__main__":
    main()
