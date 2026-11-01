#!/usr/bin/env python3
# Copyright (c) 2026 The kirimasim Authors
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
"""Static partial inout ports share physical identity, kinds and origins (23.3.3.7)."""
import argparse
import itertools
from pathlib import Path
import gen_port_kind_matrix as kinds
import gen_array_element_port_matrix as arrays
from gen_resolution_matrix import resolved

OUT = Path(__file__).resolve().parent.parent / 'tests/suite/ports'


def probes():
    rows=[]
    for kind in kinds.KINDS:
        body=f'''module leaf(inout {kind} p,input logic d);
assign (weak1,weak0) p=d;
endmodule
module top;
logic a=0,b=1;
{kind} [1:0] w;
leaf u(w[1],a);
assign (weak1,weak0) w[1]=b;
initial begin
#1; $display("%b %b",w,u.p);
a=1'bz; #1; $display("%b %b",w,u.p);
b=1'bz; #1; $display("%b %b",w,u.p);
end
endmodule'''
        expected=''
        for a,b in (('0','1'),('z','1'),('z','z')):
            value=resolved(kind,[(a,3,3),(b,3,3)])
            expected+=value+resolved(kind,[])+' '+value+'\n'
        rows.append(('partial_inout_port_'+kind,'23.3.3.7, 28.12',body,expected,[]))
    for side in ('none','inner','outer'):
        source='''module leaf(inout wire p);
INNER
endmodule
module top; wire [2:0] w; leaf u(w[1]);
OUTER
initial begin #1; $display("%b %b",w,u.p); end
endmodule'''.replace('INNER',"assign (weak1,weak0) p=1;" if side=='inner' else '').replace('OUTER',"assign (weak1,weak0) w[1]=0;" if side=='outer' else '')
        value={'none':'z','inner':'1','outer':'0'}[side]
        rows.append(('partial_inout_port_'+side+'_only','23.3.3.7',source,'z'+value+'z '+value+'\n',[]))
    rows.append(('partial_inout_port_rank2','7.4, 23.3.3.7', '''module leaf(inout wire p); assign (weak1,weak0) p=0; endmodule
module top; wire [64:0] w[1:0][-1:1]; leaf u(w[0][0][64]);
assign (weak1,weak0) w[0][0][64]=1;
initial begin #1; $display("%b %b %b",w[0][0][64],u.p,w[1][-1][64]); end
endmodule''','x x z\n',[]))
    rows.append(('partial_inout_port_passive_cycle','23.3.3.7, 28.8', '''module leaf(inout wire p,input logic d); assign (weak1,weak0) p=d; endmodule
module top; logic a=0,b=1; wire [1:0] w; leaf x(w[0],a),y(w[1],b);
tran t(w[0],w[1]);
initial begin #1; $display("%b %b %b",w,x.p,y.p);
a=1'bz; b=1'bz; #1; $display("%b %b %b",w,x.p,y.p); end
endmodule''','xx x x\nzz z z\n',[]))
    return rows


def renamed(row,old,new,generator):
    name,source=row
    return name.replace(old,new),source.replace(old,new).replace(generator,'gen_partial_inout_port_matrix.py@1')


def cases():
    for i,(inner,outer) in enumerate(itertools.product(kinds.KINDS,repeat=2)):
        yield renamed(kinds.pair(inner,outer,'inout',i),'port_kind_','partial_inout_port_kind_','gen_port_kind_matrix.py@1')
    for i,(inner,outer) in enumerate(itertools.product(kinds.OWN,repeat=2)):
        yield renamed(kinds.unwritten(inner,outer,'inout',('top','generate','instance')[i%3]),'port_kind_','partial_inout_port_kind_','gen_port_kind_matrix.py@1')
    for context in ('top','generate','instance'):
        yield renamed(kinds.mixed(context,'inout'),'port_kind_','partial_inout_port_kind_','gen_port_kind_matrix.py@1')
    for wi,width in enumerate((1,65,129)):
        for si,shape in enumerate(arrays.SHAPES):
            for mi,mapping in enumerate(('whole','bit','part','concat')):
                if width==1 and mapping=='concat':continue
                for ci,context in enumerate(('top','generate','instance')):
                    yield renamed(arrays.case(width,shape,mapping,'inout',context,(wi+si+mi+ci)%3,ascending=(wi+si+mi+ci)%2==1),'array_element_port_','partial_inout_port_array_','gen_array_element_port_matrix.py@1')
    for context in ('top','generate','instance'):
        yield renamed(arrays.aliases(context,'inout'),'array_element_port_','partial_inout_port_array_','gen_array_element_port_matrix.py@1')


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--check',action='store_true')
    args=parser.parse_args()
    sources=list(cases());rows=dict(sources);assert len(rows)==len(sources)
    bad=False
    for name,source in rows.items():
        path=OUT/(name+'.sv')
        if args.check:
            if not path.exists() or path.read_text()!=source:
                print('generated file differs:',path.name);bad=True
        else:path.write_text(source)
    for path in OUT.glob('partial_inout_port_*.sv'):
        if path.stem not in rows:print('unexpected generated file:',path.name);bad=True
    print(len(rows),'partial-inout sources', 'checked' if args.check else 'generated')
    return int(bad)


if __name__=='__main__':raise SystemExit(main())
