/*
:name: fixed_array_port_partial_wand_8
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
:expected_stdout: @fixed_array_port_partial_wand_8.expected
*/
module top; wand [7:0] a[4:5];
assign a[4]='0;assign a[4][7:4]='1;assign a[5][7:4]='1;assign a[5][3:0]='0;
initial begin #1;$display("%b %b",a[4],a[5]);end endmodule
