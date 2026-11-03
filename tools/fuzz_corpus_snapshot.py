#!/usr/bin/env python3
# Copyright (c) 2026 The kirimasim Authors
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

"""Record corpus bytes and the current libFuzzer binary, never cached verdicts."""

import argparse
import hashlib
import json
from pathlib import Path
import subprocess

ROOT = Path(__file__).resolve().parent.parent


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def snapshot(binary, corpus, command, seeds=None):
    paths = sorted(corpus.rglob('*')) if corpus.exists() else []
    if any(path.is_symlink() for path in paths):
        raise ValueError('corpus must not contain symbolic links')
    return {'schema': 'libfuzzer-corpus-snapshot@1',
            'revision': subprocess.run(['git', 'rev-parse', 'HEAD'], cwd=ROOT, check=True,
                                       capture_output=True, text=True).stdout.strip(),
            'source_dirty': bool(subprocess.run(['git', 'status', '--porcelain'], cwd=ROOT,
                                                check=True, capture_output=True, text=True).stdout.strip()),
            'binary_sha256': sha(binary), 'command': command,
            'inputs': {path.relative_to(corpus).as_posix(): sha(path) for path in paths if path.is_file()},
            'seed_inputs': ({path.relative_to(seeds).as_posix(): sha(path)
                             for path in sorted(seeds.rglob('*')) if path.is_file()}
                            if seeds is not None and seeds.is_dir() else {})}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--binary', type=Path, required=True)
    parser.add_argument('--corpus', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--seeds', type=Path)
    parser.add_argument('command', nargs=argparse.REMAINDER)
    args = parser.parse_args()
    value = snapshot(args.binary, args.corpus, args.command, args.seeds)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    with args.output.open('x') as stream:
        stream.write(json.dumps(value, indent=2)+'\n')


if __name__ == '__main__':
    main()
