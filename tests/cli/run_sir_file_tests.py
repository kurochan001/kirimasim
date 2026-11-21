#!/usr/bin/env python3
# Copyright (c) 2026 The kirimasim Authors
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

"""Exercise format-1 save/load at the command line without access to source."""

import argparse
import re
import sys
import tempfile
from pathlib import Path

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE.parents[1] / "tools"))
import bounded_run  # noqa: E402


def check(exe: str, work: Path) -> None:
    def run(*args: str, code: int = 0, diagnostic: str = ""):
        proc = bounded_run.run([exe, *args], cwd=work, timeout=30)
        if proc.returncode != code or diagnostic not in proc.stderr:
            raise ValueError(f"{args}: exit {proc.returncode}, stderr: {proc.stderr}")
        if code and proc.stdout:
            raise ValueError(f"{args}: unexpected stdout on error: {proc.stdout}")
        return proc

    source = work / "source.sv"
    source.write_text("""module top;
  logic q;
  initial begin
    $dumpvars(0, top);
    if (q) $display("taken");
    $display("q=%b arg=%d", q, $test$plusargs("flag"));
    #3 q=1;
    $display("done");
  end
endmodule
""")
    legacy = run("--dump-sir", source.name).stdout
    if run("--dump-sir=0", source.name).stdout != legacy:
        raise ValueError("explicit format 0 differs from the default")
    opts = ["--init-values=one", "--cond-x=warn"]
    saved = work / "saved.sir"
    load = "--load-sir=" + saved.name
    plain = run("--dump-sir=1", source.name).stdout
    marked = run("--dump-sir=1", *opts, source.name).stdout
    direct = run(*opts, "--stats=json", "--wave=trace.vcd", source.name, "+flag")
    wave = work / "trace.vcd"

    def trace() -> str:
        return re.sub(r"\$date\s+.*?\$end", "", wave.read_text(), count=1, flags=re.S)

    direct_wave = trace()
    wave.unlink()
    source.unlink()
    saved.write_text("// canonicalize this input\n" + marked)
    if run(load, "--dump-sir=1").stdout != marked:
        raise ValueError("format-1 canonical bytes changed on readback")
    replay = run(load, *opts, "--stats=json", "--wave=trace.vcd", "+flag")
    if (replay.stdout != direct.stdout or "q=1" not in replay.stdout or
            "done" not in replay.stdout or trace() != direct_wave):
        raise ValueError("saved design changed output, counters or VCD")
    saved.write_text(plain)
    if run(load, "--dump-sir=0").stdout != legacy:
        raise ValueError("legacy inspection differs after format-1 readback")
    run(load, "--cond-x=warn", code=2, diagnostic="instrumentation for --cond-x")
    run(load, "--init-values=one", code=2, diagnostic="instrumentation for --init-values")
    for args in ([load, "other.sv"], [load, "--top=top"], ["parse", load], [load, load],
                 ["--load-sir="], [load, "--dump-sir=2"], [load, "--dump-sir=1", "--stats=json"]):
        run(*args, code=2, diagnostic="kirimasim: error:")
    for text, error in (("sir 2 design{}", "header"), ("sir 1 design{}", "missing record field"),
                        (plain + "trailing", "unexpected text after design"),
                        (legacy, "header")):
        saved.write_text(text)
        run(load, code=2, diagnostic=error)
    with saved.open("wb") as f:
        f.truncate((64 << 20) + 1)
    run(load, code=2, diagnostic="size limit exceeded")
    saved.unlink()
    run(load, code=2, diagnostic="cannot open SIR file")


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--kirimasim", required=True, type=Path)
    args = parser.parse_args()
    try:
        with tempfile.TemporaryDirectory() as tmp:
            check(str(args.kirimasim.resolve()), Path(tmp))
    except (OSError, ValueError, bounded_run.OutputLimit) as error:
        print(error, file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
