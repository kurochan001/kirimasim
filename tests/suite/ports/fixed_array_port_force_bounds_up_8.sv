/*
:name: fixed_array_port_force_bounds_up_8
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
:expected_stdout: @fixed_array_port_force_bounds_up_8.expected
*/
module leaf(inout wire logic [7:0] p[7:6]); endmodule
module top; wire logic [7:0] a[4:5]; leaf l(a);
assign a[4] = '0;
assign a[5] = '0;
logic [128:0] idx; initial begin #1; force l.p[7] = '1; #1;
$display("%b %b %b", a[4], l.p[7], a[5]);
release a[4]; #1; $display("%b %b", a[4], l.p[7]);
idx=129'h1000000000000000000000000; $display("%b", l.p[idx]);
idx='x; $display("%b", l.p[idx]);
idx='z; $display("%b", l.p[idx]);
idx=17; $display("%b", l.p[idx]);
$display("%0d %0d", $size(l.p), $dimensions(l.p)); end endmodule
