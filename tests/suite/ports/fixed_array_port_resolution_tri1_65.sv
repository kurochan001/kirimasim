/*
:name: fixed_array_port_resolution_tri1_65
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
:expected_stdout: @fixed_array_port_resolution_tri1_65.expected
*/
module top; tri1 [64:0] a[7:4];
assign a[7]='0; assign a[7]='1; assign (weak1,weak0) a[6]='0; assign a[6]='z;
assign a[5]='1; initial begin #1; $display("%b %b %b %b", a[7], a[6], a[5], a[4]); end endmodule
