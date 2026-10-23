#!/usr/bin/env python3
# Copyright (c) 2026 The kirimasim Authors
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

"""Check delayed NBA target diagnostics, delay capture, and SIR forms."""

import argparse
import sys
import tempfile
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / 'tools'))
import bounded_run  # noqa: E402


def main():
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument('--kirimasim',required=True,type=Path)
    args = parser.parse_args()
    simulator = str(args.kirimasim.resolve())
    checked = 0
    with tempfile.TemporaryDirectory(prefix='kirimasim-delayed-nba-') as tmp:
        path = Path(tmp)/'nba.sv'
        def run(source, expected='', code=0, diagnostic='', options=()):
            nonlocal checked
            path.write_text('`timescale 1ns/1ns\n'+source+'\n')
            ran = bounded_run.run([simulator,*options,str(path)],cwd=tmp,timeout=10)
            if ran.returncode != code or ran.stdout != expected or diagnostic not in ran.stderr:
                raise AssertionError(f'NBA check {checked}: {ran.returncode}\n{ran.stdout}\n{ran.stderr}')
            if 'internal error' in ran.stderr:
                raise AssertionError(ran.stderr)
            checked += 1
            return ran
        for width in (32,129):
            for target in ('a[i]','v[pos+:3]'):
                decl = f'logic [{width-1}:0] a[2],v; int i=1,pos=2; logic [128:0] d;'
                run('module top; '+decl+f' initial begin d=129\'h10000000000000000; {target} <= #d 7; end endmodule',
                    code=2,diagnostic='the delay takes the time past the largest time')
                # Delay is captured once, even though its variable changes immediately.
                run('module top; '+decl+f''' initial begin
                  a[1]=0; v=0; d=2; {target} <= #d 7; d=0; i=0; pos=0;
                  #1; $display("before %0d %0d",a[1],v);
                  #2; $display("after %0d %0d",a[1],v); end endmodule''',
                    'before 0 0\nafter '+('7 0\n' if target=='a[i]' else '0 28\n'))
                for unknown in ("'x","'z"):
                    run('module top; '+decl+f''' initial begin
                      a[1]=0; v=0; d={unknown}; {target} <= #d 7;
                      #0; $display("inactive %0d %0d",a[1],v);
                      #1; $display("after %0d %0d",a[1],v); end endmodule''',
                        'inactive 0 0\nafter '+('7 0\n' if target=='a[i]' else '0 28\n'))
                # Automatic targets stay illegal under both compile and execution entry points.
                for local in ('v','a[i]','v[pos+:3]'):
                    source = f'module top; task automatic t(); {decl} {local} <= #2 7; endtask initial t(); endmodule'
                    for options in (('--dump-sir',),()):
                        run(source,code=2,diagnostic='error:',options=options)
        # Pending values survive the issuing task and fork branch returning.
        run('''module top;
          logic [128:0] a[2],v; int i=1,pos=62;
          task automatic issue();
            a[i] <= #2 129'h10000000000000001;
            v[pos+:65] <= #2 65'h10000000000000001;
          endtask
          initial begin a[1]=0; v=0; fork issue(); join
            #1; $display("before %h %h",a[1],v);
            #2; $display("after %h %h",a[1],v);
          end
        endmodule''', 'before '+('0'*33)+' '+('0'*33)+'\nafter '
            +f'{(1<<64)|1:033x} {(((1<<64)|1)<<62):033x}\n')
        # Bounds warnings use the value width, not the width of the delay operand.
        result = run('''module top; bit [7:0] v=0; int pos=7; initial begin
          v[pos+:3] <= #2 7; #3; $display("%0d",v);
        end endmodule''','128\n',diagnostic='--bounds-check=warn',options=('--bounds-check=warn',))
        if result.stderr.count('warning:') != 1:
            raise AssertionError(result.stderr)
        path.write_text('module top; bit [7:0] a[2],v; int i=1,p=2; initial begin a[i]<=#2 7; v[p+:3]<=#2 7; end endmodule\n')
        dumped = bounded_run.run([simulator,'--dump-sir',str(path)],cwd=tmp,timeout=10)
        if dumped.returncode or 'nba.elem.delay' not in dumped.stdout or 'nba.bits.delay' not in dumped.stdout:
            raise AssertionError(f'NBA SIR metadata: {dumped.stdout}\n{dumped.stderr}')
    print(f'delayed NBA CLI: {checked} execution/diagnostic checks and both SIR forms checked')
    return 0


if __name__ == '__main__':
    sys.exit(main())
