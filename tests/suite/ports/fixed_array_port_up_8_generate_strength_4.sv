/*
:name: fixed_array_port_up_8_generate_strength_4
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
:expected_stdout: @fixed_array_port_up_8_generate_strength_4.expected
*/
module leaf(input wire logic [7:0] p[7:6], output wire logic [7:0] q[7:6]);
assign (weak1,weak0) q[7] = p[7];
assign (weak1,weak0) q[6] = p[6];
endmodule

module top;
logic [7:0] stim[4:5];
wire logic [7:0] q[4:5];
wire logic [7:0] a[4:5];
assign a[4] = stim[4];
assign a[5] = stim[5];
assign (strong1,strong0) q[4] = ~stim[4];
assign (strong1,strong0) q[5] = ~stim[5];
if (1) begin: g
leaf l(a,q);
end
initial begin
stim[4] = 8'h19;
stim[5] = 8'h3e;
#1;
$display("%b %b %b %b", q[4], q[5], g.l.p[7], g.l.p[6]);
stim[4] = 8'h99;
#1;
$display("%b %b %b %b", q[4], q[5], g.l.p[7], g.l.p[6]);
stim[5] = 8'h3f;
#1;
$display("%b %b %b %b", q[4], q[5], g.l.p[7], g.l.p[6]);
$display("%0d %0d %0d %0d", $left(stim), $right(stim), $left(g.l.p), $right(g.l.p));
end
endmodule
