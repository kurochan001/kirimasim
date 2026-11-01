#!/usr/bin/env python3
# Copyright (c) 2026 The kirimasim Authors
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
"""Exact native source checks for the complete captured-strength formatting matrix."""
import argparse
import json
import re
import sys
import tempfile
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[2]/'tools'))
import bounded_run
from gen_strength_format_matrix import BOUNDARIES, programs


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--kirimasim',required=True)
    parser.add_argument('--report',type=Path)
    args=parser.parse_args();binary=str(Path(args.kirimasim).resolve());records=[]
    with tempfile.TemporaryDirectory(prefix='kirimasim-strength-format-') as tmp:
        directory=Path(tmp)
        for name,text,expected,file_output in programs():
            path=directory/(name+'.sv');path.write_text(text)
            output=directory/'result.txt';output.unlink(missing_ok=True)
            ran=bounded_run.run([binary,path.name],cwd=tmp,text=True,timeout=15)
            actual=output.read_text() if file_output and output.exists() else ran.stdout
            errors=[line for line in ran.stderr.splitlines() if 'error:' in line or 'internal error' in line]
            if ran.returncode or errors or actual!=expected or (file_output and ran.stdout):
                raise AssertionError(f'{name}: exit={ran.returncode}\nexpected={expected!r}\nactual={actual!r}\n{ran.stderr}')
            records.append({'case':name,'passed':True})
        for conversion in BOUNDARIES:
            path=directory/'literal_width.sv'
            path.write_text('module top;wire v;initial $display("'+conversion+'",v);endmodule\n')
            ran=bounded_run.run([binary,path.name],cwd=tmp,text=True,timeout=15)
            assert ran.returncode==2 and not ran.stdout,ran.stdout+ran.stderr
            assert "field width not allowed on '%v' format specifiers" in ran.stderr,ran.stderr
            assert re.search(r'literal_width\.sv:1:\d+: error:',ran.stderr),ran.stderr
            assert 'internal error' not in ran.stderr,ran.stderr
    if args.report:args.report.write_text(json.dumps(records,indent=2)+'\n')
    print(f'strength formats: {len(records)} exact source programs passed; {len(BOUNDARIES)} positioned frontend boundaries retained')


if __name__=='__main__':main()
