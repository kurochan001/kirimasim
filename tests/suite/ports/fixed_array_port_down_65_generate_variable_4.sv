/*
:name: fixed_array_port_down_65_generate_variable_4
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
:expected_stdout: @fixed_array_port_down_65_generate_variable_4.expected
*/
module leaf(input var logic [64:0] p[-2:-1], output var logic [64:0] q[-2:-1]);
assign q[-2] = p[-2];
assign q[-1] = p[-1];
endmodule

module top;
logic [64:0] stim[5:4];
wire logic [64:0] q[5:4];
if (1) begin: g
leaf l(stim,q);
end
initial begin
stim[5] = 65'h19;
stim[4] = 65'h3e;
#1;
$display("%b %b %b %b", q[5], q[4], g.l.p[-2], g.l.p[-1]);
stim[5] = 65'h10000000000000019;
#1;
$display("%b %b %b %b", q[5], q[4], g.l.p[-2], g.l.p[-1]);
stim[4] = 65'h3f;
#1;
$display("%b %b %b %b", q[5], q[4], g.l.p[-2], g.l.p[-1]);
$display("%0d %0d %0d %0d", $left(stim), $right(stim), $left(g.l.p), $right(g.l.p));
end
endmodule
