/*
:name: fixed_array_port_pull_strength_pull1
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
:expected_stdout: @fixed_array_port_pull_strength_pull1.expected
*/
`unconnected_drive pull1
module leaf(input wire [64:0] p[7:6]);assign (weak1,weak0)p[7]='0;endmodule
`nounconnected_drive
module top;leaf l();initial begin #1;$display("%b %b",l.p[7],l.p[6]);end endmodule
