#!/usr/bin/env python3
# Copyright (c) 2026 The kirimasim Authors
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
"""Integral converting net ports retain directional strong carriers (4.9.6, 23.3.3.7)."""
import argparse
from functools import lru_cache
import itertools
from pathlib import Path
from gen_resolution_matrix import resolved

OUT = Path(__file__).resolve().parent.parent / 'tests/suite/ports'


def convert(bits, width, signed=False, four=True):
    fill = bits[-1] if signed else '0'
    value = (bits + [fill] * width)[:width]
    return [v if four or v in ('0','1') else '0' for v in value]


def settled(direction, a, b, signed_a=False, signed_b=False, four_a=True, four_b=True):
    a = convert(a, len(a), four=four_a)
    b = convert(b, len(b), four=four_b)
    if direction == 'output':
        carrier = convert(a, len(b), signed_a, four_b)
        return [resolved('wire',[(x,6,6),(y,3,3)]) for x,y in zip(carrier,b)], a
    carrier = convert(b, len(a), signed_b, four_a)
    return b, [resolved('wire',[(x,6,6),(y,3,3)]) for x,y in zip(carrier,a)]


def declaration(width,signed=False,four=True):
    return ('logic' if four else 'bit')+(' signed' if signed else '')+f' [{width-1}:0]'


def probe(direction, formal, actual, signed=False, state='four', expression='plain', actual_signed=None):
    actual_signed=signed if actual_signed is None else actual_signed
    fa,fb=state!='formal_two',state!='actual_two'
    at = 'w'
    if expression=='cast':at=f"{actual}'(w)"
    if expression=='bit_cast':at="bit'(w)"
    if expression=='concat_cast':at=f"{{{actual-1}'(w[{actual-1}:1]),w[0]}}"
    source=f'''module leaf({direction} wire {declaration(formal,signed,fa)} p);
assign (weak1,weak0) p='0;
endmodule
module top;
wire {declaration(actual,actual_signed,fb)} w;
assign (weak1,weak0) w='1;
leaf u({at});
initial begin #1; $display("%b %b",w,u.p); end
endmodule'''
    w,p=settled(direction,['0']*formal,['1']*actual,signed,actual_signed,fa,fb)
    if expression=='bit_cast':p=['1']+['0']*(formal-1)
    suffix=('_signed' if signed else '')+('_actual_unsigned' if signed and not actual_signed else '')+('' if state=='four' else '_'+state)+('' if expression=='plain' else '_'+expression)
    name=f'converting_port_{direction}_{formal}_{actual}'+suffix
    return name,'4.9.6, 23.3.3.7',source,''.join(reversed(w))+' '+''.join(reversed(p))+'\n',[]


def probes():
    rows=[]
    for direction in ('input','output'):
        for formal,actual in ((4,2),(2,4)):
            rows.append(probe(direction,formal,actual))
        rows.append(probe(direction,2,4,signed=True))
        rows.append(probe(direction,3,3,signed=True,actual_signed=False))
    for expression in ('cast','concat_cast','bit_cast'):
        rows.append(probe('input',3,3,expression=expression))
    return rows


@lru_cache(None)
def resolve_pair(carrier, local):
    return resolved('wire',[(carrier,6,6),(local,3,3)])


