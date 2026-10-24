/*
:name: fixed_array_port_default_scope
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
:expected_stdout: @fixed_array_port_default_scope.expected
*/
module leaf #(parameter int V=7)(input wire [7:0] p[7:6]='{V, V+1});endmodule
module top;parameter V=99;leaf #(.V(12))l();initial begin #1;$display("%0d %0d",l.p[7],l.p[6]);end endmodule
