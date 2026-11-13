#!/usr/bin/env python3
# Copyright (c) 2026 The kirimasim Authors
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

"""Corpus carry-over is input preservation, not a reusable pass result."""

import importlib.util
from pathlib import Path
import tempfile
import unittest
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[2]
spec = importlib.util.spec_from_file_location('snapshot', ROOT/'tools/fuzz_corpus_snapshot.py')
snapshot = importlib.util.module_from_spec(spec)
spec.loader.exec_module(snapshot)


class CorpusTests(unittest.TestCase):
    def test_fixed_replay_batches_keep_every_file_and_propagate_failure(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            binary, script, log = root/'fake fuzzer', root/'replay.cmake', root/'argv.jsonl'
            binary.write_text('#!'+sys.executable+'\nimport json,sys\n'
                              'assert len(sys.argv[2:]) <= 64\n'
                              'with open('+repr(str(log))+',"a") as f: f.write(json.dumps(sys.argv[2:])+"\\n")\n')
            binary.chmod(0o755)
            names = [str(root/('input with spaces '+str(i)+'.sv')) for i in range(131)]
            template = (ROOT/'tests/fuzz/run_fuzz_suite.cmake.in').read_text()
            script.write_text(template.replace('@suite_sources@', ';'.join(names)))
            command = ['cmake', '-DFUZZ_BINARY='+str(binary), '-DARTIFACT_DIR='+str(root), '-P', str(script)]
            result = subprocess.run(command, capture_output=True, text=True)
            self.assertEqual(result.returncode, 0, result.stderr)
            import json
            batches = [json.loads(line) for line in log.read_text().splitlines()]
            self.assertEqual(list(map(len, batches)), [64, 64, 3])
            self.assertEqual([name for batch in batches for name in batch], names)
            binary.write_text('#!'+sys.executable+'\nimport sys; sys.exit(7)\n')
            self.assertNotEqual(subprocess.run(command, capture_output=True).returncode, 0)

    def test_binary_and_input_changes_are_recorded(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            binary, corpus = root/'binary', root/'corpus'
            binary.write_bytes(b'binary one'); corpus.mkdir()
            (corpus/'case').write_bytes(b'input one')
            first = snapshot.snapshot(binary, corpus, ['binary', 'corpus/case'])
            binary.write_bytes(b'binary two')
            self.assertNotEqual(first['binary_sha256'], snapshot.snapshot(binary, corpus, [])['binary_sha256'])
            (corpus/'case').write_bytes(b'input two')
            self.assertNotEqual(first['inputs'], snapshot.snapshot(binary, corpus, [])['inputs'])
            self.assertEqual(first['command'], ['binary', 'corpus/case'])
            self.assertNotIn('pass', first)

    def test_workflow_replays_current_binary_and_saves_across_revisions(self):
        full = (ROOT/'.github/workflows/nightly.yml').read_text()
        self.assertEqual(full.count('KIRIMA_FUZZ_SCALE=100 KIRIMA_FUZZ_SEED="$fuzz_seed"'), 2)
        self.assertIn("--gtest_filter='W4Ops.*:Valsem.*:Scheduler.*:Heap.*'", full)
        self.assertIn("--gtest_filter='Wide.*'", full)
        workflow = full.split('\n  fuzz:\n', 1)[1].split('\n  svtests-macos:', 1)[0]
        self.assertIn('restore-keys: |\n            libfuzzer-corpus-linux-clang23-v1-\n', workflow)
        self.assertLess(workflow.index('cmake --build --preset fuzz'), workflow.index('Replay carried corpus'))
        self.assertLess(workflow.index('Replay carried corpus'), workflow.index('Fuzz for 30 minutes'))
        self.assertIn('-print0 |', workflow)
        self.assertIn('xargs -0 -r -n 128', workflow)
        self.assertIn('-max_total_time=1800 -timeout=30', workflow)
        self.assertIn('name: fuzz-findings', workflow)
        self.assertIn('path: build/fuzz-new', workflow)
        self.assertIn('github.run_attempt', workflow)


if __name__ == '__main__':
    unittest.main()
