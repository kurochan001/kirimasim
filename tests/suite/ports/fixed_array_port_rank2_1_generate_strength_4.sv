/*
:name: fixed_array_port_rank2_1_generate_strength_4
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
:expected_stdout: @fixed_array_port_rank2_1_generate_strength_4.expected
*/
module leaf(input wire logic [0:0] p[3:4][-1:-3], output wire logic [0:0] q[3:4][-1:-3]);
assign (weak1,weak0) q[3][-1] = p[3][-1];
assign (weak1,weak0) q[3][-2] = p[3][-2];
assign (weak1,weak0) q[3][-3] = p[3][-3];
assign (weak1,weak0) q[4][-1] = p[4][-1];
assign (weak1,weak0) q[4][-2] = p[4][-2];
assign (weak1,weak0) q[4][-3] = p[4][-3];
endmodule

module top;
logic [0:0] stim[1:0][4:6];
wire logic [0:0] q[1:0][4:6];
wire logic [0:0] a[1:0][4:6];
assign a[1][4] = stim[1][4];
assign a[1][5] = stim[1][5];
assign a[1][6] = stim[1][6];
assign a[0][4] = stim[0][4];
assign a[0][5] = stim[0][5];
assign a[0][6] = stim[0][6];
assign (strong1,strong0) q[1][4] = ~stim[1][4];
assign (strong1,strong0) q[1][5] = ~stim[1][5];
assign (strong1,strong0) q[1][6] = ~stim[1][6];
assign (strong1,strong0) q[0][4] = ~stim[0][4];
assign (strong1,strong0) q[0][5] = ~stim[0][5];
assign (strong1,strong0) q[0][6] = ~stim[0][6];
if (1) begin: g
leaf l(a,q);
end
initial begin
stim[1][4] = 1'h1;
stim[1][5] = 1'h0;
stim[1][6] = 1'h1;
stim[0][4] = 1'h0;
stim[0][5] = 1'h1;
stim[0][6] = 1'h0;
#1;
$display("%b %b %b %b %b %b %b %b %b %b %b %b", q[1][4], q[1][5], q[1][6], q[0][4], q[0][5], q[0][6], g.l.p[3][-1], g.l.p[3][-2], g.l.p[3][-3], g.l.p[4][-1], g.l.p[4][-2], g.l.p[4][-3]);
stim[1][4] = 1'h0;
#1;
$display("%b %b %b %b %b %b %b %b %b %b %b %b", q[1][4], q[1][5], q[1][6], q[0][4], q[0][5], q[0][6], g.l.p[3][-1], g.l.p[3][-2], g.l.p[3][-3], g.l.p[4][-1], g.l.p[4][-2], g.l.p[4][-3]);
stim[1][5] = 1'h1;
#1;
$display("%b %b %b %b %b %b %b %b %b %b %b %b", q[1][4], q[1][5], q[1][6], q[0][4], q[0][5], q[0][6], g.l.p[3][-1], g.l.p[3][-2], g.l.p[3][-3], g.l.p[4][-1], g.l.p[4][-2], g.l.p[4][-3]);
$display("%0d %0d %0d %0d", $left(stim), $right(stim), $left(g.l.p), $right(g.l.p));
end
endmodule
