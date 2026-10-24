/*
:name: fixed_array_port_same_1_module_variable_2
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
:expected_stdout: @fixed_array_port_same_1_module_variable_2.expected
*/
module leaf(input var bit [0:0] p[0:1], output var bit [0:0] q[0:1]);
assign q[0] = p[0];
assign q[1] = p[1];
endmodule

module top;
bit [0:0] stim[0:1];
bit [0:0] q[0:1];
leaf l(stim,q);
initial begin
stim[0] = 1'h1;
stim[1] = 1'h0;
#1;
$display("%b %b %b %b", q[0], q[1], l.p[0], l.p[1]);
stim[0] = 1'h0;
#1;
$display("%b %b %b %b", q[0], q[1], l.p[0], l.p[1]);
stim[1] = 1'h1;
#1;
$display("%b %b %b %b", q[0], q[1], l.p[0], l.p[1]);
$display("%0d %0d %0d %0d", $left(stim), $right(stim), $left(l.p), $right(l.p));
end
endmodule
