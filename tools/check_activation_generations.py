#!/usr/bin/env python3
# Copyright (c) 2026 The kirimasim Authors
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

"""Exercise every production activation generation and record retirement.

Uses the installed C++ compiler; adds no dependency or simulator knob.
This explicit stress probe is outside regular CTest (four billion scopes).
"""
import argparse
from datetime import date
import hashlib
import json
from pathlib import Path
import re
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parent.parent
PROBE = r'''#include <array>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include "kirimasim/kernel/activation.h"
namespace k=kirimasim::kernel;
int main(){
 k::ActivationStore store;
 const std::array<k::ActivationField,1> fields{{{k::ActivationKind::Two,64,{}}}};
 auto layout=store.addLayout(fields);auto empty=store.addLayout({});if(!layout||!empty)return 2;
 auto first=store.create(layout.value);if(!first)return 3;
 auto old=store.bind(first.value,0);if(!old)return 4;
 first.value.reset();
 const auto start=std::chrono::steady_clock::now();
 for(std::uint64_t generation=2;generation<=UINT32_MAX;++generation){
  auto owner=store.create(empty.value);
  if(!owner||owner.value.token().bits!=((generation<<32)|1)){std::printf("invalid generation %llu\n",static_cast<unsigned long long>(generation));return 5;}
  if((generation&0xfffffff)==0){std::printf("generation %llu elapsed %.3f s\n",static_cast<unsigned long long>(generation),std::chrono::duration<double>(std::chrono::steady_clock::now()-start).count());std::fflush(stdout);}
 }
 auto next=store.create(layout.value);if(!next)return 6;
 if(next.value.token().bits!=((std::uint64_t{1}<<32)|2)||store.retain(old.value)||store.stats().entries!=2)return 7;
 std::printf("PASS: all 4294967295 generations, retired row and stale binding checked; elapsed %.3f s\n",std::chrono::duration<double>(std::chrono::steady_clock::now()-start).count());
}
'''


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--compiler', default='c++')
    parser.add_argument('--output', type=Path)
    args = parser.parse_args()
    version = subprocess.run([args.compiler, '--version'], capture_output=True, text=True, check=True).stdout.splitlines()[0]
    with tempfile.TemporaryDirectory(prefix='kirimasim-activation-generations-') as folder:
        source = Path(folder, 'probe.cpp')
        binary = Path(folder, 'probe')
        source.write_text(PROBE)
        subprocess.run([args.compiler, '-std=c++20', '-O3', '-ffp-contract=off',
                        '-I' + str(ROOT / 'src/kernel/include'),
                        str(ROOT / 'src/kernel/activation.cpp'), str(source), '-o', str(binary)],
                       check=True)
        result = subprocess.run([str(binary)], capture_output=True, text=True, timeout=600, check=True)
    match = re.search(r'PASS: all 4294967295 generations, retired row and stale binding checked; elapsed ([0-9.]+) s', result.stdout)
    if match is None:
        raise SystemExit('generation probe did not report completion')
    data = {'date': date.today().isoformat(), 'compiler': version,
            'generations_checked': 4294967295, 'generation_bits': 32,
            'retired_records': 1, 'total_record_entries': 2,
            'stale_binding_rejected': True, 'elapsed_seconds': float(match[1]),
            'kernel_source_sha256': hashlib.sha256((ROOT / 'src/kernel/activation.cpp').read_bytes()).hexdigest(),
            'public_header_sha256': hashlib.sha256((ROOT / 'src/kernel/include/kirimasim/kernel/activation.h').read_bytes()).hexdigest(),
            'probe_sha256': hashlib.sha256(PROBE.encode()).hexdigest(),
            'result': 'pass; production generation limit unchanged'}
    text = json.dumps(data, indent=2) + '\n'
    if args.output:
        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.output.write_text(text)
    print(text, end='')
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
