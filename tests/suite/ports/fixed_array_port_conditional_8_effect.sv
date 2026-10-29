/*
:name: fixed_array_port_conditional_8_effect
:description: Whole fixed-array ports preserve source bounds, ordinal identity and per-element sensitivity
:tags: ports
:type: simulation
:lrm: 1800-2023 7.6 11.4.11 23.3.3.5 23.3.3.7
:oracle: spec
:spec_check: ai
:fourstate: yes
:nondeterministic: no
:generator: gen_fixed_array_port_matrix.py@1
:top_module: top
:expected_stdout: @fixed_array_port_conditional_8_effect.expected
*/
typedef logic [7:0] pair_t[2]; module leaf(input var pair_t p);endmodule
module top; pair_t a='{1,2},b='{7,9};logic pick=0;int calls;function automatic pair_t next_value(input bit which);calls++;return which?a:b;endfunction
leaf l(pick?next_value(1):next_value(0));initial begin #1;$display("%b %b %0d",l.p[0],l.p[1],calls);pick=1'b1;#1;$display("%b %b %0d",l.p[0],l.p[1],calls);pick=1'bx;#1;$display("%b %b %0d",l.p[0],l.p[1],calls);end endmodule
