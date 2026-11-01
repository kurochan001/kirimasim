#!/usr/bin/env python3
# Copyright (c) 2026 The kirimasim Authors
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
"""Native VPI execution using an original C plugin and independent source cases."""
import argparse
import json
from pathlib import Path
import subprocess
import sys
import tempfile

ROOT=Path(__file__).resolve().parents[2]
sys.path.insert(0,str(ROOT/'tools'))
import bounded_run
from vpi_feature_probe import CASES
from gen_vpi_matrix import programs


def build_plugin(compiler,source,output,extra=()):
    flags=['-bundle','-undefined','dynamic_lookup'] if sys.platform=='darwin' else ['-shared']
    cmd=[compiler,'-std=c11','-fPIC','-Wall','-Wextra','-Werror','-I'+str(ROOT/'src/kernel/include'),*extra,str(source),*flags,'-o',str(output)]
    built=bounded_run.run(cmd,timeout=60)
    if built.returncode:raise AssertionError(f'original plugin build: {built.stdout}\n{built.stderr}')


EDGE_CASES = [
    ('real_function', 'module top;initial $display("%.2f",$edge_real());endmodule', '2.25\n'),
    ('automatic_scope', 'module top;task automatic run();$edge_scope();endtask initial run();endmodule', 'scope ok\n'),
    ('nested_function_scope', 'module top;function automatic void inner();$edge_scope();endfunction function automatic void outer();inner();endfunction initial outer();endmodule', 'scope ok\n'),
    ('readonly_expression', 'module top;initial $edge_errors(1+2);endmodule', 'errors ok\n'),
    ('descriptor_copy', 'module top;int a=3;initial begin $edge_later(a);a=9;end endmodule', 'later 9\n'),
    ('expired_activation', 'module top;task automatic run();int a=3;$edge_expire(a);endtask initial run();endmodule', 'expired ok\n'),
    ('finish_final_end', 'module top;initial begin $edge_lifecycle();$edge_finish();$display("BAD");end final begin $edge_echo(7);$display("final done");end endmodule', 'echo 7\nfinal done\nend\n'),
    ('nba_sync_regions', 'module top;int a=1;initial begin a<=2;$edge_sync(a);#1 $display("hdl %0d",a);end endmodule', 'rw 2\nro 7\nhdl 7\n'),
]

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--kirimasim',type=Path,required=True)
    parser.add_argument('--cc',default='cc')
    parser.add_argument('--case',action='append')
    parser.add_argument('--report',type=Path)
    args=parser.parse_args();exe=str(args.kirimasim.resolve());results=[]
    with tempfile.TemporaryDirectory(prefix='kirimasim vpi tests ') as tmp:
        root=Path(tmp);plugin=root/'original plugin.vpi'
        build_plugin(args.cc,ROOT/'tests/probes/vpi/inventory.c',plugin)
        edge=root/'edge plugin.vpi';build_plugin(args.cc,ROOT/'tests/probes/vpi/native_edges.c',edge)
        cases=[(name,source,expected,plugin,()) for name,source,expected,_ in CASES]+[(name,source,expected,plugin,()) for name,source,expected in programs()]+[(name,source,expected,edge,()) for name,source,expected in EDGE_CASES]
        cases.extend([('parse_compile_only','module top;initial $edge_compile(7);endmodule','compile\n',edge,('parse',)),('dump_compile_only','module top;initial $edge_compile(7);endmodule','compile\n',edge,('--dump-sir',))])
        for name,source,expected,selected,options in cases:
            if args.case and name not in args.case:continue
            path=root/(name+'.sv');path.write_text(source)
            ran=bounded_run.run([exe,*options,'--vpi='+str(selected),str(path),'+vpi_probe=42'],cwd=root,timeout=30)
            diagnostics='\n'.join(line for line in ran.stderr.splitlines() if not line.startswith('kirimasim: info:'))
            output_ok=ran.stdout.startswith(expected) and 'echo ' not in ran.stdout and 'foreign.object ' in ran.stdout if name=='dump_compile_only' else ran.stdout==expected
            passed=ran.returncode==0 and output_ok and not diagnostics
            results.append({'name':name,'passed':passed,'exit':ran.returncode,'stdout':ran.stdout,'diagnostics':diagnostics.replace(tmp,'<probe>')})
            print(f'{name}: {"passed" if passed else "FAILED"}',flush=True)
            if not passed:print(f'expected={expected!r}\nactual={ran.stdout!r}\n{diagnostics}',flush=True)
        # Loader failures must remain input errors; ordered startup is observable.
        loader_source=root/'loader.c'
        loader_plugins=[]
        for label in ('A','B'):
            loader_source.write_text('#include <vpi_user.h>\nstatic void start(void){vpi_printf("'+label+'\\n");}\nvoid (*vlog_startup_routines[])(void)={start,0};\n')
            output=root/(label+'.vpi');build_plugin(args.cc,loader_source,output);loader_plugins.append(output)
        empty=root/'empty.vpi';loader_source.write_text('int ordinary_symbol(void){return 0;}\n');build_plugin(args.cc,loader_source,empty)
        collision=root/'collision.vpi'
        loader_source.write_text('#include <vpi_user.h>\nstatic PLI_INT32 call(PLI_BYTE8*p){(void)p;return 0;}\nstatic void start(void){s_vpi_systf_data d={0};d.type=vpiSysTask;d.tfname="$display";d.calltf=call;vpi_register_systf(&d);}\nvoid (*vlog_startup_routines[])(void)={start,0};\n')
        build_plugin(args.cc,loader_source,collision)
        design=root/'loader.sv';design.write_text('module top;endmodule\n')
        loader_cases=[
            ('startup_order',[*("--vpi="+str(p) for p in loader_plugins)],0,'A\nB\n',''),
            ('missing_plugin',['--vpi='+str(root/'missing.vpi')],2,'','cannot load VPI plugin'),
            ('empty_path',['--vpi='],2,'','nonempty path'),
            ('missing_startup',['--vpi='+str(empty)],2,'','no vlog_startup_routines'),
            ('duplicate_registration',['--vpi='+str(plugin),'--vpi='+str(plugin)],2,'','duplicate VPI system routine'),
            ('builtin_collision',['--vpi='+str(collision)],2,'','conflicts with system routine'),
        ]
        for name,options,status,expected,message in loader_cases:
            if args.case and name not in args.case:continue
            ran=bounded_run.run([exe,*options,str(design)],cwd=root,timeout=30)
            diagnostics='\n'.join(line for line in ran.stderr.splitlines() if not line.startswith('kirimasim: info:'))
            passed=ran.returncode==status and ran.stdout==expected and (message in diagnostics if message else not diagnostics)
            results.append({'name':name,'passed':passed,'exit':ran.returncode,'stdout':ran.stdout,'diagnostics':ran.stderr.replace(tmp,'<probe>')})
            print(f'{name}: {"passed" if passed else "FAILED"}',flush=True)
            if not passed:print(f'{ran.stdout!r}\n{ran.stderr}',flush=True)
        if args.report:
            args.report.parent.mkdir(parents=True,exist_ok=True);args.report.write_text(json.dumps(results,indent=2)+'\n')
    failures=[row['name'] for row in results if not row['passed']]
    print(f'native VPI: {len(results)-len(failures)}/{len(results)} passed')
    return 1 if failures else 0


if __name__=='__main__':raise SystemExit(main())
