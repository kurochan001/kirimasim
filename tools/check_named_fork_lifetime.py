#!/usr/bin/env python3
# Copyright (c) 2026 The kirimasim Authors
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
"""Check retained descendants and frame reuse under SCHEDULER_SPEC 10.8."""
import argparse
import json
from pathlib import Path
import bounded_run


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--kirimasim', type=Path, required=True)
    args = parser.parse_args()
    source = Path(__file__).resolve().parent.parent / 'tests/suite/sched/named_fork_disable_reuse.sv'
    results = []
    for rounds in (4, 80):
        run = bounded_run.run([str(args.kirimasim.resolve()), '--stats=json', str(source), f'+n={rounds}'], timeout=30)
        lines = run.stdout.splitlines()
        if run.returncode != 0 or len(lines) != 2 or lines[0] != 'errors=0':
            raise RuntimeError(f'rounds={rounds}: exit={run.returncode}, stdout={run.stdout!r}, stderr={run.stderr}')
        stats = json.loads(lines[-1])
        results.append((stats['proc.created'], stats['proc.kept_peak']))
    if results[1][0] <= results[0][0] or results[1][1] != results[0][1]:
        raise RuntimeError(f'retained process count grows with rounds: {results}')
    print(f'named fork lifetime: created/kept_peak {results}; constant retention')
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
