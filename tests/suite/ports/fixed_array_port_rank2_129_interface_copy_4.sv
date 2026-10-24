/*
:name: fixed_array_port_rank2_129_interface_copy_4
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
:expected_stdout: @fixed_array_port_rank2_129_interface_copy_4.expected
*/
interface leaf(input wire logic [128:0] p[3:4][-1:-3], output var logic [128:0] q[3:4][-1:-3]);
assign q[3][-1] = p[3][-1];
assign q[3][-2] = p[3][-2];
assign q[3][-3] = p[3][-3];
assign q[4][-1] = p[4][-1];
assign q[4][-2] = p[4][-2];
assign q[4][-3] = p[4][-3];
endinterface

module top;
logic [128:0] stim[1:0][4:6];
wire logic [128:0] q[1:0][4:6];
leaf l(stim,q);
initial begin
stim[1][4] = 129'h19;
stim[1][5] = 129'h3e;
stim[1][6] = 129'h63;
stim[0][4] = 129'h88;
stim[0][5] = 129'had;
stim[0][6] = 129'hd2;
#1;
$display("%b %b %b %b %b %b %b %b %b %b %b %b", q[1][4], q[1][5], q[1][6], q[0][4], q[0][5], q[0][6], l.p[3][-1], l.p[3][-2], l.p[3][-3], l.p[4][-1], l.p[4][-2], l.p[4][-3]);
stim[1][4] = 129'h100000000000000000000000000000019;
#1;
$display("%b %b %b %b %b %b %b %b %b %b %b %b", q[1][4], q[1][5], q[1][6], q[0][4], q[0][5], q[0][6], l.p[3][-1], l.p[3][-2], l.p[3][-3], l.p[4][-1], l.p[4][-2], l.p[4][-3]);
stim[1][5] = 129'h3f;
#1;
$display("%b %b %b %b %b %b %b %b %b %b %b %b", q[1][4], q[1][5], q[1][6], q[0][4], q[0][5], q[0][6], l.p[3][-1], l.p[3][-2], l.p[3][-3], l.p[4][-1], l.p[4][-2], l.p[4][-3]);
$display("%0d %0d %0d %0d", $left(stim), $right(stim), $left(l.p), $right(l.p));
end
endmodule
