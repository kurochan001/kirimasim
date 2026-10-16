/*
:name: var_two_output_ports
:description: a variable is driven by the output ports of two instances
:tags: drivers error
:type: elaboration
:lrm: 1800-2023 10.3.2 23.3.3
:oracle: spec
:spec_check: ai
:fourstate: no
:nondeterministic: no
:should_fail_because: a variable is driven by the output ports of two instances
:expected_error: 21:14: error: cannot have multiple continuous assignments to variable 'v'
*/
module src (output logic o);
  assign o = 1'b1;
endmodule

module top;
  logic v;
  src u1 (.o(v));
  src u2 (.o(v));
endmodule
