/*
:name: fixed_array_port_down_129_generate_resolvedcopy_4
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
:expected_stdout: @fixed_array_port_down_129_generate_resolvedcopy_4.expected
*/
module leaf(input wire logic [128:0] p[-2:-1], output var logic [128:0] q[-2:-1]);
assign q[-2] = p[-2];
assign q[-1] = p[-1];
endmodule

module top;
logic [128:0] stim[5:4];
wire logic [128:0] q[5:4];
assign (weak1,weak0) q[5] = '1;
assign (weak1,weak0) q[4] = '1;
if (1) begin: g
leaf l(stim,q);
end
initial begin
stim[5] = 129'h19;
stim[4] = 129'h3e;
#1;
$display("%b %b %b %b", q[5], q[4], g.l.p[-2], g.l.p[-1]);
stim[5] = 129'h100000000000000000000000000000019;
#1;
$display("%b %b %b %b", q[5], q[4], g.l.p[-2], g.l.p[-1]);
stim[4] = 129'h3f;
#1;
$display("%b %b %b %b", q[5], q[4], g.l.p[-2], g.l.p[-1]);
$display("%0d %0d %0d %0d", $left(stim), $right(stim), $left(g.l.p), $right(g.l.p));
end
endmodule
