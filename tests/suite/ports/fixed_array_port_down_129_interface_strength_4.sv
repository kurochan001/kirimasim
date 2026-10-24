/*
:name: fixed_array_port_down_129_interface_strength_4
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
:expected_stdout: @fixed_array_port_down_129_interface_strength_4.expected
*/
interface leaf(input wire logic [128:0] p[-2:-1], output wire logic [128:0] q[-2:-1]);
assign (weak1,weak0) q[-2] = p[-2];
assign (weak1,weak0) q[-1] = p[-1];
endinterface

module top;
logic [128:0] stim[5:4];
wire logic [128:0] q[5:4];
wire logic [128:0] a[5:4];
assign a[5] = stim[5];
assign a[4] = stim[4];
assign (strong1,strong0) q[5] = ~stim[5];
assign (strong1,strong0) q[4] = ~stim[4];
leaf l(a,q);
initial begin
stim[5] = 129'h19;
stim[4] = 129'h3e;
#1;
$display("%b %b %b %b", q[5], q[4], l.p[-2], l.p[-1]);
stim[5] = 129'h100000000000000000000000000000019;
#1;
$display("%b %b %b %b", q[5], q[4], l.p[-2], l.p[-1]);
stim[4] = 129'h3f;
#1;
$display("%b %b %b %b", q[5], q[4], l.p[-2], l.p[-1]);
$display("%0d %0d %0d %0d", $left(stim), $right(stim), $left(l.p), $right(l.p));
end
endmodule
