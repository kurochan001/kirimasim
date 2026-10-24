/*
:name: fixed_array_port_up_8_interface_variable_2
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
:expected_stdout: @fixed_array_port_up_8_interface_variable_2.expected
*/
interface leaf(input var bit [7:0] p[7:6], output var bit [7:0] q[7:6]);
assign q[7] = p[7];
assign q[6] = p[6];
endinterface

module top;
bit [7:0] stim[4:5];
bit [7:0] q[4:5];
leaf l(stim,q);
initial begin
stim[4] = 8'h19;
stim[5] = 8'h3e;
#1;
$display("%b %b %b %b", q[4], q[5], l.p[7], l.p[6]);
stim[4] = 8'h99;
#1;
$display("%b %b %b %b", q[4], q[5], l.p[7], l.p[6]);
stim[5] = 8'h3f;
#1;
$display("%b %b %b %b", q[4], q[5], l.p[7], l.p[6]);
$display("%0d %0d %0d %0d", $left(stim), $right(stim), $left(l.p), $right(l.p));
end
endmodule
