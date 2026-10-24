/*
:name: fixed_array_port_force_bounds_rank2_129
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
:expected_stdout: @fixed_array_port_force_bounds_rank2_129.expected
*/
module leaf(inout wire logic [128:0] p[3:4][-1:-3]); endmodule
module top; wire logic [128:0] a[1:0][4:6]; leaf l(a);
assign a[1][4] = '0;
assign a[1][5] = '0;
assign a[1][6] = '0;
assign a[0][4] = '0;
assign a[0][5] = '0;
assign a[0][6] = '0;
logic [128:0] idx; initial begin #1; force l.p[3][-1] = '1; #1;
$display("%b %b %b", a[1][4], l.p[3][-1], a[0][6]);
release a[1][4]; #1; $display("%b %b", a[1][4], l.p[3][-1]);
idx=129'h1000000000000000000000000; $display("%b", l.p[idx][-1]);
idx='x; $display("%b", l.p[idx][-1]);
idx='z; $display("%b", l.p[idx][-1]);
idx=13; $display("%b", l.p[idx][-1]);
$display("%0d %0d", $size(l.p), $dimensions(l.p)); end endmodule
