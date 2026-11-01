#!/usr/bin/env python3
# Copyright (c) 2026 The kirimasim Authors
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
"""Original source matrix for approved MOS/switch contract 5/8 (21.2.1.4).

Exact text oracles are authored independently of simulator output. No reference
implementation source or test corpus is used. Export keeps every input reviewable.
"""
import argparse
import json
from pathlib import Path

CONTEXTS = ('initial', 'always', 'final', 'static_task', 'automatic_task',
            'function', 'fork', 'generate', 'interface', 'program', 'package', 'ref')
APIS = ('display', 'write', 'sformatf', 'sformat', 'runtime_string', 'runtime_packed',
        'strobe', 'monitor', 'fdisplay', 'fwrite', 'fstrobe', 'fmonitor')
POSTPONED = ('strobe', 'monitor', 'fstrobe', 'fmonitor')
BOUNDARIES = ('%0v', '%8v')


def source(context, api):
    net = context not in ('package', 'ref')
    declaration = "wire [3:0] v;assign(weak1,weak0)v=4'b10xz;" if net else "logic[3:0] v=4'b10xz;"
    expected = 'We1_We0_WeX_HiZ' if net else 'St1_St0_StX_HiZ'
    body = [] if context == 'final' else ['#1;'] if context not in ('function','package','ref') else []
    if api in ('runtime_string','runtime_packed'):
        body += ['fmt="%v";packed_fmt="%v";', f'text=$sformatf({"fmt" if api=="runtime_string" else "packed_fmt"},v);', '$write("%s",text);']
    elif api == 'sformatf': body += ['text=$sformatf("%v",v);$write("%s",text);']
    elif api == 'sformat': body += ['$sformat(text,"%v",v);$write("%s",text);']
    elif api in ('fdisplay','fwrite'):
        body += ['fd=$fopen("result.txt","w");', f'${api}(fd,"%v",v);', '$fclose(fd);']
    elif api in ('fstrobe','fmonitor'): body += [f'${api}(32\'h80000001,"%v",v);']
    else: body += [f'${api}("%v",v);']
    if api in ('display','fdisplay',*POSTPONED):expected+='\n'
    if context=='program' and api in POSTPONED:body+=['#1;']
    lines=['`timescale 1ns/1ns',{'interface':'interface worker;','program':'program worker;','package':'package worker;'}.get(context,'module top;'),declaration,'string fmt,text;bit[15:0] packed_fmt;int fd;']
    if context in ('static_task','automatic_task','function','package','ref'):
        lines += [{'function':'function automatic void run();','automatic_task':'task automatic run();','ref':'task automatic run(ref logic[3:0] v);'}.get(context,'task run();'),*body,'endfunction' if context=='function' else 'endtask']
        if context!='package':lines+=['initial begin #1;run(v);end' if context=='ref' else 'initial begin #1;run();end']
    else:
        if context=='generate':lines+=['if(1)begin:g']
        lines+=['event go;initial #1 ->go;always @(go)begin' if context=='always' else 'final begin' if context=='final' else 'initial begin']
        lines+=['fork begin',*body,'end join'] if context=='fork' else body
        lines+=['end']
        if context=='generate':lines+=['end']
    lines += {'interface':['endinterface','module top;worker w();endmodule'], 'program':['endprogram','module top;worker w();endmodule'], 'package':['endpackage','module top;initial worker::run();endmodule']}.get(context,['endmodule'])
    return '\n'.join(lines)+'\n', expected


