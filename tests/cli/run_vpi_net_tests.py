#!/usr/bin/env python3
# Copyright (c) 2026 The kirimasim Authors
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
"""Judge approved whole-net writes without conflating API rejection with support."""
import argparse
import collections
import hashlib
import json
from pathlib import Path
import sys
import tempfile

from run_vpi_tests import ROOT,build_plugin
import bounded_run
from gen_vpi_net_matrix import cases


def changes(text):
    codes=[];out=[];time=0
    for line in text.splitlines():
        if line.startswith('$var '):
            part=line.split()
            assert '$driver' not in part[4]
            if part[4]=='n':codes.append(part[3])
        elif line.startswith('#'):time=int(line[1:])
        elif codes and line[:1] in ('0','1','x','z') and line[1:]==codes[0]:out.append([time,line[0]])
    assert codes and len(set(codes))==1,codes
    return out


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--kirimasim',type=Path,required=True)
    parser.add_argument('--fst-reader',type=Path)
    parser.add_argument('--cc',default='cc')
    parser.add_argument('--case',action='append')
    parser.add_argument('--report',type=Path)
    args=parser.parse_args();binary=str(args.kirimasim.resolve())
    rows=[r for r in cases() if not args.case or r['name'] in args.case]
    assert rows and (not args.case or set(args.case)=={r['name'] for r in rows})
    records=[]
    with tempfile.TemporaryDirectory(prefix='vpi-net-native-') as temp:
        directory=Path(temp)
        plugins=[]
        for name in ('net_writes','net_edges'):
            plugin=directory/(name+'.vpi');build_plugin(args.cc,ROOT/'tests/probes/vpi'/(name+'.c'),plugin)
            plugins.append('--vpi='+str(plugin))
        for row in rows:
            path=directory/(row['name']+'.sv');path.write_text(row['source'])
            ran=bounded_run.run([binary,*row['options'],'--top=top',*plugins,path.name],cwd=directory,timeout=20)
            diagnostics='\n'.join(s for s in ran.stderr.splitlines() if not s.startswith('kirimasim: info:'))
            output=ran.stdout==row['expected_stdout']
            if row['category']=='dump-phase':
                output=ran.stdout.startswith(row['expected_stdout']) and ' net-write\n' in ran.stdout and 'runtime\n' not in ran.stdout
            good=ran.returncode==row['expected_exit'] and output and not diagnostics
            if not good:raise AssertionError(f"{row['name']}: exit={ran.returncode} expected={row['expected_exit']}\nexpected={row['expected_stdout']!r}\nactual={ran.stdout[:4096]!r}\n{diagnostics[:4096]}")
            records.append(dict(name=row['name'],source_sha256=hashlib.sha256(row['source'].encode()).hexdigest(),
                                category=row['category'],check='passed',exit=ran.returncode,
                                native_feature_support=row['category']=='execution'))
        if args.fst_reader and not args.case:
            for name,expected in [('distributed_pending_release',[[0,'x'],[4,'0'],[14,'1']]),
                                  ('module_path_pending_force',[[0,'x'],[5,'0'],[27,'1']])]:
                row=next(r for r in rows if r['name']==name)
                path=directory/(name+'.sv')
                for extension in ('vcd','fst'):
                    wave=directory/(name+'.'+extension)
                    ran=bounded_run.run([binary,'--wave='+wave.name,*plugins,path.name],cwd=directory,timeout=20)
                    assert ran.returncode==0 and ran.stdout==row['expected_stdout'],(name,ran.stderr)
                    if extension=='fst':
                        decoded=bounded_run.run([str(args.fst_reader.resolve()),'--decode',str(wave)],timeout=20)
                        assert decoded.returncode==0 and not decoded.stderr
                        text=decoded.stdout
                    else:text=wave.read_text()
                    assert changes(text)==expected,(name,extension,changes(text),expected)
                    records.append(dict(name=name+'_'+extension,category='waveform',check='passed',public_changes=expected))
    if args.report:args.report.write_text(json.dumps(records,indent=2)+'\n')
    print('whole-net VPI:',len(records),'checks passed;',dict(collections.Counter(r['category'] for r in records)))
    return 0


if __name__=='__main__':sys.exit(main())
