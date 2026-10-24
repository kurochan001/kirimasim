/*
:name: fixed_array_port_same_8_module_resolvedcopy_4
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
:expected_stdout: @fixed_array_port_same_8_module_resolvedcopy_4.expected
*/
module leaf(input wire logic [7:0] p[0:1], output var logic [7:0] q[0:1]);
assign q[0] = p[0];
assign q[1] = p[1];
endmodule

module top;
logic [7:0] stim[0:1];
wire logic [7:0] q[0:1];
assign (weak1,weak0) q[0] = '1;
assign (weak1,weak0) q[1] = '1;
leaf l(stim,q);
initial begin
stim[0] = 8'h19;
stim[1] = 8'h3e;
#1;
$display("%b %b %b %b", q[0], q[1], l.p[0], l.p[1]);
stim[0] = 8'h99;
#1;
$display("%b %b %b %b", q[0], q[1], l.p[0], l.p[1]);
stim[1] = 8'h3f;
#1;
$display("%b %b %b %b", q[0], q[1], l.p[0], l.p[1]);
$display("%0d %0d %0d %0d", $left(stim), $right(stim), $left(l.p), $right(l.p));
end
endmodule