def case(formal,actual,direction,mapping,context,sa,sb,expression='plain',ascending=False):
    size=actual if mapping=='whole' else actual+2
    coordinate=lambda bit:size-1-bit if ascending else bit
    bounds=f'[0:{size-1}]' if ascending else f'[{size-1}:0]'
    suffix='[0]' if mapping=='array' else ''
    net='w'+suffix
    if mapping=='whole':
        bits=list(range(actual));at=net
    elif mapping=='concat':
        bits=list(reversed(range(1,actual+1)))
        at='{'+','.join(f'{net}[{coordinate(b)}]' for b in reversed(bits))+'}'
    else:
        bits=list(range(1,actual+1))
        at=f'{net}[{coordinate(actual)}:{coordinate(1)}]'
    actual_signed=sb if mapping=='whole' else False
    source=at
    if expression=='cast':source=f"{actual}'({at})"
    elif expression=='concat_cast':
        source='{'+f"{actual}'({at})"+'}'
    elif expression=='bit_cast':source=f"bit'({at})"
    name=(f'converting_port_{direction}_{formal}_{actual}_{mapping}_{context}'
          +f'_{int(sa)}{int(sb)}_{expression}'+('_ascending' if ascending else ''))
    declarations=f"""logic [{formal-1}:0] a;
logic [{actual-1}:0] b;
wire logic {'signed ' if sb else ''}{bounds} w{'[-1:0]' if suffix else ''};
leaf u({source},a);
assign (weak1,weak0) {at}=b;
"""
    checks=['int errors=0;']
    steps=[('0','1'),('1','0'),('x','z'),('z','x'),('z','z'),('1','z'),('z','0')]
    vectors=[([a]*formal,[b]*actual) for a,b in steps]
    vectors += [(['0']+['1']*(formal-1),['0']+['1']*(actual-1))]
    for a,b in vectors:
        checks += [f"a={formal}'b{''.join(reversed(a))}; b={actual}'b{''.join(reversed(b))}; #1;"]
        if direction=='output':
            carrier=convert(a,actual,sa)
            parent=[resolve_pair(c,l) for c,l in zip(carrier,b)];child=a
        else:
            value=b
            if expression=='bit_cast':value=convert(b,1,four=False)
            carrier=convert(value,formal,actual_signed if expression=='plain' else False)
            parent=b;child=[resolve_pair(c,l) for c,l in zip(carrier,a)]
        expected=['z']*size
        for bit,value in zip(bits,parent):expected[bit]=value
        checks += [f"if(u.p !== {formal}'b{''.join(reversed(child))}) ++errors;",
                   f"if({net} !== {size}'b{''.join(reversed(expected))}) ++errors;"]
        if suffix:checks += [f"if(w[-1] !== {size}'b{'z'*size}) ++errors;"]
    checks += ['$display("errors=%0d",errors);']
    body=declarations+'\ninitial begin\n'+'\n'.join(checks)+'\nend\n'
    if context=='generate':body='module top; if(1) begin:g\n'+body+'end endmodule'
    elif context=='instance':body='module holder;\n'+body+'endmodule\nmodule top; holder h(); endmodule'
    else:body='module top;\n'+body+'endmodule'
    leaf=f"""module leaf({direction} wire {declaration(formal,sa)} p,input logic [{formal-1}:0] d);
assign (weak1,weak0) p=d;
endmodule
"""
    header=f"""/*
:name: {name}
:description: Converting net ports resolve directional strong carriers without identity collapse
:tags: ports
:type: simulation
:lrm: 1800-2023 4.9.6 23.3.3.7
:oracle: spec
:spec_check: ai
:fourstate: yes
:nondeterministic: no
:generator: gen_converting_port_matrix.py@1
:top_module: top
:expected_stdout: errors=0
*/
"""
    return name,header+leaf+body+'\n'


def cases():
    mappings=('whole','part','array','concat')
    contexts=('top','generate','instance')
    for i,(formal,actual) in enumerate(((1,3),(3,1),(63,65),(65,63),(129,131),(131,129))):
        for j,(direction,mapping,context) in enumerate(itertools.product(('input','output'),mappings,contexts)):
            yield case(formal,actual,direction,mapping,context,(i+j)%2==0,(i+j)//2%2==0,ascending=(i+j)%2==1)
    # A signed formal connected to an unsigned selection, or the reverse for a whole net.
    for width,(direction,mapping,context) in itertools.product((1,65,129),itertools.product(('input','output'),mappings,contexts)):
        yield case(width,width,direction,mapping,context,True,False)
    for width,expression,mapping,context in itertools.product((1,65,129),('cast','concat_cast','bit_cast'),mappings,contexts):
        yield case(width,width,'input',mapping,context,False,False,expression)


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--check',action='store_true')
    args=parser.parse_args()
    rows=dict(cases())
    failed=False
    for name,source in rows.items():
        path=OUT/(name+'.sv')
        if args.check:
            if not path.exists() or path.read_text()!=source:
                print('generated file differs:',path.name);failed=True
        else:path.write_text(source)
    print(len(rows),'converting-port sources', 'checked' if args.check else 'generated')
    return int(failed)


if __name__=='__main__':raise SystemExit(main())
