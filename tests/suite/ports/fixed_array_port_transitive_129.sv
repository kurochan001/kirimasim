/*
:name: fixed_array_port_transitive_129
:description: Whole fixed-array ports preserve source bounds, ordinal identity and per-element sensitivity
:tags: ports
:type: simulation
:lrm: 1800-2023 7.6 23.3.3.5 23.3.3.7
:oracle: spec
:spec_check: ai
:fourstate: yes
:nondeterministic: no
:generator: gen_fixed_array_port_matrix.py@1
:top_module: top
:expected_stdout: @fixed_array_port_transitive_129.expected
*/
module leaf(inout wire [128:0] p[-3:-4]);assign (weak1,weak0)p[-3]='1;assign p[-4]='1;endmodule
module middle(inout wire [128:0] p[7:6]);leaf l(p);endmodule
module top;wire [128:0] a[2:3];middle m(a);assign a[2]='0;initial begin #1;$display("%b %b %b",a[2],m.p[7],m.l.p[-3]);force m.l.p[-4]='0;#1;$display("%b %b",a[3],m.p[6]);release a[3];#1;$display("%b",m.l.p[-4]);end endmodule
