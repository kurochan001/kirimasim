/*
:name: two_always_ff
:description: a variable is written by two always_ff procedures
:tags: drivers error
:type: elaboration
:lrm: 1800-2023 9.2.2.4
:oracle: spec
:spec_check: ai
:fourstate: no
:nondeterministic: no
:should_fail_because: a variable is written by two always_ff procedures
:expected_error: 17:28: error: variable 'v' driven by always_ff procedure cannot be written to by any other process
*/
module top;
  logic clk, v;
  always_ff @(posedge clk) v <= 1'b1;
  always_ff @(posedge clk) v <= 1'b0;
endmodule
