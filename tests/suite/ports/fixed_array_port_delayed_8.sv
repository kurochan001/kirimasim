/*
:name: fixed_array_port_delayed_8
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
:expected_stdout: @fixed_array_port_delayed_8.expected
*/
module top; logic [7:0] d='0;wire [7:0] q[4:5];
assign #2 q[4]=d;assign (weak1,weak0) q[4]='1;assign #3 q[5]=~d;assign (weak1,weak0) q[5]='0;
initial begin #4;$display("%b %b",q[4],q[5]);d='1;#1;$display("%b %b",q[4],q[5]);#3;$display("%b %b",q[4],q[5]);end endmodule
