/*
:name: fixed_array_port_unconnected_default
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
:expected_stdout: @fixed_array_port_unconnected_default.expected
*/
module leaf(input wire [7:0] p[7:6] = '{8'h12,8'h34});endmodule
module top;leaf l(); initial begin #1;$display("%h %h",l.p[7],l.p[6]);end endmodule