def programs():
    for context in CONTEXTS:
        for api in APIS:
            if context in ('function','ref','final','package') and api in POSTPONED:continue
            yield f'{context}_{api}', *source(context,api), api in ('fdisplay','fwrite')
    for width in (1,65,129):
        value=('10xz'*33)[:width]
        expected='_'.join({'1':'We1','0':'We0','x':'WeX','z':'HiZ'}[x] for x in value)
        yield f'wide_{width}', f'module top;wire[{width-1}:0] v;assign(weak1,weak0)v={width}\'b{value};initial #1 $display("%v",v);endmodule\n', expected+'\n', False
    directed = {
        'runtime_width': ('wire v;assign(weak1,weak0)v=1;string f="[%8v][%-8v][%0v]";initial #1 $display("%s",$sformatf(f,v,v,v));','[     We1][We1     ][We1]\n'),
        'runtime_raw': ('wire v;assign(weak1,weak0)v=1;string f="%v %b %p";initial #1 $display("%s",$sformatf(f,v,v,v));','We1 1 1\n'),
        'fallback': ('wire v;assign(weak1,weak0)v=1;initial #1 $display("%v %v %v",v,~v,1\'(v));','We1 St0 St1\n'),
        'selected_once': ('wire[1:0] v;assign(weak1,weak0)v=2\'b10;int i=0;string text;initial begin #1;text=$sformatf("%v",v[i++]);$display("%s %0d",text,i);end','We0 1\n'),
        'runtime_selected_once': ('wire[1:0] v;assign(weak1,weak0)v=2\'b10;int i=0;string text,f="%v";initial begin #1;text=$sformatf(f,v[i++]);$display("%s %0d",text,i);end','We0 1\n'),
        'invalid_selected': ('wire[1:0] v;assign(weak1,weak0)v=2\'b10;int i=9;initial #1 $display("%v",v[i]);','StX\n'),
        'partial_selected': ('wire[3:0] v;assign(weak1,weak0)v=4\'b1010;int i=3;initial #1 $display("%v",v[i+:3]);','StX_StX_We1\n'),
        'force_release': ('wire v;logic d=1;assign(weak1,weak0)v=d;initial begin #1;force v=0;$display("%v",v);d=0;#1;release v;$display("%v",v);end','St0\nWe0\n'),
        'force_alias': ('wire[2:0] a,b;alias a=b;assign(weak1,weak0)a=3\'b101;initial begin #1;force b[1]=1;force a[2:1]=2\'b00;$display("%v",b);release b[1];$display("%v",a);release a[2:1];$display("%v",b);end','St0_St0_We1\nSt0_We0_We1\nWe1_We0_We1\n'),
        'force_live_rhs': ('wire[1:0] a;logic[1:0] f=0;assign(weak1,weak0)a=2\'b11;initial begin #1;force a=f;f=2;#1;$display("%v",a);release a;$display("%v",a);end','St1_St0\nWe1_We1\n'),
        'force_z_transit': ('wire a,b,c;supply1 s;tran t0(a,b),t1(b,c),t2(a,s);initial begin #1;force b=1\'bz;$display("%v %v %v",a,b,c);release b;$display("%v",b);end','St1 HiZ St1\nSt1\n'),
        'monitor_strength': ('logic en=0;wire a,b;assign(strong1,strong0)a=1;assign(weak1,weak0)b=1;tranif1 t(b,a,en);initial begin $monitor("%0t %v %b",$time,b,b);#1 en=1;#1 en=0;#1 $monitoroff;end','0 We1 1\n1 St1 1\n2 We1 1\n'),
        'strobe_snapshot': ('wire v;assign(weak1,weak0)v=1;initial begin #1;$strobe("%v",v);force v=0;end','St0\n'),
        'mos_uncertain': ('wire a,b;logic en=1\'bx;assign(weak1,weak0)a=1;nmos m(b,a,en);initial #1 $display("%v",b);','WeH\n'),
        'mos_resistive': ('wire a,b;assign(strong1,strong0)a=1;rnmos m(b,a,1\'b1);initial #1 $display("%v",b);','Pu1\n'),
    }
    directed.update({
        'multi_rank': ('wire a;logic en=1\'bx;assign(weak1,weak0)a=1;bufif1(strong1,strong0)t(a,1\'b1,en);initial #1 $display("%v",a);','631\n'),
        'plain_monitor': ('logic en=0;wire a,b;assign(strong1,strong0)a=1;assign(weak1,weak0)b=1;tranif1 t(b,a,en);initial begin #1;$monitor("%b",b);#1 en=1;#1 en=0;#1 $monitoroff;end','1\n'),
        'monitor_selection': ('wire[1:0] v;assign(weak1,weak0)v=2\'b10;int i=0;initial begin #1;$monitor("%v",v[i]);#1 i=1;#1 i=9;#1 $monitoroff;end','We0\nWe1\nStX\n'),
        'monitor_lifecycle': ('logic en=0;wire a,b;assign(strong1,strong0)a=1;assign(weak1,weak0)b=1;tranif1 t(b,a,en);initial begin #1;$monitor("a %v",b);#1;$monitor("b %v",b);#1;$monitoroff;en=1;#1;$monitoron;#1;$monitoroff;end','a We1\nb We1\nb St1\n'),
    })
    directed.update({
        'packed_members': ('typedef struct packed {logic[1:0] a;logic b;} pair;wire pair v;assign(weak1,weak0)v=3\'b10x;initial #1 $display("%v %v",v.a,v.b);','We1_We0 WeX\n'),
        'array_element': ('wire[1:0] v[2:1];assign(weak1,weak0)v[2]=2\'b10;assign(pull1,pull0)v[1]=2\'b01;int i=2;string f="%v",text;initial begin #1;text=$sformatf(f,v[i--]);$display("%s %v %0d",text,v[i],i);end','We1_We0 Pu0_Pu1 1\n'),
        'argument_capture': ('wire v;assign(weak1,weak0)v=1;string f="%v %v";function automatic logic change();force v=0;return v;endfunction initial #1 $display("%s",$sformatf(f,v,change()));','We1 St0\n'),
    })
    for name,(body,expected) in directed.items():
        yield name, 'module top;\n'+body+'\nendmodule\n', expected, False


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output',type=Path,required=True)
    args=parser.parse_args();args.output.mkdir(parents=True,exist_ok=True)
    manifest=[]
    for name,text,expected,file_output in programs():
        (args.output/(name+'.sv')).write_text(text)
        manifest.append({'source':name+'.sv','expected':expected,'file_output':file_output})
    (args.output/'manifest.json').write_text(json.dumps(manifest,indent=2)+'\n')
    print(f'strength formats: exported {len(manifest)} original programs')


if __name__=='__main__':main()
